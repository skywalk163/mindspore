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

#include "ops/sparse_add.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/param_validator.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/dtype/type.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/math_ops.h"
#include "mindspore/core/ops/sparse_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
using mindspore::abstract::AbstractTensor;
using mindspore::abstract::AbstractTuple;
namespace {
inline void CheckSparseAddShape(const size_t sparse_shape_size, const size_t expected_dim, const std::string &arg_name,
                                const std::string &op_name, bool is_dyn_rank) {
  if (!is_dyn_rank && sparse_shape_size != expected_dim) {
    MS_EXCEPTION(mindspore::ValueError) << "For " << op_name << ", " << arg_name << " must be a " << expected_dim
                                        << "-dimensional tensor, but got a " << sparse_shape_size
                                        << "-dimensional tensor.";
  }
}

inline void CheckSparseAddNNZ(const int64_t indices_nnz, const int64_t value_nnz, const std::string &indices_name,
                              const std::string &value_name, const std::string &op_name) {
  if (indices_nnz != value_nnz) {
    MS_EXCEPTION(mindspore::ValueError) << "For " << op_name << ", the length of " << indices_name << " and "
                                        << value_name << " must be same, but got length of " << indices_name << " is "
                                        << indices_nnz << ", and length of " << value_name << " is " << value_nnz;
  }
}

inline void CheckSparseAddSameDtype(const mindspore::TypePtr a_dtype, const mindspore::TypePtr b_dtype,
                                    const std::string &a_arg_name, const std::string &b_arg_name,
                                    const std::string &op_name) {
  if (a_dtype->type_id() != b_dtype->type_id()) {
    MS_EXCEPTION(mindspore::TypeError) << "For " << op_name << ", the type of " << a_arg_name << ", and " << b_arg_name
                                       << " should be the same data type, but got type of " << a_arg_name << " is "
                                       << a_dtype->ToString() << ", and type of " << b_arg_name << " is "
                                       << b_dtype->ToString();
  }
}

inline TypePtr GetTensorTypeElement(const AbstractBasePtr &x) {
  auto x_type = x->GetType()->cast<TensorTypePtr>();
  MS_EXCEPTION_IF_NULL(x_type);
  return x_type->element();
}

abstract::TupleShapePtr SparseAddFrontendInferShape(const PrimitivePtr &primitive,
                                                    const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const std::string op_name = primitive->name();

  auto a_indices_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  auto a_values_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  auto a_shape_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->GetShape())[kShape];
  auto b_indices_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex3]->GetShape())[kShape];
  auto b_values_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex4]->GetShape())[kShape];
  auto b_shape_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex5]->GetShape())[kShape];
  auto thresh_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex6]->GetShape())[kShape];

  // Check indices of a and b
  // 2-D indices
  constexpr size_t kIndicesShape = 2;
  auto a_indices_shape_dyn_rank = IsDynamicRank(a_indices_shape);
  auto b_indices_shape_dyn_rank = IsDynamicRank(b_indices_shape);
  CheckSparseAddShape(a_indices_shape.size(), kIndicesShape, "x1_indices", op_name, a_indices_shape_dyn_rank);
  CheckSparseAddShape(b_indices_shape.size(), kIndicesShape, "x2_indices", op_name, b_indices_shape_dyn_rank);
  // 1-D values
  auto a_values_shape_dyn_rank = IsDynamicRank(a_values_shape);
  auto b_values_shape_dyn_rank = IsDynamicRank(b_values_shape);
  CheckSparseAddShape(a_values_shape.size(), 1, "x1_values", op_name, a_values_shape_dyn_rank);
  CheckSparseAddShape(b_values_shape.size(), 1, "x2_values", op_name, b_values_shape_dyn_rank);
  // 1-D shape
  auto a_shape_shape_dyn_rank = IsDynamicRank(a_shape_shape);
  auto b_shape_shape_dyn_rank = IsDynamicRank(b_shape_shape);
  CheckSparseAddShape(a_shape_shape.size(), 1, "x1_shape", op_name, a_shape_shape_dyn_rank);
  CheckSparseAddShape(b_shape_shape.size(), 1, "x2_shape", op_name, b_shape_shape_dyn_rank);
  // 0-D shape
  auto thresh_shape_dyn_rank = IsDynamicRank(thresh_shape);
  CheckSparseAddShape(thresh_shape.size(), 0, "thresh", op_name, thresh_shape_dyn_rank);
  // Check same nnz
  if (!a_indices_shape_dyn_rank && !a_values_shape_dyn_rank && a_indices_shape[0] >= 0 && a_values_shape[0] >= 0) {
    CheckSparseAddNNZ(a_indices_shape[0], a_values_shape[0], "x1_indices", "x1_values", op_name);
  }
  if (!b_indices_shape_dyn_rank && !b_values_shape_dyn_rank && b_indices_shape[0] >= 0 && b_values_shape[0] >= 0) {
    CheckSparseAddNNZ(b_indices_shape[0], b_values_shape[0], "x2_indices", "x2_values", op_name);
  }

  ShapeVector out_indices_shape{-1, 2};
  ShapeVector out_value_shape{-1};
  return std::make_shared<abstract::TupleShape>(std::vector<abstract::BaseShapePtr>{
    std::make_shared<abstract::Shape>(out_indices_shape), std::make_shared<abstract::Shape>(out_value_shape),
    std::make_shared<abstract::Shape>(a_shape_shape)});
}

