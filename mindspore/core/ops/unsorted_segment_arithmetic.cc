/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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

#include "ops/unsorted_segment_arithmetic.h"

#include <memory>
#include <set>
#include <string>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype.h"
#include "ir/dtype/number.h"
#include "ir/dtype/tensor_type.h"
#include "ir/dtype/type.h"
#include "ir/primitive.h"
#include "ir/tensor.h"
#include "mindapi/base/type_id.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "ops/op_name.h"
#include "ops/op_utils.h"
#include "ops/primitive_c.h"
#include "ops/unsorted_segment_max.h"
#include "ops/unsorted_segment_min.h"
#include "ops/unsorted_segment_prod.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
int64_t GetNumSegmentsValue(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const std::string &op_name = primitive->name();
  int64_t num_segments_v = 0;
  auto value = input_args[kInputIndex2]->GetValue();
  MS_EXCEPTION_IF_NULL(value);
  auto num_segments_type = input_args[kInputIndex2]->GetType();
  MS_EXCEPTION_IF_NULL(num_segments_type);
  if (CheckAndConvertUtils::IsTensor(input_args[kInputIndex2])) {
    if (IsValueKnown(value)) {
      auto n_value_ptr = input_args[kInputIndex2]->GetValue();
      MS_EXCEPTION_IF_NULL(n_value_ptr);
      auto n_value_ptr_tensor =
        CheckAndConvertUtils::CheckTensorIntValue("num_segments", n_value_ptr, op_name, num_segments_type);
      if (n_value_ptr_tensor.empty()) {
        MS_EXCEPTION(ValueError) << "For '" << op_name << "' the third input should be an int value, but got empty.";
      }
      num_segments_v = n_value_ptr_tensor.back();
    } else {
      num_segments_v = abstract::Shape::kShapeDimAny;
    }
    return num_segments_v;
  } else if (CheckAndConvertUtils::IsScalar(input_args[kInputIndex2])) {
    if (!IsValueKnown(value)) {
      num_segments_v = abstract::Shape::kShapeDimAny;
      return num_segments_v;
    }

    if (num_segments_type->type_id() == kNumberTypeInt64) {
      num_segments_v = GetScalarValue<int64_t>(value).value();
    } else if (num_segments_type->type_id() == kNumberTypeInt32) {
      num_segments_v = GetScalarValue<int32_t>(value).value();
    } else {
      MS_EXCEPTION(TypeError) << "For '" << op_name << "' the third input build type is invalid:"
                              << TypeIdToString(num_segments_type->type_id()) << ".";
    }
    (void)CheckAndConvertUtils::CheckInteger("num_segments's value", num_segments_v, kGreaterThan, 0, op_name);
    return num_segments_v;
  } else {
    MS_LOG(EXCEPTION) << "For '" << op_name
                      << "', the third input type should be tensor or scalar, but got invalid abstract type:"
                      << input_args[kInputIndex2]->type_name() << ".";
  }
}
namespace {
abstract::ShapePtr InferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args,
                              const int64_t &num_segments_value) {
  auto prim_name = primitive->name();
  auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  (void)CheckAndConvertUtils::CheckInteger("input_x shape size", SizeToLong(x_shape.size()), kGreaterThan, 0,
                                           prim_name);
  auto ids_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  if (ids_shape.empty()) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "', segment_ids value cannot be 0-D.";
  }

  for (auto ids_shape_value : ids_shape) {
    if (ids_shape_value < 0) {
      MS_EXCEPTION(ValueError) << "For '" << prim_name
                               << "', segment_ids value must be non-negtive tensor, but got: " << ids_shape_value
                               << ".";
    }
  }

  if (x_shape.size() < ids_shape.size()) {
    MS_EXCEPTION(ValueError) << "For " << prim_name << ", invalid input_args and segment_ids shape size";
  }

  for (size_t i = 0; i < ids_shape.size(); i++) {
    if (x_shape[i] != ids_shape[i]) {
      MS_EXCEPTION(ValueError) << "For " << prim_name
                               << ", the first shape of input_x should be equal to length of segments_id";
    }
  }

  int64_t batch_rank = 0;
  if (primitive->HasAttr(kBatchRank)) {
    auto batch_rank_ptr = primitive->GetAttr(kBatchRank);
    batch_rank = GetValue<int64_t>(batch_rank_ptr);
  }

  std::vector<int64_t> out_shape;
  if (batch_rank != 0) {
    for (int64_t i = 0; i < batch_rank; i++) {
      out_shape.push_back(x_shape.at(i));
    }
  }

  out_shape.push_back(num_segments_value);

  for (size_t i = ids_shape.size(); i < x_shape.size(); i++) {
    out_shape.push_back(x_shape.at(i));
  }
  return std::make_shared<abstract::Shape>(out_shape);
}

