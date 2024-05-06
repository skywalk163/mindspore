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

#include "ops/sparse_cross.h"

#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/dtype/container.h"
#include "ir/dtype/number.h"
#include "ir/dtype/tensor_type.h"
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
constexpr size_t kSparseCrossFirstInput = 0;
constexpr size_t kSparseCrossInputIndicesStart = 0;
constexpr size_t kSparseCrossInputValueStart = 1;
constexpr size_t kSparseCrossInputShapeStart = 2;
constexpr size_t kSparseCrossInputDenseStart = 3;

TypePtrList GetCrossSequenceTypes(const PrimitivePtr &primitive, const AbstractBasePtr &x) {
  bool is_tuple = x->GetType()->object_type() == kObjectTypeTuple;
  bool is_list = x->GetType()->object_type() == kObjectTypeList;
  if ((!is_tuple) && (!is_list)) {
    MS_EXCEPTION(mindspore::ValueError) << "For " << primitive->name()
                                        << ", the input must be a list or tuple of sparse tensor. but got: "
                                        << x->ToString() << ".";
  }
  auto x_types = x->GetType();
  TypePtrList types_list;
  if (is_tuple) {
    types_list = x_types->cast<TuplePtr>()->elements();
  } else {
    types_list = x_types->cast<ListPtr>()->elements();
  }
  return types_list;
}

abstract::BaseShapePtrList GetCrossSequenceShapes(const PrimitivePtr &primitive, const AbstractBasePtr &x) {
  bool is_tuple = x->GetType()->object_type() == kObjectTypeTuple;
  bool is_list = x->GetType()->object_type() == kObjectTypeList;
  if ((!is_tuple) && (!is_list)) {
    MS_EXCEPTION(mindspore::ValueError) << "For " << primitive->name()
                                        << ", the input must be a list or tuple of sparse tensor. but got: "
                                        << x->ToString() << ".";
  }
  auto x_shapes = x->GetShape();
  abstract::BaseShapePtrList shapes_list;
  if (is_tuple) {
    auto shapes_tuple = x_shapes->cast<abstract::TupleShapePtr>();
    MS_EXCEPTION_IF_NULL(shapes_tuple);
    shapes_list = shapes_tuple->shape();
  } else {
    auto shapes_tuple = x_shapes->cast<abstract::ListShapePtr>();
    MS_EXCEPTION_IF_NULL(shapes_tuple);
    shapes_list = shapes_tuple->shape();
  }
  return shapes_list;
}