abstract::TupleShapePtr SparseAddInferShape(const PrimitivePtr &primitive,
                                            const std::vector<AbstractBasePtr> &input_args) {
  MS_LOG(WARNING) << "input args:" << mindspore::ToString(input_args);
  MS_EXCEPTION_IF_NULL(primitive);
  const std::string op_name = primitive->name();

  auto a_indices_shape = input_args[kInputIndex0]->GetShape()->GetShapeVector();
  auto a_values_shape = input_args[kInputIndex1]->GetShape()->GetShapeVector();
  auto a_shape_shape = input_args[kInputIndex2]->GetShape()->GetShapeVector();
  auto b_indices_shape = input_args[kInputIndex3]->GetShape()->GetShapeVector();
  auto b_values_shape = input_args[kInputIndex4]->GetShape()->GetShapeVector();
  auto b_shape_shape = input_args[kInputIndex5]->GetShape()->GetShapeVector();
  auto thresh_shape = input_args[kInputIndex6]->GetShape()->GetShapeVector();

  // Check indices of a and b
  // 2-D indices
  constexpr size_t kIndicesShape = 2;
  CheckSparseAddShape(a_indices_shape.size(), kIndicesShape, "x1_indices", op_name, false);
  CheckSparseAddShape(b_indices_shape.size(), kIndicesShape, "x2_indices", op_name, false);
  // 1-D values
  auto a_values_shape_dyn_rank = IsDynamicRank(a_values_shape);
  auto b_values_shape_dyn_rank = IsDynamicRank(b_values_shape);
  CheckSparseAddShape(a_values_shape.size(), 1, "x1_values", op_name, a_values_shape_dyn_rank);
  CheckSparseAddShape(b_values_shape.size(), 1, "x2_values", op_name, b_values_shape_dyn_rank);
  // 1-D shape
  auto a_shape_shape_dyn_rank = IsDynamicRank(a_shape_shape);
  auto b_shape_shape_dyn_rank = IsDynamicRank(b_shape_shape);
  CheckSparseAddShape(a_shape_shape.size(), 1, "x1_shape", op_name, a_shape_shape_dyn_rank);
  CheckSparseAddShape(b_shape_shape.size(), 1, "x2_shape", op_name, b_shape_shape_dyn_rank);
  // 0-D shape
  auto thresh_shape_dyn_rank = IsDynamicRank(thresh_shape);
  CheckSparseAddShape(thresh_shape.size(), 0, "thresh", op_name, thresh_shape_dyn_rank);
  // Check same nnz
  if (!a_values_shape_dyn_rank && a_indices_shape[0] >= 0 && a_values_shape[0] >= 0) {
    CheckSparseAddNNZ(a_indices_shape[0], a_values_shape[0], "x1_indices", "x1_values", op_name);
  }
  if (!b_values_shape_dyn_rank && b_indices_shape[0] >= 0 && b_values_shape[0] >= 0) {
    CheckSparseAddNNZ(b_indices_shape[0], b_values_shape[0], "x2_indices", "x2_values", op_name);
  }

  ShapeVector out_indices_shape{a_indices_shape[0] + b_indices_shape[0], 2};
  ShapeVector out_value_shape{a_values_shape[0] + b_values_shape[0]};
  return std::make_shared<abstract::TupleShape>(std::vector<abstract::BaseShapePtr>{
    std::make_shared<abstract::Shape>(out_indices_shape), std::make_shared<abstract::Shape>(out_value_shape),
    std::make_shared<abstract::Shape>(a_shape_shape)});
}

