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

#include "tools/converter/adapter/acl/mapper/multinomial_mapper.h"
#include <memory>
#include "tools/converter/adapter/acl/mapper/primitive_mapper_register.h"
#include "src/common/log_util.h"
#include "tools/converter/adapter/acl/mapper/tbe_op_def.h"
#include "ops/op_utils.h"
#include "tools/converter/adapter/acl/common/utils.h"

namespace mindspore {
namespace lite {
STATUS MultinomialMapper::Mapper(const CNodePtr &cnode) {
  ValueNodePtr value_node = nullptr;
  PrimitivePtr src_prim = nullptr;
  if (GetValueNodeAndPrimFromCnode(cnode, &value_node, &src_prim) != lite::RET_OK) {
    MS_LOG(ERROR) << "Get primitive from cnode failed.";
    return lite::RET_ERROR;
  }
  ops::Multinomial multinomial_op;
  auto dst_prim = multinomial_op.GetPrim();
  CHECK_NULL_RETURN(dst_prim);
  dst_prim->SetAttrs(src_prim->attrs());

  auto dst_type = src_prim->GetAttr(ops::kOutputDType);
  if (dst_type != nullptr) {
    if (!dst_type->isa<Type>()) {
      auto type_id = static_cast<TypeId>(GetValue<int64_t>(dst_type));
      dst_prim->AddAttr("dtype", TypeIdToType(type_id));
    } else {
      dst_prim->AddAttr("dtype", TypeIdToType(acl::GetTypeFromNode(cnode)));
    }
  }
  if (src_prim->HasAttr(ops::kSeed)) {
    auto seed_attr = src_prim->GetAttr(ops::kSeed);
    dst_prim->AddAttr(ops::kSeed, seed_attr);
    dst_prim->AddAttr(ops::kSeed2, seed_attr);
  }
  if (src_prim->HasAttr(ops::kSeed2)) {
    auto seed2_attr = src_prim->GetAttr(ops::kSeed2);
    dst_prim->AddAttr(ops::kSeed2, seed2_attr);
  }
  auto func_graph = cnode->func_graph();
  CHECK_NULL_RETURN(func_graph);
  value_node->set_value(dst_prim);
  return lite::RET_OK;
}

REGISTER_PRIMITIVE_MAPPER(kNameMultinomial, MultinomialMapper)
}  // namespace lite
}  // namespace mindspore
