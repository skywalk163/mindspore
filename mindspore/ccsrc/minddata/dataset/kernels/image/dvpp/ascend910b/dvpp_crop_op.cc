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
#include "minddata/dataset/kernels/image/dvpp/ascend910b/dvpp_crop_op.h"

#include <vector>

#include "minddata/dataset/kernels/data/data_utils.h"
#ifndef ENABLE_ANDROID
#include "minddata/dataset/kernels/image/dvpp/acl_adapter.h"
#include "minddata/dataset/kernels/image/dvpp/utils/dvpp_image_utils.h"
#include "minddata/dataset/kernels/image/dvpp/utils/ErrorCode.h"
#include "minddata/dataset/kernels/image/image_utils.h"
#else
#include "minddata/dataset/kernels/image/lite_image_utils.h"
#endif
#include "minddata/dataset/util/status.h"

namespace mindspore {
namespace dataset {
constexpr int64_t h_lb = 4;      // height lower bound
constexpr int64_t h_ub = 32768;  // height upper bound
constexpr int64_t w_lb = 6;      // width lower bound
constexpr int64_t w_ub = 32768;  // width upper bound

Status DvppCropOp::Compute(const std::shared_ptr<DeviceTensorAscend910B> &input,
                           std::shared_ptr<DeviceTensorAscend910B> *output) {
  IO_CHECK(input, output);

  // the input should be NHWC, N is 1.
  CHECK_FAIL_RETURN_UNEXPECTED(
    input->GetShape().Rank() == kNHWCImageRank,
    "DvppCrop: the input tensor is not HW, HWC or 1HWC, but got: " + std::to_string(input->GetShape().Rank()));

  // crop region should not exceed image shape
  int64_t input_h = input->GetShape()[kHeightIndexNHWC];
  int64_t input_w = input->GetShape()[kWidthIndexNHWC];
  CHECK_FAIL_RETURN_UNEXPECTED(top_ + height_ <= input_h,
                               "DvppCrop: Crop height dimension: " + std::to_string(top_ + height_) +
                                 " exceeds image height: " + std::to_string(input_h));
  CHECK_FAIL_RETURN_UNEXPECTED(left_ + width_ <= input_w,
                               "DvppCrop: Crop width dimension: " + std::to_string(left_ + width_) +
                                 " exceeds image width: " + std::to_string(input_w));

  // Dvpp Limit
  RETURN_IF_NOT_OK(CheckDvppLimit(input_h, input_w, h_lb, w_lb, h_ub, w_ub, kDvppCropOp, "input"));
  RETURN_IF_NOT_OK(CheckDvppLimit(height_, width_, h_lb, w_lb, h_ub, w_ub, kDvppCropOp, "output"));

  APP_ERROR ret = AclAdapter::GetInstance().DvppCrop(input, output, top_, left_, height_, width_);
  if (ret != APP_ERR_OK) {
    std::string error = "DvppCrop: Error in dvpp processing: " + std::to_string(ret);
    RETURN_STATUS_UNEXPECTED(error);
  }
  return Status::OK();
}

Status DvppCropOp::OutputShape(const std::vector<TensorShape> &inputs, std::vector<TensorShape> &outputs) {
  outputs.clear();
  int32_t input_h = inputs[0][kHeightIndex];
  int32_t input_w = inputs[0][kWidthIndex];
  int32_t output_h = height_;
  int32_t output_w = width_;

  if ((input_h < h_lb || input_h > h_ub) || (input_w < w_lb || input_w > w_ub)) {
    auto error = "DvppCrop: the input shape should be from [4, 6] to [32768, 32768], but got [" +
                 std::to_string(input_h) + ", " + std::to_string(input_w) + "].";
    RETURN_STATUS_UNEXPECTED(error);
  }
  if ((output_h < h_lb || output_h > h_ub) || (output_w < w_lb || output_w > w_ub)) {
    auto error = "DvppCrop: the output shape should be from [4, 6] to [32768, 32768], but got [" +
                 std::to_string(output_h) + ", " + std::to_string(output_w) + "].";
    RETURN_STATUS_UNEXPECTED(error);
  }

  TensorShape out = TensorShape{height_, width_};
  CHECK_FAIL_RETURN_UNEXPECTED(!inputs.empty(), "DvppCrop: inputs cannot be empty.");
  if (inputs[0].Rank() == kMinImageRank) {
    (void)outputs.emplace_back(out);
  }
  if (inputs[0].Rank() == kDefaultImageRank) {
    (void)outputs.emplace_back(out.AppendDim(inputs[0][kChannelIndexHWC]));
  }
  CHECK_FAIL_RETURN_UNEXPECTED(!outputs.empty(),
                               "DvppCrop: invalid input shape, expected 2D or 3D input, but got input dimension is:" +
                                 std::to_string(inputs[0].Rank()));
  return Status::OK();
}

Status DvppCropOp::OutputType(const std::vector<DataType> &inputs, std::vector<DataType> &outputs) {
  RETURN_IF_NOT_OK(TensorOp::OutputType(inputs, outputs));
  return Status::OK();
}
}  // namespace dataset
}  // namespace mindspore
