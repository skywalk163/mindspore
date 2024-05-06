/**
 * Copyright 2020-2021 Huawei Technologies Co., Ltd
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

#include "ops/ops_func_impl/addn.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
namespace {
// Special handle for empty shape and shape{1}.
inline bool ShapeHasSingleElement(const ShapeVector &shape) {
  return shape.empty() || (shape.size() == 1 && shape[0] == 1);
}

// shape1 is dst_shape, shape2 is source_shape.
bool AddNDynShapeJoin(ShapeVector *shape1, const ShapeVector *shape2) {
  MS_EXCEPTION_IF_NULL(shape1);
  MS_EXCEPTION_IF_NULL(shape2);
  if (ShapeHasSingleElement(*shape1) && ShapeHasSingleElement(*shape2)) {
    return true;
  }
  // shape size not compatible.
  if (shape1->size() != shape2->size()) {
    MS_LOG(ERROR) << "Shape1 size:" << shape1->size() << ", Shape2 size:" << shape2->size();
    return false;
  }
  for (size_t i = 0; i < shape1->size(); ++i) {
    if ((*shape1)[i] == (*shape2)[i]) {
      continue;
    }
    // If shape1 is dynamic, use shape of shape2. If shape2 is dynamic, keep shape1.
    if ((*shape1)[i] == abstract::Shape::kShapeDimAny) {
      (*shape1)[i] = (*shape2)[i];
      continue;
    }
    if ((*shape2)[i] == abstract::Shape::kShapeDimAny) {
      continue;
    }
    // If shape1 != shape2
    MS_LOG(ERROR) << "Shape1[" << i << "]:" << (*shape1)[i] << ", Shape2[" << i << "]:" << (*shape2)[i] << ".";
    return false;
  }
  return true;
}
}  // namespace

BaseShapePtr AddNFuncImpl::InferShape(const PrimitivePtr &primitive,
                                      const std::vector<AbstractBasePtr> &input_args) const {
  const auto &prim_name = primitive->name();
  AbstractBasePtrList elements = input_args;
  // If called from the backend, the input_args[0] is a KernelTensor, not AbstractSequence
  if (input_args.size() == 1 && input_args[0]->isa<abstract::AbstractSequence>()) {
    elements = input_args[0]->cast<abstract::AbstractSequencePtr>()->elements();
  }
  (void)CheckAndConvertUtils::CheckInteger("input num", SizeToLong(elements.size()), kGreaterEqual, 1, prim_name);
  auto shape_0 = elements[0]->GetShape();
  ShapeVector output_shape;
  for (size_t i = 0; i < elements.size(); ++i) {
    auto shape = elements[i]->GetShape();
    ShapeVector shape_vec;
    // If shape is no shape, it is a scalar, use empty shape vector as scalar shape.
    if (shape->isa<abstract::TensorShape>()) {
      shape_vec = shape->GetShapeVector();
    }
    // If any shape is dynamic rank, return a dynamic rank.
    if (IsDynamicRank(shape_vec)) {
      return std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeRankAny}));
    }
    // Record input0's shape.
    if (i == 0) {
      output_shape = shape_vec;
      continue;
    }
    // Join input[i] with input[0]
    if (!AddNDynShapeJoin(&output_shape, &shape_vec)) {
      MS_EXCEPTION(ValueError) << "For '" << prim_name << "', input shape must be same, but got shape of input[" << i
                               << "]: " << shape->ToString() << ", shape of input[0]: " << shape_0->ToString() << ".";
    }
  }
  return std::make_shared<abstract::Shape>(output_shape);
}

TypePtr AddNFuncImpl::InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const {
  MS_EXCEPTION_IF_NULL(primitive);
  const auto &prim_name = primitive->name();
  AbstractBasePtrList elements = input_args;
  if (input_args.size() == 1 && input_args[0]->isa<abstract::AbstractSequence>()) {
    elements = input_args[0]->cast<abstract::AbstractSequencePtr>()->elements();
  }
  (void)CheckAndConvertUtils::CheckInteger("concat element num", SizeToLong(elements.size()), kGreaterEqual, 1,
                                           prim_name);
  std::map<std::string, TypePtr> types;
  (void)types.emplace("element_0", elements[0]->GetType());
  for (size_t i = 0; i < elements.size(); ++i) {
    if (elements[i]->GetType()->type_id() == kObjectTypeUndeterminedType) {
      return elements[0]->GetType()->Clone();
    }
    std::string element_i = "element_" + std::to_string(i);
    (void)types.emplace(element_i, elements[i]->GetType());
  }
  std::set<TypePtr> valid_types = common_valid_types_with_complex_and_bool;
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, valid_types, prim_name);
  return elements[0]->GetType()->Clone();
}

AbstractBasePtr AddNFuncImpl::AddNInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                        const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, 1, prim_name);
  auto infer_type = InferType(primitive, input_args);
  auto infer_shape = InferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

}  // namespace ops
}  // namespace mindspore
