/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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
#include "minddata/dataset/kernels/image/dvpp/utils/dvpp_image_utils.h"

#include <opencv2/imgproc/types_c.h>
#include <algorithm>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#include <stdexcept>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include "utils/file_utils.h"
#include "utils/ms_utils.h"
#include "minddata/dataset/core/cv_tensor.h"
#include "minddata/dataset/core/tensor.h"
#include "minddata/dataset/core/tensor_shape.h"
#include "minddata/dataset/core/type_id.h"
#include "minddata/dataset/include/dataset/constants.h"
#include "minddata/dataset/kernels/image/affine_op.h"
#include "minddata/dataset/kernels/image/auto_contrast_op.h"
#include "minddata/dataset/kernels/image/invert_op.h"
#include "minddata/dataset/kernels/image/image_utils.h"
#include "minddata/dataset/kernels/image/math_utils.h"
#include "minddata/dataset/kernels/image/posterize_op.h"
#include "minddata/dataset/kernels/image/resize_cubic_op.h"
#include "minddata/dataset/kernels/image/sharpness_op.h"
#include "minddata/dataset/kernels/image/solarize_op.h"
#include "minddata/dataset/kernels/data/data_utils.h"

#include "acldvppop/acldvpp_adjust_brightness.h"
#include "acldvppop/acldvpp_adjust_contrast.h"
#include "acldvppop/acldvpp_adjust_hue.h"
#include "acldvppop/acldvpp_adjust_saturation.h"
#include "acldvppop/acldvpp_crop.h"
#include "acldvppop/acldvpp_crop_and_resize.h"
#include "acldvppop/acldvpp_decode_jpeg.h"
#include "acldvppop/acldvpp_gaussian_blur.h"
#include "acldvppop/acldvpp_horizontal_flip.h"
#include "acldvppop/acldvpp_normalize.h"
#include "acldvppop/acldvpp_pad.h"
#include "acldvppop/acldvpp_resize.h"
#include "acldvppop/acldvpp_vertical_flip.h"
#include "acldvppop/acldvpp_warp_affine.h"
#include "acldvppop/acldvpp_warp_perspective.h"
#include "plugin/device/ascend/hal/device/ascend_stream_manager.h"

namespace mindspore {
namespace dataset {
APP_ERROR DvppAdjustBrightness(const std::shared_ptr<DeviceTensorAscend910B> &input,
                               std::shared_ptr<DeviceTensorAscend910B> *output, float factor) {
  MS_LOG(DEBUG) << "Begin execute adjust brightness.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_ADJUST_BRIGHTNESS_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_ADJUST_BRIGHTNESS_FAIL;
  }

  // the channel should be equal to 3 or 1
  if (input->GetShape().AsVector()[kChannelIndexNHWC] != kDefaultImageChannel &&
      input->GetShape().AsVector()[kChannelIndexNHWC] != 1) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";
    return APP_ERR_DVPP_ADJUST_BRIGHTNESS_FAIL;
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_ADJUST_BRIGHTNESS_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_ADJUST_BRIGHTNESS_FAIL;
  }

