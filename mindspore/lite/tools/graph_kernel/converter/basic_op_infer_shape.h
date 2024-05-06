/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_LITE_TOOLS_GRAPH_KERNEL_CONVERTER_BASIC_OP_INFER_SHAPE_H_
#define MINDSPORE_LITE_TOOLS_GRAPH_KERNEL_CONVERTER_BASIC_OP_INFER_SHAPE_H_
#include "tools/optimizer/graph/node_infershape.h"
#include "include/backend/optimizer/pass.h"

namespace mindspore::graphkernel {
class BasicOpInferShape : public opt::NodeInferShape {
 public:
  BasicOpInferShape() : opt::NodeInferShape() {}
  ~BasicOpInferShape() = default;
  void Infer(const CNodePtr &cnode);

 private:
  void InferShapeRealKernel(const CNodePtr &cnode);
  void InsertAbstract(const CNodePtr &cnode);
};

class DynOpInferShape : public opt::Pass {
 public:
  DynOpInferShape() : Pass("dynamic_infer_shape") {}
  ~DynOpInferShape() override = default;
  bool Run(const FuncGraphPtr &func_graph) override;

 private:
  bool HasDynamicShapeInput(const FuncGraphPtr &func_graph) const;
  bool InferShapeRealKernel(const CNodePtr &cnode) const;
  bool InferShape(const CNodePtr &cnode) const;
};
}  // namespace mindspore::graphkernel
#endif  // MINDSPORE_LITE_TOOLS_GRAPH_KERNEL_CONVERTER_BASIC_OP_INFER_SHAPE_H_
