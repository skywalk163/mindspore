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

#ifndef MINDSPORE_LITE_TOOLS_OPTIMIZER_GRAPH_REMOVE_ADD_0_PASS_H_
#define MINDSPORE_LITE_TOOLS_OPTIMIZER_GRAPH_REMOVE_ADD_0_PASS_H_
#include <vector>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include "include/backend/optimizer/optimizer.h"
#include "include/common/utils/utils.h"
#include "tools/optimizer/common/format_utils.h"
#include "tools/optimizer/graph/transpose_strategy.h"

using mindspore::converter::FmkType;
namespace mindspore {
namespace opt {
class RemoveUnusedAddNodePass : public Pass {
 public:
  RemoveUnusedAddNodePass() : Pass("RemoveUnusedAddNodePass") {}
  ~RemoveUnusedAddNodePass() override = default;
  bool Run(const FuncGraphPtr &func_graph) override;
};
}  // namespace opt
}  // namespace mindspore

#endif  // MINDSPORE_LITE_TOOLS_OPTIMIZER_GRAPH_REMOVE_ADD_0_PASS_H_
