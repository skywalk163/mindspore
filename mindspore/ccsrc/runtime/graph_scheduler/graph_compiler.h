/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_RUNTIME_FRAMEWORK_GRAPH_COMPILER_H_
#define MINDSPORE_CCSRC_RUNTIME_FRAMEWORK_GRAPH_COMPILER_H_

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <set>
#include "utils/hash_map.h"
#include "runtime/hardware/device_context.h"
#include "runtime/graph_scheduler/actor/actor_common.h"
#include "runtime/graph_scheduler/control_node_parser.h"
#include "backend/common/session/session_basic.h"
#include "backend/common/session/session_factory.h"
#include "ir/tensor.h"
#include "include/backend/visible.h"

namespace mindspore {
using device::DeviceContext;
using session::BackendOpRunInfo;
using session::CallBackFunc;
using session::GraphOutputInfo;
using session::InputTensorInfo;
using session::KernelGraph;
using session::KernelWithIndex;
using tensor::TensorPtr;

const char kModelNameRuntime[] = "Runtime";
const char kEventDeviceInit[] = "DeviceInit";
const char kEventCompileGraph[] = "CompileGraph";
const char kEventRunGraph[] = "RunGraph";
const char kStageDeviceInit[] = "DeviceInit";
const char kStageCompileGraphs[] = "CompileGraphs";
const char kStageGraphPartition[] = "GraphPartition";
const char kStageConstructKernelGraph[] = "ConstructKernelGraph";
const char kStageOptimizeGraph[] = "OptimizeGraph";
const char kStageCreateKernel[] = "CreateKernel";
const char kStageGraphTransform[] = "GraphTransform";
const char kStageBuild[] = "Build";
const char kStageLink[] = "Link";
const char kStageOptimize[] = "Optimize";
const char kStageRunGraph[] = "RunGraph";
const char kStageGetInputs[] = "GetInputs";
const char kStageRun[] = "Run";
const char kStageConstructOutputs[] = "ConstructOutputs";
namespace runtime {
// Position of kernel with index, the value pair<branch_id, vector<pos>> means the branch id of the kernel and the pos
// of the kernel. Generally, there is only one branch, and the branch id is 0 at this time. In control flow, there are
// multiple branch scenarios, and pos represents the position of the kernel in the branch.
using KernelMapPosition = std::map<KernelWithIndex, std::vector<size_t>, session::KernelWithIndexCmp>;

// The graph compiler info generated by graph compiler is the express of executable graph.
// The device context is unified interface of interaction with device of corresponding graph.
// The tensors mask is used to distinguish input tensor's type.
// The input tensor is used to link graphs in the dynamic build scenario.
// The control node is used to link graphs in the control flow scenario.
// The control node parser is used to parse the edge info in control nodes.
// The origin parameters order is used to correspond to the input args.
// The origin outputs order is used to correspond to the output args.
// The need_erase means need erase this GraphCompilerInfo object after run actor set.
struct BACKEND_EXPORT GraphCompilerInfo {
  GraphCompilerInfo(const std::vector<KernelGraphPtr> &graphs, const std::vector<DeviceContext *> &device_contexts,
                    const std::vector<std::vector<int64_t> *> &tensors_mask,
                    const std::vector<std::vector<TensorPtr> *> &input_tensors,
                    const std::vector<AnfNodePtr> &control_nodes,
                    const std::vector<AnfNodePtr> &origin_parameters_order, const ControlNodeParserPtr &parser,
                    const KernelMapPosition &origin_outputs_order, const size_t outputs_num, const std::string &name,
                    bool need_erase, GraphExecutionStrategy strategy)
      : graphs_(graphs),
        device_contexts_(device_contexts),
        tensors_mask_(tensors_mask),
        input_tensors_(input_tensors),
        control_nodes_(control_nodes),
        control_node_parser_(parser),
        origin_parameters_order_(origin_parameters_order),
        origin_outputs_order_(origin_outputs_order),
        outputs_num_(outputs_num),
        name_(name),
        need_erase_(need_erase),
        strategy_(strategy) {}
  ~GraphCompilerInfo();
  std::vector<KernelGraphPtr> graphs_;
  std::vector<DeviceContext *> device_contexts_;
  std::vector<std::vector<int64_t> *> tensors_mask_;
  std::vector<std::vector<TensorPtr> *> input_tensors_;
  std::vector<AnfNodePtr> control_nodes_;
  ControlNodeParserPtr control_node_parser_;
  std::vector<AnfNodePtr> origin_parameters_order_;
  KernelMapPosition origin_outputs_order_;
  size_t outputs_num_;
  std::string name_;
  bool need_erase_;
  mutable GraphExecutionStrategy strategy_;
};

class GraphCompiler {
 public:
  GraphCompiler() { session_ = session::SessionFactory::Get().Create(kSessionBasic); }
  ~GraphCompiler() = default;

  // Construct kernel graph from anf nodes list and compile kernel graph in Graph mode,
  // the detailed implementation of compiling graph is in 'CompileGraphImpl'.
  GraphId CompileGraph(const GraphSegmentPtr &segment, const AnfNodePtrList &outputs,
                       const DeviceContext *device_context, device::RunMode run_mode, bool run_in_pynative = false);