bool SparseCrossCheckShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  bool hashed_output = GetValue<bool>(primitive->GetAttr("hashed_output"));
  if (!hashed_output) {
    MS_EXCEPTION(TypeError) << "For SparseCross, only support int64, so hashed_output should be true"
                            << ".";
  }
  auto op_name = primitive->name();

  auto inputs_indices_shapes = GetCrossSequenceShapes(primitive, input_args[kSparseCrossInputIndicesStart]);
  auto indices_element0_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_indices_shapes[0])[kShape];
  auto inputs_values_shapes = GetCrossSequenceShapes(primitive, input_args[kSparseCrossInputValueStart]);
  auto values_element0_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_values_shapes[0])[kShape];
  auto inputs_shapes_shapes = GetCrossSequenceShapes(primitive, input_args[kSparseCrossInputShapeStart]);
  auto shapes_element0_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_shapes_shapes[0])[kShape];
  auto inputs_denses_shapes = GetCrossSequenceShapes(primitive, input_args[kSparseCrossInputDenseStart]);
  auto denses_element0_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_denses_shapes[0])[kShape];

  if (IsDynamic(indices_element0_shape) || IsDynamic(values_element0_shape) || IsDynamic(shapes_element0_shape) ||
      IsDynamic(denses_element0_shape)) {
    return false;
  }
  int64_t dim = 2;
  uint32_t dim_size = 2;
  if (indices_element0_shape[1] != dim) {
    MS_EXCEPTION(mindspore::ValueError) << "For " << op_name << ", the indices shape rank should be 2.";
  }
  if (denses_element0_shape.size() != dim_size) {
    MS_EXCEPTION(mindspore::ValueError) << "For " << op_name << ", the denses shape rank should be 2.";
  }
  if (shapes_element0_shape[0] != dim) {
    MS_EXCEPTION(mindspore::ValueError) << "For " << op_name << ", the shapes rank should be 2.";
  }
  if (indices_element0_shape[1] != shapes_element0_shape[0]) {
    MS_EXCEPTION(mindspore::ValueError) << "For " << op_name << ", the indices shape rank is "
                                        << indices_element0_shape[1] << ", but the shape rank is "
                                        << shapes_element0_shape[0] << ".";
  }
  if (indices_element0_shape[0] != values_element0_shape[0]) {
    MS_EXCEPTION(mindspore::ValueError) << "For " << op_name << ", the indices element number is "
                                        << indices_element0_shape[0] << ", but the value element number is "
                                        << values_element0_shape[0] << ".";
  }
  int64_t value = 2;
  (void)CheckAndConvertUtils::CheckInteger("rank of indices", SizeToLong(indices_element0_shape.size()), kEqual, value,
                                           op_name);
  (void)CheckAndConvertUtils::CheckInteger("rank of values", SizeToLong(values_element0_shape.size()), kEqual, 1,
                                           op_name);
  (void)CheckAndConvertUtils::CheckInteger("rank of shape", SizeToLong(shapes_element0_shape.size()), kEqual, 1,
                                           op_name);
  (void)CheckAndConvertUtils::CheckInteger("rank of start", SizeToLong(denses_element0_shape.size()), kEqual, value,
                                           op_name);
  return true;
}

abstract::TupleShapePtr SparseCrossInferShape(const PrimitivePtr &primitive,
                                              const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  if (!SparseCrossCheckShape(primitive, input_args)) {
    auto out_indices_shape =
      std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny, abstract::Shape::kShapeDimAny}));
    auto out_value_shape = std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny}));
    auto out_shape_shape = std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny}));

    return std::make_shared<abstract::TupleShape>(
      std::vector<abstract::BaseShapePtr>{out_indices_shape, out_value_shape, out_shape_shape});
  }
  auto inputs_indices_shapes1 = GetCrossSequenceShapes(primitive, input_args[kSparseCrossInputIndicesStart]);
  auto indices_element0_shape1 = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_indices_shapes1[0])[kShape];
  auto inputs_dense_shapes1 = GetCrossSequenceShapes(primitive, input_args[kSparseCrossInputDenseStart]);
  auto inputs_shape_shapes1 = GetCrossSequenceShapes(primitive, input_args[kSparseCrossInputShapeStart]);
  auto shape_shape1 = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_shape_shapes1[0])[kShape];
  int64_t rank = indices_element0_shape1[1];
  int64_t indices_row = 0;
  std::vector<int64_t> nnz(shape_shape1[0], 1);
  for (uint32_t r = 0; r < shape_shape1[0]; r++) {
    for (unsigned int i = 0; i < inputs_indices_shapes1.size(); i++) {
      auto indices_shape2 = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_indices_shapes1[i])[kShape];
      nnz[r] = nnz[r] * indices_shape2[0];
    }
    for (uint32_t i = 0; i < inputs_dense_shapes1.size(); i++) {
      auto denses_shape2 = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_dense_shapes1[i])[kShape];
      nnz[r] = nnz[r] * denses_shape2[1];
    }
    indices_row = indices_row + nnz[r];
  }

  auto out_indices_shape = std::make_shared<abstract::Shape>(ShapeVector({indices_row, rank}));
  auto out_value_shape = std::make_shared<abstract::Shape>(ShapeVector({indices_row}));
  auto out_shape_shape = std::make_shared<abstract::Shape>(ShapeVector({rank}));
  return std::make_shared<abstract::TupleShape>(
    std::vector<abstract::BaseShapePtr>{out_indices_shape, out_value_shape, out_shape_shape});
}

