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

#include "ops/ops_func_impl/apply_rotary_pos_emb.h"

#include <set>
#include <string>
#include "abstract/ops/primitive_infer_map.h"
#include "ops/nn_ops.h"
#include "utils/check_convert_utils.h"
#include "ops/primitive_c.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
BaseShapePtr ApplyRotaryPosEmbFuncImpl::InferShape(const PrimitivePtr &primitive,
                                                   const std::vector<AbstractBasePtr> &input_args) const {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kApplyRotaryPosEmbInputsNum, op_name);
  auto query_shape_vector = input_args[kApplyRotaryPosEmbQueryIndex]->GetShape()->GetShapeVector();
  auto key_shape_vector = input_args[kApplyRotaryPosEmbKeyIndex]->GetShape()->GetShapeVector();
  auto query_shape = std::make_shared<abstract::Shape>(query_shape_vector);
  auto key_shape = std::make_shared<abstract::Shape>(key_shape_vector);
  return std::make_shared<abstract::TupleShape>(abstract::BaseShapePtrList{query_shape, key_shape});
}

TypePtr ApplyRotaryPosEmbFuncImpl::InferType(const PrimitivePtr &prim,
                                             const std::vector<AbstractBasePtr> &input_args) const {
  const std::set valid_types = {kFloat16, kFloat32, kBFloat16};
  auto op_name = prim->name();
  std::map<std::string, TypePtr> types;

  (void)types.emplace("query", input_args[kApplyRotaryPosEmbQueryIndex]->GetType());
  (void)types.emplace("key", input_args[kApplyRotaryPosEmbKeyIndex]->GetType());
  (void)types.emplace("cos", input_args[kApplyRotaryPosEmbCosIndex]->GetType());
  (void)types.emplace("sin", input_args[kApplyRotaryPosEmbSinIndex]->GetType());
  auto type = CheckAndConvertUtils::CheckTensorTypeSame(types, valid_types, op_name);
  auto position_ids_type = input_args[kApplyRotaryPosEmbPositionIdsIndex]->GetType();
  const std::set<TypePtr> valid_position_ids_types = {kInt32, kInt64, kUInt32, kUInt64};
  (void)CheckAndConvertUtils::CheckTypeValid("position_ids", position_ids_type, valid_position_ids_types, op_name);

  TypePtrList output_type_ptr_list(kFApplyRotaryPosEmbOutputsNum);
  output_type_ptr_list[kApplyRotaryPosEmbQueryEmbedIndex] = type;
  output_type_ptr_list[kApplyRotaryPosEmbKeyEmbedIndex] = type;
  return std::make_shared<Tuple>(output_type_ptr_list);
}
}  // namespace ops
}  // namespace mindspore
