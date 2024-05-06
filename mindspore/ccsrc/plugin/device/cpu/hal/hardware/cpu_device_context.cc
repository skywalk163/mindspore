/**
 * Copyright 2021-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/hal/hardware/cpu_device_context.h"
#include <map>
#include <string>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "plugin/device/cpu/hal/device/cpu_memory_manager.h"
#include "plugin/device/cpu/optimizer/reg_cpu_const_input_to_attr.h"
#include "plugin/device/cpu/optimizer/print_value_type.h"
#include "plugin/device/cpu/hal/hardware/cpu_somas.h"
#include "plugin/device/cpu/hal/device/cpu_hash_table_util.h"
#ifdef ENABLE_AKG
#include "plugin/device/cpu/kernel/akg/akg_cpu_kernel_build.h"
#endif
#include "plugin/factory/ms_factory.h"
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "kernel/kernel_build_info.h"
#include "kernel/framework_utils.h"
#include "plugin/device/cpu/hal/device/kernel_select_cpu.h"
#include "utils/trace_base.h"
#include "backend/common/graph_kernel/graph_kernel_flags.h"
#include "include/backend/optimizer/optimizer.h"
#include "include/backend/optimizer/pass_manager.h"
#include "backend/common/optimizer/common_backend_optimization.h"
#include "backend/common/optimizer/dynamic_shape/dynamic_shape_helper.h"
#include "plugin/device/cpu/optimizer/insert_cast_cpu.h"
#include "plugin/device/cpu/optimizer/insert_cast_to_pyexecute.h"
#include "plugin/device/cpu/optimizer/insert_format_transform_op.h"
#include "plugin/device/cpu/optimizer/softmax_grad_fusion.h"
#include "plugin/device/cpu/optimizer/matmul_biasadd_fusion.h"
#include "plugin/device/cpu/optimizer/matmul_biasadd_relu_fusion.h"
#include "backend/common/pass/insert_type_transform_op.h"
#include "backend/common/pass/flatten_value_sequence_in_pyexecute.h"
#include "backend/common/pass/communication_op_fusion.h"
#include "backend/common/pass/replace_node_by_proxy.h"
#include "backend/common/pass/erase_visit_attr.h"
#include "backend/common/pass/add_training_attr.h"
#include "backend/common/pass/insert_tensor_move_for_communication.h"
#include "backend/common/pass/dynamic_sequence_ops_adaptation.h"
#include "backend/common/graph_kernel/adapter/graph_kernel_optimization.h"
#include "backend/common/expander/fallback/expander_fallback.h"
#include "backend/common/graph_kernel/value_graph_binder.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "plugin/device/cpu/hal/profiler/cpu_profiling.h"
#if defined(__linux__) && defined(WITH_BACKEND)
#include "plugin/device/cpu/hal/hardware/ms_collective_comm_lib.h"
#endif
#ifndef ENABLE_SECURITY
#include "include/backend/debug/data_dump/dump_json_parser.h"
#endif
#ifdef ENABLE_DUMP_IR
#include "include/common/debug/anf_ir_dump.h"
#endif
#include "include/common/profiler.h"
#include "plugin/device/cpu/hal/device/cpu_kernel_task.h"
#include "plugin/device/cpu/hal/device/cpu_device_synchronizer.h"
#include "ops/framework_ops.h"
#include "kernel/oplib/oplib.h"

namespace mindspore {
namespace device {
namespace cpu {
namespace {
const char kModelNameCPU[] = "CPU";
const char kEventOptimizeGraph[] = "OptimizeGraph";
const char kStageSetKernelInfo[] = "SetKernelInfo";

runtime::KernelTaskPtr GetTaskByTaskType(const runtime::KernelTaskType &task_type,
                                         const std::shared_ptr<runtime::KernelTaskContext> &task_context) {
  switch (task_type) {
    case runtime::KernelTaskType::kCONTIGUOUS_TASK:
      return std::make_shared<CpuContiguousKernelTask>(task_context);
    case runtime::KernelTaskType::kCOPY_TASK:
      return std::make_shared<CpuCopyWithSliceKernelTask>(task_context);
    default:
      MS_LOG(EXCEPTION) << "KernelTaskType is invalid, task_type:" << task_type;
  }
}
}  // namespace
using mindspore::kernel::KernelBuildInfo;

void CPUDeviceContext::Initialize() {
#ifdef __APPLE__
  std::lock_guard<SpinLock> spin_lock(init_lock_);
#else
  std::lock_guard<std::mutex> lock(init_mutex_);
#endif
  if (initialized_) {
    return;
  }
  MS_EXCEPTION_IF_NULL(device_res_manager_);
  device_res_manager_->Initialize();
  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
#ifndef ENABLE_SECURITY
  if (ms_context->get_param<std::string>(MS_CTX_DEVICE_TARGET) == kCPUDevice) {
    // Dump json config file if dump is enabled.
    uint32_t rank_id = 0;
    auto &json_parser = DumpJsonParser::GetInstance();
    json_parser.Parse();
    json_parser.CopyDumpJsonToDir(rank_id);
    json_parser.CopyMSCfgJsonToDir(rank_id);
  }
#endif
#ifdef __linux__
  if (ms_context->IsDefaultDeviceTarget() && ms_context->get_param<std::string>(MS_CTX_DEVICE_TARGET) == kCPUDevice) {
    MS_LOG(INFO)
      << "No device_target set, set CPU as default. You can call mindspore.set_context(device_target=\"XXX\")";
  }
#endif  // __linux__
  initialized_ = true;
}

void CPUDeviceContext::Destroy() {
  MS_EXCEPTION_IF_NULL(device_res_manager_);
  device_res_manager_->Destroy();
  initialized_ = false;
}

void CPUDeviceResManager::Initialize() {
  mem_manager_ = std::make_shared<CPUMemoryManager>();
  MS_EXCEPTION_IF_NULL(mem_manager_);
}

void CPUDeviceResManager::Destroy() {
  // Release memory.
  if (mem_manager_ != nullptr) {
    mem_manager_->Finalize();
    mem_manager_ = nullptr;
  }
}

void *CPUDeviceResManager::AllocateMemory(size_t size, uint32_t stream_id) const {
  MS_EXCEPTION_IF_NULL(mem_manager_);
  return mem_manager_->MallocMemFromMemPool(size, false, false, stream_id);
}

void CPUDeviceResManager::FreeMemory(void *ptr) const {
  MS_EXCEPTION_IF_NULL(ptr);
  MS_EXCEPTION_IF_NULL(mem_manager_);
  mem_manager_->FreeMemFromMemPool(ptr);
}

void CPUDeviceResManager::FreePartMemorys(const std::vector<void *> &free_addrs, const std::vector<void *> &keep_addrs,
                                          const std::vector<size_t> &keep_addr_sizes) const {
  CPUMemoryPool::GetInstance().FreePartTensorMems(free_addrs, keep_addrs, keep_addr_sizes);
}

std::vector<void *> CPUDeviceResManager::AllocateContinuousMemory(const std::vector<size_t> &size_list,
                                                                  uint32_t stream_id) const {
  MS_EXCEPTION_IF_NULL(mem_manager_);
  return mem_manager_->MallocContinuousMemFromMemPool(size_list, stream_id);
}

namespace {
// Create user data content(such as CPU hash table) and set user data reference into device_address.
void FillUserData(const UserDataPtr &user_data, DeviceAddress *device_address) {
  MS_EXCEPTION_IF_NULL(user_data);
  MS_EXCEPTION_IF_NULL(device_address);

  // Save reference of user data in device address.
  device_address->set_user_data(user_data);

  const auto &user_data_type = user_data->get<UserDataType>(kUserDataType);
  if (user_data_type == nullptr) {
    return;
  }
  if (*user_data_type == UserDataType::kUserTypeHashTable) {
    auto key_type = user_data->get<TypeId>(kHashTableKeyType);
    auto value_type = user_data->get<TypeId>(kHashTableValueType);
    MS_EXCEPTION_IF_NULL(key_type);
    MS_EXCEPTION_IF_NULL(value_type);
    const auto &iter = cpu_hash_table_funcs.find({*key_type, *value_type});
    if (iter != cpu_hash_table_funcs.end()) {
      // Create CPU hash table and set into `user_data`.
      return std::get<kCreateFuncIndex>(iter->second)(user_data);
    } else {
      MS_LOG(EXCEPTION) << "Unsupported hash table type, key type:" << TypeIdLabel(*key_type)
                        << ", value type:" << TypeIdLabel(*value_type);
    }
  } else {
    MS_LOG(EXCEPTION) << "Invalid user data type:" << *user_data_type;
  }
}
}  // namespace

DeviceAddressPtr CPUDeviceResManager::CreateDeviceAddress(const KernelTensorPtr &kernel_tensor) const {
  MS_EXCEPTION_IF_NULL(kernel_tensor);
  if (kernel_tensor->device_name().empty()) {
    kernel_tensor->set_device_name(device_context_->device_context_key().device_name_);
    kernel_tensor->set_device_id(device_context_->device_context_key().device_id_);
  }
  auto device_address = std::make_shared<CPUDeviceAddress>(kernel_tensor);

  const auto &user_data = kernel_tensor->user_data();
  if (user_data != nullptr) {
    FillUserData(user_data, device_address.get());
  }
  device_address->set_device_synchronizer(std::make_shared<CPUDeviceSynchronizer>());
  return device_address;
}

void CPUKernelExecutor::OptimizeGraph(const FuncGraphPtr &graph) const {
  MS_EXCEPTION_IF_NULL(graph);
  auto kernel_graph = graph->cast<KernelGraphPtr>();
  MS_EXCEPTION_IF_NULL(kernel_graph);
  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  auto enable_lazy_inline = ms_context->CellReuseLevel() != CellReuseLevel::kNoCellReuse;
  if (enable_lazy_inline) {
    MS_LOG(EXCEPTION) << "CPU does not support the lazy_inline feature, "
                      << "please do not mark @lazy_inline in cell's __init__ func.";
  }
  if (kernel_graph->is_from_single_op()) {
    SetOperatorInfo(kernel_graph);
    SingleOpGraphOptimize(kernel_graph);
    UpdateKernelRefInfo(kernel_graph);
  } else {
    // The passes in this function must be before ops select: SetOperatorInfo()
    OptimizeMindIR(kernel_graph);
    // Update Graph Dynamic Shape Attr.
    opt::AddDynamicShapeAttrPass(kernel_graph);

    SetOperatorInfo(kernel_graph);
    // SetOperatorInfo may generate new node, so need set kernel object type again.
    kernel_graph->SetKernelObjectTypesForUnrealNodes();
#ifdef ENABLE_DUMP_IR
    if (ms_context->CanDump(kIntroductory)) {
      DumpIR("hwopt_comm_after_kernel_select_" + graph->ToString() + ".ir", graph, true);
    }
#endif

    OptimizeGraphImpl(kernel_graph);

    // Run final optimization.
    opt::CommonFinalOptimization(kernel_graph);

    // Run graph kernel fusion optimization
    if (graphkernel::GraphKernelFlags::GetInstance().IsEnableGraphKernel()) {
      graphkernel::GraphKernelOptimize(kernel_graph);
      kernel_graph->SetExecOrderByDefault();
    }
  }
}

void CPUKernelExecutor::UpdateKernelRefInfo(const KernelGraphPtr &graph) const {
  MS_EXCEPTION_IF_NULL(graph);
  const std::vector<CNodePtr> &kernels = graph->execution_order();
  for (const auto &kernel : kernels) {
    MS_EXCEPTION_IF_NULL(kernel);
    const std::string &op_name = common::AnfAlgo::GetCNodeName(kernel);
    if (IsPrimitiveCNode(kernel, prim::kPrimCustom) &&
        mindspore::kernel::OpLib::FindOp(op_name, kernel::OpImplyType::kImplyCPU) == nullptr) {
      MS_LOG(DEBUG) << "Not find operator information for Custom operator [" << op_name << "]";
      return;
    }

    auto kernel_attr_list = kernel::NativeCpuKernelMod::GetCpuSupportedList(op_name);
    if (kernel_attr_list.empty()) {
      MS_LOG(DEBUG) << "kernel_attr_list is empty";
      return;
    }

    auto kernel_info = dynamic_cast<device::KernelInfo *>(kernel->kernel_info());
    MS_EXCEPTION_IF_NULL(kernel_info);
    kernel_info->set_ref_map(kernel_attr_list[0].GetAllOutInRef(), kernel_attr_list[0].GetOutInRefMap());
  }
}

void CPUKernelExecutor::OptimizeMindIR(const KernelGraphPtr &graph) const {
  MS_EXCEPTION_IF_NULL(graph);
  auto optimizer = std::make_shared<opt::GraphOptimizer>();
  auto pm = std::make_shared<opt::PassManager>();
  pm->AddPass(std::make_shared<opt::SoftmaxGradFusionCpu>("softmax_grad_fusion_cpu"));
  // Match MatMul+BiasAdd+ReLU first, if no match, then match MatMul+BiasAdd
  pm->AddPass(std::make_shared<opt::MatMulBiasAddReluFusionCPU>("matmul_biasadd_relu_fusion_cpu"));
  pm->AddPass(std::make_shared<opt::DynamicSequenceOpsAdaptation>());
  optimizer->AddPassManager(pm);
  (void)optimizer->Optimize(graph);
  graph->SetExecOrderByDefault();
}

void CPUKernelExecutor::OptimizeGraphImpl(const KernelGraphPtr &graph) const {
  MS_EXCEPTION_IF_NULL(graph);
  auto optimizer = std::make_shared<opt::GraphOptimizer>();
  auto pm = std::make_shared<opt::PassManager>();
  pm->AddPass(std::make_shared<opt::InsertTypeTransformOp>("insert_type_transform_op"));
  pm->AddPass(std::make_shared<opt::FlattenValueSequenceInPyExecute>("flatten_value_sequence_in_pyexecute"));
  pm->AddPass(std::make_shared<opt::InsertFormatTransformOpCPU>("insert_format_transform_op_cpu"));
  pm->AddPass(std::make_shared<opt::AllReduceFusion>());
  pm->AddPass(std::make_shared<opt::InsertCastCPU>("insert_cast"));
  pm->AddPass(std::make_shared<opt::EraseVisitAttr>());
  pm->AddPass(std::make_shared<opt::InsertTensorMoveForCommunication>());
  pm->AddPass(std::make_shared<opt::AddTrainingAttr>());
  pm->AddPass(std::make_shared<opt::PrintValueType>("print_value_type"));
  pm->AddPass(std::make_shared<opt::InsertCastToPyExecute>("insert_cast_for_pyexecute"));
  optimizer->AddPassManager(pm);
  (void)optimizer->Optimize(graph);
  graph->SetExecOrderByDefault();
}

void CPUKernelExecutor::SingleOpGraphOptimize(const KernelGraphPtr &graph) const {
  MS_EXCEPTION_IF_NULL(graph);
  auto optimizer = std::make_shared<opt::GraphOptimizer>();
  auto pm = std::make_shared<opt::PassManager>();
  pm->AddPass(std::make_shared<opt::InsertCastCPU>("insert_cast"));
  optimizer->AddPassManager(pm);
  (void)optimizer->Optimize(graph);
  graph->SetExecOrderByDefault();
}

namespace {
void SetControlOpInfo(const CNodePtr &kernel_node) {
  MS_EXCEPTION_IF_NULL(kernel_node);
  std::vector<std::string> inputs_format;
  std::vector<TypeId> inputs_type;
  size_t input_num = common::AnfAlgo::GetInputTensorNum(kernel_node);
  for (size_t input_index = 0; input_index < input_num; ++input_index) {
    (void)inputs_format.emplace_back(kOpFormat_DEFAULT);
    inputs_type.push_back(common::AnfAlgo::GetPrevNodeOutputInferDataType(kernel_node, input_index));
  }
  std::vector<std::string> outputs_format;
  std::vector<TypeId> outputs_type;
  size_t output_num = AnfAlgo::GetOutputTensorNum(kernel_node);
  for (size_t output_index = 0; output_index < output_num; ++output_index) {
    (void)outputs_format.emplace_back(kOpFormat_DEFAULT);
    outputs_type.push_back(common::AnfAlgo::GetOutputInferDataType(kernel_node, output_index));
  }

  auto builder = std::make_shared<KernelBuildInfo::KernelBuildInfoBuilder>();
  builder->SetInputsFormat(inputs_format);
  builder->SetInputsDeviceType(inputs_type);
  builder->SetOutputsFormat(outputs_format);
  builder->SetOutputsDeviceType(outputs_type);

  AnfAlgo::SetSelectKernelBuildInfo(builder->Build(), kernel_node.get());
}

// Before creating the kernel, check whether the node has completed the operator selection. If not, the operator
// selection needs to be performed to set kernel info.
void SetKernelInfoBeforeCreateKernel(const std::vector<CNodePtr> &nodes) {
  // Check whether the node has completed operator selection.
  for (const auto &node : nodes) {
    if (AnfAlgo::GetSelectKernelBuildInfo(node) != nullptr) {
      continue;
    }

    // Kernel selection process for non control op.
    if (!common::AnfAlgo::IsBpropCutOpExecInBackend(node)) {
      auto [msg, etype] = SetKernelInfoWithMsg(node);
      if (!msg.empty()) {
        MS_EXCEPTION(etype) << "#umsg#Kernel select failed:#umsg#" << msg;
      }
    } else {
      // Kernel selection process for control op.
      SetControlOpInfo(node);
    }
  }
}
}  // namespace

void CPUKernelExecutor::SetOperatorInfo(const KernelGraphPtr &graph) const {
  MS_EXCEPTION_IF_NULL(graph);
  (void)profiler::CollectHostInfo(kModelNameCPU, kEventOptimizeGraph, kStageSetKernelInfo, 1, 0, 0);
  bool do_expand = false;
  auto mng = graph->manager();
  if (mng == nullptr) {
    mng = Manage(graph, true);
    MS_EXCEPTION_IF_NULL(mng);
    graph->set_manager(mng);
  }
  auto &node_list = graph->execution_order();
  for (auto &node : node_list) {
    if (!common::AnfAlgo::IsBpropCutOpExecInBackend(node)) {
      auto [msg, etype] = SetKernelInfoWithMsg(node);
      if (msg.empty()) {
        continue;
      }
      auto f = [](const CNodePtr &n) {
        auto res = SetKernelInfoWithMsg(n);
        return res.first.empty();
      };
      auto expand_ret = expander::TryExpandCNode(node, f);
      if (!expand_ret) {
        constexpr auto recursive_level = 2;
        MS_EXCEPTION(etype) << "#umsg#Kernel select failed:#umsg#" << msg
                            << "\nnode: " << node->DebugString(recursive_level);
      }
      MS_LOG(INFO) << msg << " but expand success.";
      do_expand = true;
    } else {
      SetControlOpInfo(node);
    }
  }
  if (do_expand) {
    (void)graphkernel::BindValueToGraph().Run(graph);
    graph->SetExecOrderByDefault();
  }
  (void)profiler::CollectHostInfo(kModelNameCPU, kEventOptimizeGraph, kStageSetKernelInfo, 1, 0, 1);
}

kernel::KernelModPtr CPUKernelExecutor::CreateKernelMod(const std::string &op_name) const {
  return kernel::Factory<kernel::NativeCpuKernelMod>::Instance().Create(op_name);
}

void CPUKernelExecutor::CreateKernel(const std::vector<CNodePtr> &nodes) const {
  SetKernelInfoBeforeCreateKernel(nodes);

  kernel::KernelMeta *bin_map = kernel::KernelMeta::GetInstance();
  std::vector<AnfNodePtr> akg_nodes;
  for (const auto &node : nodes) {
    MS_EXCEPTION_IF_NULL(node);
    if (common::AnfAlgo::IsBpropCutOpExecInBackend(node)) {
      continue;
    }
    if (session::AnfRuntimeAlgorithm::GetKernelType(node) == KernelType::AKG_KERNEL) {
      if (!bin_map->initialized()) {
        bin_map->Initialize();
      }
      akg_nodes.push_back(node);
      continue;
    }
    std::string kernel_name = common::AnfAlgo::GetCNodeName(node);

    std::shared_ptr<kernel::NativeCpuKernelMod> cpu_kernel =
      kernel::Factory<kernel::NativeCpuKernelMod>::Instance().Create(kernel_name);

    if (cpu_kernel == nullptr) {
      MS_LOG(INTERNAL_EXCEPTION) << "#dmsg#Kernel build failed:#dmsg#Build cpu operator[" << node->fullname_with_scope()
                                 << "] failed";
    }

    auto kernel_attrs = cpu_kernel->GetOpSupport();
    kernel::SetCpuRefMapToKernelInfo(node, kernel_attrs);
    auto thread_pool = kernel::GetActorMgrInnerThreadPool();
    cpu_kernel->SetThreadPool(thread_pool);
    std::vector<KernelTensor *> input_kernel_tensors = AnfAlgo::GetOrCreateAllInputKernelTensors(node);
    std::vector<KernelTensor *> output_kernel_tensors = AnfAlgo::GetOrCreateAllOutputKernelTensors(node);
    auto ret = cpu_kernel->Init(common::AnfAlgo::GetCNodePrimitive(node), input_kernel_tensors, output_kernel_tensors);
    if (!ret) {
      MS_LOG(EXCEPTION) << trace::DumpSourceLines(node);
    }
    if (kernel::CheckResizeCondition(node)) {
      if (cpu_kernel->Resize(input_kernel_tensors, output_kernel_tensors) == kernel::KRET_RESIZE_FAILED) {
        MS_LOG(INTERNAL_EXCEPTION) << "#dmsg#Kernel build failed:#dmsg#CPU kernel op [" << node->fullname_with_scope()
                                   << "] resize failed.";
      }
    }

    AnfAlgo::SetKernelMod(cpu_kernel, node.get());
  }
#ifdef ENABLE_AKG
  kernel::AkgCpuKernelBuilder akg_cpu_kernel_builder;
  (void)akg_cpu_kernel_builder.SingleOpParallelBuild(akg_nodes);
#endif
}

void CPUKernelExecutor::PreprocessBeforeRun(const FuncGraphPtr &graph) const {
  MS_EXCEPTION_IF_NULL(graph);
  auto kernel_graph = graph->cast<KernelGraphPtr>();
  MS_EXCEPTION_IF_NULL(kernel_graph);
  if (!kernel_graph->is_from_single_op()) {
    // Remove reorder after PS feature finish adapting push/pull in auto_monad.
    auto execution_order = kernel_graph->execution_order();
    common::AnfAlgo::ReorderPosteriorExecList(NOT_NULL(&execution_order));
    kernel_graph->set_execution_order(execution_order);
  }
  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  // somas
  if (ms_context->get_param<int>(MS_CTX_MEMORY_OPTIMIZE_LEVEL) != kOptimizeO0) {
    auto somas = std::make_shared<CPUSomas>();
    bool ret = somas->Assign(kernel_graph);
    if (ret) {
      MS_LOG(INFO) << "Somas allocate success for graph " << kernel_graph->graph_id()
                   << " somas size: " << kernel_graph->somas_whole_block_size();
    } else if (somas->IsSupportSomas(*kernel_graph)) {
      MS_LOG(WARNING) << "Somas allocate failed for graph " << kernel_graph->graph_id();
    }
  }
  MS_LOG(INFO) << "Status record: end preprocess before run graph. graph id: " << kernel_graph->graph_id();
}

bool CPUKernelExecutor::LaunchKernel(const CNodePtr &kernel, const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &workspace,
                                     const std::vector<KernelTensor *> &outputs, KernelMod *kernel_mod,
                                     void * /* stream*/) const {
  MS_EXCEPTION_IF_NULL(kernel);

#ifndef ENABLE_SECURITY
  const auto &profiler_inst = profiler::cpu::CPUProfiler::GetInstance();
  MS_EXCEPTION_IF_NULL(profiler_inst);
  if (profiler_inst->GetEnableFlag() && profiler_inst->GetOpTimeFlag()) {
    auto ret = LaunchKernelWithProfiling(kernel, inputs, workspace, outputs, kernel_mod);
    return ret;
  }
#endif
  auto ret = DoLaunchKernel(kernel, inputs, workspace, outputs, kernel_mod);
  return ret;
}

