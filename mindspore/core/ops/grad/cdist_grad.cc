/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#include "ops/grad/cdist_grad.h"

#include <map>
#include <memory>
#include <set>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/math_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
constexpr size_t kCdistGradInputDimsMin = 2;

abstract::ShapePtr CdistGradInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  size_t batch_rank = 0;
  if (primitive->HasAttr(kBatchRank)) {
    auto value_ptr = primitive->GetAttr(kBatchRank);
    batch_rank = GetValue<int64_t>(value_ptr);
  }
  auto grad_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->GetShape())[kShape];
  auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->GetShape())[kShape];
  auto y_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[2]->GetShape())[kShape];
  auto cdist_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[3]->GetShape())[kShape];
  auto x_size = x_shape.size();
  auto y_size = y_shape.size();
  if (IsDynamic(x_shape) || IsDynamic(y_shape)) {
    return std::make_shared<abstract::Shape>(x_shape);
  }
  CheckAndConvertUtils::Check("grad shape", grad_shape, kEqual, cdist_shape, prim_name, ValueError);
  if (x_size != y_size) {
    MS_EXCEPTION(ValueError) << "For 'CdistGrad', rank of input_x and input_y must be equal, but got input_x size: "
                             << x_size << ", input_y size: " << y_size << ".";
  }

  if (batch_rank == 0) {
    CheckAndConvertUtils::CheckInRange("input_x dim", x_size, kIncludeBoth, {2, 3}, "Cdist");
  }

  if (x_size < kCdistGradInputDimsMin) {
    MS_EXCEPTION(ValueError) << "For '" << primitive->name() << "', rank of input must be greater than "
                             << kCdistGradInputDimsMin << ", but got rank of input: " << x_size << ".";
  }

  if (x_size > kCdistGradInputDimsMin) {
    for (size_t i = 0; i < x_size - kCdistGradInputDimsMin; i++) {
      if (x_shape[i] != y_shape[i]) {
        MS_EXCEPTION(ValueError) << "For '" << primitive->name()
                                 << "', the batch shape of 'x' must be the same as the shape of 'y', "
                                    "but got 'x_shape["
                                 << i << "]': " << x_shape[i] << " and 'y_shape[" << i << "]': " << y_shape[i];
      }
    }
  }
  if (x_shape[x_size - kInputIndex1] != y_shape[y_size - kInputIndex1]) {
    MS_EXCEPTION(ValueError) << "For '" << primitive->name()
                             << "', the number of columns of 'x' must be the same as the number of 'y', "
                                "but got 'x_shape["
                             << x_size - kInputIndex1 << "]': " << x_shape[x_size - kInputIndex1] << " and 'y_shape["
                             << y_size - kInputIndex1 << "]': " << y_shape[y_size - kInputIndex1];
  }

  auto out_shape = x_shape;
  return std::make_shared<abstract::Shape>(out_shape);
}

TypePtr CdistGradInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  const std::set<TypePtr> valid_types = {kFloat64, kFloat32, kFloat16};
  std::map<std::string, TypePtr> types;
  (void)types.emplace("grad", input_args[0]->GetType());
  (void)types.emplace("input_x", input_args[1]->GetType());
  (void)types.emplace("input_y", input_args[2]->GetType());
  (void)types.emplace("cdist", input_args[3]->GetType());
  return CheckAndConvertUtils::CheckTensorTypeSame(types, valid_types, primitive->name());
}
}  // namespace

float CdistGrad::get_p() const {
  auto value_ptr = this->GetAttr(kP);
  return GetValue<float>(value_ptr);
}
void CdistGrad::set_p(const float p) { (void)this->AddAttr(kP, api::MakeValue(p)); }

MIND_API_OPERATOR_IMPL(CdistGrad, BaseOperator);
AbstractBasePtr CdistGradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                               const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t input_num = 4;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, primitive->name());
  auto infer_type = CdistGradInferType(primitive, input_args);
  auto infer_shape = CdistGradInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

// AG means auto generated
class MIND_API AGCdistGradInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return CdistGradInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return CdistGradInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return CdistGradInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(CdistGrad, prim::kPrimCdistGrad, AGCdistGradInfer, false);
}  // namespace ops
}  // namespace mindspore
