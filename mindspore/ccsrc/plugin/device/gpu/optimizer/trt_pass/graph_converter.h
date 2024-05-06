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
#ifndef MINDSPORE_CCSRC_BACKEND_OPTIMIZER_TRT_PASS_GRAPH_CONVERTER_H_
#define MINDSPORE_CCSRC_BACKEND_OPTIMIZER_TRT_PASS_GRAPH_CONVERTER_H_

#include <map>
#include <tuple>
#include <memory>
#include "include/backend/optimizer/optimizer.h"
#include "plugin/device/gpu/optimizer/trt_pass/graph_partitioner.h"

namespace mindspore {
namespace opt {
// Pass replace MindIR operators to TrtNode contains serialized data generated by TensorRT.
// It mainly includes three steps:
// 1. Segment the network with `GraphPartition`.
// 2. Build the TRT network with segmentation and takes its serialized data as an attribute of the `TrtNode`.
// 3. Replace the segmentation with `TrtNode`.
class GraphConverter : public Pass {
 public:
  GraphConverter() : Pass("mindir_to_trt_pass") {}
  ~GraphConverter() override = default;

  // Run the pass replace subgraph to TrtNode.
  bool Run(const FuncGraphPtr &fg) override;

 private:
  // Replace subgraph with TrtNode which keep model data serialized by TensorRT network.
  bool ReplaceSubgraphWithTrtNode(const FuncGraphPtr &root_graph, const Subgraph &sub_graph);

  // Build the TrtNode from subgraph including serialized model data and input shapes and dtypes.
  std::tuple<std::map<size_t, size_t>, CNodePtr> BuildTrtNode(const FuncGraphPtr &root_graph,
                                                              const FuncGraphPtr &sub_graph,
                                                              const AnfNodePtrList &arguments);

  // Remove useless parameters which had been folded in Tensor-RT network.
  void RemoveParameterWithoutUser(const FuncGraphPtr &graph);

  // Get useful arguments in the root graph after conversion.
  AnfNodePtrList GetUsefulArguments(const AnfNodePtrList &arguments, const AnfNodePtrList &parameters,
                                    const AnfNodePtrList &used_parameter);
};
}  // namespace opt
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_OPTIMIZER_TRT_PASS_GRAPH_CONVERTER_H_
