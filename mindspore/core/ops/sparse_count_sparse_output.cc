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
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/param_validator.h"
#include "base/base.h"
#include "include/common/utils/utils.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sparse_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "ops/sparse_count_sparse_output.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
using mindspore::abstract::AbstractTensor;
using mindspore::abstract::AbstractTuple;
namespace {
abstract::TupleShapePtr SparseCountSparseOutputInferShape(const PrimitivePtr &,
                                                          const std::vector<AbstractBasePtr> &input_args) {
  auto indices_shape = input_args[kInputIndex0]->GetShape()->GetShapeVector();
  auto values_shape = input_args[kInputIndex1]->GetShape()->GetShapeVector();
  auto dense_shape_shape = input_args[kInputIndex2]->GetShape()->GetShapeVector();
  const int maxIndexRank = 2;
  if (indices_shape.size() != maxIndexRank) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput, indices must be a 2-D tensor"
                             << ", but got " << indices_shape.size() << ".";
  }
  if (values_shape.size() != 1) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput, values must be a 1-D tensor"
                             << ", but got " << values_shape.size() << ".";
  }
  if (dense_shape_shape.size() != 1) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput"
                             << ", dense_shape must be a 1-D tensor, while dense_shape dim num is "
                             << dense_shape_shape.size() << ".";
  }
  if (indices_shape[0] != values_shape[0]) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput"
                             << ", number of values must be same as dim0 of indices"
                             << " but indices dim0 size is " << indices_shape[0] << ", values_shape dim0 size is "
                             << values_shape[0] << ".";
  }

  if (dense_shape_shape[0] != indices_shape[1]) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput"
                             << ", dense_shape dimensions must be equal to second dimension of indices "
                             << " dense_shape dimensions is " << dense_shape_shape[0]
                             << ", second dimension of indices is " << indices_shape[1] << ".";
  }

  if (dense_shape_shape[0] <= 0) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput, dense_shape needs at least 1 element "
                             << ", but got " << dense_shape_shape[0] << ".";
  }

  ShapeVector indices_max_shape = {indices_shape[0] * indices_shape[1], maxIndexRank};
  ShapeVector values_max_shape = {indices_shape[0] * indices_shape[1]};
  ShapeVector dense_shape_max_shape = {maxIndexRank};

  auto out_indices_shape = std::make_shared<mindspore::abstract::Shape>(indices_max_shape);
  auto out_values_shape = std::make_shared<mindspore::abstract::Shape>(values_max_shape);
  auto out_dense_shape_shape = std::make_shared<mindspore::abstract::Shape>(dense_shape_max_shape);
  return std::make_shared<abstract::TupleShape>(
    std::vector<abstract::BaseShapePtr>{out_indices_shape, out_values_shape, out_dense_shape_shape});
}

abstract::TupleShapePtr SparseCountSparseOutputFrontendInferShape(const PrimitivePtr &,
                                                                  const std::vector<AbstractBasePtr> &input_args) {
  auto indices_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  auto values_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  auto dense_shape_shape = input_args[kInputIndex2]->GetShape()->GetShapeVector();
  const int maxIndexRank = 2;
  if (IsDynamic(indices_shape) || IsDynamic(values_shape) || IsDynamic(dense_shape_shape)) {
    auto out_indices_shape = std::make_shared<mindspore::abstract::Shape>(
      ShapeVector({abstract::Shape::kShapeDimAny, abstract::Shape::kShapeDimAny}));
    auto out_values_shape = std::make_shared<mindspore::abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny}));
    auto out_dense_shape_shape =
      std::make_shared<mindspore::abstract::Shape>(ShapeVector({abstract::Shape::kShapeDimAny}));
    return std::make_shared<abstract::TupleShape>(
      std::vector<abstract::BaseShapePtr>{out_indices_shape, out_values_shape, out_dense_shape_shape});
  }
  if (indices_shape.size() != maxIndexRank) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput, indices must be a 2-D tensor"
                             << ", but got " << indices_shape.size() << ".";
  }
  if (values_shape.size() != 1) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput, values must be a 1-D tensor"
                             << ", but got " << values_shape.size() << ".";
  }
  if (dense_shape_shape.size() != 1) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput"
                             << ", dense_shape must be a 1-D tensor, while dense_shape dim num is "
                             << dense_shape_shape.size() << ".";
  }
  if (indices_shape[0] != values_shape[0]) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput"
                             << ", number of values must be same as dim0 of indices"
                             << " but indices dim0 size is " << indices_shape[0] << ", values_shape dim0 size is "
                             << values_shape[0] << ".";
  }

  if (dense_shape_shape[0] != indices_shape[1]) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput"
                             << ", dense_shape dimensions must be equal to second dimension of indices "
                             << " dense_shape dimensions is " << dense_shape_shape[0]
                             << ", second dimension of indices is " << indices_shape[1] << ".";
  }

  if (dense_shape_shape[0] <= 0) {
    MS_EXCEPTION(ValueError) << "For SparseCountSparseOutput, dense_shape needs at least 1 element "
                             << ", but got " << dense_shape_shape[0] << ".";
  }

  ShapeVector indices_shape_ = {-1, -1};
  ShapeVector values_shape_ = {-1};
  ShapeVector dense_shape_shape_ = {-1};
  auto out_indices_shape = std::make_shared<mindspore::abstract::Shape>(indices_shape_);
  auto out_values_shape = std::make_shared<mindspore::abstract::Shape>(values_shape_);
  auto out_dense_shape_shape = std::make_shared<mindspore::abstract::Shape>(dense_shape_shape_);
  return std::make_shared<abstract::TupleShape>(
    std::vector<abstract::BaseShapePtr>{out_indices_shape, out_values_shape, out_dense_shape_shape});
}