bool CPUKernelExecutor::ExecuteKernelTask(const runtime::KernelTaskType &task_type,
                                          const device::DeviceAddressPtrList &input_addr_list,
                                          const device::DeviceAddressPtrList &output_addr_list,
                                          const size_t &stream_id) const {
  auto task_context =
    std::make_shared<runtime::KernelTaskContext>(device_context_, input_addr_list, output_addr_list, nullptr);
  auto task = GetTaskByTaskType(task_type, task_context);
  MS_EXCEPTION_IF_NULL(task);

  auto ret = task->RunWithRet();
  if (!ret) {
    MS_LOG(EXCEPTION) << "Exec task failed, task_type:" << task_type;
  }
  return ret;
}

bool CPUDeviceResManager::LoadCollectiveCommLib() {
  bool using_mpi = common::UseMPI();
  if (using_mpi) {
    std::string mpi_comm_lib_name = "libmpi_collective.so";
    auto loader = std::make_shared<CollectiveCommLibLoader>(mpi_comm_lib_name);
    MS_EXCEPTION_IF_NULL(loader);
    if (!loader->Initialize()) {
      MS_LOG(EXCEPTION) << "Failed to load mpi collective library.";
    }

    void *collective_comm_lib_handle = loader->collective_comm_lib_ptr();
    MS_EXCEPTION_IF_NULL(collective_comm_lib_handle);

    auto instance_func = DlsymFuncObj(communication_lib_instance, collective_comm_lib_handle);
    collective_comm_lib_ = instance_func();
    MS_EXCEPTION_IF_NULL(collective_comm_lib_);
  } else {
#if defined(__linux__) && defined(WITH_BACKEND)
    collective_comm_lib_ = &MsCollectiveCommLib::GetInstance();
    MS_EXCEPTION_IF_NULL(collective_comm_lib_);
#endif
  }
  return true;
}