TuplePtr SparseAddInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const std::string op_name = primitive->name();
  constexpr size_t kNumOfInputs = 7;
  mindspore::abstract::CheckArgsSize(op_name, input_args, kNumOfInputs);
  auto a_indices = CheckAndConvertUtils::CheckArgsType(op_name, input_args, kInputIndex0, kObjectTypeTensorType);
  auto a_values = CheckAndConvertUtils::CheckArgsType(op_name, input_args, kInputIndex1, kObjectTypeTensorType);
  auto a_shape = CheckAndConvertUtils::CheckArgsType(op_name, input_args, kInputIndex2, kObjectTypeTensorType);
  auto b_indices = CheckAndConvertUtils::CheckArgsType(op_name, input_args, kInputIndex3, kObjectTypeTensorType);
  auto b_values = CheckAndConvertUtils::CheckArgsType(op_name, input_args, kInputIndex4, kObjectTypeTensorType);
  auto b_shape = CheckAndConvertUtils::CheckArgsType(op_name, input_args, kInputIndex5, kObjectTypeTensorType);
  auto thresh = CheckAndConvertUtils::CheckArgsType(op_name, input_args, kInputIndex6, kObjectTypeTensorType);

  // Check dtype
  // a_indices and b_indices should be int64
  auto a_indices_type = GetTensorTypeElement(a_indices);
  auto b_indices_type = GetTensorTypeElement(b_indices);
  const std::set<TypePtr> indices_valid_types = {kInt64};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x1_indices", a_indices->GetType(), indices_valid_types, op_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x2_indices", b_indices->GetType(), indices_valid_types, op_name);
  // a_shape and b_shape should be int64
  auto a_shape_type = GetTensorTypeElement(a_shape);
  auto b_shape_type = GetTensorTypeElement(b_shape);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x1_shape", a_shape->GetType(), indices_valid_types, op_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x2_shape", b_shape->GetType(), indices_valid_types, op_name);
  // check a_values and b_values
  auto a_value_type = GetTensorTypeElement(a_values);
  auto b_value_type = GetTensorTypeElement(b_values);
  const std::set<TypePtr> value_valid_types = {kInt8,    kInt16,   kInt32,     kInt64,
                                               kFloat32, kFloat64, kComplex64, kComplex128};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x1_values", a_values->GetType(), value_valid_types, op_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x2_values", b_values->GetType(), value_valid_types, op_name);
  // Check thresh
  auto thresh_type = GetTensorTypeElement(thresh);
  const std::set<TypePtr> thresh_valid_types = {kInt8, kInt16, kInt32, kInt64, kFloat32, kFloat64};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("thresh", thresh->GetType(), thresh_valid_types, op_name);

  // Check same type
  // value
  CheckSparseAddSameDtype(a_value_type, b_value_type, "x1_values", "x2_values", op_name);
  // indices
  CheckSparseAddSameDtype(a_indices_type, b_indices_type, "x1_values", "x2_values", op_name);
  // shape
  CheckSparseAddSameDtype(a_shape_type, b_shape_type, "x1_values", "x2_values", op_name);
  // thresh and value
  if (!((a_value_type->equal(mindspore::kComplex64) && thresh_type->equal(mindspore::kFloat32)) ||
        (a_value_type->equal(mindspore::kComplex128) && thresh_type->equal(mindspore::kFloat64)))) {
    CheckSparseAddSameDtype(a_value_type, thresh_type, "x1_values", "thresh", op_name);
  }

  return std::make_shared<Tuple>(std::vector<TypePtr>{a_indices_type, a_value_type, a_shape_type});
}

AbstractBasePtr SparseAddInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                               const std::vector<abstract::AbstractBasePtr> &input_args) {
  auto tuple_shapes = SparseAddFrontendInferShape(primitive, input_args);
  auto tuple_types = SparseAddInferType(primitive, input_args);
  return abstract::MakeAbstract(tuple_shapes, tuple_types);
}
}  // namespace

MIND_API_OPERATOR_IMPL(SparseAdd, BaseOperator);
class MIND_API AGSparseAddInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseAddInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseAddInferType(primitive, input_args);
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseAddInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(SparseAdd, prim::kPrimSparseAdd, AGSparseAddInfer, false);
}  // namespace ops
}  // namespace mindspore
