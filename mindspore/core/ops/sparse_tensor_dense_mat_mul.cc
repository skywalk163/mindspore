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
#include "ops/sparse_tensor_dense_mat_mul.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype.h"
#include "ir/dtype/number.h"
#include "ir/dtype/tensor_type.h"
#include "ir/dtype/type.h"
#include "ir/named.h"
#include "ir/primitive.h"
#include "ir/tensor.h"
#include "ir/value.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/math_ops.h"
#include "mindspore/core/ops/sparse_ops.h"
#include "ops/op_name.h"
#include "ops/op_utils.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
const int kDimensionOne = 1;
const int kDimensionTwo = 2;
void CheckShapeRank(const size_t cur_rank, const size_t expected_rank, const std::string &op_name,
                    const std::string &arg_name) {
  if (cur_rank != expected_rank) {
    MS_EXCEPTION(ValueError) << "For '" << op_name << "', '" << arg_name << "' must be a " << expected_rank
                             << "-dimensional tensor, but got a " << cur_rank << "-dimensional tensor.";
  }
}
}  // namespace

bool checkType(std::string name, TypePtr dtype, const std::set<TypePtr> &vtypes, const PrimitivePtr &primitive) {
  std::map<std::string, TypePtr> types;
  (void)types.emplace(name, dtype);
  try {
    (void)CheckAndConvertUtils::CheckTensorTypeSame(types, vtypes, primitive->name());
  } catch (...) {
    return false;
  }
  return true;
}

bool checkContainer(const std::vector<AbstractBasePtr> &input_args, std::string *const info) {
  const int kTwo = 2;
  const int kOne = 1;
  const int kZero = 0;
  const int kThree = 3;
  if (!CheckAndConvertUtils::IsTensor(input_args[kTwo]) && !CheckAndConvertUtils::IsTuple(input_args[kTwo])) {
    *info = ", the input sparse_shape only support tensor or tuple!";
    return false;
  }
  if (!CheckAndConvertUtils::IsTensor(input_args[kZero])) {
    *info = ", the input indices only support tensor!";
    return false;
  }
  if (!CheckAndConvertUtils::IsTensor(input_args[kOne])) {
    *info = ", the input values only support tensor!";
    return false;
  }
  if (!CheckAndConvertUtils::IsTensor(input_args[kThree])) {
    *info = ", the input dense only support tensor!";
    return false;
  }
  return true;
}

void SparseTensorDenseMatmulCheckShape(const std::string &prim_name, const bool &is_dynamic_rank,
                                       const bool &is_dynamic, const ShapeVector &indices_shape,
                                       const ShapeVector &values_shape, const ShapeVector &shape_shape,
                                       const ShapeVector &x2_shape) {
  if (!is_dynamic_rank) {
    CheckShapeRank(indices_shape.size(), kDimensionTwo, prim_name, "indices");
    CheckShapeRank(values_shape.size(), kDimensionOne, prim_name, "values");
    CheckShapeRank(shape_shape.size(), kDimensionOne, prim_name, "sparse_shape");
    CheckShapeRank(x2_shape.size(), kDimensionTwo, prim_name, "the shape of input dense");
  }
  if (!is_dynamic) {
    if (indices_shape[1] != kDimensionTwo) {
      MS_EXCEPTION(ValueError) << "For '" << prim_name << "', the 2nd dimension of indices "
                               << "should be 2, but got " << indices_shape[1] << ".";
    }
    if (values_shape[0] != indices_shape[0]) {
      MS_EXCEPTION(ValueError) << "For '" << prim_name << "', the input values' length "
                               << "is different from indices' first dimension";
    }
    if (shape_shape[0] != kDimensionTwo) {
      MS_EXCEPTION(ValueError) << "For '" << prim_name << "', the 1st dimension of sparse_shape "
                               << "should be 2, but got " << shape_shape[0] << ".";
    }
  }
}

