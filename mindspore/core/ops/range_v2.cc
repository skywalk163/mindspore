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
#include "ops/range_v2.h"

#include <memory>
#include <set>
#include <type_traits>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/dtype/type.h"
#include "ir/named.h"
#include "ir/primitive.h"
#include "ir/tensor.h"
#include "ir/value.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "ops/op_name.h"
#include "ops/op_utils.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
#define IsSameType(source_type, cmp_type) (cmp_type->equal(source_type))
#define IsNoneOrAnyValue(value_ptr) ((value_ptr->isa<None>()) || (value_ptr->ContainsValueAny()))
constexpr auto op_name = "RangeV2";

template <typename T>
int64_t RangeV2CalculateShape(const AbstractBasePtr &start_ptr, const AbstractBasePtr limit_ptr,
                              const AbstractBasePtr delta_ptr) {
  auto start_array = GetArrayValue<T>(start_ptr);
  auto limit_array = GetArrayValue<T>(limit_ptr);
  auto delta_array = GetArrayValue<T>(delta_ptr);
  if (!start_array.has_value() || start_array.value().size() != 1) {
    MS_EXCEPTION(TypeError) << "For RangeV2, start must a scalar but element number more than 1.";
  }
  if (!limit_array.has_value() || limit_array.value().size() != 1) {
    MS_EXCEPTION(TypeError) << "For RangeV2, limit must a scalar but element number more than 1.";
  }
  if (!delta_array.has_value() || delta_array.value().size() != 1) {
    MS_EXCEPTION(TypeError) << "For RangeV2, delta must a scalar but element number more than 1.";
  }
  T start = start_array.value()[0];
  T limit = limit_array.value()[0];
  T delta = delta_array.value()[0];
  bool valid_value = (delta == T(0) || (delta > 0 && start > limit) || (delta < 0 && start < limit));
  if (valid_value) {
    if (delta == T(0)) {
      MS_EXCEPTION(ValueError) << "For RangeV2, delta cannot be equal to zero.";
    }
    if (delta > 0 && start > limit) {
      MS_EXCEPTION(ValueError) << "For RangeV2, delta cannot be positive when limit < start.";
    }
    if (delta < 0 && start < limit) {
      MS_EXCEPTION(ValueError) << "For RangeV2, delta cannot be negative when limit > start.";
    }
  }
  int64_t shape_size = 0;
  if (std::is_integral<T>::value) {
    shape_size = static_cast<int64_t>((std::abs(limit - start) + std::abs(delta) - 1) / std::abs(delta));
  } else {
    shape_size = static_cast<int64_t>(std::ceil(std::abs((limit - start) / delta)));
  }
  return shape_size;
}

abstract::ShapePtr RangeV2CheckAndInferShape(const PrimitivePtr &primitive,
                                             const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive->GetAttr(kMaxLen));
  auto start = input_args[kInputIndex0];
  auto limit = input_args[kInputIndex1];
  auto delta = input_args[kInputIndex2];
  // support dynamic rank
  auto start_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(start->GetShape())[kShape];
  auto limit_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(limit->GetShape())[kShape];
  auto delta_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(delta->GetShape())[kShape];
  if (IsDynamicRank(start_shape) || IsDynamicRank(limit_shape) || IsDynamicRank(delta_shape)) {
    return std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeRankAny}));
  }
  int64_t shape_size = abstract::Shape::kShapeDimAny;

  bool is_compile = !IsValueKnown(start) || !IsValueKnown(limit) || !IsValueKnown(delta);
  // not in compile, need inferShape
  if (!is_compile) {
    auto dtype = CheckAndConvertUtils::GetTensorInputType(op_name, input_args, kInputIndex0);
    if (IsSameType(dtype, kInt) || IsSameType(dtype, kInt32)) {
      shape_size = RangeV2CalculateShape<int32_t>(start, limit, delta);
    } else if (IsSameType(dtype, kInt64)) {
      shape_size = RangeV2CalculateShape<int64_t>(start, limit, delta);
    } else if (IsSameType(dtype, kFloat) || IsSameType(dtype, kFloat32)) {
      shape_size = RangeV2CalculateShape<float>(start, limit, delta);
    } else if (IsSameType(dtype, kFloat64)) {
      shape_size = RangeV2CalculateShape<double>(start, limit, delta);
    } else {
      MS_EXCEPTION(TypeError) << "For RangeV2, the dtype of input must be int32, int64, float32, float64, but got "
                              << dtype->meta_type() << ".";
    }
    if (shape_size < 0) {
      MS_EXCEPTION(ValueError) << "For RangeV2, infer shape error, shape_size [" << shape_size << "] is negative.";
    }
  }

  ShapeVector out_shape = {};
  if (is_compile) {
    (void)out_shape.emplace_back(abstract::Shape::kShapeDimAny);
    return std::make_shared<abstract::Shape>(out_shape);
  }

  (void)out_shape.emplace_back(shape_size);
  return std::make_shared<abstract::Shape>(out_shape);
}

TypePtr RangeV2CheckAndInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  std::set<TypePtr> support_types = {kInt32, kInt64, kFloat32, kFloat64};
  auto start_type = CheckAndConvertUtils::CheckTensorTypeValid("start", input_args[kInputIndex0]->GetType(),
                                                               support_types, prim->name());
  auto limit_type = CheckAndConvertUtils::CheckTensorTypeValid("limit", input_args[kInputIndex1]->GetType(),
                                                               support_types, prim->name());
  auto delta_type = CheckAndConvertUtils::CheckTensorTypeValid("delta", input_args[kInputIndex1]->GetType(),
                                                               support_types, prim->name());
  MS_EXCEPTION_IF_NULL(start_type);
  MS_EXCEPTION_IF_NULL(limit_type);
  MS_EXCEPTION_IF_NULL(delta_type);
  bool same_type = IsSameType(start_type, limit_type) && IsSameType(limit_type, delta_type);
  if (!same_type) {
    MS_EXCEPTION(TypeError) << "For RangeV2, start, limit delta should have same type, but get start["
                            << start_type->meta_type() << "], limit[" << limit_type->meta_type() << "], delta["
                            << delta_type->meta_type() << "].";
  }
  return start_type;
}
}  // namespace

AbstractBasePtr RangeV2Infer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                             const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t input_num = 3;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, op_name);
  (void)CheckAndConvertUtils::CheckArgsType(op_name, input_args, kInputIndex0, kObjectTypeTensorType);
  (void)CheckAndConvertUtils::CheckArgsType(op_name, input_args, kInputIndex1, kObjectTypeTensorType);
  (void)CheckAndConvertUtils::CheckArgsType(op_name, input_args, kInputIndex2, kObjectTypeTensorType);
  // infer type must in before
  auto infer_type = RangeV2CheckAndInferType(primitive, input_args);
  auto infer_shape = RangeV2CheckAndInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

MIND_API_OPERATOR_IMPL(RangeV2, BaseOperator);

// AG means auto generated
class MIND_API AGRangeV2Infer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return RangeV2CheckAndInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return RangeV2CheckAndInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return RangeV2Infer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {0, 1, 2}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(RangeV2, prim::kPrimRangeV2, AGRangeV2Infer, false);
}  // namespace ops
}  // namespace mindspore