abstract::TupleShapePtr SparseCrossFrontendInferShape(const PrimitivePtr &primitive,
                                                      const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  if (!SparseCrossCheckShape(primitive, input_args)) {
    auto out_indices_shape =
      std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny, abstract::Shape::kShapeDimAny}));
    auto out_value_shape = std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny}));
    auto out_shape_shape = std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny}));

    return std::make_shared<abstract::TupleShape>(
      std::vector<abstract::BaseShapePtr>{out_indices_shape, out_value_shape, out_shape_shape});
  }
  auto inputs_indices_shapes1 = GetCrossSequenceShapes(primitive, input_args[kSparseCrossInputIndicesStart]);
  auto indices_element0_shape1 = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_indices_shapes1[0])[kShape];
  auto inputs_dense_shapes1 = GetCrossSequenceShapes(primitive, input_args[kSparseCrossInputDenseStart]);
  auto inputs_shape_shapes1 = GetCrossSequenceShapes(primitive, input_args[kSparseCrossInputShapeStart]);
  auto shape_shape1 = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_shape_shapes1[0])[kShape];
  int64_t rank = indices_element0_shape1[1];
  int64_t indices_row = 0;
  std::vector<int64_t> nnz(shape_shape1[0], 1);
  for (uint32_t r = 0; r < shape_shape1[0]; r++) {
    for (unsigned int i = 0; i < inputs_indices_shapes1.size(); i++) {
      auto indices_shape2 = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_indices_shapes1[i])[kShape];
      nnz[r] = nnz[r] * indices_shape2[0];
    }
    for (uint32_t i = 0; i < inputs_dense_shapes1.size(); i++) {
      auto denses_shape2 = CheckAndConvertUtils::ConvertShapePtrToShapeMap(inputs_dense_shapes1[i])[kShape];
      nnz[r] = nnz[r] * denses_shape2[1];
    }
    indices_row = indices_row + nnz[r];
  }

  auto out_indices_shape = std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny, rank}));
  auto out_value_shape = std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny}));
  auto out_shape_shape = std::make_shared<abstract::Shape>(ShapeVector({rank}));
  return std::make_shared<abstract::TupleShape>(
    std::vector<abstract::BaseShapePtr>{out_indices_shape, out_value_shape, out_shape_shape});
}

TuplePtr SparseCrossInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  auto op_name = primitive->name();
  auto input_values_types = GetCrossSequenceTypes(primitive, input_args[kSparseCrossInputValueStart]);
  for (size_t i = 0; i < input_values_types.size(); ++i) {
    auto input_dtype = input_values_types[i];
    (void)CheckAndConvertUtils::CheckTypeValid("values", input_dtype, {kInt64}, op_name);
  }
  auto sparse_values_type = std::make_shared<TensorType>(kInt64);
  auto sparse_indices_type = std::make_shared<TensorType>(kInt64);
  auto sparse_dense_shape_type = std::make_shared<TensorType>(kInt64);
  return std::make_shared<Tuple>(
    std::vector<TypePtr>{sparse_indices_type, sparse_values_type, sparse_dense_shape_type});
}
};  // namespace

AbstractBasePtr SparseCrossInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                 const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t kInputsNum = 4;
  CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, kInputsNum, primitive->name());
  auto infer_type = SparseCrossInferType(primitive, input_args);
  auto infer_shape = SparseCrossFrontendInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}
MIND_API_OPERATOR_IMPL(SparseCross, BaseOperator);

// AG means auto generated
class MIND_API AGSparseCrossInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseCrossInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseCrossInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseCrossInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(SparseCross, prim::kPrimSparseCross, AGSparseCrossInfer, false);
}  // namespace ops
}  // namespace mindspore
