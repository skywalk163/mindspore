/**
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_BACKEND_SESSION_SESSION_BASIC_H
#define MINDSPORE_CCSRC_BACKEND_SESSION_SESSION_BASIC_H

#include <vector>
#include <string>
#include <utility>
#include <memory>
#include <map>
#include <set>
#include "utils/hash_map.h"
#include "backend/common/session/kernel_graph_mgr.h"
#include "backend/common/session/session_context.h"
#include "include/backend/kernel_graph.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "include/common/utils/tensor_future.h"
#include "ir/anf.h"
#include "ir/tensor.h"
#include "utils/any.h"
#include "include/common/utils/contract.h"
#include "include/backend/kernel_info.h"
#include "utils/ms_context.h"
#include "pipeline/pynative/base.h"

#if defined(ENABLE_DEBUGGER) && !defined(_WIN32) && !defined(_WIN64)
#include "include/backend/debug/debugger/debugger.h"
#endif
#include "mindspore/ccsrc/debug/summary/summary.h"
#include "runtime/hardware/device_context.h"
#include "include/backend/visible.h"

namespace mindspore {
namespace runtime {
class GraphCompiler;
}  // namespace runtime
}  // namespace mindspore

namespace mindspore {
const char kSessionBasic[] = "SessionBasic";

namespace session {
using mindspore::debug::CallBackFunc;
#ifndef ENABLE_SECURITY
using mindspore::debug::Summary;
#endif

using AnyList = std::vector<Any>;
using AnyListPtr = std::shared_ptr<AnyList>;

struct BackendOpRunInfo {
  ~BackendOpRunInfo() = default;
  BackendOpRunInfo(pynative::BaseOpRunInfo base_op_run_info, PrimitivePtr prim, bool is_infer, bool is_gradient_out)
      : base_op_run_info(std::move(base_op_run_info)),
        op_prim(std::move(prim)),
        is_infer(is_infer),
        is_gradient_out(is_gradient_out) {}

  pynative::BaseOpRunInfo base_op_run_info;
  PrimitivePtr op_prim;
  bool is_infer = false;
  bool is_gradient_out = false;
  std::vector<pynative::DeviceAddressPromisePtr> device_sync_promises;
};
using BackendOpRunInfoPtr = std::shared_ptr<BackendOpRunInfo>;

struct InputInfo {
  std::vector<ValuePtr> input_values;
  std::vector<InputType> input_types;
  std::set<KernelWithIndex> input_kernel;
  abstract::AbstractBasePtrList input_abs;
};

struct OutputTensorInfo {
  tensor::TensorPtr output_stub_tensor;
  bool is_weight;
};

struct GraphOutputInfo {
  VectorRef *graph_outputs;
  std::map<KernelWithIndex, std::vector<std::vector<size_t>>> output_indexes;
  std::vector<tensor::TensorPtr> graph_output_tensors;
};

class Executor;

class BACKEND_EXPORT SessionBasic : public KernelGraphMgr, public std::enable_shared_from_this<SessionBasic> {
 public:
  using KernelGraphMgr::ConstructKernelGraph;
  SessionBasic() : context_(nullptr), summary_callback_(nullptr), device_id_(0) {
#if defined(ENABLE_DEBUGGER) && !defined(_WIN32) && !defined(_WIN64)
    debugger_ = nullptr;
#endif
  }

  virtual void Init(uint32_t device_id) { device_id_ = device_id; }
  void InitExecutor(const std::string &device_name, uint32_t device_id);
  virtual void SyncStream() const {}
  virtual ~SessionBasic() { summary_callback_ = nullptr; }

  GraphId CompileGraph(const GraphSegmentPtr &segment, const AnfNodePtrList &outputs);
  GraphId CompileGraph(NotNull<FuncGraphPtr> func_graph);
  void BuildGraph(GraphId graphId);
  void RunGraph(const GraphId &graph_id, const std::vector<tensor::TensorPtr> &inputs, VectorRef *outputs);
  void RunGraphAsync(const GraphId &graph_id, const std::vector<tensor::TensorPtr> &inputs, VectorRef *outputs);

#ifndef ENABLE_SECURITY
  virtual void RegisterSummaryCallBackFunc(const CallBackFunc &callback);
#endif
  virtual GraphId GetFinalRunGraph() const { return kInvalidGraphId; }
  bool IsGetNextGraph(const std::shared_ptr<KernelGraph> &kernel_graph, std::string *channel_name) const;
  virtual bool CheckModelInputs(uint32_t graph_id, const std::vector<tensor::TensorPtr> &inputs,
                                std::string *error_msg) const {
    return true;
  }
  void GetModelInputsInfo(uint32_t graph_id, std::vector<tensor::TensorPtr> *inputs,
                          std::vector<std::string> *inputs_name) const;
  void GetModelOutputsInfo(uint32_t graph_id, std::vector<tensor::TensorPtr> *outputs,
                           std::vector<std::string> *output_names) const;
  std::vector<tensor::TensorPtr> GetInputNeedLockTensors(const GraphId &graph_id,
                                                         const std::vector<tensor::TensorPtr> &inputs) const;
  // create a single run op graph
  std::shared_ptr<KernelGraph> ConstructSingleOpGraph(const BackendOpRunInfoPtr &op_run_info,
                                                      const std::vector<ValuePtr> &input_values,
                                                      const std::vector<InputType> &input_type);
  void EraseValueNodeTensor(const std::vector<InputType> &input_types,
                            std::vector<tensor::TensorPtr> *input_tensors) const;
  void RunOpRemoveNopNode(const KernelGraphPtr &kernel_graph) const;
  static void RunOpHideNopNode(const KernelGraphPtr &kernel_graph);
  virtual void ReportWarningMessage() {}
  virtual void ReportErrorMessage() {}
  virtual void SetThreadContext() {}
#ifdef ENABLE_DEBUGGER
  // set debugger
  void SetDebugger() {
    debugger_ = Debugger::GetInstance();
    auto ms_context = MsContext::GetInstance();
    MS_EXCEPTION_IF_NULL(ms_context);
    MS_EXCEPTION_IF_NULL(debugger_);
    debugger_->Init(device_id_, ms_context->get_param<std::string>(MS_CTX_DEVICE_TARGET));
  }
#endif
  static BaseRef CreateNodeOutputTensors(const AnfNodePtr &anf, const KernelGraphPtr &graph,
                                         const std::vector<tensor::TensorPtr> &input_tensors,
                                         std::map<tensor::TensorPtr, session::KernelWithIndex> *tensor_to_node,
                                         KernelMapTensor *node_to_tensor);

 private:
  void GetParameterIndex(const KernelGraph *graph, const std::vector<tensor::TensorPtr> &inputs,
                         std::map<AnfNodePtr, size_t> *parameter_index) const;
  void CreateOutputPlaceholder(const KernelGraphPtr &kernel_graph, const std::vector<tensor::TensorPtr> &input_tensors,
                               VectorRef *const outputs,
                               std::map<KernelWithIndex, std::vector<std::vector<size_t>>> *output_indexes) const;
  void GetRefCount(const KernelGraph *graph, std::map<KernelWithIndex, size_t> *ref_count) const;
  // Cut op not flatten, so we need calculate maketuple input ref count.
  void CalculateRefCount(const AnfNodePtr &node, std::map<KernelWithIndex, size_t> *ref_count) const;
  void GetForwardOpOutputRefCount(const KernelGraph *graph, const std::vector<tensor::TensorPtr> &inputs,
                                  std::map<std::string, size_t> *forward_op_output_tensor_id,
                                  const std::map<AnfNodePtr, size_t> &parameter_index) const;
  void ReleaseForwardOpOutput(const std::vector<ValuePtr> &input_tensors,
                              std::map<std::string, size_t> *forward_op_output_tensor_id) const;
  void HandleOpInputs(const std::set<KernelWithIndex> &input_kernel, std::map<KernelWithIndex, size_t> *ref_count,
                      std::map<KernelWithIndex, tensor::TensorPtr> *op_output_map) const;

  void HandleOpOutputs(const AnfNodePtr &kernel, const VectorRef &op_outputs,
                       const std::map<KernelWithIndex, size_t> &ref_count,
                       std::map<KernelWithIndex, tensor::TensorPtr> *op_output_map,
                       GraphOutputInfo *const graph_output_info) const;

 protected:
  friend class Executor;
  friend class CompileNodesTask;
  friend class CompileGraphTask;
  friend class BuildGraphTask;
  friend class RunGraphTask;
  friend class mindspore::runtime::GraphCompiler;
  virtual bool IsSupportSummary() { return true; }
  virtual void CreateOutputTensors(const GraphId &graph_id, const std::vector<tensor::TensorPtr> &input_tensors,
                                   VectorRef *outputs,
                                   std::map<tensor::TensorPtr, session::KernelWithIndex> *tensor_to_node,
                                   KernelMapTensor *node_to_tensor);
  // When the device address of the node is used as the output of the graph, the device address will be passed
  // to the output tensor, and the output node will recreate a new device address. This third parameter records
  // the relationship between the new and old device address.
  virtual void UpdateOutputTensors(const VectorRef *outputs,
                                   const std::map<tensor::TensorPtr, session::KernelWithIndex> &tensor_to_node,
                                   std::map<DeviceAddressPtr, DeviceAddressPtr> *);
  virtual void FinalOptimize(const KernelGraphPtr &graph) const;
  virtual GraphId CompileGraphImpl(const AnfNodePtrList &lst, const AnfNodePtrList &outputs) { return 0; }
  virtual GraphId CompileGraphImpl(NotNull<FuncGraphPtr>) { return kInvalidGraphId; }
  virtual void BuildGraphImpl(GraphId) {}
  virtual void PreExecuteGraph(const std::shared_ptr<KernelGraph> &kernel_graph,
                               const std::vector<tensor::TensorPtr> &inputs, VectorRef *const outputs) {
    MS_EXCEPTION_IF_NULL(kernel_graph);
    MS_EXCEPTION_IF_NULL(outputs);
    MS_LOG(INFO) << "Call default PreExecuteGraph with input size: " << inputs.size();
  }

  virtual void PostExecuteGraph(const std::shared_ptr<KernelGraph> &kernel_graph,
                                const std::vector<tensor::TensorPtr> &inputs, VectorRef *const outputs) {
    MS_EXCEPTION_IF_NULL(kernel_graph);
    MS_EXCEPTION_IF_NULL(outputs);
    MS_LOG(INFO) << "Call default PostExecuteGraph with input size: " << inputs.size();
  }

  virtual void ExecuteGraph(const std::shared_ptr<KernelGraph> &kernel_graph) { MS_EXCEPTION_IF_NULL(kernel_graph); }

  void RunGraphImpl(const GraphId &graph_id, const std::vector<tensor::TensorPtr> &inputs, VectorRef *outputs);

  void ProcessInputTensorsForHeterogeneous(const std::string &cur_target,
                                           const std::vector<tensor::TensorPtr> &input_tensors) const;
#ifndef ENABLE_SECURITY
  virtual void SetSummaryNodes(KernelGraph *graph);
  void RecurseSetSummaryNodesForAllGraphs(KernelGraph *graph);
#endif

  void LoadInputs(const GraphId &graph_id, const std::vector<tensor::TensorPtr> &inputs_const) const {
    MS_LOG(INFO) << "Status record: start load input. graph id: " << graph_id;
    auto kernel_graph = GetGraph(graph_id);
    MS_EXCEPTION_IF_NULL(kernel_graph);
    if (!kernel_graph->executable()) {
      return;
    }
    LoadInputData(kernel_graph, inputs_const);
    MS_LOG(INFO) << "Status record: end load input. graph id: " << graph_id;
  }

  virtual void LoadInputData(const std::shared_ptr<KernelGraph> &kernel_graph,
                             const std::vector<tensor::TensorPtr> &inputs_const) const {
    MS_EXCEPTION_IF_NULL(kernel_graph);
    MS_LOG(INFO) << "Call default LoadInputData with input size: " << inputs_const.size();
  }

  void UpdateOutputs(const std::shared_ptr<KernelGraph> &kernel_graph, VectorRef *const outputs,
                     const std::vector<tensor::TensorPtr> &input_tensors,
                     std::map<tensor::TensorPtr, session::KernelWithIndex> *tensor_to_node) const;
#ifndef ENABLE_SECURITY
  void Summary(KernelGraph *graph);
#endif
  // create graph output for RunOp
  void CreateOutputNode(const CNodePtr &cnode, const std::shared_ptr<KernelGraph> &graph) const;

  BackendOpRunInfoPtr GetSingleOpRunInfo(const CNodePtr &cnode, const InputInfo &input_info,
                                         const GraphOutputInfo *const graph_output_info) const;
  ValuePtr GetValueNodeOutput(const AnfNodePtr &node, size_t output_index) const;
  tensor::TensorPtr GetParameterOutputTensor(const AnfNodePtr &node,
                                             const std::map<AnfNodePtr, size_t> &parameter_index,
                                             const std::vector<tensor::TensorPtr> &graph_inputs) const;
  tensor::TensorPtr GetCNodeOutputTensor(const KernelWithIndex &kernel_with_index,
                                         const std::map<KernelWithIndex, tensor::TensorPtr> &op_output) const;
  void GetOpInputTensors(const CNodePtr &cnode, const std::map<KernelWithIndex, tensor::TensorPtr> &op_output,
                         const std::map<AnfNodePtr, size_t> &parameter_index,
                         const std::vector<tensor::TensorPtr> &graph_inputs, InputInfo *input_info) const;
  void GetOpInputTensorsFromCNode(const CNodePtr &cnode, const std::map<KernelWithIndex, tensor::TensorPtr> &op_output,
                                  const std::map<AnfNodePtr, size_t> &parameter_index,
                                  const std::vector<tensor::TensorPtr> &graph_inputs, InputInfo *input_info) const;
  tensor::TensorPtr GetOpInputTensorByIndex(const CNodePtr &cnode,
                                            const std::map<KernelWithIndex, tensor::TensorPtr> &op_output,
                                            const std::map<AnfNodePtr, size_t> &parameter_index,
                                            const std::vector<tensor::TensorPtr> &graph_inputs, InputInfo *input_info,
                                            size_t input_index) const;

  AnfNodePtr FindPullNode(const AnfNodePtr &push_node, const std::vector<AnfNodePtr> &node_list) const;
  std::vector<uint32_t> GetAllReduceSplitIndex();
  virtual std::string GetCommWorldGroup() { return std::string(); }
  void DumpGraphs(const std::vector<KernelGraphPtr> &graphs) const;
  void GetConstValueDepend(const CNodePtr &cnode, std::set<int64_t> *const_input_attr_index) const;
  mindspore::HashMap<GraphInfo, std::shared_ptr<KernelGraph>> run_op_graphs_;
  std::shared_ptr<Context> context_;
  CallBackFunc summary_callback_;
  uint32_t device_id_;
  // rank id of physical device
  uint32_t rank_id_{0};
  std::shared_ptr<Executor> executor_;
#if defined(ENABLE_DEBUGGER) && !defined(_WIN32) && !defined(_WIN64)
  std::shared_ptr<Debugger> debugger_;
#endif
};

using SessionPtr = std::shared_ptr<session::SessionBasic>;
using NamedSummaryOutputs = std::map<std::string, std::pair<AnfNodePtr, int>>;
}  // namespace session
BACKEND_EXPORT void DumpGraphExeOrder(const std::string &file_name, const std::string &target_dir,
                                      const std::vector<CNodePtr> &execution_order);
BACKEND_EXPORT uint32_t GetRankId();
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_SESSION_SESSION_BASIC_H