bool CPUKernelExecutor::LaunchKernelWithProfiling(const CNodePtr &kernel, const std::vector<KernelTensor *> &inputs,
                                                  const std::vector<KernelTensor *> &workspace,
                                                  const std::vector<KernelTensor *> &outputs,
                                                  KernelMod *kernel_mod) const {
  MS_EXCEPTION_IF_NULL(kernel);

  auto profiler_inst = profiler::cpu::CPUProfiler::GetInstance();
  MS_EXCEPTION_IF_NULL(profiler_inst);

  uint32_t pid = IntToUint(getpid());
  // cpu support multi-thread with mindrt for profiling.
  profiler_inst->OpDataProducerBeginParallel(kernel->fullname_with_scope(), pid);
  bool ret = DoLaunchKernel(kernel, inputs, workspace, outputs, kernel_mod);
  profiler_inst->OpDataProducerEndParallel(kernel->fullname_with_scope());
  profiler_inst->RecordFrameWorkInfo(kernel);
  return ret;
}

bool CPUKernelExecutor::DoLaunchKernel(const CNodePtr &kernel, const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &workspace,
                                       const std::vector<KernelTensor *> &outputs, KernelMod *kernel_mod) const {
  MS_EXCEPTION_IF_NULL(kernel);
  MS_EXCEPTION_IF_NULL(kernel_mod);
  uint64_t start_time = 0;
  PROFILER_START(start_time);
  auto ret = kernel_mod->Launch(inputs, workspace, outputs, nullptr);
  PROFILER_END(start_time, runtime::ProfilerModule::kKernel, runtime::ProfilerEvent::kKernelLaunch,
               kernel->fullname_with_scope(), false);
  return ret;
}

