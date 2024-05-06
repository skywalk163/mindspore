/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CONVERT_DYNAMIC_BROADCAST_TO_ATTR_H
#define MINDSPORE_CONVERT_DYNAMIC_BROADCAST_TO_ATTR_H

#include <memory>
#include "include/backend/optimizer/pattern_to_pattern.h"

namespace mindspore {
namespace opt {
class ConvertDynamicBroadcastTo : public PatternToPatternPass {
 public:
  ConvertDynamicBroadcastTo() : PatternToPatternPass("convert_dynamic_broadcast_to", true) {}
  ~ConvertDynamicBroadcastTo() override = default;
  void DefineSrcPattern(SrcPattern *src_pattern) override;
  void DefineDstPattern(DstPattern *dst_pattern) override;
  bool CheckMatchedDAG(const PatternMap &, const FuncGraphPtr &, const AnfNodePtr &) const override;
};
}  // namespace opt
}  // namespace mindspore

#endif  // MINDSPORE_CONVERT_DYNAMIC_BROADCAST_TO_ATTR_H
