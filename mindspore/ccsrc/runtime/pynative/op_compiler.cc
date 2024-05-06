/**
 * Copyright 2022-2024 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "runtime/pynative/op_compiler.h"

#include <memory>
#include <algorithm>
#include <vector>
#include <unordered_set>
#include "mindspore/core/ops/op_utils.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "ops/nn_op_name.h"
#include "ops/conv_pool_op_name.h"
#include "runtime/pynative/op_executor.h"
#include "runtime/pynative/op_runtime_info.h"
#include "runtime/device/device_address_utils.h"
#include "backend/common/optimizer/common_backend_optimization.h"
#ifdef ENABLE_D
#include "transform/acl_ir/acl_adapter_info.h"
#endif

namespace mindspore {
using runtime::DeviceAddressUtils;
namespace pynative {
namespace {
using KernelWithIndex = std::pair<AnfNodePtr, size_t>;
mindspore::HashSet<std::string> kExcludedAttr = {"input_names", "output_names", "IsFeatureMapOutput",
                                                 "IsFeatureMapInputList", "pri_format"};
std::vector<std::string> kNumStrCache;

inline std::string GetNumString(int n) {
  if (n >= static_cast<int>(kNumStrCache.size())) {
    return std::to_string(n);
  }

  return kNumStrCache[n];
}

void UpdateRefInfoBeforeCreateKernel(const session::BackendOpRunInfoPtr &op_run_info, const KernelGraphPtr &graph) {
  // Building Graph and Create Kernel is async, under pynative mode.Ref info is bind with kernel.
  // So need to get ref info to generate output addr, before create kernel.
  if (op_run_info->base_op_run_info.device_target != kCPUDevice &&
      op_run_info->base_op_run_info.device_target != kGPUDevice) {
    // just ascend ref mode is diff with cpu and gpu
    return;
  }

  AnfAlgo::AddOutInRefToGraph(graph);
}

void CreateDeviceAddressWithoutWorkspace(const KernelGraphPtr &graph, const DeviceContext *device_context,
                                         bool is_gradient_out) {
  DeviceAddressUtils::CreateParameterDeviceAddress(device_context, graph);
  DeviceAddressUtils::CreateValueNodeDeviceAddress(device_context, graph);
  DeviceAddressUtils::CreateKernelOutputDeviceAddress(device_context, graph, is_gradient_out);
  DeviceAddressUtils::UpdateDeviceAddressForInplaceNode(graph);
  DeviceAddressUtils::UpdateDeviceAddressForRefNode(graph);
}

void SetIgnoreSyncHostToDeviceList(const SimpleGraphPtr &simple_graph) {
  const auto &single_ops = simple_graph->single_ops_;
  for (const auto &single_op : single_ops) {
    const auto &kernel = single_op->kernel_;
    const auto &edges = single_op->inputs_;

    auto kernel_mod = AnfAlgo::GetKernelMod(kernel);
    MS_EXCEPTION_IF_NULL(kernel_mod);
    std::vector<size_t> ignore_input_index_list = kernel_mod->GetLaunchIgnoredInputAddressIdx();
    for (size_t index : ignore_input_index_list) {
      // Some input may be converted to attribute or input size is wrong.
      // This behavior is incorrect, but it does exist in the current kernel
      // and needs to be rectified by the operators who develop this kernel.
      if (index >= edges.size()) {
        MS_LOG(INFO) << simple_graph->name_ << " ignore input index is " << index << ", but total input num is "
                     << edges.size();
        continue;
      }
      edges[index]->ignore_h2d_ = true;
      MS_LOG(INFO) << "For graph " << simple_graph->name_ << " ignore input host to device " << index;
    }
  }
}
}  // namespace

OpCompiler::OpCompiler() {
  session_ = session::SessionFactory::Get().Create(kSessionBasic);
  for (size_t i = 0; i < kNumberTypeEnd; i++) {
    (void)kNumStrCache.emplace_back(std::to_string(i));
  }
}

OpCompiler &OpCompiler::GetInstance() {
  static OpCompiler instance;
  return instance;
}

void OpCompilerInfo::UpdateStatus(bool ready) { ready_.store(ready, std::memory_order_release); }

void OpCompilerInfo::WaitReady() const {
  runtime::ProfilerRecorder profiler(runtime::ProfilerModule::kPynative, runtime::ProfilerEvent::kWaitTaskFinish,
                                     graph_info_, true);
  while (!ready_.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
}

bool OpCompiler::IsInvalidInferResultOp(const std::string &op_name) const {
  static const std::unordered_set<std::string> kInvalidInferResultOp = {kDropoutOpName, kMaxPoolWithArgmaxOpName,
                                                                        kLSTMOpName};
  return kInvalidInferResultOp.find(op_name) != kInvalidInferResultOp.end();
}

KernelGraphPtr OpCompiler::GenerateKernelGraph(const session::BackendOpRunInfoPtr &op_run_info,
                                               const device::DeviceContext *device_context) const {
  MS_EXCEPTION_IF_NULL(session_);
  MS_EXCEPTION_IF_NULL(device_context);
  MS_EXCEPTION_IF_NULL(op_run_info->op_prim);
  KernelGraphPtr graph;
  graph = session_->ConstructSingleOpGraph(op_run_info, op_run_info->base_op_run_info.expanded_input_values,
                                           op_run_info->base_op_run_info.input_types);
  graph->set_is_from_single_op(true);
  return graph;
}

void OpCompiler::AssignStreamIdForSingleOpGraph(const KernelGraphPtr &graph, uint32_t stream_id) {
  MS_EXCEPTION_IF_NULL(graph);

  for (const auto &cnode : graph->execution_order()) {
    MS_EXCEPTION_IF_NULL(cnode);
    AnfAlgo::SetStreamId(stream_id, cnode.get());
    size_t input_num = common::AnfAlgo::GetInputTensorNum(cnode);
    for (size_t index = 0; index < input_num; ++index) {
      const auto &input_node = common::AnfAlgo::GetInputNode(cnode, index);
      AnfAlgo::SetStreamId(stream_id, input_node.get());
    }
  }
}

OpCompilerInfoPtr OpCompiler::Compile(const session::BackendOpRunInfoPtr &op_run_info, bool *single_op_cache_hit,
                                      const std::string &device_name, const uint32_t &device_id) {
  MS_EXCEPTION_IF_NULL(op_run_info);
  const auto &graph_info = GetSingleOpGraphInfo(op_run_info->base_op_run_info, op_run_info->op_prim);
  const auto &iter = op_compiler_infos_.find(graph_info);
  // Check if the graph cache exists.
  if (iter != op_compiler_infos_.end()) {
    MS_EXCEPTION_IF_NULL(iter->second);
    const auto &op_compiler_info = iter->second;
    MS_EXCEPTION_IF_NULL(op_compiler_info);
    *single_op_cache_hit = true;
    return iter->second;
  }

  MS_LOG(INFO) << "Run Op cache miss " << graph_info;
  runtime::ProfilerRecorder profiler(runtime::ProfilerModule::kPynative, runtime::ProfilerEvent::kPyNativeOpCompile,
                                     graph_info, true);

  *single_op_cache_hit = false;
  // Generate kernel graph.
  MS_EXCEPTION_IF_NULL(session_);
  const auto &device_context =
    device::DeviceContextManager::GetInstance().GetOrCreateDeviceContext({device_name, device_id});
  MS_EXCEPTION_IF_NULL(device_context);
  device_context->Initialize();
  py::gil_scoped_acquire acquire_gil;
  KernelGraphPtr graph = GenerateKernelGraph(op_run_info, device_context);
  MS_EXCEPTION_IF_NULL(graph);

  graph->set_run_mode(device::RunMode::kKernelMode);
  bool use_dynamic_shape_process = op_run_info->base_op_run_info.use_dynamic_shape_process;
  auto kernel_executor = device_context->GetKernelExecutor(use_dynamic_shape_process);
  MS_EXCEPTION_IF_NULL(kernel_executor);

  opt::OptimizationWithoutBackend(graph);
  // Unify the MindIR, must be before of the graph optimization.
  kernel_executor->AddMindIRPass(graph);

  // Select kernel and optimize
  kernel_executor->OptimizeGraph(graph);

  UpdateRefInfoBeforeCreateKernel(op_run_info, graph);
  AssignStreamIdForSingleOpGraph(graph, op_run_info->base_op_run_info.stream_id);
  // Create device address for all anf nodes of graph.
  CreateDeviceAddressWithoutWorkspace(graph, device_context, op_run_info->is_gradient_out);

  auto output_nodes = graph->outputs();
  std::vector<KernelWithIndex> outputs_with_index;
  std::vector<size_t> outputs_tensor_num;
  std::vector<std::string> outputs_padding_type;
  bool need_refresh_abstract = IsInvalidInferResultOp(op_run_info->base_op_run_info.op_name);
  for (auto &node : output_nodes) {
    MS_EXCEPTION_IF_NULL(node);
    const auto &output_with_index = common::AnfAlgo::VisitKernel(node, 0);
    (void)outputs_with_index.emplace_back(output_with_index);
    (void)outputs_tensor_num.emplace_back(AnfAlgo::GetOutputTensorNum(output_with_index.first));
    const auto &padding_type = (device_context->GetDeviceType() == device::DeviceType::kAscend
                                  ? AnfAlgo::GetOutputReshapeType(output_with_index.first, output_with_index.second)
                                  : "");
    (void)outputs_padding_type.emplace_back(padding_type);

    MS_EXCEPTION_IF_NULL(output_with_index.first);
    const auto &abstract = output_with_index.first->abstract();
    MS_EXCEPTION_IF_NULL(abstract);
    const auto &shape = abstract->BuildShape();
    MS_EXCEPTION_IF_NULL(shape);
    if (shape->IsDynamic()) {
      need_refresh_abstract = true;
    }
  }
  AnfAlgo::UpdateGraphValidRefPair(graph);
  UpdateRefNodeOutputDeviceAddress(graph);
  auto simple_graph = IrConverter::Convert(op_run_info->base_op_run_info.op_name, graph, device_context);
  MS_LOG(DEBUG) << "DEBUG generate new IR " << simple_graph->DebugInfo().dump();

  auto op_compiler_info = std::make_shared<OpCompilerInfo>(
    graph_info, graph->graph_id(), graph, device_context, op_run_info->base_op_run_info.need_earse_cache,
    need_refresh_abstract, outputs_with_index, outputs_tensor_num, outputs_padding_type, std::move(simple_graph));

  graph->set_graph_info(graph_info);
  op_compiler_infos_[graph_info] = op_compiler_info;
  return op_compiler_info;
}

void OpCompiler::KernelBuild(const OpCompilerInfoPtr &op_compiler_info, const DeviceContext *device_context,
                             bool is_dynamic) const {
  MS_EXCEPTION_IF_NULL(device_context);
  MS_EXCEPTION_IF_NULL(device_context->device_res_manager_);
  // The compilation task may be in a child thread that has not yet set rt_context,
  // but the AICPU.so loading needs to use rt_context
  if (!device_context->device_res_manager_->BindDeviceToCurrentThread(true)) {
    MS_LOG(EXCEPTION) << "Bind device failed";
  }
  std::vector<CNodePtr> node_to_build;
  const auto &graph = op_compiler_info->graph_;
  MS_EXCEPTION_IF_NULL(graph);
  const auto &nodes = graph->execution_order();
  (void)std::copy(nodes.begin(), nodes.end(), std::back_inserter(node_to_build));
  // Kernel build
  auto kernel_executor = device_context->GetKernelExecutor(is_dynamic);
  MS_EXCEPTION_IF_NULL(kernel_executor);
  kernel_executor->CreateKernel(node_to_build);
  kernel_executor->PreprocessBeforeRun(graph);
  DeviceAddressUtils::CreateKernelWorkspaceDeviceAddress(device_context, graph);
  // Need to execute after PreprocessBeforeRunSingleOpGraph
  runtime::OpRuntimeInfo::CacheGraphOpRuntimeInfo(graph);

  // After kernel generated.
  SetIgnoreSyncHostToDeviceList(op_compiler_info->simple_graph_);
}

#ifdef ENABLE_D
std::string GetGraphInfoForAscendSpecial(const pynative::BaseOpRunInfo &op_info, const PrimitivePtr &op_prim,
                                         const std::string &graph_info) {
  std::string ascend_special_info = graph_info;
  MS_EXCEPTION_IF_NULL(op_prim);
  auto op_name = op_prim->name();
  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  if (ms_context->get_param<std::string>(MS_CTX_DEVICE_TARGET) == kAscendDevice &&
      transform::AclAdapterManager::GetInstance().CheckAclAdapter(op_name)) {
    auto acl_info = transform::AclAdapterManager::GetInstance().GetOpInfo(op_name);
    if (!acl_info.input_selector().empty() || acl_info.output_selector() != nullptr) {
      if (op_info.expanded_input_values.size() == 0) {
        return ascend_special_info;
      }
      TypeId first_dtype = TypeId::kTypeUnknown;
      std::vector<ShapeVector> input_shapes;
      (void)std::transform(op_info.expanded_input_values.begin(), op_info.expanded_input_values.end(),
                           std::back_inserter(input_shapes), [&first_dtype](const ValuePtr &value) -> ShapeVector {
                             auto tensor = value->cast<tensor::TensorPtr>();
                             if (tensor != nullptr) {
                               if (first_dtype == TypeId::kTypeUnknown) {
                                 first_dtype = tensor->data_type();
                               }
                               return tensor->shape();
                             }
                             return {};
                           });

      auto in_func_map = acl_info.input_selector();
      for (auto [index, in_func] : in_func_map) {
        MS_EXCEPTION_IF_NULL(in_func);
        auto tensor = op_info.expanded_input_values[index]->cast<tensor::TensorPtr>();
        MS_EXCEPTION_IF_NULL(tensor);
        ascend_special_info += in_func(tensor->data_type(), input_shapes);
      }

      auto out_func = acl_info.output_selector();
      if (out_func != nullptr) {
        auto tensor = op_info.expanded_input_values[0]->cast<tensor::TensorPtr>();
        MS_EXCEPTION_IF_NULL(tensor);
        auto out_format = out_func(tensor->data_type(), input_shapes);
        ascend_special_info += out_format;
      }
      MS_EXCEPTION_IF_NULL(out_func);
      auto tensor = op_info.expanded_input_values[0]->cast<tensor::TensorPtr>();
      MS_EXCEPTION_IF_NULL(tensor);
      auto out_format = out_func(tensor->data_type(), input_shapes);
      ascend_special_info += out_format;
    }
  }
  return ascend_special_info;
}
#endif

inline std::set<int64_t> GetDependList(const pynative::BaseOpRunInfo &op_info, const PrimitivePtr &op_prim) {
  auto depend_list = mindspore::ops::GetInputDependValueList(op_prim);
  if (!op_info.dyn_input_sizes.empty()) {
    auto list_tmp = depend_list;
    depend_list.clear();
    for (const auto item : list_tmp) {
      int64_t bias = 0;
      for (int64_t i = 0; i < item; i++) {
        auto idx = static_cast<size_t>(i);
        if (op_info.dyn_input_sizes[idx] == -1) {
          bias += 1;
        } else {
          bias += op_info.dyn_input_sizes[idx];
        }
      }
      (void)depend_list.emplace(bias);
      MS_LOG(DEBUG) << "Adjust depend list from " << item << " to " << bias << " for op: " << op_prim->name();
    }
  }

  return depend_list;
}

std::string OpCompiler::GetSingleOpGraphInfo(const pynative::BaseOpRunInfo &op_info,
                                             const PrimitivePtr &op_prim) const {
  MS_EXCEPTION_IF_NULL(op_prim);
  if (op_info.expanded_input_values.size() != op_info.input_types.size()) {
    MS_LOG(EXCEPTION) << "Input tensors size " << op_info.expanded_input_values.size()
                      << " should be equal to tensors mask size " << op_info.input_types.size();
  }
  std::string graph_info = op_info.device_target;

  if (op_info.use_dynamic_shape_process) {
    graph_info += "_1_";
  } else {
    graph_info += "_0_";
  }
  auto op_name = op_prim->name();
  graph_info += op_name;
  bool has_hidden_side_effect;
  {
    PrimitiveReadLock read_lock(op_prim->shared_mutex());
    if (op_info.need_earse_cache) {
      return graph_info;
    }
    has_hidden_side_effect = op_prim->HasAttr(GRAPH_FLAG_SIDE_EFFECT_HIDDEN);
    // The value of the attribute affects the operator selection
    const auto &attr_map = op_prim->attrs();
    (void)std::for_each(attr_map.begin(), attr_map.end(), [&graph_info](const auto &element) {
      if (kExcludedAttr.find(element.first) != kExcludedAttr.end()) {
        return;
      }
      MS_EXCEPTION_IF_NULL(element.second);
      graph_info.append(element.second->ToString());
    });
  }

  const auto &depend_list = GetDependList(op_info, op_prim);
  for (size_t index = 0; index < op_info.expanded_input_values.size(); ++index) {
    auto const &value = op_info.expanded_input_values[index];
    if (value->isa<tensor::Tensor>()) {
      const auto &input_tensor = value->cast<tensor::TensorPtr>();
      MS_EXCEPTION_IF_NULL(input_tensor);
      if (op_info.use_dynamic_shape_process) {
        graph_info += GetNumString(static_cast<int>(input_tensor->shape().size()));
      } else {
        if (input_tensor->base_shape_ptr() != nullptr) {
          graph_info += input_tensor->base_shape_ptr()->ToString();
        } else if (!input_tensor->shape().empty()) {
          const auto &shape_str =
            std::accumulate(std::next(input_tensor->shape().begin()), input_tensor->shape().end(),
                            std::to_string(input_tensor->shape()[0]),
                            [](std::string cur, size_t n) { return cur.append("-").append(std::to_string(n)); });
          graph_info += shape_str;
        }
      }

      graph_info += GetNumString(input_tensor->data_type());
      // In the case of the same shape, but dtype and format are inconsistent
      auto tensor_addr = input_tensor->device_address();
      if (tensor_addr != nullptr && !has_hidden_side_effect) {
        auto p_address = std::dynamic_pointer_cast<device::DeviceAddress>(tensor_addr);
        MS_EXCEPTION_IF_NULL(p_address);
        graph_info += p_address->format();
        graph_info += p_address->padding_type();
      }

      if (op_info.input_types[index] == InputType::kConstant || depend_list.find(index) != depend_list.end()) {
        graph_info += common::AnfAlgo::GetTensorValueString(input_tensor);
      }
    } else {
      graph_info += value->ToString();
    }

    graph_info += "_";
  }

  graph_info += std::to_string(op_info.stream_id);

  // Operator with hidden side effect.
  if (has_hidden_side_effect) {
    (void)graph_info.append("r_").append(std::to_string(op_info.py_prim_id_)).append("_");
  }

#ifdef ENABLE_D
  // Ascend special info.
  graph_info = GetGraphInfoForAscendSpecial(op_info, op_prim, graph_info);
#endif

  return graph_info;
}

void OpCompiler::ClearOpCache(const GraphInfo &graph_info) { (void)op_compiler_infos_.erase(graph_info); }

void OpCompiler::ClearAllCache() { op_compiler_infos_.clear(); }

void OpCompiler::UpdateRefNodeOutputDeviceAddress(const KernelGraphPtr &graph) {
  MS_EXCEPTION_IF_NULL(graph);
  auto ref_node_map = graph->GetRefMap();
  for (const auto &[output_pair, input_pair] : ref_node_map) {
    const auto &[ref_node, output_index] = output_pair;
    const auto &[input_node, input_node_output_index] = input_pair;
    if (!AnfAlgo::OutputAddrExist(input_node, input_node_output_index, false)) {
      MS_EXCEPTION_IF_NULL(input_node);
      MS_LOG(WARNING) << "Output address not exist, node " << input_node->fullname_with_scope() << " index "
                      << input_node_output_index;
      continue;
    }
    auto input_addr = AnfAlgo::GetMutableOutputAddr(input_node, input_node_output_index, false);
    AnfAlgo::SetOutputAddr(input_addr, output_index, ref_node.get());
  }
}
}  // namespace pynative
}  // namespace mindspore
