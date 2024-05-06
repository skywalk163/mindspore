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

#include "tools/converter/adapter/acl/mapper/split_mapper.h"
#include <memory>
#include "tools/converter/adapter/acl/mapper/primitive_mapper_register.h"
#include "src/common/log_util.h"
#include "tools/converter/adapter/acl/mapper/tbe_op_def.h"
#include "ops/op_utils.h"

namespace mindspore {
namespace lite {
namespace {
const size_t kNumInputSize = 2;
}
STATUS SplitMapper::Mapper(const CNodePtr &cnode) {
  auto func_graph = cnode->func_graph();
  CHECK_NULL_RETURN(func_graph);
  auto prim = GetValueNode<PrimitivePtr>(cnode->input(0));
  CHECK_NULL_RETURN(prim);

  auto split_num_val = prim->GetAttr(ops::kOutputNum);
  CHECK_NULL_RETURN(split_num_val);
  prim->AddAttr("num_split", split_num_val);
  ValuePtr axis_value = prim->GetAttr("axis");

  bool size_split_is_equla = true;
  ValuePtr size_splits_value = prim->GetAttr("size_splits");
  if (size_splits_value != nullptr) {
    auto size_splits_vector = GetValue<std::vector<int64_t>>(size_splits_value);
    // SplitV not support dynamic shape in CANN.
    size_split_is_equla = std::all_of(size_splits_vector.begin() + 1, size_splits_vector.end(),
                                      [&size_splits_vector](int64_t x) { return x == size_splits_vector.at(0); });
  }
  PrimitivePtr dst_prim = nullptr;
  if (cnode->size() == opt::kInputSizeThree || (cnode->size() == kNumInputSize && axis_value != nullptr &&
                                                size_splits_value != nullptr && !size_split_is_equla)) {
    dst_prim = std::make_shared<acl::SplitV>();
    if (MoveAttrMap(cnode, dst_prim) != RET_OK) {
      MS_LOG(ERROR) << "Split mapper failed.";
      return RET_ERROR;
    }
    if (axis_value != nullptr) {
      dst_prim->AddAttr("split_dim", axis_value);
    }
    if (size_splits_value != nullptr) {
      dst_prim->AddAttr("size_splits", size_splits_value);
    }
    if (size_splits_value == nullptr) {
      int status = AddIntAttrToInput(func_graph, cnode, prim, ops::kAxis, false);
      if (status != RET_OK) {
        MS_LOG(ERROR) << "Add axis constant value to input failed.";
        return RET_ERROR;
      }
    }
  }
  return RET_OK;
}

REGISTER_PRIMITIVE_MAPPER(kNameSplit, SplitMapper)
}  // namespace lite
}  // namespace mindspore
