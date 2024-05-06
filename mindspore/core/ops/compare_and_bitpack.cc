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

#include "ops/compare_and_bitpack.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/dtype/number.h"
#include "ir/dtype/tensor_type.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/comparison_ops.h"
#include "mindspore/core/ops/image_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr CompareAndBitpackInferShape(const PrimitivePtr &primitive,
                                               const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  // support dynamic rank
  if (IsDynamicRank(x_shape)) {
    return std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeRankAny}));
  }

  auto threshold_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  auto x_rank = x_shape.size();

  // threshold must be a scalar tensor
  const size_t kShapeSize_ = 0;
  const size_t divisible_num = 8;
  auto threshold_shape_size = SizeToLong(threshold_shape.size());
  (void)CheckAndConvertUtils::CheckInteger("threshold's rank'", threshold_shape_size, kEqual, SizeToLong(kShapeSize_),
                                           prim_name);

  // Input should be at least a vector
  (void)CheckAndConvertUtils::CheckInteger("x's rank'", SizeToLong(x_rank), kNotEqual, SizeToLong(kShapeSize_),
                                           prim_name);

  // check the innermost dimension of `x`'s shape is disvisible by 8.
  if (x_shape[x_rank - 1] != -1) {
    CheckAndConvertUtils::Check("x innermost dimension % 8", x_shape[x_rank - 1] % SizeToLong(divisible_num), kEqual, 0,
                                prim_name);
  }
  std::vector<int64_t> out_shape;
  for (int dim = 0; dim < SizeToLong(x_rank - 1); dim = dim + 1) {
    (void)out_shape.emplace_back(x_shape[IntToSize(dim)]);
  }

  (void)out_shape.emplace_back(x_shape[x_rank - 1] / SizeToLong(divisible_num));
  auto return_shape = out_shape;
  return std::make_shared<abstract::Shape>(return_shape);
}

TypePtr CompareAndBitpackInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const std::set<TypePtr> valid_types = {kBool, kFloat16, kFloat32, kFloat64, kInt8, kInt16, kInt32, kInt64};
  std::map<std::string, TypePtr> types;
  auto x_type = input_args[kInputIndex0]->GetType();
  MS_EXCEPTION_IF_NULL(x_type);
  auto threshold_type = input_args[kInputIndex1]->GetType();
  MS_EXCEPTION_IF_NULL(threshold_type);
  (void)types.emplace("x", x_type);
  (void)types.emplace("threshold", threshold_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, valid_types, primitive->name());
  return std::make_shared<TensorType>(kUInt8);
}
}  // namespace

AbstractBasePtr CompareAndBitpackInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                       const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t input_num = 2;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, primitive->name());
  auto types = CompareAndBitpackInferType(primitive, input_args);
  auto shapes = CompareAndBitpackInferShape(primitive, input_args);
  return abstract::MakeAbstract(shapes, types);
}

MIND_API_OPERATOR_IMPL(CompareAndBitpack, BaseOperator);

// AG means auto generated
class MIND_API AGCompareAndBitpackInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return CompareAndBitpackInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return CompareAndBitpackInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return CompareAndBitpackInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(CompareAndBitpack, prim::kPrimCompareAndBitpack, AGCompareAndBitpackInfer, false);
}  // namespace ops
}  // namespace mindspore
