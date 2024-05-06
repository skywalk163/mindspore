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

#include "ops/ragged_tensor_to_sparse.h"

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
#include "ir/dtype/container.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sparse_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
constexpr size_t kRttsFirstInput = 0;
constexpr size_t kRttsInputSplitsStart = 0;
constexpr size_t kRttsInputValuesStart = 1;

abstract::TupleShapePtr RaggedTensorToSparseInferShape(const PrimitivePtr &primitive,
                                                       const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  abstract::BaseShapePtrList inputs_splits;
  if (input_args[kRttsFirstInput]->GetType()->object_type() == kObjectTypeTuple) {
    inputs_splits = input_args[kRttsFirstInput]->GetShape()->cast<abstract::TupleShapePtr>()->shape();
  } else if (input_args[kRttsFirstInput]->GetType()->object_type() == kObjectTypeList) {
    inputs_splits = input_args[kRttsFirstInput]->GetShape()->cast<abstract::ListShapePtr>()->shape();
  } else {
    MS_EXCEPTION(TypeError) << "For '" << primitive->name()
                            << "', the input data type must be list or tuple of tensors.";
  }

  auto rt_dense_values_shape = input_args[kRttsInputValuesStart]->GetShape();
  auto in_values_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(rt_dense_values_shape)[kShape];
  auto inputs_splits_element0_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_splits[0])[kShape];
  if (IsDynamic(inputs_splits_element0_shape) || IsDynamic(in_values_shape)) {
    abstract::ShapePtr out_indices =
      std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny, abstract::Shape::kShapeDimAny}));
    abstract::ShapePtr out_values = std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny}));
    abstract::ShapePtr out_shape = std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny}));
    return std::make_shared<abstract::TupleShape>(
      std::vector<abstract::BaseShapePtr>{out_indices, out_values, out_shape});
  }
  (void)CheckAndConvertUtils::CheckInteger("rank of 'rt_dense_values'", SizeToLong(in_values_shape.size()),
                                           kGreaterEqual, 1, primitive->name());
  ShapeVector out_values_shape = {};
  int64_t values_tensor_size = SizeToLong(SizeOf(in_values_shape));
  out_values_shape.push_back(values_tensor_size);
  auto ndim = inputs_splits.size() + in_values_shape.size();
  ShapeVector out_indices_shape = {};
  out_indices_shape.push_back(values_tensor_size);
  out_indices_shape.push_back(ndim);
  ShapeVector out_shape_shape = {};
  out_shape_shape.push_back(ndim);
  abstract::ShapePtr out_indices = std::make_shared<abstract::Shape>(out_indices_shape);
  abstract::ShapePtr out_values = std::make_shared<abstract::Shape>(out_values_shape);
  abstract::ShapePtr out_shape = std::make_shared<abstract::Shape>(out_shape_shape);
  return std::make_shared<abstract::TupleShape>(
    std::vector<abstract::BaseShapePtr>{out_indices, out_values, out_shape});
}

TuplePtr RaggedTensorToSparseInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  auto op_name = primitive->name();
  const std::set<TypePtr> valid_types = {kBool,  kInt8,   kInt16,   kInt32,   kInt64,
                                         kUInt8, kUInt16, kFloat16, kFloat32, kFloat64};
  auto sparse_values_type =
    CheckAndConvertUtils::CheckTensorTypeValid("rt_dense_values", input_args[1]->GetType(), valid_types, op_name);
  auto t_splits_type = GetValue<TypePtr>(primitive->GetAttr("Tsplits"));
  (void)CheckAndConvertUtils::CheckTypeValid("Tsplits", t_splits_type, {kInt64, kInt32}, op_name);
  TypePtrList tensors;
  if (input_args[kRttsInputSplitsStart]->GetType()->object_type() == kObjectTypeTuple) {
    tensors = input_args[kRttsInputSplitsStart]->GetType()->cast<TuplePtr>()->elements();
  } else if (input_args[kRttsInputSplitsStart]->GetType()->object_type() == kObjectTypeList) {
    tensors = input_args[kRttsInputSplitsStart]->GetType()->cast<ListPtr>()->elements();
  } else {
    MS_EXCEPTION(TypeError) << "For '" << op_name << "', the rt_nested_splits must be list or tuple of tensors.";
  }
  for (size_t i = 0; i < tensors.size(); ++i) {
    (void)CheckAndConvertUtils::CheckTypeValid("rt_nested_splits", tensors[i], {t_splits_type}, op_name);
  }
  auto sparse_indices_type = kInt64;
  auto sparse_dense_shape_type = kInt64;
  return std::make_shared<Tuple>(
    std::vector<TypePtr>{sparse_indices_type, sparse_values_type, sparse_dense_shape_type});
}
}  // namespace

MIND_API_OPERATOR_IMPL(RaggedTensorToSparse, BaseOperator);
AbstractBasePtr RaggedTensorToSparseInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                          const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t input_num = 2;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, primitive->name());
  auto types = RaggedTensorToSparseInferType(primitive, input_args);
  auto shapes = RaggedTensorToSparseInferShape(primitive, input_args);
  return abstract::MakeAbstract(shapes, types);
}

// AG means auto generated
class MIND_API AGRaggedTensorToSparseInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return RaggedTensorToSparseInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return RaggedTensorToSparseInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return RaggedTensorToSparseInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(RaggedTensorToSparse, prim::kPrimRaggedTensorToSparse, AGRaggedTensorToSparseInfer,
                                 false);
}  // namespace ops
}  // namespace mindspore
