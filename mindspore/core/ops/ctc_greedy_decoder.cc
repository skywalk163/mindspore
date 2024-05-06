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
#include "ops/ctc_greedy_decoder.h"

#include <map>
#include <memory>
#include <string>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/param_validator.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/nn_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/ms_context.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
const int64_t kInputsRank = 3;
const int64_t kSeqLenRank = 1;
std::vector<abstract::BaseShapePtr> GetOutputsShape(const ShapeVector &x_shape, const int64_t max_shape_value) {
  ShapeVector decoded_indices_shape = {max_shape_value, 2};
  ShapeVector decoded_values_shape = {max_shape_value};
  ShapeVector decoded_shape_shape = {2};
  ShapeVector log_probability_shape =
    IsDynamicRank(x_shape) ? ShapeVector{abstract::TensorShape::kShapeDimAny, 1} : ShapeVector{x_shape[1], 1};

  auto decoded_indices_shape_ptr = std::make_shared<abstract::TensorShape>(std::move(decoded_indices_shape));
  auto decoded_values_shape_ptr = std::make_shared<abstract::TensorShape>(std::move(decoded_values_shape));
  auto decoded_shape_shape_ptr = std::make_shared<abstract::TensorShape>(std::move(decoded_shape_shape));
  auto log_probability_shape_ptr = std::make_shared<abstract::TensorShape>(std::move(log_probability_shape));

  std::vector<abstract::BaseShapePtr> out_shapes{decoded_indices_shape_ptr, decoded_values_shape_ptr,
                                                 decoded_shape_shape_ptr, log_probability_shape_ptr};

  return out_shapes;
}

void CheckType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = primitive->name();
  const int64_t kInputNum = 2;
  CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, kInputNum, prim_name);
  auto inputs_x_dtype = input_args[kInputIndex0]->GetType();
  auto sequence_length_dtype = input_args[kInputIndex1]->GetType();
  (void)CheckAndConvertUtils::CheckTensorTypeValid("inputs type", inputs_x_dtype, {kFloat32, kFloat64}, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("sequence length dtype", sequence_length_dtype, {kInt32}, prim_name);
}
}  // namespace

void CTCGreedyDecoder::Init(const bool merge_repeated) { this->set_merge_repeated(merge_repeated); }

void CTCGreedyDecoder::set_merge_repeated(const bool merge_repeated) {
  (void)this->AddAttr(kMergeRepeated, api::MakeValue(merge_repeated));
}

bool CTCGreedyDecoder::get_merge_repeated() const {
  auto value_ptr = this->GetAttr(kMergeRepeated);
  return GetValue<bool>(value_ptr);
}

MIND_API_OPERATOR_IMPL(CTCGreedyDecoder, BaseOperator);
class CTCGreedyDecoderInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    auto x_shape = input_args[kInputIndex0]->GetShape()->GetShapeVector();
    if (x_shape.size() != kInputsRank) {
      MS_LOG(EXCEPTION) << "Rank of " << primitive->name() << "'s input must be " << kInputsRank;
    }
    auto max_shape_value = x_shape[kInputIndex0] * x_shape[kInputIndex1];
    auto out_shapes = GetOutputsShape(x_shape, max_shape_value);
    return std::make_shared<abstract::TupleShape>(std::move(out_shapes));
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    // Infer Shape for frontend
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    // Check type
    CheckType(primitive, input_args);
    auto inputs_x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
    auto sequence_length_shape =
      CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
    auto context = MsContext::GetInstance();
    MS_EXCEPTION_IF_NULL(context);
    auto value_ptr = primitive->GetAttr(kMergeRepeated);
    MS_EXCEPTION_IF_NULL(value_ptr);
    bool merge_repeated = GetValue<bool>(value_ptr);
    if (!merge_repeated && context->get_param<std::string>(MS_CTX_DEVICE_TARGET) == kAscendDevice) {
      MS_EXCEPTION(ValueError) << "For '" << prim_name
                               << ", 'merge_repeated' can't be set to false on ascend platform.";
    }
    if (!IsDynamicRank(inputs_x_shape) && inputs_x_shape.size() != kInputsRank) {
      MS_EXCEPTION(ValueError) << "For '" << prim_name
                               << "', inputs's dim must be 3, but got: " << inputs_x_shape.size() << ".";
    }
    if (!IsDynamicRank(sequence_length_shape) && sequence_length_shape.size() != kSeqLenRank) {
      MS_EXCEPTION(ValueError) << "For '" << prim_name
                               << "', sequence_length's dims must be 1, but got: " << sequence_length_shape.size()
                               << ".";
    }
    if (!(IsDynamic(inputs_x_shape) || IsDynamic(sequence_length_shape)) &&
        inputs_x_shape[1] != sequence_length_shape[0]) {
      MS_EXCEPTION(ValueError) << "For '" << prim_name
                               << "', inputs batch_size must be the same with sequence_length batch_size, "
                               << "but now inputs batch_size: " << inputs_x_shape[1]
                               << " and sequence_length batch_size: " << sequence_length_shape[0] << ".";
    }
    // Get out shapes
    int64_t max_shape_value = abstract::TensorShape::kShapeDimAny;
    auto out_shapes = GetOutputsShape(inputs_x_shape, max_shape_value);
    // Get log_probability_type
    auto x_ptr = abstract::CheckArg<abstract::AbstractTensor>(prim_name, input_args, 0);
    auto log_probability_type = x_ptr->element()->GetType();
    // Get outputs
    auto decoded_indices = std::make_shared<abstract::AbstractTensor>(kInt64, out_shapes[kInputIndex0]);
    auto decoded_values = std::make_shared<abstract::AbstractTensor>(kInt64, out_shapes[kInputIndex1]);
    auto decoded_shape = std::make_shared<abstract::AbstractTensor>(kInt64, out_shapes[kInputIndex2]);
    auto log_probability = std::make_shared<abstract::AbstractTensor>(log_probability_type, out_shapes[kInputIndex3]);
    AbstractBasePtrList elements{decoded_indices, decoded_values, decoded_shape, log_probability};

    return std::make_shared<abstract::AbstractTuple>(std::move(elements));
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    CheckType(primitive, input_args);
    auto inputs_x_ptr = abstract::CheckArg<abstract::AbstractTensor>(prim_name, input_args, 0);
    return std::make_shared<Tuple>(std::vector<TypePtr>{kInt64, kInt64, kInt64, inputs_x_ptr->element()->GetType()});
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(CTCGreedyDecoder, prim::kPrimCTCGreedyDecoder, CTCGreedyDecoderInfer, false);
}  // namespace ops
}  // namespace mindspore