  // create the output shape and type, it's 1HWC or 1CHW
  TensorShape shape = input->GetShape();
  DataType type = input->GetType();

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, true) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_ADJUST_BRIGHTNESS_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  auto ret = acldvppAdjustBrightnessGetWorkspaceSize(reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), factor,
                                                     reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()),
                                                     &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppAdjustBrightnessGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_ADJUST_BRIGHTNESS_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_ADJUST_BRIGHTNESS_FAIL;
    }

    // call DVPP step3
    ret = acldvppAdjustBrightness(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_ADJUST_BRIGHTNESS_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppAdjustBrightness(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppAdjustBrightness failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_ADJUST_BRIGHTNESS_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppAdjustContrast(const std::shared_ptr<DeviceTensorAscend910B> &input,
                             std::shared_ptr<DeviceTensorAscend910B> *output, float factor) {
  MS_LOG(WARNING) << "Begin execute adjust contrast.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_ADJUST_CONTRAST_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_ADJUST_CONTRAST_FAIL;
  }

  // the channel should be equal to 3
  if (input->GetShape().AsVector()[kChannelIndexNHWC] != kDefaultImageChannel) {
    MS_LOG(ERROR) << "The input data's channel is not 3.";
    return APP_ERR_DVPP_ADJUST_CONTRAST_FAIL;
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_ADJUST_CONTRAST_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_ADJUST_CONTRAST_FAIL;
  }

  // create the output shape and type, it's 1HWC or 1CHW
  TensorShape shape = input->GetShape();
  DataType type = input->GetType();

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, true) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_ADJUST_CONTRAST_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  auto ret = acldvppAdjustContrastGetWorkspaceSize(reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), factor,
                                                   reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()),
                                                   &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppAdjustContrastGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_ADJUST_CONTRAST_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_ADJUST_CONTRAST_FAIL;
    }

    // call DVPP step3
    ret = acldvppAdjustContrast(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_ADJUST_CONTRAST_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppAdjustContrast(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppAdjustContrast failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_ADJUST_CONTRAST_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppAdjustHue(const std::shared_ptr<DeviceTensorAscend910B> &input,
                        std::shared_ptr<DeviceTensorAscend910B> *output, float factor) {
  MS_LOG(WARNING) << "Begin execute adjust hue.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_ADJUST_HUE_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_ADJUST_HUE_FAIL;
  }

  // the channel should be equal to 3 or 1
  if (input->GetShape().AsVector()[3] != 3 && input->GetShape().AsVector()[3] != 1) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";
    return APP_ERR_DVPP_ADJUST_HUE_FAIL;
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_ADJUST_HUE_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_ADJUST_HUE_FAIL;
  }

  // create the output shape and type, it's 1HWC or 1CHW
  TensorShape shape = input->GetShape();
  DataType type = input->GetType();

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, true) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_ADJUST_HUE_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  auto ret = acldvppAdjustHueGetWorkspaceSize(reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), factor,
                                              reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()),
                                              &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppAdjustHueGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_ADJUST_HUE_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_ADJUST_HUE_FAIL;
    }

    // call DVPP step3
    ret = acldvppAdjustHue(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_ADJUST_HUE_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppAdjustHue(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppAdjustHue failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_ADJUST_HUE_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppAdjustSaturation(const std::shared_ptr<DeviceTensorAscend910B> &input,
                               std::shared_ptr<DeviceTensorAscend910B> *output, float factor) {
  MS_LOG(WARNING) << "Begin execute adjust saturation.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_ADJUST_SATURATION_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_ADJUST_SATURATION_FAIL;
  }

  // the channel should be equal to 3 or 1
  if (input->GetShape().AsVector()[3] != 3 && input->GetShape().AsVector()[3] != 1) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";
    return APP_ERR_DVPP_ADJUST_SATURATION_FAIL;
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_ADJUST_SATURATION_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_ADJUST_SATURATION_FAIL;
  }

  // create the output shape and type, it's 1HWC or 1CHW
  TensorShape shape = input->GetShape();
  DataType type = input->GetType();

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, true) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_ADJUST_SATURATION_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  auto ret = acldvppAdjustSaturationGetWorkspaceSize(reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), factor,
                                                     reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()),
                                                     &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppAdjustSaturationGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_ADJUST_SATURATION_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_ADJUST_SATURATION_FAIL;
    }

    // call DVPP step3
    ret = acldvppAdjustSaturation(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_ADJUST_SATURATION_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppAdjustSaturation(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppAdjustSaturation failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_ADJUST_SATURATION_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppAffine(const std::shared_ptr<DeviceTensorAscend910B> &input,
                     std::shared_ptr<DeviceTensorAscend910B> *output, const std::vector<float> &matrix,
                     uint32_t interpolation_mode, uint32_t padding_mode, const std::vector<float> &fill) {
  MS_LOG(DEBUG) << "Begin execute dvpp affine.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  // the channel should be equal to 3 or 1
  if (input->GetShape().AsVector()[kChannelIndexNHWC] != kDefaultImageChannel &&
      input->GetShape().AsVector()[kChannelIndexNHWC] != kMinImageChannel) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  // create the output shape and type
  TensorShape shape = input->GetShape();
  DataType type = input->GetType();

  // create matrix vector
  aclFloatArray *acl_matrix = aclCreateFloatArray(matrix.data(), matrix.size());
  if (acl_matrix == nullptr) {
    MS_LOG(ERROR) << "Call aclCreateFloatArray failed.";
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  // create fill vector
  aclFloatArray *acl_fill = aclCreateFloatArray(fill.data(), fill.size());
  if (acl_fill == nullptr) {
    MS_LOG(ERROR) << "Call aclCreateFloatArray failed.";
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  // the memory will be released when the map / executor is finished
  if (!input->AddMaintenFloatArrayMemory(reinterpret_cast<void *>(acl_matrix))) {
    MS_LOG(ERROR) << "Add float array [acl_matrix] to the input failed";
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  if (!input->AddMaintenFloatArrayMemory(reinterpret_cast<void *>(acl_fill))) {
    MS_LOG(ERROR) << "Add float array [acl_fill] to the input failed";
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, true) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  auto ret = acldvppWarpAffineGetWorkspaceSize(
    reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), acl_matrix, interpolation_mode, padding_mode, acl_fill,
    reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()), &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppWarpAffineGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_AFFINE_FAIL;
    }

    // call DVPP step3
    ret = acldvppWarpAffine(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_AFFINE_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppWarpAffine(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppWarpAffine failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_AFFINE_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppCrop(const std::shared_ptr<DeviceTensorAscend910B> &input,
                   std::shared_ptr<DeviceTensorAscend910B> *output, uint32_t top, uint32_t left, uint32_t height,
                   uint32_t width) {
  MS_LOG(DEBUG) << "Begin execute dvpp crop.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_CROP_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_CROP_FAIL;
  }

  // the channel should be equal to 3 or 1
  if (input->GetShape().AsVector()[kChannelIndexNHWC] != kDefaultImageChannel &&
      input->GetShape().AsVector()[kChannelIndexNHWC] != kMinImageChannel) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";
    return APP_ERR_DVPP_CROP_FAIL;
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_CROP_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_CROP_FAIL;
  }

  // create the output shape and type, it's 1HWC or 1CHW
  TensorShape shape{static_cast<int64_t>(input->GetShape()[0]), height, width,
                    static_cast<int64_t>(input->GetShape()[kChannelIndexHWC + 1])};
  DataType type = input->GetType();

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, true) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_CROP_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  // decode output C is 3
  // don't recovery truncate
  auto ret = acldvppCropGetWorkspaceSize(reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), top, left, height,
                                         width, reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()),
                                         &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppCropGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_CROP_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_CROP_FAIL;
    }

    // call DVPP step3
    ret = acldvppCrop(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_CROP_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppCrop(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppCrop failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_CROP_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppDecode(const std::shared_ptr<DeviceTensorAscend910B> &input,
                     std::shared_ptr<DeviceTensorAscend910B> *output) {
  MS_LOG(DEBUG) << "Begin execute dvpp decode.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_JPEG_DECODE_FAIL;
  }

  // the output DeviceTensorAscend910B had been created in npu_map_job.cc,
  // because we need get image height and width from the JPEG header
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = *output;

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  // decode output C is 3
  // don't recovery truncate
  auto ret = acldvppDecodeJpegGetWorkspaceSize(reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), 3, true,
                                               reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()),
                                               &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppDecodeJpegGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_JPEG_DECODE_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_JPEG_DECODE_FAIL;
    }

    // call DVPP step3
    ret = acldvppDecodeJpeg(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_JPEG_DECODE_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppDecodeJpeg(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppDecodeJpeg failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_JPEG_DECODE_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppGaussianBlur(const std::shared_ptr<DeviceTensorAscend910B> &input,
                           std::shared_ptr<DeviceTensorAscend910B> *output, const std::vector<int64_t> &kernel_size,
                           const std::vector<float> &sigma, uint32_t padding_mode) {
  MS_LOG(DEBUG) << "Begin execute dvpp GaussianBlur.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  // the channel should be equal to 3 or 1
  if (input->GetShape().AsVector()[kChannelIndexNHWC] != kDefaultImageChannel &&
      input->GetShape().AsVector()[kChannelIndexNHWC] != kMinImageChannel) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  // create the output shape and type
  TensorShape shape = input->GetShape();
  DataType type = input->GetType();

  // create matrix vector
  aclIntArray *acl_kernel_size = aclCreateIntArray(kernel_size.data(), kernel_size.size());
  if (acl_kernel_size == nullptr) {
    MS_LOG(ERROR) << "Call aclCreateIntArray failed.";
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  // create fill vector
  aclFloatArray *acl_sigma = aclCreateFloatArray(sigma.data(), sigma.size());
  if (acl_sigma == nullptr) {
    MS_LOG(ERROR) << "Call aclCreateFloatArray failed.";
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  // the memory will be released when the map / executor is finished
  if (!input->AddMaintenIntArrayMemory(reinterpret_cast<void *>(acl_kernel_size))) {
    MS_LOG(ERROR) << "Add float array [acl_kernel_size] to the input failed";
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  if (!input->AddMaintenFloatArrayMemory(reinterpret_cast<void *>(acl_sigma))) {
    MS_LOG(ERROR) << "Add float array [acl_sigma] to the input failed";
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, true) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  auto ret = acldvppGaussianBlurGetWorkspaceSize(
    reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), acl_kernel_size, acl_sigma, padding_mode,
    reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()), &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppGaussianBlurGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
    }

    // call DVPP step3
    ret = acldvppGaussianBlur(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppGaussianBlur(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppGaussianBlur failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_GAUSSIAN_BLUR_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppHorizontalFlip(const std::shared_ptr<DeviceTensorAscend910B> &input,
                             std::shared_ptr<DeviceTensorAscend910B> *output) {
  MS_LOG(DEBUG) << "Begin execute dvpp horizontal flip.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_HORIZONTAL_FLIP_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_HORIZONTAL_FLIP_FAIL;
  }

  // the channel should be equal to 3 or 1
  if (input->GetShape().AsVector()[kChannelIndexNHWC] != kDefaultImageChannel &&
      input->GetShape().AsVector()[kChannelIndexNHWC] != kMinImageChannel) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";
    return APP_ERR_DVPP_HORIZONTAL_FLIP_FAIL;
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_HORIZONTAL_FLIP_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_HORIZONTAL_FLIP_FAIL;
  }

  // create the output shape and type, it's 1HWC or 1CHW
  TensorShape shape = input->GetShape();
  DataType type = input->GetType();

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, true) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_HORIZONTAL_FLIP_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  // decode output C is 3
  // don't recovery truncate
  auto ret = acldvppHorizontalFlipGetWorkspaceSize(reinterpret_cast<aclTensor *>(input->GetDeviceTensor()),
                                                   reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()),
                                                   &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppHorizontalFlipGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_HORIZONTAL_FLIP_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_HORIZONTAL_FLIP_FAIL;
    }

    // call DVPP step3
    ret = acldvppHorizontalFlip(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_HORIZONTAL_FLIP_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppHorizontalFlip(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppHorizontalFlip failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_HORIZONTAL_FLIP_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppNormalize(const std::shared_ptr<DeviceTensorAscend910B> &input,
                        std::shared_ptr<DeviceTensorAscend910B> *output, std::vector<float> mean,
                        std::vector<float> std, bool is_hwc) {
  MS_LOG(DEBUG) << "Begin execute dvpp normalize.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_NORMALIZE_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_NORMALIZE_FAIL;
  }

  if (!is_hwc) {
    if (input->GetShape().AsVector()[1] != kDefaultImageChannel && input->GetShape().AsVector()[1] != 1) {
      MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";  // C == 3 or 1
      return APP_ERR_DVPP_NORMALIZE_FAIL;
    }

    // the channel should be equal to the size of mean
    if (mean.size() != std.size() || std.size() != input->GetShape().AsVector()[1]) {
      MS_LOG(ERROR) << "The channel is not equal to the size of mean or std.";
      return APP_ERR_DVPP_NORMALIZE_FAIL;
    }
  } else {
    if (input->GetShape().AsVector()[kChannelIndexNHWC] != kDefaultImageChannel &&
        input->GetShape().AsVector()[kChannelIndexNHWC] != 1) {
      MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";  // C == 3 or 1
      return APP_ERR_DVPP_NORMALIZE_FAIL;
    }

    // the channel should be equal to the size of mean
    if (mean.size() != std.size() || std.size() != input->GetShape().AsVector()[kChannelIndexNHWC]) {
      MS_LOG(ERROR) << "The channel is not equal to the size of mean or std.";
      return APP_ERR_DVPP_NORMALIZE_FAIL;
    }
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_NORMALIZE_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_NORMALIZE_FAIL;
  }

  // create the output shape and type, it's 1HWC or 1CHW
  TensorShape shape = input->GetShape();
  DataType type(DataType::DE_FLOAT32);

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, is_hwc) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_NORMALIZE_FAIL;
  }

  // create aclFloatArray for mean
  aclFloatArray *acl_mean = aclCreateFloatArray(mean.data(), mean.size());
  if (acl_mean == nullptr) {
    MS_LOG(ERROR) << "Call aclCreateFloatArray failed.";
    return APP_ERR_DVPP_NORMALIZE_FAIL;
  }

  // create aclFloatArray for std
  aclFloatArray *acl_std = aclCreateFloatArray(std.data(), std.size());
  if (acl_std == nullptr) {
    MS_LOG(ERROR) << "Call aclCreateFloatArray failed.";
    return APP_ERR_DVPP_NORMALIZE_FAIL;
  }

  // the memory will be released when the map / executor is finished
  if (!input->AddMaintenFloatArrayMemory(reinterpret_cast<void *>(acl_mean))) {
    MS_LOG(ERROR) << "Add float array [acl_mean] to the input failed";
    return APP_ERR_DVPP_NORMALIZE_FAIL;
  }

  if (!input->AddMaintenFloatArrayMemory(reinterpret_cast<void *>(acl_std))) {
    MS_LOG(ERROR) << "Add float array [acl_std] to the input failed";
    return APP_ERR_DVPP_NORMALIZE_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  auto ret = acldvppNormalizeGetWorkspaceSize(reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), acl_mean,
                                              acl_std, reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()),
                                              &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call aclvisionNormalizeGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_NORMALIZE_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_NORMALIZE_FAIL;
    }

    // call DVPP step3
    ret = acldvppNormalize(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_NORMALIZE_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppNormalize(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppNormalize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_NORMALIZE_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppPad(const std::shared_ptr<DeviceTensorAscend910B> &input, std::shared_ptr<DeviceTensorAscend910B> *output,
                  const std::vector<int64_t> &padding, uint32_t padding_mode, const std::vector<float> &fill) {
  MS_LOG(DEBUG) << "Begin execute dvpp Pad.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_PAD_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_PAD_FAIL;
  }

  // the channel should be equal to 3 or 1
  if (input->GetShape().AsVector()[kChannelIndexNHWC] != kDefaultImageChannel &&
      input->GetShape().AsVector()[kChannelIndexNHWC] != kMinImageChannel) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";
    return APP_ERR_DVPP_PAD_FAIL;
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_PAD_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_PAD_FAIL;
  }

  // create the output shape and type
  TensorShape input_shape = input->GetShape();
  int32_t left = padding[0];
  int32_t top = padding[1];
  int32_t right = padding[2];
  int32_t bottom = padding[3];
  TensorShape output_shape =
    TensorShape(std::vector<int64_t>{input_shape[0], input_shape[kHeightIndexNHWC] + top + bottom,
                                     input_shape[kWidthIndexNHWC] + left + right, input_shape[kChannelIndexNHWC]});
  DataType type = input->GetType();

  // create pad vector
  aclIntArray *acl_padding = aclCreateIntArray(padding.data(), padding.size());
  if (acl_padding == nullptr) {
    MS_LOG(ERROR) << "Call aclCreateIntArray failed.";
    return APP_ERR_DVPP_PAD_FAIL;
  }

  // create fill vector
  aclFloatArray *acl_fill = aclCreateFloatArray(fill.data(), fill.size());
  if (acl_fill == nullptr) {
    MS_LOG(ERROR) << "Call aclCreateFloatArray failed.";
    return APP_ERR_DVPP_PAD_FAIL;
  }

  // the memory will be released when the map / executor is finished
  if (!input->AddMaintenIntArrayMemory(reinterpret_cast<void *>(acl_padding))) {
    MS_LOG(ERROR) << "Add int array [acl_padding] to the input failed";
    return APP_ERR_DVPP_PAD_FAIL;
  }

  if (!input->AddMaintenFloatArrayMemory(reinterpret_cast<void *>(acl_fill))) {
    MS_LOG(ERROR) << "Add float array [acl_fill] to the input failed";
    return APP_ERR_DVPP_PAD_FAIL;
  }

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(output_shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, true) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_PAD_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  auto ret = acldvppPadGetWorkspaceSize(
    reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), acl_padding, padding_mode, acl_fill,
    reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()), &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppPadGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_PAD_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_PAD_FAIL;
    }

    // call DVPP step3
    ret = acldvppPad(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_PAD_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppPad(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppPad failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_PAD_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppPerspective(const std::shared_ptr<DeviceTensorAscend910B> &input,
                          std::shared_ptr<DeviceTensorAscend910B> *output,
                          const std::vector<std::vector<int32_t>> &start_points,
                          const std::vector<std::vector<int32_t>> &end_points, InterpolationMode interpolation) {
  MS_LOG(DEBUG) << "Begin execute dvpp Perspective.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  // the channel should be equal to 3 or 1
  if (input->GetShape().AsVector()[kChannelIndexNHWC] != kDefaultImageChannel &&
      input->GetShape().AsVector()[kChannelIndexNHWC] != kMinImageChannel) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  // create the output shape and type, it's 1HWC or 1CHW
  TensorShape shape = input->GetShape();
  DataType type = input->GetType();

  // create transform matrix
  // Get Point
  const int kListSize = 4;
  cv::Point2f cv_src_point[kListSize];
  cv::Point2f cv_dst_point[kListSize];
  for (int i = 0; i < kListSize; i++) {
    cv_src_point[i] = cv::Point2f(static_cast<float>(start_points[i][0]), static_cast<float>(start_points[i][1]));
    cv_dst_point[i] = cv::Point2f(static_cast<float>(end_points[i][0]), static_cast<float>(end_points[i][1]));
  }

  // Get transform matrix by cv::getPerspectiveTransform function
  cv::Mat M = cv::getPerspectiveTransform(cv_src_point, cv_dst_point, cv::DECOMP_LU);
  cv::Mat input_matrix;
  cv::invert(M, input_matrix);
  const int kMatrixSize = 3;
  std::vector<float> transform_matrix;
  for (int i = 0; i < kMatrixSize; i++) {
    double *data = input_matrix.ptr<double>(i);
    for (int j = 0; j < kMatrixSize; j++) {
      transform_matrix.push_back(static_cast<float>(data[j]));
    }
  }

  aclFloatArray *matrix = aclCreateFloatArray(transform_matrix.data(), transform_matrix.size());
  if (matrix == nullptr) {
    MS_LOG(ERROR) << "Call aclCreateFloatArray failed.";
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  // get pad data
  std::vector<float> fill_data = {0.0, 0.0, 0.0};
  auto *fill = aclCreateFloatArray(fill_data.data(), fill_data.size());
  if (fill == nullptr) {
    MS_LOG(ERROR) << "Call aclCreateFloatArray failed.";
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  // the memory will be released when the map / executor is finished
  if (!input->AddMaintenFloatArrayMemory(reinterpret_cast<void *>(matrix))) {
    MS_LOG(ERROR) << "Add float array [matrix] to the input failed";
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  if (!input->AddMaintenFloatArrayMemory(reinterpret_cast<void *>(fill))) {
    MS_LOG(ERROR) << "Add float array [fill] to the input failed";
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, true) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  // convert InterpolationMode mode to DVPP mode
  auto dvpp_interpolation_mode = GetDVPPInterpolationMode(interpolation);
  if (dvpp_interpolation_mode == kInvalidInterpolationMode) {
    std::string err_msg = "The current InterpolationMode is not supported by DVPP. It is " +
                          std::to_string(static_cast<int>(interpolation));
    MS_LOG(ERROR) << err_msg;
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  auto ret = acldvppWarpPerspectiveGetWorkspaceSize(
    reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), matrix, dvpp_interpolation_mode, 0, fill,
    reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()), &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppWarpPerspectiveGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_PERSPECTIVE_FAIL;
    }

    // call DVPP step3
    ret = acldvppWarpPerspective(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_PERSPECTIVE_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppWarpPerspective(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppWarpPerspective failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_PERSPECTIVE_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppResize(const std::shared_ptr<DeviceTensorAscend910B> &input,
                     std::shared_ptr<DeviceTensorAscend910B> *output, int32_t output_height, int32_t output_width,
                     double fx, double fy, InterpolationMode mode) {
  MS_LOG(DEBUG) << "Begin execute dvpp resize.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_RESIZE_FAIL;
  }

  // the input should be HWC
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_RESIZE_FAIL;
  }
  if (input->GetShape().AsVector()[kChannelIndexNHWC] != kDefaultImageChannel &&
      input->GetShape().AsVector()[kChannelIndexNHWC] != 1) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";  // C == 3 or 1
    return APP_ERR_DVPP_RESIZE_FAIL;
  }

  if (output_height == 0 || output_width == 0) {
    std::string err_msg = "DvppResize: the input value of 'resize' is invalid, width or height is zero.";
    MS_LOG(ERROR) << err_msg;
    return APP_ERR_DVPP_RESIZE_FAIL;
  }

  // create the output shape and type, it's HWC
  TensorShape shape{static_cast<int>(input->GetShape()[0]), output_height, output_width,
                    static_cast<int>(input->GetShape()[kChannelIndexHWC + 1])};
  DataType type = input->GetType();

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_RESIZE_FAIL;
  }

  // convert InterpolationMode mode to DVPP mode
  auto dvpp_interpolation_mode = GetDVPPInterpolationMode(mode);
  if (dvpp_interpolation_mode == kInvalidInterpolationMode) {
    std::string err_msg =
      "The current InterpolationMode is not supported by DVPP. It is " + std::to_string(static_cast<int>(mode));
    MS_LOG(ERROR) << err_msg;
    return APP_ERR_DVPP_RESIZE_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  auto ret = acldvppResizeGetWorkspaceSize(
    reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), dvpp_interpolation_mode,
    reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()), &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppResizeGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_RESIZE_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_RESIZE_FAIL;
    }

    // call DVPP step3
    ret = acldvppResize(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_RESIZE_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppResize(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call aclvisionResize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_RESIZE_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppResizedCrop(const std::shared_ptr<DeviceTensorAscend910B> &input,
                          std::shared_ptr<DeviceTensorAscend910B> *output, int32_t top, int32_t left, int32_t height,
                          int32_t width, int32_t output_height, int32_t output_width, InterpolationMode mode) {
  MS_LOG(DEBUG) << "Begin execute dvpp crop and resize.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }

  // the input should be HWC
  if (input->GetShape().Rank() != kNHWCImageRank) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }
  if (input->GetShape().AsVector()[kChannelIndexNHWC] != kDefaultImageChannel &&
      input->GetShape().AsVector()[kChannelIndexNHWC] != 1) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";  // C == 3 or 1
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }

  const uint32_t kResizeShapeLimits = 1000;
  // resize image too large or too small, 1000 is arbitrarily chosen here to prevent open cv from segmentation fault
  if ((std::numeric_limits<int>::max() / kResizeShapeLimits) <= input->GetShape().AsVector()[kHeightIndexNHWC]) {
    MS_LOG(ERROR) << "DvppResizedCrop: in_image rows out of bounds.";
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }
  if ((std::numeric_limits<int>::max() / kResizeShapeLimits) <= input->GetShape().AsVector()[kWidthIndexNHWC]) {
    MS_LOG(ERROR) << "DvppResizedCrop: in_image cols out of bounds.";
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }
  if (output_height > input->GetShape().AsVector()[1] * kResizeShapeLimits ||
      output_width > input->GetShape().AsVector()[kWidthIndexNHWC] * kResizeShapeLimits) {
    std::string err_msg =
      "DvppResizedCrop: the resizing width or height is too big, it's 1000 times bigger than the original image, got "
      "output "
      "height: " +
      std::to_string(output_height) + ", width: " + std::to_string(output_width) +
      ", and original image size:" + std::to_string(input->GetShape().AsVector()[1]) + ", " +
      std::to_string(input->GetShape().AsVector()[kWidthIndexNHWC]);
    MS_LOG(ERROR) << err_msg;
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }
  if (output_height == 0 || output_width == 0) {
    std::string err_msg = "DvppResizedCrop: the input value of 'resize' is invalid, width or height is zero.";
    MS_LOG(ERROR) << err_msg;
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }

  // create the output shape and type, it's HWC
  TensorShape shape{static_cast<int>(input->GetShape()[0]), output_height, output_width,
                    static_cast<int>(input->GetShape()[kChannelIndexHWC + 1])};
  DataType type = input->GetType();

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }

  // convert InterpolationMode mode to DVPP mode
  auto dvpp_interpolation_mode = GetDVPPInterpolationMode(mode);
  if (dvpp_interpolation_mode == kInvalidInterpolationMode) {
    std::string err_msg =
      "The current InterpolationMode is not supported by DVPP. It is " + std::to_string(static_cast<int>(mode));
    MS_LOG(ERROR) << err_msg;
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }

  // call DVPP step1
  std::vector<int64_t> size_data = {output_height, output_width};
  aclIntArray *size = aclCreateIntArray(size_data.data(), size_data.size());
  if (size == nullptr) {
    MS_LOG(ERROR) << "Call aclCreateIntArray failed.";
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }

  // the memory will be released when the map / executor is finished
  if (!input->AddMaintenIntArrayMemory(reinterpret_cast<void *>(size))) {
    MS_LOG(ERROR) << "Add int array [size] to the input failed";
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }

  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  auto ret = acldvppCropAndResizeGetWorkspaceSize(
    reinterpret_cast<aclTensor *>(input->GetDeviceTensor()), top, left, height, width, size, dvpp_interpolation_mode,
    reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()), &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppCropAndResizeGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_RESIZED_CROP_FAIL;
    }

    // call DVPP step3
    ret = acldvppCropAndResize(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_RESIZED_CROP_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppCropAndResize(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppCropAndResize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_RESIZED_CROP_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

APP_ERROR DvppVerticalFlip(const std::shared_ptr<DeviceTensorAscend910B> &input,
                           std::shared_ptr<DeviceTensorAscend910B> *output) {
  MS_LOG(DEBUG) << "Begin execute dvpp vertical flip.";
  if (input == nullptr || output == nullptr) {
    MS_LOG(ERROR) << "The input or output is nullptr.";
    return APP_ERR_DVPP_VERTICAL_FLIP_FAIL;
  }

  // the input should be 1HWC or 1CHW
  if (input->GetShape().Rank() != 4) {
    MS_LOG(ERROR) << "The input data's dims is not 4.";  // NHWC
    return APP_ERR_DVPP_VERTICAL_FLIP_FAIL;
  }

  // the channel should be equal to 3 or 1
  if (input->GetShape().AsVector()[3] != 3 && input->GetShape().AsVector()[3] != 1) {
    MS_LOG(ERROR) << "The input data's channel is not 3 or 1.";
    return APP_ERR_DVPP_VERTICAL_FLIP_FAIL;
  }

  if (input->GetShape().AsVector()[0] != 1) {
    MS_LOG(ERROR) << "The input data is not 1HWC or 1CHW.";  // N == 1
    return APP_ERR_DVPP_VERTICAL_FLIP_FAIL;
  }

  // the type is uint8 / float
  if (input->GetType() != DataType::DE_UINT8 && input->GetType() != DataType::DE_FLOAT32) {
    MS_LOG(ERROR) << "The input data is not uint8 or float32";
    return APP_ERR_DVPP_VERTICAL_FLIP_FAIL;
  }

  // create the output shape and type, it's 1HWC or 1CHW
  TensorShape shape = input->GetShape();
  DataType type = input->GetType();

  // create output DeviceTensorAscend910B
  std::shared_ptr<DeviceTensorAscend910B> device_tensor = nullptr;
  if (DeviceTensorAscend910B::CreateDeviceTensor(shape, type, input->GetDeviceContext(), input->GetStreamID(),
                                                 &device_tensor, true) != Status::OK()) {
    MS_LOG(ERROR) << "Create output device tensor failed.";
    return APP_ERR_DVPP_VERTICAL_FLIP_FAIL;
  }

  // call DVPP step1
  uint64_t workspace_size = 0;
  aclOpExecutor *executor;
  // decode output C is 3
  // don't recovery truncate
  auto ret = acldvppVerticalFlipGetWorkspaceSize(reinterpret_cast<aclTensor *>(input->GetDeviceTensor()),
                                                 reinterpret_cast<aclTensor *>(device_tensor->GetDeviceTensor()),
                                                 &workspace_size, &executor);
  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppVerticalFlipGetWorkspaceSize failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_VERTICAL_FLIP_FAIL;
  }

  // call DVPP step2
  void *workspace_addr = nullptr;
  if (workspace_size > 0) {
    // create new device address for data copy
    workspace_addr = input->GetDeviceContext()->device_res_manager_->AllocateMemory(workspace_size);
    if (workspace_addr == nullptr) {
      MS_LOG(ERROR) << "Allocate dynamic workspace memory failed";
      return APP_ERR_DVPP_VERTICAL_FLIP_FAIL;
    }

    // call DVPP step3
    ret = acldvppVerticalFlip(
      workspace_addr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));

    // use the input to hold the workspace and release it when the executor / npu_map_job finish
    if (!input->AddWorkSpace(workspace_addr)) {
      MS_LOG(ERROR) << "Add workspace to the input failed";
      return APP_ERR_DVPP_VERTICAL_FLIP_FAIL;
    }
  } else {
    // call DVPP step3
    ret = acldvppVerticalFlip(
      nullptr, workspace_size, executor,
      static_cast<aclrtStream>(input->GetDeviceContext()->device_res_manager_->GetStream(input->GetStreamID())));
  }

  if (ret != ACL_SUCCESS) {
    MS_LOG(ERROR) << "Call acldvppVerticalFlip failed, error code: " + std::to_string(ret) + ".";
    return APP_ERR_DVPP_VERTICAL_FLIP_FAIL;
  }

  *output = std::move(device_tensor);  // currently the data is still in device
  return APP_ERR_OK;
}

// acl
APP_ERROR GetSocName(std::string *soc_name) {
  const char *soc_name_c = aclrtGetSocName();
  if (soc_name_c == nullptr) {
    *soc_name = "";
  }
  *soc_name = std::string(soc_name_c);
  return APP_ERR_OK;
}

APP_ERROR CreateAclTensor(const int64_t *view_dims, uint64_t view_dims_num, mindspore::TypeId data_type,
                          const int64_t *stride, int64_t offset, const int64_t *storage_dims, uint64_t storage_dims_num,
                          void *tensor_data, bool is_hwc, void **acl_tensor) {
  if (view_dims == nullptr) {
    MS_LOG(ERROR) << "Input view_dims is null.";
    return APP_ERR_COMM_FAILURE;
  }
  if (stride == nullptr) {
    MS_LOG(ERROR) << "Input stride is null.";
    return APP_ERR_COMM_FAILURE;
  }
  if (storage_dims == nullptr) {
    MS_LOG(ERROR) << "Input storage_dims is null.";
    return APP_ERR_COMM_FAILURE;
  }
  if (tensor_data == nullptr) {
    MS_LOG(ERROR) << "Input tensor_data is null.";
    return APP_ERR_COMM_FAILURE;
  }
  if (acl_tensor == nullptr) {
    MS_LOG(ERROR) << "Input acl_tensor is null.";
    return APP_ERR_COMM_FAILURE;
  }

  aclDataType acl_data_type = aclDataType::ACL_DT_UNDEFINED;

  switch (data_type) {
    case kNumberTypeBool:
      acl_data_type = aclDataType::ACL_BOOL;
      break;
    case kNumberTypeInt8:
      acl_data_type = aclDataType::ACL_INT8;
      break;
    case kNumberTypeUInt8:
      acl_data_type = aclDataType::ACL_UINT8;
      break;
    case kNumberTypeInt16:
      acl_data_type = aclDataType::ACL_INT16;
      break;
    case kNumberTypeUInt16:
      acl_data_type = aclDataType::ACL_UINT16;
      break;
    case kNumberTypeInt32:
      acl_data_type = aclDataType::ACL_INT32;
      break;
    case kNumberTypeUInt32:
      acl_data_type = aclDataType::ACL_UINT32;
      break;
    case kNumberTypeInt64:
      acl_data_type = aclDataType::ACL_INT64;
      break;
    case kNumberTypeUInt64:
      acl_data_type = aclDataType::ACL_UINT64;
      break;
    case kNumberTypeFloat16:
      acl_data_type = aclDataType::ACL_FLOAT16;
      break;
    case kNumberTypeFloat32:
      acl_data_type = aclDataType::ACL_FLOAT;
      break;
    case kNumberTypeFloat64:
      acl_data_type = aclDataType::ACL_DOUBLE;
      break;
    case kObjectTypeString:
      acl_data_type = aclDataType::ACL_STRING;
      break;
    default:
      acl_data_type = aclDataType::ACL_DT_UNDEFINED;
      break;
  }

  if (acl_data_type == aclDataType::ACL_DT_UNDEFINED) {
    MS_LOG(ERROR) << "Invalid data type: " << data_type << ", which couldn't be converted to aclDataType.";
    return APP_ERR_COMM_FAILURE;
  }

  if (is_hwc) {
    *acl_tensor = reinterpret_cast<void *>(aclCreateTensor(view_dims, view_dims_num, acl_data_type, stride, offset,
                                                           aclFormat::ACL_FORMAT_NHWC, storage_dims, storage_dims_num,
                                                           tensor_data));
  } else {
    *acl_tensor = reinterpret_cast<void *>(aclCreateTensor(view_dims, view_dims_num, acl_data_type, stride, offset,
                                                           aclFormat::ACL_FORMAT_NCHW, storage_dims, storage_dims_num,
                                                           tensor_data));
  }
  return APP_ERR_OK;
}

APP_ERROR DestroyTensor(void *tensor) {
  if (aclDestroyTensor(reinterpret_cast<aclTensor *>(tensor)) != OK) {
    MS_LOG(ERROR) << "Call aclDestroyTensor failed.";
    return APP_ERR_DESTORY_TENSOR;
  }
  return APP_ERR_OK;
}

APP_ERROR DestroyFloatArray(void *float_array) {
  if (aclDestroyFloatArray(reinterpret_cast<aclFloatArray *>(float_array)) != OK) {
    MS_LOG(ERROR) << "Call aclDestroyFloatArray failed.";
    return APP_ERR_DESTORY_FLOAT_ARRAY;
  }
  return APP_ERR_OK;
}

APP_ERROR DestroyIntArray(void *int_array) {
  if (aclDestroyIntArray(reinterpret_cast<aclIntArray *>(int_array)) != OK) {
    MS_LOG(ERROR) << "Call aclDestroyIntArray failed.";
    return APP_ERR_DESTORY_INT_ARRAY;
  }
  return APP_ERR_OK;
}
}  // namespace dataset
}  // namespace mindspore
