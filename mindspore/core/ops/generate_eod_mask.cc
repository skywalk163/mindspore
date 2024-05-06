/**
 * Copyright 2024 Huawei Technologies Co., Ltd
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

#include "ops/generate_eod_mask.h"
#include <string>
#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"

namespace mindspore {
namespace ops {
void GenerateEodMask::set_eod_token_id(const int64_t eod_token_id) {
  (void)this->AddAttr(kEodTokenId, api::MakeValue(eod_token_id));
}

void GenerateEodMask::set_n_pos(const int64_t n_pos) { (void)this->AddAttr(kNPos, api::MakeValue(n_pos)); }
void GenerateEodMask::set_n_step(const std::vector<int64_t> n_step) {
  (void)this->AddAttr(kNStep, api::MakeValue(n_step));
}
void GenerateEodMask::set_n_error_mode(const std::string n_error_mode) {
  (void)this->AddAttr(kNErrorMode, api::MakeValue(n_error_mode));
}

/// \brief Get EodTokenId.
///
/// \return EodTokenId.
int64_t GenerateEodMask::get_eod_token_id() const { return GetValue<int64_t>(GetAttr(kEodTokenId)); }

int64_t GenerateEodMask::get_n_pos() const { return GetValue<int64_t>(GetAttr(kNPos)); }
std::vector<int64_t> GenerateEodMask::get_n_step() const { return GetValue<std::vector<int64_t>>(GetAttr(kNStep)); }
std::string GenerateEodMask::get_n_error_mode() const { return GetValue<std::string>(GetAttr(kNErrorMode)); }

MIND_API_OPERATOR_IMPL(GenerateEodMask, BaseOperator);

// AG means auto generated
class MIND_API AGGenerateEodMaskInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    for (const auto &item : input_args) {
      MS_EXCEPTION_IF_NULL(item);
    }
    const int64_t no_repeat_kShapeSize = 2;
    auto inputs_ids_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->GetShape());
    auto inputs_ids_shape = inputs_ids_shape_map[kShape];

    if (IsDynamicRank(inputs_ids_shape)) {
      auto any_shape = std::make_shared<abstract::Shape>(std::vector<int64_t>{abstract::Shape::kShapeRankAny});
      std::vector<BaseShapePtr> shapes_list = {any_shape, any_shape};
      return std::make_shared<abstract::TupleShape>(shapes_list);
    }

    (void)CheckAndConvertUtils::CheckInteger("rank of inputs_ids", SizeToLong(inputs_ids_shape.size()), kEqual,
                                             no_repeat_kShapeSize, prim_name);

    std::vector<BaseShapePtr> shapes_list = {};
    (void)shapes_list.emplace_back(std::make_shared<abstract::Shape>(inputs_ids_shape));
    return input_args[0]->GetShape();
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    for (const auto &item : input_args) {
      MS_EXCEPTION_IF_NULL(item);
    }
    std::map<std::string, TypePtr> input_types;
    auto input_ids_type = input_args[0]->BuildType();
    (void)input_types.emplace("inputs_ids", input_ids_type);
    std::set<TypePtr> valid_input_types = {kInt16,  kInt32,    kInt64,   kUInt16, kUInt32,
                                           kUInt64, kBFloat16, kFloat16, kFloat32};
    (void)CheckAndConvertUtils::CheckTensorTypeSame(input_types, valid_input_types, primitive->name());
    return input_ids_type;
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    const int64_t kInputsNum = 1;
    CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputsNum, primitive->name());
    const auto &type = InferType(primitive, input_args);
    const auto &shape = InferShape(primitive, input_args);
    return abstract::MakeAbstract(shape, type);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(GenerateEodMask, prim::kPrimGenerateEodMask, AGGenerateEodMaskInfer, false);
}  // namespace ops
}  // namespace mindspore
