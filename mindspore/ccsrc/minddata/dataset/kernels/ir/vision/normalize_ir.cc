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
#include "minddata/dataset/kernels/ir/vision/normalize_ir.h"

#include "minddata/dataset/kernels/image/normalize_op.h"
#if !defined(BUILD_LITE) && defined(ENABLE_D)
#include "minddata/dataset/kernels/image/dvpp/ascend910b/dvpp_normalize_v2_op.h"
#endif
#include "minddata/dataset/kernels/ir/validators.h"
#include "minddata/dataset/util/validators.h"

namespace mindspore {
namespace dataset {
namespace vision {
// NormalizeOperation
NormalizeOperation::NormalizeOperation(const std::vector<float> &mean, const std::vector<float> &std, bool is_hwc,
                                       const std::string &device_target)
    : mean_(mean), std_(std), is_hwc_(is_hwc), device_target_(device_target) {}

NormalizeOperation::~NormalizeOperation() = default;

std::string NormalizeOperation::Name() const { return kNormalizeOperation; }

Status NormalizeOperation::ValidateParams() {
  RETURN_IF_NOT_OK(ValidateVectorMeanStd("Normalize", mean_, std_));
  // device target
  if (device_target_ != "CPU" && device_target_ != "Ascend") {
    std::string err_msg = "Normalize: Invalid device target. It's not CPU or Ascend.";
    LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
  }
  return Status::OK();
}

std::shared_ptr<TensorOp> NormalizeOperation::Build() {
  if (device_target_ == "CPU") {
    return std::make_shared<NormalizeOp>(mean_, std_, is_hwc_);
#if !defined(BUILD_LITE) && defined(ENABLE_D)
  } else if (device_target_ == "Ascend") {
    return std::make_shared<DvppNormalizeV2Op>(mean_, std_, is_hwc_);
#endif
  } else {
    MS_LOG(ERROR) << "Normalize: Invalid device target. It's not CPU or Ascend.";
    return nullptr;
  }
}

Status NormalizeOperation::to_json(nlohmann::json *out_json) {
  RETURN_UNEXPECTED_IF_NULL(out_json);
  nlohmann::json args;
  args["mean"] = mean_;
  args["std"] = std_;
  args["is_hwc"] = is_hwc_;
  args["device_target"] = device_target_;
  *out_json = args;
  return Status::OK();
}

Status NormalizeOperation::from_json(nlohmann::json op_params, std::shared_ptr<TensorOperation> *operation) {
  RETURN_UNEXPECTED_IF_NULL(operation);
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "mean", kNormalizeOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "std", kNormalizeOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "is_hwc", kNormalizeOperation));
  RETURN_IF_NOT_OK(ValidateParamInJson(op_params, "device_target", kNormalizeOperation));
  std::vector<float> mean = op_params["mean"];
  std::vector<float> std = op_params["std"];
  bool is_hwc = op_params["is_hwc"];
  std::string device_target = op_params["device_target"];
  *operation = std::make_shared<vision::NormalizeOperation>(mean, std, is_hwc, device_target);
  return Status::OK();
}

MapTargetDevice NormalizeOperation::Type() {
  if (device_target_ == "CPU") {
    return MapTargetDevice::kCpu;
  } else if (device_target_ == "Ascend") {
    return MapTargetDevice::kAscend910B;
  } else {
    MS_LOG(ERROR) << "Normalize: Invalid device target. It's not CPU or Ascend.";
    return MapTargetDevice::kInvalid;
  }
}
}  // namespace vision
}  // namespace dataset
}  // namespace mindspore
