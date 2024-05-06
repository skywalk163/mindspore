/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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

#include "ops/max_pool3d.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <string>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "ir/value.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/conv_pool_ops.h"
#include "ops/base_operator.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
constexpr size_t kMaxPool3DPadDims = 6;

MIND_API_OPERATOR_IMPL(MaxPool3D, BaseOperator);

void MaxPool3D::Init(const std::vector<int64_t> &kernel_size, const std::vector<int64_t> &stride,
                     const PadMode &pad_mode, const Format &format, const std::vector<int64_t> &pad) {
  this->set_pad_mode(pad_mode);
  this->set_kernel_size(kernel_size);
  this->set_strides(stride);
  this->set_format(format);
  this->set_pad(pad);
}

void MaxPool3D::set_pad_mode(const PadMode &pad_mode) {
  int64_t swi = pad_mode;
  (void)this->AddAttr(kPadMode, api::MakeValue(swi));
}

PadMode MaxPool3D::get_pad_mode() const { return PadMode(GetValue<int64_t>(GetAttr(kPadMode))); }
void MaxPool3D::set_kernel_size(const std::vector<int64_t> &kernel_size) {
  (void)this->AddAttr(
    kKernelSize, api::MakeValue(CheckAndConvertUtils::CheckPositiveVector(kKernelSize, kernel_size, this->name())));
}

std::vector<int64_t> MaxPool3D::get_kernel_size() const { return GetValue<std::vector<int64_t>>(GetAttr(kKernelSize)); }
void MaxPool3D::set_strides(const std::vector<int64_t> &strides) {
  (void)this->AddAttr(kStrides,
                      api::MakeValue(CheckAndConvertUtils::CheckPositiveVector(kStrides, strides, this->name())));
}

std::vector<int64_t> MaxPool3D::get_strides() const { return GetValue<std::vector<int64_t>>(GetAttr(kStrides)); }

void MaxPool3D::set_format(const Format &format) {
  int64_t f = format;
  (void)this->AddAttr(kFormat, api::MakeValue(f));
}

Format MaxPool3D::get_format() const { return Format(GetValue<int64_t>(GetAttr(kFormat))); }

void MaxPool3D::set_pad(const std::vector<int64_t> &pad) { (void)this->AddAttr(kPad, api::MakeValue(pad)); }

std::vector<int64_t> MaxPool3D::get_pad() const {
  auto value_ptr = GetAttr(kPad);
  return GetValue<std::vector<int64_t>>(value_ptr);
}

