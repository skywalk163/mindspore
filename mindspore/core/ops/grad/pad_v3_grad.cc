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
#include "ops/grad/pad_v3_grad.h"
#include <set>
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/math_ops.h"
#include "mindspore/core/ops/nn_ops.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "utils/ms_context.h"

namespace mindspore {
namespace ops {
namespace {
void PaddingsValueCheck(const PrimitivePtr &primitive, const ShapeVector &x_shape,
                        const std::vector<int64_t> &paddings_val, const std::string &prim_name) {
  const int64_t max_x_dim = 5;
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  if (context->get_param<std::string>(MS_CTX_DEVICE_TARGET) == kAscendDevice) {
    (void)CheckAndConvertUtils::CheckInteger("x_dim", SizeToLong(x_shape.size()), kLessThan, max_x_dim, prim_name);
    // For Ascend, ge::PadV3Grad only support paddings has positive value, and this node is called when mode
    // is not 'constant'
    auto mode = GetValue<std::string>(primitive->GetAttr("mode"));
    if (mode != "constant") {
      (void)CheckAndConvertUtils::CheckPositiveVector("paddings", paddings_val, prim_name);
    }
  }
}

abstract::ShapePtr PadV3GradInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  constexpr size_t kTwo = 2;
  constexpr size_t kThree = 3;
  constexpr size_t kFour = 4;
  constexpr size_t kFive = 5;
  constexpr size_t kPaddingsSizeTwo = 2;
  constexpr size_t kPaddingsSizeFour = 4;
  constexpr size_t kPaddingsSizeSix = 6;
  constexpr size_t paddings_pos_2 = 2;
  constexpr size_t paddings_pos_3 = 3;
  constexpr size_t paddings_pos_4 = 4;
  constexpr size_t paddings_pos_5 = 5;
  auto x_shape_ptr = input_args[kInputIndex0]->GetShape();
  MS_EXCEPTION_IF_NULL(x_shape_ptr);
  // support dynamic rank
  if (x_shape_ptr->IsDimUnknown()) {
    return std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeRankAny}));
  }
  auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(x_shape_ptr)[kShape];
  if (x_shape_ptr->IsDynamic()) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>(x_shape.size(), abstract::Shape::kShapeDimAny));
  }

  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();

  std::vector<int64_t> paddings_arg;
  auto padding_type = input_args[kInputIndex1]->GetType();
  if (padding_type->isa<TensorType>()) {
    auto paddings_shape_ptr = input_args[kInputIndex1]->GetShape();
    MS_EXCEPTION_IF_NULL(paddings_shape_ptr);
    if (paddings_shape_ptr->IsDynamic()) {
      return std::make_shared<abstract::Shape>(std::vector<int64_t>(x_shape.size(), abstract::Shape::kShapeDimAny));
    }
    auto paddings_value = input_args[kInputIndex1]->GetValue();
    MS_EXCEPTION_IF_NULL(paddings_value);
    if (paddings_value->isa<ValueAny>()) {
      return std::make_shared<abstract::Shape>(std::vector<int64_t>(x_shape.size(), abstract::Shape::kShapeDimAny));
    }
    paddings_arg = CheckAndConvertUtils::CheckTensorIntValue("paddings value", paddings_value, prim_name, padding_type);
  } else if (padding_type->isa<Tuple>() || padding_type->isa<List>()) {
    auto value = input_args[1]->GetValue();
    if (IsValueKnown(value)) {
      paddings_arg = CheckAndConvertUtils::CheckIntOrTupleInt("paddings value", input_args[1], prim_name);
    } else {
      return std::make_shared<abstract::Shape>(std::vector<int64_t>(x_shape.size(), abstract::Shape::kShapeDimAny));
    }
  } else {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>(x_shape.size(), abstract::Shape::kShapeDimAny));
  }

  int64_t paddings_size = SizeToLong(paddings_arg.size());
  std::vector<int64_t> paddings_val;
  for (int64_t i = 0; i < paddings_size; ++i) {
    paddings_val.push_back(int64_t(paddings_arg[LongToSize(i)]));
  }
  PaddingsValueCheck(primitive, x_shape, paddings_val, prim_name);
  auto paddings_contiguous = GetValue<bool>(primitive->GetAttr("paddings_contiguous"));
  if (!paddings_contiguous) {
    std::vector<int64_t> tmp = paddings_val;
    for (int64_t i = 0; i < paddings_size; ++i) {
      if (i % SizeToLong(kTwo) == 0) {
        paddings_val[LongToSize(i)] = tmp[LongToSize(i) / kTwo];
      } else {
        paddings_val[LongToSize(i)] = tmp[LongToSize(i + paddings_size) / kTwo];
      }
    }
  }

  std::vector<int64_t> out_shape;
  if (paddings_size == SizeToLong(kPaddingsSizeTwo)) {
    (void)CheckAndConvertUtils::CheckInteger("input dims when padding's size equal 2", SizeToLong(kThree), kEqual,
                                             SizeToLong(x_shape.size()), prim_name);
    (void)out_shape.emplace_back(x_shape[0]);
    (void)out_shape.emplace_back(x_shape[1]);
    (void)out_shape.emplace_back(x_shape[kInputIndex2] - paddings_val[0] - paddings_val[1]);
  } else if (paddings_size == SizeToLong(kPaddingsSizeFour)) {
    (void)CheckAndConvertUtils::CheckInteger("input dims when padding's size equal 4", SizeToLong(kFour), kEqual,
                                             SizeToLong(x_shape.size()), prim_name);
    (void)out_shape.emplace_back(x_shape[0]);
    (void)out_shape.emplace_back(x_shape[1]);
    (void)out_shape.emplace_back(x_shape[kInputIndex2] - paddings_val[paddings_pos_2] - paddings_val[paddings_pos_3]);
    (void)out_shape.emplace_back(x_shape[kInputIndex3] - paddings_val[0] - paddings_val[1]);
  } else if (paddings_size == SizeToLong(kPaddingsSizeSix)) {
    (void)CheckAndConvertUtils::CheckInteger("input dims when padding's size equal 6", SizeToLong(kFive), kEqual,
                                             SizeToLong(x_shape.size()), prim_name);
    (void)out_shape.emplace_back(x_shape[0]);
    (void)out_shape.emplace_back(x_shape[1]);
    (void)out_shape.emplace_back(x_shape[kInputIndex2] - paddings_val[paddings_pos_4] - paddings_val[paddings_pos_5]);
    (void)out_shape.emplace_back(x_shape[kInputIndex3] - paddings_val[paddings_pos_2] - paddings_val[paddings_pos_3]);
    (void)out_shape.emplace_back(x_shape[kInputIndex4] - paddings_val[0] - paddings_val[1]);
  } else {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "', the length of paddings must be 2, 4 or 6, but got "
                             << paddings_size;
  }
  (void)CheckAndConvertUtils::CheckPositiveVector("out_shape", out_shape, prim_name);
  return std::make_shared<abstract::Shape>(out_shape);
}

