/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
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

#include "cpu_kernel/utils/sparse_group.h"
#include <algorithm>

namespace aicpu {
void GroupIterable::IteratorStep::UpdateEndOfGroup() {
  ++next_loc_;
  const auto &ix_t = iter_->ix_matrix_;
  const int64_t N = ix_t.dimension(0);
  while (next_loc_ < N && iter_->GroupMatches(ix_t, loc_, next_loc_)) {
    ++next_loc_;
  }
}

bool GroupIterable::IteratorStep::operator!=(const IteratorStep &rhs) const { return (rhs.loc_ != loc_); }

bool GroupIterable::IteratorStep::operator==(const IteratorStep &rhs) const { return (rhs.loc_ == loc_); }

GroupIterable::IteratorStep &GroupIterable::IteratorStep::operator++() {  // prefix ++
  loc_ = next_loc_;
  UpdateEndOfGroup();
  return *this;
}

const GroupIterable::IteratorStep GroupIterable::IteratorStep::operator++(int) {  // postfix ++
  IteratorStep lhs(*this);
  ++(*this);
  return lhs;
}

Group GroupIterable::IteratorStep::operator*() const { return Group(iter_, loc_, next_loc_); }

std::vector<int64_t> Group::group() const {
  std::vector<int64_t> g;
  const auto &ix_t = iter_->ix_matrix_;
  std::transform(iter_->group_dims_.begin(), iter_->group_dims_.end(), std::back_inserter(g),
                 [&ix_t, this](const auto &d) { return ix_t(loc_, d); });
  return g;
}

TTypes<int64_t>::UnalignedConstMatrix Group::indices() const {
  return TTypes<int64_t>::UnalignedConstMatrix(&(iter_->ix_matrix_(loc_, 0)), next_loc_ - loc_, iter_->dims_);
}
}  // namespace aicpu