TuplePtr SparseCountSparseOutputInferType(const PrimitivePtr &primitive,
                                          const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const auto prim_name = primitive->name();
  for (auto item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  const int64_t kInputNum = 4;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputNum, prim_name);

  const std::set<TypePtr> indices_valid_types = {kInt64};
  const std::set<TypePtr> values_valid_types = {kInt32, kInt64};
  const std::set<TypePtr> dense_shape_valid_types = {kInt64};
  const std::set<TypePtr> weights_valid_types = {kInt32, kInt64, kFloat32, kFloat64};
  auto indices_type = input_args[kInputIndex0]->GetType();
  auto values_type = input_args[kInputIndex1]->GetType();
  auto dense_shape_type = input_args[kInputIndex2]->GetType();
  auto weights_type = input_args[kInputIndex3]->GetType();
  auto weights_ptr = CheckAndConvertUtils::CheckArgsType(prim_name, input_args, 3, kObjectTypeTensorType);
  auto weights_element_type_ = weights_ptr->GetType()->cast<TensorTypePtr>();
  MS_EXCEPTION_IF_NULL(weights_element_type_);
  auto weights_element_type = weights_element_type_->element();
  (void)CheckAndConvertUtils::CheckTensorTypeValid("indices", indices_type, indices_valid_types, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("values", values_type, values_valid_types, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("dense_shape", dense_shape_type, dense_shape_valid_types, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("weights", weights_type, weights_valid_types, prim_name);

  return std::make_shared<Tuple>(std::vector<TypePtr>{kInt64, weights_element_type, kInt64});
}

AbstractBasePtr SparseCountSparseOutputInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                             const std::vector<abstract::AbstractBasePtr> &input_args) {
  auto infer_type = SparseCountSparseOutputInferType(primitive, input_args);
  auto infer_shape = SparseCountSparseOutputFrontendInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}
}  // namespace

void SparseCountSparseOutput::Init(bool binary_output, int64_t minlength, int64_t maxlength) {
  set_binary_output(binary_output);
  set_minlength(minlength);
  set_maxlength(maxlength);
}

void SparseCountSparseOutput::set_binary_output(bool binary_output) {
  (void)AddAttr(kAttrBinaryOutput, api::MakeValue(binary_output));
}
bool SparseCountSparseOutput::get_binary_output() const { return GetValue<bool>(GetAttr(kAttrBinaryOutput)); }

void SparseCountSparseOutput::set_minlength(const int64_t &minlength) {
  (void)AddAttr(kAttrMinLength, api::MakeValue(minlength));
}

int64_t SparseCountSparseOutput::get_minlength() const { return GetValue<int64_t>(GetAttr(kAttrMinLength)); }

void SparseCountSparseOutput::set_maxlength(const int64_t &maxlength) {
  (void)AddAttr(kAttrMaxLength, api::MakeValue(maxlength));
}

int64_t SparseCountSparseOutput::get_maxlength() const { return GetValue<int64_t>(GetAttr(kAttrMaxLength)); }

MIND_API_OPERATOR_IMPL(SparseCountSparseOutput, BaseOperator);
class MIND_API AGSparseCountSparseOutputInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseCountSparseOutputInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseCountSparseOutputInferType(primitive, input_args);
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseCountSparseOutputInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(SparseCountSparseOutput, prim::kPrimSparseCountSparseOutput,
                                 AGSparseCountSparseOutputInfer, false);
}  // namespace ops
}  // namespace mindspore
