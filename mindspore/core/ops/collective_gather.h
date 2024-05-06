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

#ifndef MINDSPORE_CORE_OPS_COLLECTIVE_GATHER_H_
#define MINDSPORE_CORE_OPS_COLLECTIVE_GATHER_H_

#include <memory>
#include <string>
#include <vector>
#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameCollectiveGather = "CollectiveGather";
class MIND_API CollectiveGather : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(CollectiveGather);
  CollectiveGather() : BaseOperator(kNameCollectiveGather) { InitIOName({"input_x"}, {"output"}); }
  void Init() {}
  void set_group(const std::string &format);
  std::string get_group() const;
  void set_rank_size(int rank_size);
  int get_rank_size() const;
  void set_dest_rank(int dest_rank);
  int get_dest_rank() const;
};
}  // namespace ops
}  // namespace mindspore
#endif  // MINDSPORE_CORE_OPS_COLLECTIVE_GATHER_H_
