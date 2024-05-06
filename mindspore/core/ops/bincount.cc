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

#include <memory>
#include <set>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "base/base.h"
#include "ir/dtype/number.h"
#include "ir/named.h"
#include "ir/primitive.h"
#include "ir/tensor.h"
#include "ir/value.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "ops/bincount.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"
#include "ops/op_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr BincountInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto arr_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  auto size_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  auto w_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->GetShape())[kShape];
  // support dynamic rank
  if (IsDynamicRank(arr_shape) || IsDynamicRank(size_shape) || IsDynamicRank(w_shape)) {
    return std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeRankAny}));
  }

  // support dynamic shape
  if (IsDynamic(arr_shape) || IsDynamic(size_shape) || IsDynamic(w_shape)) {
    ShapeVector shape_out{abstract::Shape::kShapeDimAny};
    return std::make_shared<abstract::Shape>(shape_out);
  }
  (void)CheckAndConvertUtils::CheckInteger("size", SizeToLong(size_shape.size()), kEqual, 0, primitive->name());
  auto size_value_ptr = input_args[kInputIndex1]->GetValue();
  MS_EXCEPTION_IF_NULL(size_value_ptr);
  if (IsValueKnown(size_value_ptr)) {
    if (!CheckAndConvertUtils::IsTensor(input_args[kInputIndex1])) {
      MS_EXCEPTION(ValueError) << "For primitive[" << primitive->name() << "], the input argument[size]"
                               << " must be a tensor, but got " << size_value_ptr->ToString();
    }
    auto out_shape = CheckAndConvertUtils::CheckTensorIntValue("size", size_value_ptr, primitive->name(),
                                                               input_args[kInputIndex1]->GetType());
    (void)CheckAndConvertUtils::CheckPositiveVectorExcludeZero("size", out_shape, primitive->name());
    return std::make_shared<abstract::Shape>(out_shape);
  } else {
    ShapeVector shape_out{abstract::Shape::kShapeDimAny};
    return std::make_shared<abstract::Shape>(shape_out);
  }
}
TypePtr BincountInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const std::set<TypePtr> valid_type = {kInt32};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("array", input_args[kInputIndex0]->GetType(), valid_type,
                                                   primitive->name());
  (void)CheckAndConvertUtils::CheckTensorTypeValid("size", input_args[kInputIndex1]->GetType(), valid_type,
                                                   primitive->name());
  const std::set<TypePtr> valid_types = {kFloat32, kFloat64, kInt32, kInt64};
  auto weights_type = input_args[kInputIndex2]->GetType();
  return CheckAndConvertUtils::CheckTensorTypeValid("weights", weights_type, valid_types, primitive->name());
}
}  // namespace

AbstractBasePtr BincountInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                              const std::vector<AbstractBasePtr> &input_args) {
  const int64_t input_num = 3;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, primitive->name());
  auto infer_type = BincountInferType(primitive, input_args);
  auto infer_shape = BincountInferShape(primitive, input_args);
  return std::make_shared<abstract::AbstractTensor>(infer_type, infer_shape);
}

MIND_API_OPERATOR_IMPL(Bincount, BaseOperator);

// AG means auto generated
class MIND_API AGBincountInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return BincountInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return BincountInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return BincountInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {1}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(Bincount, prim::kPrimBincount, AGBincountInfer, false);
}  // namespace ops
}  // namespace mindspore
