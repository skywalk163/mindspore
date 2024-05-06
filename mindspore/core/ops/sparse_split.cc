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

#include "ops/sparse_split.h"

#include <memory>

#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "ir/dtype/container.h"
#include "ir/dtype/number.h"
#include "ir/dtype/tensor_type.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sparse_ops.h"
#include "ops/op_name.h"
#include "ops/op_utils.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
namespace {
void SparseSplitShapeCheck(const PrimitivePtr &prim, const ShapeVector &split_dim_shape_vec,
                           const ShapeVector &indices_shape_vec, const ShapeVector &values_shape_vec,
                           const ShapeVector &shape_shape_vec) {
  (void)CheckAndConvertUtils::CheckInteger("split_dim's rank'", (SizeToLong)(split_dim_shape_vec.size()), kLessEqual, 1,
                                           prim->name());
  (void)CheckAndConvertUtils::CheckInteger("values' rank'", (SizeToLong)(values_shape_vec.size()), kEqual, 1,
                                           prim->name());
  (void)CheckAndConvertUtils::CheckInteger("shape' rank'", (SizeToLong)(shape_shape_vec.size()), kEqual, 1,
                                           prim->name());
  if (!IsDynamic(split_dim_shape_vec) && split_dim_shape_vec.size() == 1) {
    (void)CheckAndConvertUtils::CheckInteger("split_dim's size", split_dim_shape_vec[0], kEqual, 1, prim->name());
  }
  if (!IsDynamicRank(indices_shape_vec)) {
    const int64_t rank = 2;
    (void)CheckAndConvertUtils::CheckInteger("indices' rank'", (SizeToLong)(indices_shape_vec.size()), kEqual, rank,
                                             prim->name());
  }
}

std::vector<abstract::BaseShapePtr> GetOutputShapes(const ShapeVector &output_indices_vec,
                                                    const ShapeVector &output_values_vec,
                                                    const ShapeVector &shape_shape_vec, const int64_t num_splits) {
  std::vector<abstract::BaseShapePtr> shape_tuple;
  for (auto i = 0; i < num_splits; i++) {
    abstract::BaseShapePtr output_indices_shape = std::make_shared<abstract::TensorShape>(output_indices_vec);
    shape_tuple.push_back(output_indices_shape);
  }
  for (auto i = 0; i < num_splits; i++) {
    abstract::BaseShapePtr output_values_shape = std::make_shared<abstract::TensorShape>(output_values_vec);
    shape_tuple.push_back(output_values_shape);
  }
  for (auto i = 0; i < num_splits; i++) {
    abstract::BaseShapePtr output_shape_shape = std::make_shared<abstract::TensorShape>(shape_shape_vec);
    shape_tuple.push_back(output_shape_shape);
  }
  return shape_tuple;
}

abstract::TupleShapePtr SparseSplitInferShape(const PrimitivePtr &prim,
                                              const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = prim->name();
  (void)CheckAndConvertUtils::CheckInteger("input numbers", SizeToLong(input_args.size()), kEqual, 4L, prim_name);

  const auto &split_dim_shape_vec = input_args[kInputIndex0]->GetShape()->GetShapeVector();
  const auto &indices_shape_vec = input_args[kInputIndex1]->GetShape()->GetShapeVector();
  const auto &values_shape_vec = input_args[kInputIndex2]->GetShape()->GetShapeVector();
  const auto &shape_shape_vec = input_args[kInputIndex3]->GetShape()->GetShapeVector();

  // check shapes
  SparseSplitShapeCheck(prim, split_dim_shape_vec, indices_shape_vec, values_shape_vec, shape_shape_vec);

  // get out shapes
  ShapeVector output_indices_vec = {abstract::TensorShape::kShapeDimAny, abstract::TensorShape::kShapeDimAny};
  ShapeVector output_values_vec = {abstract::TensorShape::kShapeDimAny};
  if (!IsDynamicRank(shape_shape_vec)) {
    output_indices_vec[kInputIndex1] = shape_shape_vec[kInputIndex0];
  }
  auto num_splits = GetValue<int64_t>(prim->GetAttr("num_split"));
  auto shape_tuple = GetOutputShapes(output_indices_vec, output_values_vec, shape_shape_vec, num_splits);

  return std::make_shared<abstract::TupleShape>(std::move(shape_tuple));
}

TuplePtr SparseSplitInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  auto num_split = GetValue<int64_t>(prim->GetAttr("num_split"));
  auto split_dim_type = input_args[kInputIndex0]->GetType();
  auto indices_type = input_args[kInputIndex1]->GetType();
  auto values_type = input_args[kInputIndex2]->GetType();
  auto shape_type = input_args[kInputIndex3]->GetType();
  MS_EXCEPTION_IF_NULL(split_dim_type);
  MS_EXCEPTION_IF_NULL(indices_type);
  MS_EXCEPTION_IF_NULL(values_type);
  MS_EXCEPTION_IF_NULL(shape_type);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("split_dim's type", split_dim_type, {kInt64}, prim->name());
  (void)CheckAndConvertUtils::CheckTensorTypeValid("indices's type", indices_type, {kInt64}, prim->name());
  (void)CheckAndConvertUtils::CheckTensorTypeValid("shape's type", shape_type, {kInt64}, prim->name());
  auto type = CheckAndConvertUtils::CheckTensorTypeValid("values", values_type,
                                                         common_valid_types_with_complex_and_bool, prim->name());
  std::vector<TypePtr> type_tuple;
  for (int64_t i = 0; i < num_split; i++) {
    type_tuple.push_back(std::make_shared<TensorType>(kInt64));
  }
  for (int64_t i = 0; i < num_split; i++) {
    type_tuple.push_back(type);
  }
  for (int64_t i = 0; i < num_split; i++) {
    type_tuple.push_back(std::make_shared<TensorType>(kInt64));
  }
  return std::make_shared<Tuple>(type_tuple);
}
}  // namespace

MIND_API_OPERATOR_IMPL(SparseSplit, BaseOperator);
AbstractBasePtr SparseSplitInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                 const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t input_num = 4;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, primitive->name());
  auto infertype = SparseSplitInferType(primitive, input_args);
  auto infershape = SparseSplitInferShape(primitive, input_args);
  return abstract::MakeAbstract(infershape, infertype);
}

// AG means auto generated
class MIND_API AGSparseSplitInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    auto value_shape = input_args[kInputIndex2]->GetShape()->GetShapeVector();
    auto shape_shape = input_args[kInputIndex3]->GetShape()->GetShapeVector();

    MS_CHECK_VALUE(value_shape.size() == 1, CheckAndConvertUtils::FormatCheckIntegerMsg(
                                              "rank of values", SizeToLong(value_shape.size()), kEqual, 1, primitive));
    MS_CHECK_VALUE(shape_shape.size() == 1, CheckAndConvertUtils::FormatCheckIntegerMsg(
                                              "rank of shape", SizeToLong(shape_shape.size()), kEqual, 1, primitive));
    ShapeVector output_indices_vec = {value_shape[0], shape_shape[0]};
    ShapeVector output_values_vec = {value_shape[0]};

    auto num_splits = GetValue<int64_t>(primitive->GetAttr("num_split"));
    auto shape_tuple = GetOutputShapes(output_indices_vec, output_values_vec, shape_shape, num_splits);

    return std::make_shared<abstract::TupleShape>(std::move(shape_tuple));
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseSplitInferType(primitive, input_args);
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseSplitInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(SparseSplit, prim::kPrimSparseSplit, AGSparseSplitInfer, false);
}  // namespace ops
}  // namespace mindspore