namespace {
int64_t MaxPool3DCeilDiv(int64_t a, int64_t b) {
  if (b == 0) {
    return 0;
  }
  int64_t result = a / b;
  if (a % b != 0) {
    result += 1;
  }
  return result;
}
void GetAttrs(const PrimitivePtr &primitive, std::vector<int64_t> *kernel_size, std::vector<int64_t> *strides,
              int64_t *pad_mode, std::vector<int64_t> *pad_list, bool *ceil_mode) {
  constexpr size_t kKernelDims = 5;
  constexpr size_t kStridesDims = 5;
  MS_EXCEPTION_IF_NULL(primitive);
  // attr kernel size
  *kernel_size = GetValue<std::vector<int64_t>>(primitive->GetAttr(kKernelSize));
  if (kernel_size->size() != kKernelDims) {
    MS_EXCEPTION(ValueError) << "For '" << primitive->name() << "', 'kernel_size' must be 5, but got "
                             << kernel_size->size() << ".";
  }
  // attr strides
  *strides = GetValue<std::vector<int64_t>>(primitive->GetAttr(kStrides));
  if (strides->size() != kStridesDims) {
    MS_EXCEPTION(ValueError) << "For '" << primitive->name() << "', 'strides' must be 5, but got " << strides->size()
                             << ".";
  }
  if (std::any_of(strides->begin(), strides->end(), [](int64_t stride) { return stride <= 0; })) {
    MS_EXCEPTION(ValueError) << "For '" << primitive->name()
                             << "', 'strides' must be all positive, but got 'strides': " << strides << ".";
  }
  // attr pad_list
  *pad_list = GetValue<std::vector<int64_t>>(primitive->GetAttr(kPadList));
  // attr pad_mode
  CheckAndConvertUtils::GetPadModEnumValue(primitive->GetAttr(kPadMode), pad_mode, true);
  // attr ceil mode
  *ceil_mode = GetValue<int64_t>(primitive->GetAttr(kCeilMode)) == 1;
}

int64_t FixCeil(int64_t input, int16_t output, int64_t stride, int64_t pad) {
  if (((output - 1) * stride) >= (input + pad)) {
    return output - 1;
  }
  return output;
}

std::vector<int64_t> GetOutputShape(const PrimitivePtr &primitive, const std::vector<int64_t> &in_shape,
                                    int64_t kernel_d, int64_t kernel_h, int64_t kernel_w, int64_t stride_d,
                                    int64_t stride_h, int64_t stride_w, const std::vector<int64_t> &pad_list,
                                    bool ceil_mode, int64_t pad_mode) {
  auto in_d = in_shape[kInputIndex2];
  auto in_h = in_shape[kInputIndex3];
  auto in_w = in_shape[kInputIndex4];
  int64_t out_d = 0;
  int64_t out_h = 0;
  int64_t out_w = 0;
  auto kernel_name = primitive->name();
  if (stride_d == 0 || stride_h == 0 || stride_w == 0) {
    MS_EXCEPTION(ValueError) << "For '" << kernel_name
                             << "', stride_d or stride_h or stride_w must be non-zero, but got stride_d: " << stride_d
                             << ", stride_h: " << stride_h << ", stride_w: " << stride_w << ".";
  }

  if (pad_mode == PadMode::VALID) {
    out_d = in_d == -1 ? -1 : MaxPool3DCeilDiv((in_d - (kernel_d - 1)), stride_d);
    out_h = in_h == -1 ? -1 : MaxPool3DCeilDiv((in_h - (kernel_h - 1)), stride_h);
    out_w = in_w == -1 ? -1 : MaxPool3DCeilDiv((in_w - (kernel_w - 1)), stride_w);
  } else if (pad_mode == PadMode::SAME) {
    out_d = in_d == -1 ? -1 : MaxPool3DCeilDiv(in_d, stride_d);
    out_h = in_h == -1 ? -1 : MaxPool3DCeilDiv(in_h, stride_h);
    out_w = in_w == -1 ? -1 : MaxPool3DCeilDiv(in_w, stride_w);
  } else {
    auto pad_d = pad_list[kInputIndex0] + pad_list[kInputIndex1];
    if (pad_d > kernel_d) {
      MS_EXCEPTION(ValueError) << "For '" << kernel_name
                               << "', the summation of padding on head and tail must be smaller than, or equal to the "
                                  "kernel size on depth, but got padding: ["
                               << pad_list[kInputIndex0] << ", " << pad_list[kInputIndex1]
                               << "], kernel_d: " << kernel_d << ".";
    }

    auto pad_h = pad_list[kInputIndex2] + pad_list[kInputIndex3];
    if (pad_h > kernel_h) {
      MS_EXCEPTION(ValueError) << "For '" << kernel_name
                               << "', the summation of padding on top and bottom must be smaller than, or equal to the "
                                  "kernel size on height, but got padding: ["
                               << pad_list[kInputIndex2] << ", " << pad_list[kInputIndex3]
                               << "], kernel_h: " << kernel_h << ".";
    }

    auto pad_w = pad_list[kInputIndex4] + pad_list[kInputIndex5];
    if (pad_w > kernel_w) {
      MS_EXCEPTION(ValueError) << "For '" << kernel_name
                               << "', the summation of padding on left and right must be smaller than, or equal to the "
                                  "kernel size on width, but got padding: ["
                               << pad_list[kInputIndex4] << ", " << pad_list[kInputIndex5]
                               << "], kernel_w: " << kernel_w << ".";
    }

    double out_d_tmp = in_d == -1 ? -1 : static_cast<double>(in_d + pad_d - kernel_d) / stride_d + 1;
    double out_h_tmp = in_h == -1 ? -1 : static_cast<double>(in_h + pad_h - kernel_h) / stride_h + 1;
    double out_w_tmp = in_w == -1 ? -1 : static_cast<double>(in_w + pad_w - kernel_w) / stride_w + 1;

    if (ceil_mode) {
      out_d = DoubleToLong(std::ceil(out_d_tmp));
      out_h = DoubleToLong(std::ceil(out_h_tmp));
      out_w = DoubleToLong(std::ceil(out_w_tmp));
      out_d = FixCeil(in_d, out_d, stride_d, pad_list[kInputIndex0]);
      out_h = FixCeil(in_h, out_h, stride_h, pad_list[kInputIndex2]);
      out_w = FixCeil(in_w, out_w, stride_w, pad_list[kInputIndex4]);
    } else {
      out_d = DoubleToLong(std::floor(out_d_tmp));
      out_h = DoubleToLong(std::floor(out_h_tmp));
      out_w = DoubleToLong(std::floor(out_w_tmp));
    }
  }

  std::vector<int64_t> output_shape = {in_shape[0], in_shape[1], out_d, out_h, out_w};
  return output_shape;
}

void GetPadsByPadding(const PrimitivePtr &primitive, int64_t in_d, int64_t in_h, int64_t in_w, int64_t kernel_d,
                      int64_t kernel_h, int64_t kernel_w, int64_t stride_d, int64_t stride_h, int64_t stride_w,
                      const int64_t &pad_mode, const std::vector<int64_t> &padding, std::vector<int64_t> *pad_list) {
  MS_EXCEPTION_IF_NULL(pad_list);
  if (pad_mode == PadMode::VALID) {
    (void)pad_list->insert(pad_list->begin(), kMaxPool3DPadDims, 0);
  } else if (pad_mode == PadMode::SAME) {
    if (stride_d == 0 || stride_h == 0 || stride_w == 0) {
      MS_EXCEPTION(ValueError) << "For '" << primitive->name()
                               << "', stride_d or stride_h or stride_w must be non-zero, but got stride_d: " << stride_d
                               << ", stride_h: " << stride_h << ", stride_w: " << stride_w << ".";
    }
    int64_t tail_d = in_d % stride_d;
    int64_t tail_h = in_h % stride_h;
    int64_t tail_w = in_w % stride_w;
    int64_t pad_d = std::max((tail_d > 0 ? kernel_d - tail_d : kernel_d - stride_d), (int64_t)0);
    int64_t pad_h = std::max((tail_h > 0 ? kernel_h - tail_h : kernel_h - stride_h), (int64_t)0);
    int64_t pad_w = std::max((tail_w > 0 ? kernel_w - tail_w : kernel_w - stride_w), (int64_t)0);
    constexpr int twice = 2;
    pad_list->push_back(static_cast<int64_t>(std::floor(pad_d / twice)));
    pad_list->push_back(pad_d - pad_list->at(0));
    pad_list->push_back(static_cast<int64_t>(std::floor(pad_h / twice)));
    pad_list->push_back(pad_h - pad_list->at(kInputIndex2));
    pad_list->push_back(static_cast<int64_t>(std::floor(pad_w / twice)));
    pad_list->push_back(pad_w - pad_list->at(kInputIndex4));
  } else if (pad_mode == PadMode::PAD) {
    pad_list->assign(padding.begin(), padding.end());
  }
}

abstract::ShapePtr MaxPool3DInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  constexpr int64_t k5DInputDims = 5;
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  (void)CheckAndConvertUtils::CheckInteger("input size", int64_t(input_args.size()), kEqual, 1, op_name);
  auto in_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->GetShapeTrack())[kShape];
  if (!IsDynamicRank(in_shape)) {
    (void)CheckAndConvertUtils::CheckInteger("x_rank", SizeToLong(in_shape.size()), kEqual, k5DInputDims, op_name);
  }
  if (IsDynamic(in_shape)) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>(k5DInputDims, abstract::Shape::kShapeDimAny));
  }
  std::vector<int64_t> kernel_size;
  std::vector<int64_t> strides;
  std::vector<int64_t> pad_list;
  int64_t pad_mode = 0;
  bool ceil_mode = false;
  GetAttrs(primitive, &kernel_size, &strides, &pad_mode, &pad_list, &ceil_mode);
  auto kernel_d = kernel_size[kInputIndex2];
  auto kernel_h = kernel_size[kInputIndex3];
  auto kernel_w = kernel_size[kInputIndex4];
  auto stride_d = strides[kInputIndex2];
  auto stride_h = strides[kInputIndex3];
  auto stride_w = strides[kInputIndex4];

  std::vector<int64_t> new_pad_list;
  GetPadsByPadding(primitive, in_shape[kInputIndex2], in_shape[kInputIndex3], in_shape[kInputIndex4], kernel_d,
                   kernel_h, kernel_w, stride_d, stride_h, stride_w, pad_mode, pad_list, &new_pad_list);
  primitive->set_attr(kPadList, MakeValue(new_pad_list));
  std::vector<int64_t> out_shape = GetOutputShape(primitive, in_shape, kernel_d, kernel_h, kernel_w, stride_d, stride_h,
                                                  stride_w, new_pad_list, ceil_mode, pad_mode);
  if (std::any_of(out_shape.begin(), out_shape.end(), [](int64_t shp_v) { return shp_v <= 0; })) {
    MS_EXCEPTION(ValueError) << "For '" << primitive->name()
                             << "', output shape's all elements must be positive, but got shape: " << out_shape << ".";
  }
  return std::make_shared<abstract::Shape>(out_shape);
}

TypePtr MaxPool3DInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  (void)CheckAndConvertUtils::CheckInteger("input size", int64_t(input_args.size()), kEqual, 1, op_name);
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  auto x_dtype = input_args[0]->GetType();
  const std::set<TypePtr> valid_types = {kFloat16, kFloat32, kFloat64};
  return CheckAndConvertUtils::CheckTensorTypeValid("x", x_dtype, valid_types, op_name);
}
}  // namespace

AbstractBasePtr MaxPool3DInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                               const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  (void)CheckAndConvertUtils::CheckInteger("input size", int64_t(input_args.size()), kEqual, 1, prim_name);
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  if (input_args[0]->GetType()->object_type() != kObjectTypeTensorType) {
    MS_EXCEPTION(TypeError) << "For '" << prim_name << "', the input data type must be tensor.";
  }
  return abstract::MakeAbstract(MaxPool3DInferShape(primitive, input_args), MaxPool3DInferType(primitive, input_args));
}

// AG means auto generated
class MIND_API AGMaxPool3DInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return MaxPool3DInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return MaxPool3DInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return MaxPool3DInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(MaxPool3D, prim::kPrimMaxPool3D, AGMaxPool3DInfer, false);
}  // namespace ops
}  // namespace mindspore
