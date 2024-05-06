/**
 * Copyright 2020-2021 Huawei Technologies Co., Ltd
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

#include "ops/adam.h"

#include <functional>
#include <map>
#include <numeric>
#include <set>
#include <string>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/container.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/nn_optimizer_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
MIND_API_OPERATOR_IMPL(Adam, BaseOperator);
class AdamInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    const int64_t input_num = 10;
    (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kGreaterEqual, input_num,
                                             primitive->name());
    auto prim_name = primitive->name();
    auto var_shape_ptr = input_args[kInputIndex0]->GetShape();
    auto m_shape_ptr = input_args[kInputIndex1]->GetShape();
    auto v_shape_ptr = input_args[kInputIndex2]->GetShape();
    auto grad_shape_ptr = input_args[kInputIndex9]->GetShape();
    auto var_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
    auto m_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
    auto v_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->GetShape())[kShape];
    auto beta1_power_shape =
      CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex3]->GetShape())[kShape];
    auto beta2_power_shape =
      CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex4]->GetShape())[kShape];
    auto lr_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex5]->GetShape())[kShape];
    auto grad_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex9]->GetShape())[kShape];
    if (IsDynamicRank(var_shape) || IsDynamicRank(m_shape) || IsDynamicRank(v_shape)) {
      auto unknow_shape_ptr = std::make_shared<abstract::Shape>(std::vector<int64_t>{abstract::Shape::kShapeRankAny});
      return std::make_shared<abstract::TupleShape>(
        std::vector<abstract::BaseShapePtr>{unknow_shape_ptr, unknow_shape_ptr, unknow_shape_ptr});
    }
    MS_EXCEPTION_IF_NULL(var_shape_ptr);
    MS_EXCEPTION_IF_NULL(m_shape_ptr);
    MS_EXCEPTION_IF_NULL(v_shape_ptr);
    MS_EXCEPTION_IF_NULL(grad_shape_ptr);
    if (var_shape_ptr->IsDynamic() || m_shape_ptr->IsDynamic() || v_shape_ptr->IsDynamic() ||
        grad_shape_ptr->IsDynamic()) {
      return std::make_shared<abstract::TupleShape>(
        std::vector<abstract::BaseShapePtr>{var_shape_ptr, m_shape_ptr, v_shape_ptr});
    }
    CheckAndConvertUtils::Check("var_shape", var_shape, kEqual, m_shape, prim_name);
    CheckAndConvertUtils::Check("var_shape", var_shape, kEqual, v_shape, prim_name);
    CheckAndConvertUtils::Check("var_shape", var_shape, kEqual, grad_shape, prim_name);

    int64_t batch_rank = 0;
    if (primitive->HasAttr(kBatchRank)) {
      auto value_ptr = primitive->GetAttr(kBatchRank);
      batch_rank = GetValue<int64_t>(value_ptr);
    }
    if (batch_rank != 0) {
      (void)CheckAndConvertUtils::CheckInteger("beta1_power_shape size", SizeToLong(beta1_power_shape.size()), kEqual,
                                               batch_rank, prim_name);
      (void)CheckAndConvertUtils::CheckInteger("beta2_power_shape size", SizeToLong(beta2_power_shape.size()), kEqual,
                                               batch_rank, prim_name);
      (void)CheckAndConvertUtils::CheckInteger("lr_shape size", SizeToLong(lr_shape.size()), kEqual, batch_rank,
                                               prim_name);
    } else {
      if (beta1_power_shape.size() == 1 || beta1_power_shape.size() == 0) {
        (void)CheckAndConvertUtils::CheckInteger(
          "beta1_power_shape element num",
          std::accumulate(beta1_power_shape.begin(), beta1_power_shape.end(), 1, std::multiplies<int>()), kEqual, 1,
          prim_name);
      } else {
        MS_EXCEPTION(ValueError) << "The rank of beta1_power must be 0 or 1 but got " << lr_shape.size();
      }
      if (beta2_power_shape.size() == 1 || beta2_power_shape.size() == 0) {
        (void)CheckAndConvertUtils::CheckInteger(
          "beta2_power_shape element num",
          std::accumulate(beta2_power_shape.begin(), beta2_power_shape.end(), 1, std::multiplies<int>()), kEqual, 1,
          prim_name);
      } else {
        MS_EXCEPTION(ValueError) << "The rank of beta2_power must be 0 or 1 but got " << lr_shape.size();
      }
      if (lr_shape.size() == 1 || lr_shape.size() == 0) {
        (void)CheckAndConvertUtils::CheckInteger(
          "lr_shape element num", std::accumulate(lr_shape.begin(), lr_shape.end(), 1, std::multiplies<int>()), kEqual,
          1, prim_name);
      } else {
        MS_EXCEPTION(ValueError) << "The rank of lr must be 0 or 1 but got " << lr_shape.size();
      }
    }

    return std::make_shared<abstract::TupleShape>(
      std::vector<abstract::BaseShapePtr>{var_shape_ptr, m_shape_ptr, v_shape_ptr});
  }
  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    const int64_t input_num = 10;
    (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kGreaterEqual, input_num,
                                             primitive->name());
    auto prim_name = primitive->name();
    auto var_type = input_args[kInputIndex0]->GetType();
    auto m_type = input_args[kInputIndex1]->GetType();
    auto v_type = input_args[kInputIndex2]->GetType();
    auto grad_type = input_args[kInputIndex9]->GetType();
    std::map<std::string, TypePtr> type_dict1;
    (void)type_dict1.emplace("var", var_type);
    (void)type_dict1.emplace("grad", grad_type);
    std::set<TypePtr> num_type = {kFloat16, kFloat32};
    (void)CheckAndConvertUtils::CheckScalarOrTensorTypesSame(type_dict1, num_type, prim_name, true);
    return std::make_shared<Tuple>(std::vector<TypePtr>{var_type, m_type, v_type});
  }
};
void Adam::Init(const bool use_locking, const bool use_nesterov) {
  this->set_use_locking(use_locking);
  this->set_use_nesterov(use_nesterov);
}

void Adam::set_use_locking(const bool use_locking) { (void)this->AddAttr(kUseLocking, api::MakeValue(use_locking)); }

void Adam::set_use_nesterov(const bool use_nesterov) {
  (void)this->AddAttr(kUseNesterov, api::MakeValue(use_nesterov));
}

bool Adam::get_use_locking() const {
  auto value_ptr = GetAttr(kUseLocking);
  return GetValue<bool>(value_ptr);
}

bool Adam::get_use_nesterov() const {
  auto value_ptr = GetAttr(kUseNesterov);
  return GetValue<bool>(value_ptr);
}

abstract::AbstractBasePtr ApplyAdamInferFunc(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                             const std::vector<abstract::AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  AdamInfer apply_adamd;
  auto type = apply_adamd.InferType(primitive, input_args);
  auto shape = apply_adamd.InferShape(primitive, input_args);
  return abstract::MakeAbstract(shape, type);
}

REGISTER_PRIMITIVE_OP_INFER_IMPL(Adam, prim::kPrimAdam, AdamInfer, false);
}  // namespace ops
}  // namespace mindspore
