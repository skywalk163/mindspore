/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_OPTIMIZER_ENHANCER_TRANSPOSE_OPTIMIZER_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_OPTIMIZER_ENHANCER_TRANSPOSE_OPTIMIZER_H_
#include <string>
#include <memory>
#include "ir/func_graph.h"
#include "ir/anf.h"
#include "include/backend/optimizer/helper.h"
#include "include/backend/optimizer/optimizer.h"
#include "include/backend/optimizer/pass.h"
#include "plugin/device/ascend/optimizer/ascend_helper.h"

namespace mindspore {
namespace opt {
class TransposeOptimizer : public PatternProcessPass {
 public:
  explicit TransposeOptimizer(bool multigraph = true) : PatternProcessPass("transpose_optimizer", multigraph) {}
  ~TransposeOptimizer() override = default;
  const BaseRef DefinePattern() const override;
  const AnfNodePtr Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node, const EquivPtr &) const override;
};
}  // namespace opt
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_OPTIMIZER_ENHANCER_TRANSPOSE_OPTIMIZER_H_