  GraphId CompileDynamicGraph(const GraphSegmentPtr &segment, const AnfNodePtrList &outputs,
                              const DeviceContext *device_context);

  // Construct kernel graph from function graph and compile kernel graph in Graph mode,
  // the detailed implementation of compiling graph is in 'CompileGraphImpl'.
  GraphId CompileWholeGraphForGraphRunMode(const FuncGraphPtr &func_graph, const DeviceContext *device_context);

  // Get graph by graph id, if not exist return nullptr, used in Graph mode.
  KernelGraphPtr Fetch(GraphId graph_id) const;

  // The following four methods used in PyNative back propagation to split complete kernel graph to single
  // op graph, and these methods will be removed to class MindRTBackend after deleting session module.

  // Cache index for all parameter and output nodes of kernel graph, used to get parameter of single op and
  // recover output of original complete back propagation kernel graph.
  void GetParamAndOutputIndex(const KernelGraphPtr &graph, const std::vector<TensorPtr> &inputs,
                              VectorRef *const outputs, std::map<AnfNodePtr, size_t> *parameter_index,
                              std::map<KernelWithIndex, std::vector<std::vector<size_t>>> *output_indexes);

  // Get input tensors for single op compile and run, input tensors may convert from value node and parameter in graph
  // and prev kernel node's output.
  void GetSingleOpInputTensors(const CNodePtr &kernel, const std::map<KernelWithIndex, TensorPtr> &op_output,
                               const std::map<AnfNodePtr, size_t> &parameter_index,
                               const std::vector<TensorPtr> &graph_inputs, InputTensorInfo *const input_tensor_info);
  // Get one input tensor for single control op, such as bprop_cut.
  TensorPtr GetSingleOpInputTensorByIndex(const CNodePtr &kernel, const std::map<KernelWithIndex, TensorPtr> &op_output,
                                          const std::map<AnfNodePtr, size_t> &parameter_index,
                                          const std::vector<TensorPtr> &graph_inputs,
                                          InputTensorInfo *const input_tensor_info, size_t input_index);

  // Get OpRunInfo and GraphInfo for single op compile and run.
  void GetSingleOpRunInfoAndGraphInfo(const CNodePtr &kernel, const InputTensorInfo &tensor_info,
                                      bool use_dynamic_shape_process, session::BackendOpRunInfoPtr *op_run_info,
                                      GraphInfo *graph_info, const GraphOutputInfo *const graph_output_info);

  // Calculate ref count of PyNative back propagation operators.
  void CalculateRefCount(const KernelGraphPtr &graph, std::map<KernelWithIndex, size_t> *ref_count) const;

  // Calculate forward op output ref count of PyNative back graph.
  void CalculateForwardOpOutputCount(const KernelGraphPtr &graph, const std::vector<tensor::TensorPtr> &inputs,
                                     std::map<std::string, size_t> *forward_op_output_tensor_id,
                                     const std::map<AnfNodePtr, size_t> &parameter_index) const;

  // Update ref count of PyNative back propagation operators.
  void UpdateRefCount(const std::set<KernelWithIndex> &input_kernels_with_index,
                      std::map<KernelWithIndex, size_t> *ref_count,
                      std::map<KernelWithIndex, tensor::TensorPtr> *op_output_map) const;

  // Update forward op output ref count of PyNative back graph.
  void UpdateForwardOpOutputRefCount(const std::vector<tensor::TensorPtr> &input_tensor,
                                     std::map<std::string, size_t> *forward_op_output_tensor_id) const;

  // Handle single op output tensor and recover output of original complete kernel graph.
  void RecoverGraphOutput(const AnfNodePtr &kernel, const VectorRef &op_outputs,
                          const std::map<KernelWithIndex, size_t> &ref_count,
                          std::map<KernelWithIndex, TensorPtr> *op_output_map,
                          GraphOutputInfo *const graph_output_info) const;

  // Register a summary callback function, which is called in the final stages of summary.
  void RegisterSummaryCallBackFunc(const CallBackFunc &callback) const;
  // Execute graph summary.
  void Summary(const std::vector<KernelGraphPtr> &graphs) const;

  // The implementation of compiling graph in Graph Mode, including optimizing graph,
  // setting operator info, creating kernel and transforming kernel graph to ActorSet.
  GraphId CompileGraphImpl(const KernelGraphPtr &graph, const DeviceContext *device_context,
                           bool run_in_pynative = true) const;

 private:
  DISABLE_COPY_AND_ASSIGN(GraphCompiler);

  // Create device address for all anf nodes of graph.
  void CreateDeviceAddress(const KernelGraphPtr &graph, const DeviceContext *device_context) const;

  // Set Graph's dependencies for pre_graph and post_graph.
  void SetGraphDependency(const KernelGraphPtr &graph, const GraphSegmentPtr &segment) const;

  // The member variable 'session_' will be removed after removing session module.
  // Now all the GraphCompiler share the same 'session_'.
  session::SessionPtr session_;
};

}  // namespace runtime
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_RUNTIME_FRAMEWORK_GRAPH_COMPILER_H_
