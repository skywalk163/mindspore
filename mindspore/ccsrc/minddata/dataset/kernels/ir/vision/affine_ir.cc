/**
 * Copyright 2021-2023 Huawei Technologies Co., Ltd
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
#include "minddata/dataset/kernels/ir/vision/affine_ir.h"

#include "minddata/dataset/kernels/image/affine_op.h"
#if !defined(BUILD_LITE) && defined(ENABLE_D)
#include "minddata/dataset/kernels/image/dvpp/ascend910b/dvpp_affine_op.h"
#endif
#include "minddata/dataset/kernels/ir/validators.h"
#include "minddata/dataset/util/validators.h"

namespace mindspore {
namespace dataset {
namespace vision {
AffineOperation::AffineOperation(float_t degrees, const std::vector<float> &translation, float scale,
                                 const std::vector<float> &shear, InterpolationMode interpolation,
                                 const std::vector<uint8_t> &fill_value, const std::string &device_target)
    : degrees_(degrees),
      translation_(translation),
      scale_(scale),
      shear_(shear),
      interpolation_(interpolation),
      fill_value_(fill_value),
      device_target_(device_target) {}

AffineOperation::~AffineOperation() = default;

std::string AffineOperation::Name() const { return kAffineOperation; }

Status AffineOperation::ValidateParams() {
  // Degrees
  if (degrees_ < -180 || degrees_ > 180) {
    std::string err_msg = "Affine: rotation angle in degrees between -180 and 180, but got " + std::to_string(degrees_);
    LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
  }

  // Translate
  constexpr size_t kExpectedTranslationSize = 2;
  if (translation_.size() != kExpectedTranslationSize) {
    std::string err_msg =
      "Affine: translate expecting size 2, got: translation.size() = " + std::to_string(translation_.size());
    LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
  }
  RETURN_IF_NOT_OK(ValidateScalar("Affine", "translate", translation_[0], {-1, 1}, false, false));
  RETURN_IF_NOT_OK(ValidateScalar("Affine", "translate", translation_[1], {-1, 1}, false, false));

  // Scale
  RETURN_IF_NOT_OK(ValidateScalar("Affine", "scale", scale_, {0}, true));

  // Shear
  constexpr size_t kExpectedShearSize = 2;
  if (shear_.size() != kExpectedShearSize) {
    std::string err_msg = "Affine: shear_ranges expecting size 2, got: shear.size() = " + std::to_string(shear_.size());
    LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
  }
  for (const auto &s : shear_) {
    if (s < -180 || s > 180) {
      std::string err_msg = "Affine: shear angle value between -180 and 180, but got " + std::to_string(s);
      LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
    }
  }

  // Fill Value
  RETURN_IF_NOT_OK(ValidateVectorFillvalue("Affine", fill_value_));

  // interpolation
  if (interpolation_ != InterpolationMode::kLinear && interpolation_ != InterpolationMode::kNearestNeighbour &&
      interpolation_ != InterpolationMode::kCubic && interpolation_ != InterpolationMode::kArea) {
    std::string err_msg = "Affine: Invalid InterpolationMode, only support Linear, Nearest, Cubic and Area.";
    LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
  }
  // device target
  if (device_target_ != "CPU" && device_target_ != "Ascend") {
    std::string err_msg = "Affine: Invalid device target. It's not CPU or Ascend.";
    LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
  }

  return Status::OK();
}

std::shared_ptr<TensorOp> AffineOperation::Build() {
  if (device_target_ == "CPU") {
    std::shared_ptr<AffineOp> tensor_op =
      std::make_shared<AffineOp>(degrees_, translation_, scale_, shear_, interpolation_, fill_value_);
    return tensor_op;
#if !defined(BUILD_LITE) && defined(ENABLE_D)
  } else if (device_target_ == "Ascend") {
    std::shared_ptr<DvppAffineOp> dvpp_tensor_op =
      std::make_shared<DvppAffineOp>(degrees_, translation_, scale_, shear_, interpolation_, fill_value_);
    return dvpp_tensor_op;
#endif
  } else {
    MS_LOG(ERROR) << "Affine: Invalid device target. It's not CPU or Ascend.";
    return nullptr;
  }
}

Status AffineOperation::to_json(nlohmann::json *out_json) {
  RETURN_UNEXPECTED_IF_NULL(out_json);
  nlohmann::json args;
  args["degrees"] = degrees_;
  args["translate"] = translation_;
  args["scale"] = scale_;
  args["shear"] = shear_;
  args["resample"] = interpolation_;
  args["fill_value"] = fill_value_;
  args["device_target"] = device_target_;
  *out_json = args;
  return Status::OK();
}

Status AffineOperation::from_json(nlohmann::json op_params, std::shared_ptr<TensorOperation> *operation) {
  RETURN_UNEXPECTED_IF_NULL(operation);
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "degrees", kAffineOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "translate", kAffineOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "scale", kAffineOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "shear", kAffineOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "resample", kAffineOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "fill_value", kAffineOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "device_target", kAffineOperation));
  float_t degrees = op_params["degrees"];
  std::vector<float> translation = op_params["translate"];
  float scale = op_params["scale"];
  std::vector<float> shear = op_params["shear"];
  auto interpolation = static_cast<InterpolationMode>(op_params["resample"]);
  std::vector<uint8_t> fill_value = op_params["fill_value"];
  std::string device_target = op_params["device_target"];
  *operation = std::make_shared<vision::AffineOperation>(degrees, translation, scale, shear, interpolation, fill_value,
                                                         device_target);
  return Status::OK();
}

MapTargetDevice AffineOperation::Type() {
  if (device_target_ == "CPU") {
    return MapTargetDevice::kCpu;
  } else if (device_target_ == "Ascend") {
    return MapTargetDevice::kAscend910B;
  } else {
    MS_LOG(ERROR) << "Affine: Invalid device target. It's not CPU or Ascend.";
    return MapTargetDevice::kInvalid;
  }
}
}  // namespace vision
}  // namespace dataset
}  // namespace mindspore
