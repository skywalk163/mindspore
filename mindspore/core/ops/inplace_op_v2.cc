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

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/macros.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/math_ops.h"
#include "ops/base_operator.h"
#include "ops/inplace_update_v2.h"
#include "ops/op_name.h"
#include "ops/op_utils.h"
#include "ops/primitive_c.h"
#include "utils/anf_utils.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
namespace {
bool IsIndicesDynamic(const PrimitivePtr &, const AbstractBasePtr &indices_abs) {
  if (CheckAndConvertUtils::IsTensor(indices_abs)) {
    ShapeVector indices_shape = indices_abs->GetShape()->GetShapeVector();
    if (indices_shape.size() != kSizeOne) {
      MS_LOG(EXCEPTION) << "Shape of indices only could be one-dimensional. But got " << indices_shape;
    }
    return IsDynamic(indices_shape);
  } else if (CheckAndConvertUtils::IsSequence(indices_abs)) {
    return indices_abs->GetShape()->isa<abstract::DynamicSequenceShape>();
  } else if (CheckAndConvertUtils::IsScalar(indices_abs)) {
    return false;
  } else {
    MS_EXCEPTION(TypeError) << "Input 'indices' should be scalar, tuple or Tensor.";
  }
}

ShapeVector GetIndicesShape(const PrimitivePtr &primitive, const AbstractBasePtr &indices_abs) {
  if (CheckAndConvertUtils::IsTensor(indices_abs) || CheckAndConvertUtils::IsSequence(indices_abs)) {
    ShapeVector indices_shape = GetShapeValue(primitive, indices_abs);
    return {SizeToLong(indices_shape.size())};
  } else if (CheckAndConvertUtils::IsScalar(indices_abs)) {
    return {1};
  } else {
    MS_EXCEPTION(TypeError) << "Input 'indices' should be scalar, tuple or Tensor.";
  }
}

abstract::ShapePtr InplaceOpV2InferShape(const PrimitivePtr &primitive,
                                         const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);

  auto x_shape_ptr = input_args[0]->GetShape();
  MS_EXCEPTION_IF_NULL(x_shape_ptr);
  auto v_shape_ptr = input_args[2]->GetShape();
  MS_EXCEPTION_IF_NULL(v_shape_ptr);
  if (x_shape_ptr->IsDynamic() || v_shape_ptr->IsDynamic() || IsIndicesDynamic(primitive, input_args[kInputIndex1])) {
    return x_shape_ptr->cast<abstract::ShapePtr>();
  }

  auto x_in_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(x_shape_ptr)[kShape];
  auto v_in_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(v_shape_ptr)[kShape];

  // check dimensions except for the first one
  (void)CheckAndConvertUtils::CheckValue<size_t>("rank of x", x_in_shape.size(), kEqual, "rank of v", v_in_shape.size(),
                                                 primitive->name());

  for (size_t i = 1; i < x_in_shape.size(); ++i) {
    (void)CheckAndConvertUtils::CheckValue<int64_t>(std::to_string(i) + "th dim of x", x_in_shape.at(i), kEqual,
                                                    std::to_string(i) + "th dim of v", v_in_shape.at(i),
                                                    primitive->name());
  }

  ShapeVector indices_shape = GetIndicesShape(primitive, input_args[kInputIndex1]);
  // check indices
  (void)CheckAndConvertUtils::CheckValue<size_t>("size of indices", LongToSize(indices_shape.at(0)), kEqual,
                                                 "v.shape[0]", LongToSize(v_in_shape.at(0)), primitive->name());

  return x_shape_ptr->cast<abstract::ShapePtr>();
}

TypePtr InplaceOpV2InferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  if (std::any_of(input_args.begin(), input_args.end(), [](const AbstractBasePtr &a) { return a == nullptr; })) {
    MS_LOG(EXCEPTION) << "For '" << prim->name()
                      << ", the input args used for infer shape and type is necessary, but missing it.";
  }
  const std::set<TypePtr> valid_types = {kInt32, kFloat16, kFloat32};
  std::map<std::string, TypePtr> args = {
    {"x", input_args[0]->GetType()},
    {"v", input_args[2]->GetType()},
  };

  const auto &indices_abs = input_args[1];
  const std::set<TypePtr> indices_valid_types = {kInt32, kInt64};
  if (CheckAndConvertUtils::IsTensor(indices_abs) || CheckAndConvertUtils::IsScalar(indices_abs)) {
    (void)CheckAndConvertUtils::CheckTypeValid("indices", indices_abs->GetType(), indices_valid_types, prim->name());
  } else if (CheckAndConvertUtils::IsTuple(indices_abs)) {
    auto indices_type = indices_abs->GetType();
    MS_EXCEPTION_IF_NULL(indices_type);
    auto const &type_ele = indices_type->cast<TuplePtr>()->elements();
    if (type_ele.empty()) {
      MS_EXCEPTION(ValueError) << "Input indices should not be empty: " << indices_abs->ToString();
    }
    for (const auto &ele_type : type_ele) {
      (void)CheckAndConvertUtils::CheckTypeValid("indices", ele_type, indices_valid_types, prim->name());
    }
  } else {
    MS_EXCEPTION(TypeError) << "Input 'indices' should be int scalar, tuple or Tensor, but got "
                            << indices_abs->ToString();
  }
  return CheckAndConvertUtils::CheckTensorTypeSame(args, valid_types, prim->name());
}
}  // namespace

MIND_API_OPERATOR_IMPL(InplaceUpdateV2, BaseOperator);
AbstractBasePtr InplaceOpV2Infer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                 const std::vector<AbstractBasePtr> &input_args) {
  constexpr auto inputs_num = 3;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, inputs_num, primitive->name());
  auto dtype = InplaceOpV2InferType(primitive, input_args);
  auto shape = InplaceOpV2InferShape(primitive, input_args);
  return abstract::MakeAbstract(shape, dtype);
}

// AG means auto generated
class MIND_API AGInplaceOpV2Infer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return InplaceOpV2InferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return InplaceOpV2InferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return InplaceOpV2Infer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {1}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(InplaceUpdateV2, prim::kPrimInplaceUpdateV2, AGInplaceOpV2Infer, false);
}  // namespace ops
}  // namespace mindspore
