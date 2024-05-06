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

#ifndef MINDSPORE_MINDSPORE_CCSRC_PIPELINE_PYNATIVE_FORWARD_CAST_BASE_H_H_
#define MINDSPORE_MINDSPORE_CCSRC_PIPELINE_PYNATIVE_FORWARD_CAST_BASE_H_H_

#include <string>
#include <vector>
#include <utility>
#include "pipeline/pynative/base.h"
#include "pipeline/pynative/pynative_cache.h"

namespace mindspore {
namespace pynative {
class CastBaseOperation {
 public:
  CastBaseOperation() {
    type_prim_cache_.reserve(kDefaultContainerSize);
    implicit_cast_map_.reserve(kDefaultContainerSize);
  }
  ~CastBaseOperation() = default;

 protected:
  PrimitivePtr GetPrimByTypeId(const TypeId &type_id) const;
  const std::string &TypeIdToMsTypeStr(const TypeId &type_id) const;
  bool GetSignatureType(const std::vector<Signature> &signatures, std::vector<SignatureEnumDType> *dtypes) const;
  // Modify tensor data type, when op input source dtype is not tensor without dispatch cast op.
  tensor::TensorPtr TensorToDstDtypeValue(const ValuePtr &src_value, const TypeId &dst_type_id) const;
  ValuePtr ScalarToDstDtypeValue(const ValuePtr &src_value, const std::pair<TypeId, bool> &dst_type) const;

  mutable mindspore::HashMap<TypeId, PrimitivePtr> type_prim_cache_;
  mutable ImplicitCastCache implicit_cast_map_;
};
}  // namespace pynative
}  // namespace mindspore

#endif  // MINDSPORE_MINDSPORE_CCSRC_PIPELINE_PYNATIVE_FORWARD_CAST_BASE_H_H_