void SparseTensorDenseMatmulCheckShapeSetShape(const std::string &prim_name, int64_t *shape_ptr,
                                               const ShapeVector &shape_shape, const AbstractBasePtr &x1_shape) {
  if (CheckAndConvertUtils::IsTensor(x1_shape) && IsValueKnown(x1_shape->GetValue())) {
    auto a_shape_value = x1_shape->GetValue();
    MS_EXCEPTION_IF_NULL(a_shape_value);
    auto a_shape_vec =
      CheckAndConvertUtils::CheckTensorIntValue("sparse_shape", a_shape_value, prim_name, x1_shape->GetType());
    if (!IsDynamic(shape_shape)) {
      auto a_shape_size = a_shape_vec.size();
      auto expect_size = std::accumulate(shape_shape.begin(), shape_shape.end(), 1, std::multiplies{});
      MS_EXCEPTION_IF_CHECK_FAIL(a_shape_size == LongToSize(expect_size),
                                 "For '" + prim_name + "', something unexpected happened.");
    }
    for (size_t i = 0; i < kDimensionTwo; ++i) {
      shape_ptr[i] = a_shape_vec[i];
    }
  } else if (IsIdentidityOrSubclass(x1_shape->GetType(), kTuple)) {
    auto value_tuple = GetValue<std::vector<int64_t>>(x1_shape->GetValue());
    for (size_t i = 0; i < kDimensionTwo; ++i) {
      shape_ptr[i] = value_tuple[i];
    }
  }
}

abstract::ShapePtr SparseTensorDenseMatmulInferShape(const PrimitivePtr &primitive,
                                                     const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = primitive->name();
  auto indices_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->GetShape())[kShape];
  auto values_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->GetShape())[kShape];
  auto shape_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[2]->GetShape())[kShape];
  auto x2_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[3]->GetShape())[kShape];
  auto x1_shape = input_args[2];
  auto x1_shape_value = x1_shape->GetValue();
  std::string info;
  if (!checkContainer(input_args, &info)) {
    MS_EXCEPTION(ValueError) << "For " << prim_name << info;
  }
  if (CheckAndConvertUtils::IsTuple(x1_shape)) {
    if (IsValueKnown(x1_shape_value)) {
      int64_t shape_len = static_cast<int64_t>(GetValue<std::vector<int64_t>>(x1_shape_value).size());
      shape_shape = std::vector<int64_t>{shape_len};
    } else {
      return std::make_shared<abstract::Shape>(std::vector<int64_t>{-1, -1});
    }
  }
  std::vector<std::vector<int64_t>> all_shapes = {indices_shape, values_shape, shape_shape, x2_shape};
  bool is_dynamic = std::any_of(all_shapes.begin(), all_shapes.end(), IsDynamic);
  bool is_dynamic_rank = std::any_of(all_shapes.begin(), all_shapes.end(), IsDynamicRank);
  SparseTensorDenseMatmulCheckShape(prim_name, is_dynamic_rank, is_dynamic, indices_shape, values_shape, shape_shape,
                                    x2_shape);
  if (!is_dynamic) {
    if (x1_shape_value->isa<ValueAny>() || x1_shape_value->isa<None>()) {
      if (!CheckAndConvertUtils::IsTensor(x1_shape)) {
        MS_EXCEPTION(ValueError) << "For '" << primitive->name() << "', the input sparse_shape "
                                 << "should be constant.";
      }
    }
  }
  auto adjoint_a = primitive->GetAttr("adjoint_st");
  auto adjoint_b = primitive->GetAttr("adjoint_dt");
  bool adjoint_av = GetValue<bool>(adjoint_a);
  bool adjoint_bv = GetValue<bool>(adjoint_b);
  int64_t x1_row = -1;
  int64_t x1_col = -1;
  int64_t x2_row = -1;
  int64_t x2_col = -1;
  if (is_dynamic_rank) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>{x1_row, x2_col});
  }
  ShapeVector shape{-1, -1};
  SparseTensorDenseMatmulCheckShapeSetShape(prim_name, shape.data(), shape_shape, x1_shape);
  if (shape.size() == kDimensionTwo) {
    x1_row = shape[0];
    x1_col = shape[1];
  }
  if (x2_shape.size() == kDimensionTwo) {
    x2_row = x2_shape[0];
    x2_col = x2_shape[1];
  }
  if (adjoint_av) {
    std::swap(x1_row, x1_col);
  }
  if (adjoint_bv) {
    std::swap(x2_row, x2_col);
  }
  int64_t y_row = x1_row;
  int64_t y_col = x2_col;
  std::vector<int64_t> y_shape{y_row, y_col};
  return std::make_shared<abstract::Shape>(y_shape);
}