abstract::ShapePtr UnsortedSegmentArithmeticInferShape(const PrimitivePtr &primitive,
                                                       const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();

  auto x_shape_ptr = input_args[kInputIndex0]->GetShape();
  MS_EXCEPTION_IF_NULL(x_shape_ptr);
  if (IsDynamicRank(CheckAndConvertUtils::ConvertShapePtrToShapeMap(x_shape_ptr)[kShape])) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>{abstract::Shape::kShapeRankAny});
  }

  auto segment_ids_shape_ptr = input_args[kInputIndex1]->GetShape();
  MS_EXCEPTION_IF_NULL(segment_ids_shape_ptr);
  if (IsDynamicRank(CheckAndConvertUtils::ConvertShapePtrToShapeMap(segment_ids_shape_ptr)[kShape])) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>{abstract::Shape::kShapeRankAny});
  }

  auto num_segments_shape_ptr = input_args[kInputIndex2]->GetShape();
  MS_EXCEPTION_IF_NULL(num_segments_shape_ptr);

  if (x_shape_ptr->IsDynamic() || segment_ids_shape_ptr->IsDynamic() || num_segments_shape_ptr->IsDynamic()) {
    return x_shape_ptr->cast<abstract::ShapePtr>();
  }

  int64_t num_segments_value = GetNumSegmentsValue(primitive, input_args);
  if (num_segments_value <= 0) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name
                             << "', num_segments value must be greater than 0, but got: " << num_segments_value << ".";
  }

  return InferShape(primitive, input_args, num_segments_value);
}

TypePtr UnsortedSegmentArithmeticInferType(const PrimitivePtr &primitive,
                                           const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();

  /* check segment_ids */
  auto ids_ptr = input_args[kInputIndex1]->GetType();
  MS_EXCEPTION_IF_NULL(ids_ptr);
  if (!ids_ptr->isa<TensorType>()) {
    MS_EXCEPTION(TypeError) << "For '" << prim_name
                            << "', segment_ids must be a tensor, but got: " << ids_ptr->ToString() << ".";
  }
  std::set<TypePtr> ids_type_set = {kInt32, kInt64};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("segment_ids", ids_ptr, ids_type_set, prim_name);

  /* check num_segments */
  auto num_ptr = input_args[kInputIndex2]->GetType();
  MS_EXCEPTION_IF_NULL(num_ptr);
  std::set<TypePtr> num_type_set = {kInt32, kInt64};

  if (CheckAndConvertUtils::IsTensor(input_args[kInputIndex2])) {
    auto num_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->GetShape())[kShape];
    if (num_shape.size() != 0) {
      MS_EXCEPTION(TypeError) << "For '" << prim_name
                              << "', num_segments must be an integer, but got: " << num_ptr->ToString() << ".";
    }
  }
  (void)CheckAndConvertUtils::CheckTypeValid("num_segments", num_ptr, num_type_set, prim_name);

  /* check input_x */
  auto in_type_ptr = input_args[kInputIndex0]->GetType();
  MS_EXCEPTION_IF_NULL(in_type_ptr);
  if (!CheckAndConvertUtils::IsTensor(input_args[kInputIndex0])) {
    MS_EXCEPTION(TypeError) << "For '" << prim_name << "', input must be a tensor, but got: " << in_type_ptr->ToString()
                            << ".";
  }
  return CheckAndConvertUtils::CheckSubClass("x", in_type_ptr, {kTensorType}, prim_name);
}
}  // namespace

AbstractBasePtr UnsortedSegmentArithmeticInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                               const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  const int64_t input_num = 3;
  CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, input_num, primitive->name());
  auto infer_type = UnsortedSegmentArithmeticInferType(primitive, input_args);
  auto infer_shape = UnsortedSegmentArithmeticInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

MIND_API_OPERATOR_IMPL(UnsortedSegmentMax, BaseOperator);
MIND_API_OPERATOR_IMPL(UnsortedSegmentMin, BaseOperator);
MIND_API_OPERATOR_IMPL(UnsortedSegmentProd, BaseOperator);

// AG means auto generated
class MIND_API AGUnsortedSegmentArithmeticInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return UnsortedSegmentArithmeticInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return UnsortedSegmentArithmeticInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return UnsortedSegmentArithmeticInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {2}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(UnsortedSegmentMax, prim::kPrimUnsortedSegmentMax, AGUnsortedSegmentArithmeticInfer,
                                 false);
REGISTER_PRIMITIVE_OP_INFER_IMPL(UnsortedSegmentMin, prim::kPrimUnsortedSegmentMin, AGUnsortedSegmentArithmeticInfer,
                                 false);
REGISTER_PRIMITIVE_OP_INFER_IMPL(UnsortedSegmentProd, prim::kPrimUnsortedSegmentProd, AGUnsortedSegmentArithmeticInfer,
                                 false);
}  // namespace ops
}  // namespace mindspore
