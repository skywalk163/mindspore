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
#ifndef MINDSPORE_CCSRC_BACKEND_OPTIMIZER_PASS_CUSTOM_OP_REG_INFO_TO_ATTR_H_
#define MINDSPORE_CCSRC_BACKEND_OPTIMIZER_PASS_CUSTOM_OP_REG_INFO_TO_ATTR_H_
#include <memory>
#include "include/backend/optimizer/pattern_to_pattern.h"

namespace mindspore {
namespace opt {
class CustomOpRegInfoToAttr : public PatternToPatternPass {
 public:
  CustomOpRegInfoToAttr() : PatternToPatternPass("custom_op_reg_info_to_attr", true) {}
  ~CustomOpRegInfoToAttr() override = default;
  void DefineSrcPattern(SrcPattern *src_pattern) override;
  void DefineDstPattern(DstPattern *dst_pattern) override;
  bool CheckMatchedDAG(const PatternMap &, const FuncGraphPtr &, const AnfNodePtr &) const override;
};
}  // namespace opt
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_OPTIMIZER_PASS_CUSTOM_OP_REG_INFO_TO_ATTR_H_
