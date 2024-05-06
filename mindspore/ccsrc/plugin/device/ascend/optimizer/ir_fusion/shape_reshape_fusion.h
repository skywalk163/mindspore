/**
 * Copyright 2024 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_OPTIMIZER_PASS_SHAPE_RESHAPE_FUSION_H_
#define MINDSPORE_CCSRC_BACKEND_OPTIMIZER_PASS_SHAPE_RESHAPE_FUSION_H_

#include <memory>
#include "include/backend/optimizer/optimizer.h"

namespace mindspore {
namespace opt {
class BACKEND_EXPORT ShapeReshapeFusion : public PatternProcessPass {
 public:
  explicit ShapeReshapeFusion(bool multigraph = true) : PatternProcessPass("shape_reshape", multigraph) {}
  ~ShapeReshapeFusion() override = default;
  const BaseRef DefinePattern() const override;
  const AnfNodePtr Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                           const EquivPtr &equiv) const override;

 private:
  VarPtr reshape_input_ = std::make_shared<Var>();
};

class BACKEND_EXPORT ShapeReshapeDirectFusion : public PatternProcessPass {
 public:
  explicit ShapeReshapeDirectFusion(bool multigraph = true) : PatternProcessPass("shape_reshape_direct", multigraph) {}
  ~ShapeReshapeDirectFusion() override = default;
  const BaseRef DefinePattern() const override;
  const AnfNodePtr Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                           const EquivPtr &equiv) const override;
};
}  // namespace opt
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_OPTIMIZER_PASS_SHAPE_RESHAPE_FUSION_H_
