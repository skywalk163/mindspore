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

#include "minddata/dataset/kernels/image/dvpp/ascend310/dvpp_resize_jpeg_op.h"

#include <string>
#include <vector>

#include "minddata/dataset/core/cv_tensor.h"
#include "minddata/dataset/core/data_type.h"
#include "minddata/dataset/core/device_tensor.h"
#include "minddata/dataset/kernels/image/dvpp/utils/CommonDataType.h"
#include "minddata/dataset/kernels/image/image_utils.h"

namespace mindspore {
namespace dataset {
Status DvppResizeJpegOp::Compute(const std::shared_ptr<DeviceTensor> &input, std::shared_ptr<DeviceTensor> *output) {
  IO_CHECK(input, output);
  try {
    CHECK_FAIL_RETURN_UNEXPECTED(input->GetDeviceBuffer() != nullptr, "The input image buffer is empty.");
    std::string last_step = "Decode";
    DvppDataInfo *imageinfo(AclAdapter::GetInstance().GetDecodeDeviceData(processor_.get()));
    if (!imageinfo->data) {
      last_step = "Crop";
    }
    APP_ERROR ret = AclAdapter::GetInstance().JPEG_R(processor_.get(), last_step);
    if (ret != APP_ERR_OK) {
      ret = AclAdapter::GetInstance().ReleaseAclProcess(processor_.get());
      CHECK_FAIL_RETURN_UNEXPECTED(ret == APP_ERR_OK, "Release memory failed.");
      std::string error = "Error in dvpp processing: " + std::to_string(ret);
      RETURN_STATUS_UNEXPECTED(error);
    }
    DvppDataInfo *ResizeOut(AclAdapter::GetInstance().GetResizedDeviceData(processor_.get()));
    const TensorShape dvpp_shape({1, 1, 1});
    const DataType dvpp_data_type(DataType::DE_UINT8);
    RETURN_IF_NOT_OK(mindspore::dataset::DeviceTensor::CreateEmpty(dvpp_shape, dvpp_data_type, output));
    RETURN_IF_NOT_OK((*output)->SetAttributes(ResizeOut->data, ResizeOut->dataSize, ResizeOut->width,
                                              ResizeOut->widthStride, ResizeOut->height, ResizeOut->heightStride));
    if (!((*output)->HasDeviceData())) {
      std::string error = "[ERROR] Fail to get the Output result from device memory!";
      RETURN_STATUS_UNEXPECTED(error);
    }
  } catch (const std::exception &e) {
    std::string error = "[ERROR] Fail in DvppResizeJpegOp: " + std::string(e.what());
    RETURN_STATUS_UNEXPECTED(error);
  }
  return Status::OK();
}

Status DvppResizeJpegOp::Compute(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output) {
  IO_CHECK(input, output);
  if (!IsNonEmptyJPEG(input)) {
    RETURN_STATUS_UNEXPECTED("DvppReiszeJpegOp only support process jpeg image.");
  }
  try {
    CHECK_FAIL_RETURN_UNEXPECTED(input->GetBuffer() != nullptr, "The input image buffer is empty.");
    auto *buffer = const_cast<unsigned char *>(input->GetBuffer());
    DvppDataInfo imageinfo;
    imageinfo.dataSize = input->SizeInBytes();
    imageinfo.data = static_cast<uint8_t *>(buffer);
    std::vector<uint32_t> yuv_shape_ = input->GetYuvShape();
    imageinfo.width = yuv_shape_[0];
    imageinfo.widthStride = yuv_shape_[1];
    imageinfo.height = yuv_shape_[2];
    imageinfo.heightStride = yuv_shape_[3];
    imageinfo.format = 1;  // 1 means PIXEL_FORMAT_YUV_SEMIPLANAR_420
    ResourceInfo resource;
    resource.deviceIds.insert(0);
    APP_ERROR ret = AclAdapter::GetInstance().InitResource(&resource);
    if (ret != APP_ERR_OK) {
      AclAdapter::GetInstance().Release();
      std::string error = "Error in Init D-chip: " + std::to_string(ret);
      RETURN_STATUS_UNEXPECTED(error);
    }
    int deviceId = *(resource.deviceIds.begin());
    void *context = AclAdapter::GetInstance().GetContext(deviceId);
    // Second part end where we initialize the resource of D-chip and set up all configures
    std::shared_ptr<void> process(AclAdapter::GetInstance().CreateAclProcessWithPara(resized_width_, resized_height_,
                                                                                     context, false, nullptr, nullptr),
                                  [](void *ptr) { AclAdapter::GetInstance().DestroyAclProcess(ptr); });

    ret = AclAdapter::GetInstance().InitAclProcess(process.get());
    if (ret != APP_ERR_OK) {
      AclAdapter::GetInstance().Release();
      std::string error = "Error in Init resource: " + std::to_string(ret);
      RETURN_STATUS_UNEXPECTED(error);
    }

    ret = AclAdapter::GetInstance().JPEG_R_WITH_DATA(process.get(), imageinfo);
    if (ret != APP_ERR_OK) {
      AclAdapter::GetInstance().Release();
      std::string error = "Error in dvpp processing: " + std::to_string(ret);
      RETURN_STATUS_UNEXPECTED(error);
    }

    // Third part end where we execute the core function of dvpp
    auto *ret_ptr = static_cast<unsigned char *>(AclAdapter::GetInstance().GetMemoryData(process.get()));
    DvppDataInfo *ResizeOut = AclAdapter::GetInstance().GetResizedDeviceData(process.get());
    dsize_t dvpp_length = ResizeOut->dataSize;
    const TensorShape dvpp_shape({dvpp_length, 1, 1});
    uint32_t resized_height = ResizeOut->height;
    uint32_t resized_heightStride = ResizeOut->heightStride;
    uint32_t resized_width = ResizeOut->width;
    uint32_t resized_widthStride = ResizeOut->widthStride;
    const DataType dvpp_data_type(DataType::DE_UINT8);
    RETURN_IF_NOT_OK(mindspore::dataset::Tensor::CreateFromMemory(dvpp_shape, dvpp_data_type, ret_ptr, output));
    RETURN_IF_NOT_OK((*output)->SetYuvShape(resized_width, resized_widthStride, resized_height, resized_heightStride));
    if (!((*output)->HasData())) {
      std::string error = "[ERROR] Fail to get the Output result from memory!";
      RETURN_STATUS_UNEXPECTED(error);
    }
    ret = AclAdapter::GetInstance().DeviceMemoryRelease(process.get());
    CHECK_FAIL_RETURN_UNEXPECTED(ret == APP_ERR_OK, "Release device memory failed.");
    ret = AclAdapter::GetInstance().ReleaseAclProcess(process.get());
    CHECK_FAIL_RETURN_UNEXPECTED(ret == APP_ERR_OK, "Release host memory failed.");
    // Last part end where we transform the processed data into a tensor which can be applied in later units.
  } catch (const std::exception &e) {
    std::string error = "[ERROR] Fail in DvppResizeJpegOp: " + std::string(e.what());
    RETURN_STATUS_UNEXPECTED(error);
  }
  return Status::OK();
}

Status DvppResizeJpegOp::SetAscendResource(const std::shared_ptr<DeviceResource> &resource) {
  processor_ = resource->GetInstance();
  if (!processor_) {
    RETURN_STATUS_UNEXPECTED("Resource initialize fail, please check your env.");
  }
  APP_ERROR ret = AclAdapter::GetInstance().SetResizeParas(processor_.get(), resized_width_, resized_height_);
  CHECK_FAIL_RETURN_UNEXPECTED(ret == APP_ERR_OK, "SetResizeParas failed.");
  return Status::OK();
}

Status DvppResizeJpegOp::OutputShape(const std::vector<TensorShape> &inputs, std::vector<TensorShape> &outputs) {
  RETURN_IF_NOT_OK(TensorOp::OutputShape(inputs, outputs));
  outputs.clear();
  TensorShape out({-1, 1, 1});  // we don't know what is output image size, but we know it should be 1 channels
  CHECK_FAIL_RETURN_UNEXPECTED(!inputs.empty(), "DvppResizeJpeg: inputs cannot be empty.");
  if (inputs[0].Rank() == 1) {
    (void)outputs.emplace_back(out);
  }
  CHECK_FAIL_RETURN_UNEXPECTED(!outputs.empty(), "DvppResizeJpeg: Invalid input shape.");
  return Status::OK();
}
}  // namespace dataset
}  // namespace mindspore
