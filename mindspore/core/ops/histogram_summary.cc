/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ops/histogram_summary.h"

#include <memory>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/structure_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
namespace {
constexpr int BASE_SIZE = 1;
abstract::ShapePtr HistogramSummaryInferShape(const PrimitivePtr &primitive,
                                              const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  // check
  MS_EXCEPTION_IF_NULL(input_args[1]);
  auto v_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->GetShape())[kShape];
  (void)CheckAndConvertUtils::CheckInteger("v rank", int64_t(v_shape.size()), kGreaterEqual, BASE_SIZE, prim_name);
  return std::make_shared<abstract::Shape>(ShapeVector(1));
}
}  // namespace

MIND_API_OPERATOR_IMPL(HistogramSummary, BaseOperator);
void HistogramSummary::set_side_effect_io() { (void)this->AddAttr(kSideEffectIO, api::MakeValue(true)); }

bool HistogramSummary::get_side_effect_io() const {
  auto value_ptr = GetAttr(kSideEffectIO);
  return GetValue<bool>(value_ptr);
}

void HistogramSummary::Init() { this->set_side_effect_io(); }

class MIND_API HistogramSummaryInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    primitive->AddAttr("dyn_input_sizes", MakeValue(std::vector<int64_t>{-1, 1}));
    return HistogramSummaryInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    // check
    CheckAndConvertUtils::CheckSummaryParam(input_args[0], input_args[1], primitive->name());
    return kInt32;
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(HistogramSummary, prim::kPrimHistogramSummary, HistogramSummaryInfer, false);
}  // namespace ops
}  // namespace mindspore