TypePtr SparseTensorDenseMatmulInferType(const PrimitivePtr &primitive,
                                         const std::vector<AbstractBasePtr> &input_args) {
  std::map<std::string, TypePtr> types;
  std::set<TypePtr> valid_types = {kFloat16, kFloat32, kFloat64, kInt32, kInt64, kComplex64, kComplex128};
  TypePtr indices_type = input_args[0]->GetType();
  TypePtr values_type = input_args[1]->GetType();
  TypePtr shape_type = input_args[2]->GetType();
  TypePtr x2_type = input_args[3]->GetType();
  auto x1_shape = input_args[2];
  (void)types.emplace("values", values_type);
  (void)types.emplace("x2", x2_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, valid_types, primitive->name());
  if (!checkType("indices", indices_type, {kInt64, kInt32}, primitive)) {
    MS_EXCEPTION(TypeError) << "For '" << primitive->name() << "', the input indices "
                            << "data type should be int32 or int64.";
  }
  if (!CheckAndConvertUtils::IsTuple(x1_shape) && !checkType("shape_type", shape_type, {kInt64, kInt32}, primitive)) {
    MS_EXCEPTION(TypeError) << "For '" << primitive->name() << "', the input shape "
                            << "data type should be int32 or int64.";
  }
  auto x2_tensor_type = x2_type->cast<TensorTypePtr>();
  auto x2_element = x2_tensor_type->element();
  MS_EXCEPTION_IF_NULL(x2_element);
  return x2_element;
}
MIND_API_OPERATOR_IMPL(SparseTensorDenseMatmul, BaseOperator);
void SparseTensorDenseMatmul::Init(const bool adjoint_st, const bool adjoint_dt) {
  this->set_adjoint_st(adjoint_st);
  this->set_adjoint_dt(adjoint_dt);
}

void SparseTensorDenseMatmul::set_adjoint_st(const bool adjoint_st) {
  (void)this->AddAttr("adjoint_st", api::MakeValue(adjoint_st));
}

bool SparseTensorDenseMatmul::get_adjoint_st() const {
  auto value_ptr = this->GetAttr("adjoint_st");
  return GetValue<bool>(value_ptr);
}
void SparseTensorDenseMatmul::set_adjoint_dt(const bool adjoint_dt) {
  (void)this->AddAttr("adjoint_dt", api::MakeValue(adjoint_dt));
}

bool SparseTensorDenseMatmul::get_adjoint_dt() const {
  auto value_ptr = this->GetAttr("adjoint_dt");
  return GetValue<bool>(value_ptr);
}
AbstractBasePtr SparseTensorDenseMatmulInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                             const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  const int64_t input_num = 4;
  (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual, input_num, prim_name);
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  // infer type
  auto type = SparseTensorDenseMatmulInferType(primitive, input_args);
  // infer shape
  auto shape = SparseTensorDenseMatmulInferShape(primitive, input_args);

  return std::make_shared<abstract::AbstractTensor>(type, shape);
}

// AG means auto generated
class MIND_API AGSparseTensorDenseMatmulInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseTensorDenseMatmulInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseTensorDenseMatmulInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseTensorDenseMatmulInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {2}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(SparseTensorDenseMatmul, prim::kPrimSparseTensorDenseMatmul,
                                 AGSparseTensorDenseMatmulInfer, false);
}  // namespace ops
}  // namespace mindspore
