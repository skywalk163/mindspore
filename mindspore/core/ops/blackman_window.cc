/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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
#include "ops/blackman_window.h"

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
#include "ir/dtype/tensor_type.h"
#include "ir/dtype/type.h"
#include "ir/named.h"
#include "ir/primitive.h"
#include "ir/tensor.h"
#include "ir/value.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/base/type_id.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/other_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"
#include "ops/op_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr BlackmanWindowInferShape(const PrimitivePtr &primitive,
                                            const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto max_length_ptr = primitive->GetAttr("max_length");
  MS_EXCEPTION_IF_NULL(max_length_ptr);
  int64_t max_length = GetValue<int64_t>(max_length_ptr);
  if (CheckAndConvertUtils::IsTensor(input_args[0]) && IsValueKnown(input_args[0]->GetValue())) {
    auto window_length_value_ptr = input_args[0];
    MS_EXCEPTION_IF_NULL(window_length_value_ptr);
    auto input_type = input_args[0]->GetType();
    MS_EXCEPTION_IF_NULL(input_type);
    auto input_type_id = input_type->cast<TensorTypePtr>();
    MS_EXCEPTION_IF_NULL(input_type_id);
    auto input_type_element = input_type_id->element();
    MS_EXCEPTION_IF_NULL(input_type_element);
    auto window_length_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->GetShape());
    auto window_length_shape = window_length_shape_map[kShape];
    if (IsDynamicRank(window_length_shape)) {
      return std::make_shared<abstract::Shape>(std::vector<int64_t>{-2});
    }

    if (window_length_shape.size() != 0) {
      if (window_length_shape[0] == 0) {
        MS_EXCEPTION(ValueError) << "For '" << primitive->name() << "', the input window_length can not be empty.";
      } else {
        MS_EXCEPTION(ValueError) << "For '" << primitive->name()
                                 << "', the dim of window_length must be 0, but got: " << window_length_shape.size()
                                 << ".";
      }
    }

    std::vector<int64_t> out_shape;
    int64_t window_length_value = 0;
    if (input_type_element->type_id() == kNumberTypeInt32) {
      auto value_opt = GetArrayValue<int32_t>(window_length_value_ptr);
      window_length_value = static_cast<int64_t>(value_opt.value()[0]);
    } else if (input_type_element->type_id() == kNumberTypeInt64) {
      auto value_opt = GetArrayValue<int64_t>(window_length_value_ptr);
      window_length_value = value_opt.value()[0];
    }

    if (window_length_value >= 0 && window_length_value <= max_length) {
      out_shape.push_back(window_length_value);
      return std::make_shared<abstract::Shape>(out_shape);
    } else {
      MS_EXCEPTION(ValueError) << "For '" << primitive->name() << "', the value range of window_length must be [0, "
                               << max_length << "], but got: " << window_length_value << ".";
    }
  } else {
    auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->GetShape())[kShape];
    if (IsDynamicRank(x_shape)) {
      return std::make_shared<abstract::Shape>(std::vector<int64_t>{-2});
    }
    auto x_size = x_shape.size();
    if (x_size != 0) {
      if (x_shape[0] == 0) {
        MS_EXCEPTION(ValueError) << "For '" << primitive->name() << "', the input window_length can not be empty.";
      } else {
        MS_EXCEPTION(ValueError) << "For '" << primitive->name()
                                 << "', the dim of window_length must be 0, but got: " << x_size << ".";
      }
    }
    std::vector<int64_t> out_shape = {abstract::Shape::kShapeDimAny};
    return std::make_shared<abstract::Shape>(out_shape);
  }
}

TypePtr BlackmanWindowInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto input_type = input_args[0]->GetType();
  MS_EXCEPTION_IF_NULL(input_type);
  const std::set<TypePtr> valid_types = {kInt32, kInt64};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("window_length", input_type, valid_types, prim->name());
  auto dtype_attr = prim->GetAttr("dtype");
  MS_EXCEPTION_IF_NULL(dtype_attr);
  auto infer_type = dtype_attr->cast<TypePtr>();
  MS_EXCEPTION_IF_NULL(infer_type);
  return infer_type;
}
}  // namespace

void BlackmanWindow::Init(const bool periodic) { set_periodic(periodic); }

void BlackmanWindow::set_periodic(const bool periodic) { (void)this->AddAttr(kPeriodic, api::MakeValue(periodic)); }

bool BlackmanWindow::get_periodic() const { return GetValue<bool>(GetAttr(kPeriodic)); }

AbstractBasePtr BlackmanWindowInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t input_num = 1;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, primitive->name());
  auto infer_type = BlackmanWindowInferType(primitive, input_args);
  auto infer_shape = BlackmanWindowInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}
MIND_API_OPERATOR_IMPL(BlackmanWindow, BaseOperator);

// AG means auto generated
class MIND_API AGBlackmanWindowInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return BlackmanWindowInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return BlackmanWindowInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return BlackmanWindowInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {0}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(BlackmanWindow, prim::kPrimBlackmanWindow, AGBlackmanWindowInfer, false);
}  // namespace ops
}  // namespace mindspore