TypePtr PadV3GradInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  std::map<std::string, TypePtr> args = {{"x", input_args[0]->GetType()}};
  auto mode = GetValue<string>(prim->GetAttr("mode"));
  if (mode == kConstant) {
    return CheckAndConvertUtils::CheckTensorTypeSame(args,
                                                     {kInt8, kInt16, kInt32, kInt64, kUInt8, kUInt16, kUInt32, kUInt64,
                                                      kFloat16, kFloat32, kFloat64, kComplex64, kComplex128, kBool},
                                                     prim->name());
  } else {
    return CheckAndConvertUtils::CheckTensorTypeSame(args,
                                                     {kInt8, kInt16, kInt32, kInt64, kUInt8, kUInt16, kUInt32, kUInt64,
                                                      kFloat16, kFloat32, kFloat64, kComplex64, kComplex128},
                                                     prim->name());
  }
}
}  // namespace

AbstractBasePtr PadV3GradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                               const std::vector<AbstractBasePtr> &input_args) {
  const int64_t kInputNum = 2;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputNum, primitive->name());
  auto infer_type = PadV3GradInferType(primitive, input_args);
  auto infer_shape = PadV3GradInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

bool PadV3Grad::get_paddings_contiguous() const { return GetValue<bool>(GetAttr("paddings_contiguous")); }
std::string PadV3Grad::get_mode() const { return GetValue<string>(GetAttr("mode")); }

MIND_API_OPERATOR_NAME_IMPL(PadV3Grad, kNamePadV3Grad, BaseOperator);

// AG means auto generated
class MIND_API AGPadV3GradInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return PadV3GradInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return PadV3GradInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return PadV3GradInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {kInputIndex1}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(PadV3Grad, prim::kPrimPadV3Grad, AGPadV3GradInfer, false);
}  // namespace ops
}  // namespace mindspore
