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

#include "minddata/dataset/kernels/ir/vision/pad_ir.h"

#include <algorithm>

#if !defined(ENABLE_ANDROID) || defined(ENABLE_MINDDATA_PYTHON)
#include "minddata/dataset/kernels/image/pad_op.h"
#endif
#if !defined(BUILD_LITE) && defined(ENABLE_D)
#include "minddata/dataset/kernels/image/dvpp/ascend910b/dvpp_pad_op.h"
#endif
#include "minddata/dataset/kernels/ir/validators.h"
#include "minddata/dataset/util/validators.h"

namespace mindspore {
namespace dataset {
namespace vision {
#if !defined(ENABLE_ANDROID) || defined(ENABLE_MINDDATA_PYTHON)
// PadOperation
PadOperation::PadOperation(const std::vector<int32_t> &padding, const std::vector<uint8_t> &fill_value,
                           BorderType padding_mode, const std::string &device_target)
    : padding_(padding), fill_value_(fill_value), padding_mode_(padding_mode), device_target_(device_target) {}

PadOperation::~PadOperation() = default;

std::string PadOperation::Name() const { return kPadOperation; }

Status PadOperation::ValidateParams() {
  // padding
  RETURN_IF_NOT_OK(ValidateVectorPadding("Pad", padding_));
  // fill_value
  RETURN_IF_NOT_OK(ValidateVectorFillvalue("Pad", fill_value_));
  // padding_mode
  if (padding_mode_ != BorderType::kConstant && padding_mode_ != BorderType::kEdge &&
      padding_mode_ != BorderType::kReflect && padding_mode_ != BorderType::kSymmetric) {
    std::string err_msg = "Pad: Invalid BorderType, check input value of enum.";
    LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
  }
  // device target
  if (device_target_ != "CPU" && device_target_ != "Ascend") {
    std::string err_msg = "Pad: Invalid device target. It's not CPU or Ascend.";
    LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
  }
  return Status::OK();
}

std::shared_ptr<TensorOp> PadOperation::Build() {
  constexpr size_t dimension_zero = 0;
  constexpr size_t dimension_one = 1;
  constexpr size_t dimension_two = 2;
  constexpr size_t dimension_three = 3;
  constexpr size_t size_one = 1;
  constexpr size_t size_two = 2;
  constexpr size_t size_three = 3;
  int32_t pad_top, pad_bottom, pad_left, pad_right;
  switch (padding_.size()) {
    case size_one:
      pad_left = padding_[dimension_zero];
      pad_top = padding_[dimension_zero];
      pad_right = padding_[dimension_zero];
      pad_bottom = padding_[dimension_zero];
      break;
    case size_two:
      pad_left = padding_[dimension_zero];
      pad_right = padding_[dimension_zero];
      pad_top = padding_[dimension_one];
      pad_bottom = padding_[dimension_one];
      break;
    default:
      pad_left = padding_[dimension_zero];
      pad_top = padding_[dimension_one];
      pad_right = padding_[dimension_two];
      pad_bottom = padding_[dimension_three];
  }
  uint8_t fill_r, fill_g, fill_b;

  fill_r = fill_value_[dimension_zero];
  fill_g = fill_value_[dimension_zero];
  fill_b = fill_value_[dimension_zero];

  if (fill_value_.size() == size_three) {
    fill_r = fill_value_[dimension_zero];
    fill_g = fill_value_[dimension_one];
    fill_b = fill_value_[dimension_two];
  }

  if (device_target_ == "CPU") {
    std::shared_ptr<PadOp> tensor_op =
      std::make_shared<PadOp>(pad_top, pad_bottom, pad_left, pad_right, padding_mode_, fill_r, fill_g, fill_b);
    return tensor_op;
#if !defined(BUILD_LITE) && defined(ENABLE_D)
  } else if (device_target_ == "Ascend") {
    std::shared_ptr<DvppPadOp> dvpp_tensor_op =
      std::make_shared<DvppPadOp>(pad_top, pad_bottom, pad_left, pad_right, padding_mode_, fill_r, fill_g, fill_b);
    return dvpp_tensor_op;
#endif
  } else {
    MS_LOG(ERROR) << "Pad: Invalid device target. It's not CPU or Ascend.";
    return nullptr;
  }
}

Status PadOperation::to_json(nlohmann::json *out_json) {
  RETURN_UNEXPECTED_IF_NULL(out_json);
  nlohmann::json args;
  args["padding"] = padding_;
  args["fill_value"] = fill_value_;
  args["padding_mode"] = padding_mode_;
  args["device_target"] = device_target_;
  *out_json = args;
  return Status::OK();
}

Status PadOperation::from_json(nlohmann::json op_params, std::shared_ptr<TensorOperation> *operation) {
  RETURN_UNEXPECTED_IF_NULL(operation);
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "padding", kPadOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "fill_value", kPadOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "padding_mode", kPadOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "device_target", kPadOperation));
  std::vector<int32_t> padding = op_params["padding"];
  std::vector<uint8_t> fill_value = op_params["fill_value"];
  auto padding_mode = static_cast<BorderType>(op_params["padding_mode"]);
  std::string device_target = op_params["device_target"];
  *operation = std::make_shared<vision::PadOperation>(padding, fill_value, padding_mode, device_target);
  return Status::OK();
}

MapTargetDevice PadOperation::Type() {
  if (device_target_ == "CPU") {
    return MapTargetDevice::kCpu;
  } else if (device_target_ == "Ascend") {
    return MapTargetDevice::kAscend910B;
  } else {
    MS_LOG(ERROR) << "Pad: Invalid device target. It's not CPU or Ascend.";
    return MapTargetDevice::kInvalid;
  }
}
#endif
}  // namespace vision
}  // namespace dataset
}  // namespace mindspore