void CPUKernelExecutor::RebuildKernelSelectBackoffOp(const std::vector<CNodePtr> &nodes) const {
  for (auto &node : nodes) {
    MS_EXCEPTION_IF_NULL(node);
    if (!AnfAlgo::IsKernelSelectBackoffOp(node)) {
      continue;
    }
    auto [failure_info, failure_type] = AnfAlgo::GetKernelSelectBackoffInfo(node);
    if (IsVmapNotSupported(node)) {
      MS_EXCEPTION(failure_type) << "#umsg#Kernel select failed:#umsg#" << failure_info;
    }

    // Judge whether match strictly between kernel build info and supported kernel attrs.
    const auto &kernel_build_info = AnfAlgo::GetSelectKernelBuildInfo(node);
    MS_EXCEPTION_IF_NULL(kernel_build_info);
    const auto &kernel_attr = kernel::GetKernelAttrFromBuildInfo(kernel_build_info);
    const auto &supported_kernel_attrs =
      kernel::NativeCpuKernelMod::GetCpuSupportedList(common::AnfAlgo::GetCNodeName(node));
    const auto &match_result = kernel::MatchKernelAttrStrict(kernel_attr, supported_kernel_attrs);
    auto attr_info = kernel::FetchPrintInfoByKernelAttr(kernel_attr);
    if (!match_result.first) {
      MS_LOG(INFO) << "Backoff and rebuild kernel on CPU failed for node: " << node->fullname_with_scope()
                   << ", node attr: " << attr_info;
      MS_EXCEPTION(failure_type) << "#umsg#Kernel select failed:#umsg#" << failure_info;
    } else {
      // Set the CPU flag.
      common::AnfAlgo::SetNodeAttr(kAttrPrimitiveTarget, MakeValue(kCPUDevice), node);
      kernel_build_info->set_kernel_type(CPU_KERNEL);
      kernel_build_info->set_processor(kernel::Processor::CPU);
      MS_LOG(INFO) << "Backoff and rebuild kernel on CPU successfully for node: " << node->fullname_with_scope()
                   << ", node attr: " << attr_info;
    }

    CreateKernel({node});
  }
}

MS_REGISTER_DEVICE(kCPUDevice, CPUDeviceContext);
#ifdef WITH_BACKEND
MSCONTEXT_REGISTER_INIT_FUNC(kCPUDevice, [](MsContext *ctx) -> void {
  MS_EXCEPTION_IF_NULL(ctx);
  if (ctx->backend_policy() != "ms") {
    (void)ctx->set_backend_policy("ms");
  }
});
#endif

// Register functions to _c_expression so python hal module could call CPU device interfaces.
void PybindCPUStatelessFunc(py::module *m) { MS_EXCEPTION_IF_NULL(m); }
REGISTER_DEV_STATELESS_FUNC_CB(kCPUDevice, PybindCPUStatelessFunc);
}  // namespace cpu
}  // namespace device
}  // namespace mindspore
