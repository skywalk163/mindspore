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

#include "backend/common/pass/add_attr_to_node/add_attr_to_node_register.h"

namespace mindspore {
namespace opt {
const AnfNodePtr LogFusionProcess(const FuncGraphPtr &graph, const AnfNodePtr &node) {
  constexpr auto kBaseName = "base";
  constexpr auto kScaleName = "scale";
  constexpr auto kShiftName = "shift";
  constexpr pyfloat kDefaultValue = 1.0;

  MS_EXCEPTION_IF_NULL(graph);
  MS_EXCEPTION_IF_NULL(node);

  const auto &prim = common::AnfAlgo::GetCNodePrimitive(node);
  MS_EXCEPTION_IF_NULL(prim);
  if (prim->GetAttr(kBaseName) == nullptr) {
    prim->AddAttr(kBaseName, MakeValue(kDefaultValue));
  }
  if (prim->GetAttr(kScaleName) == nullptr) {
    prim->AddAttr(kScaleName, MakeValue(kDefaultValue));
  }
  if (prim->GetAttr(kShiftName) == nullptr) {
    prim->AddAttr(kShiftName, MakeValue(kDefaultValue));
  }

  return node;
}
}  // namespace opt
}  // namespace mindspore
