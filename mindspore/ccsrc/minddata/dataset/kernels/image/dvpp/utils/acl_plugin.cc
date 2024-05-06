/**
* Copyright 2022-2023 Huawei Technologies Co., Ltd
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at

* http://www.apache.org/licenses/LICENSE-2.0

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include "minddata/dataset/kernels/image/dvpp/utils/acl_plugin.h"

#include <string>

#include "minddata/dataset/kernels/image/dvpp/utils/dvpp_video.h"
#include "minddata/dataset/kernels/image/dvpp/utils/ResourceManager.h"
#include "minddata/dataset/kernels/image/dvpp/utils/MDAclProcess.h"
#if !defined(BUILD_LITE) && defined(ENABLE_D)
#include "minddata/dataset/kernels/image/dvpp/utils/dvpp_image_utils.h"
#include "minddata/dataset/core/device_tensor_ascend910b.h"
#endif
#include "minddata/dataset/include/dataset/constants.h"
#include "transform/symbol/acl_rt_symbol.h"
#include "transform/symbol/symbol_utils.h"

void *PluginCreateDvppVideo(aclrtContext context, uint8_t *data, uint32_t size, uint32_t width, uint32_t height,
                            uint32_t type, uint32_t out_format, const std::string &output) {
  return new DvppVideo(context, data, size, width, height, type, out_format, output);
}

AclLiteError PluginInitDvppVideo(void *dvpp_video) {
  if (dvpp_video == nullptr) {
    return ACLLITE_ERROR;
  }

  return static_cast<DvppVideo *>(dvpp_video)->Init();
}

AclLiteError PluginCloseDvppVideo(void *dvpp_video) {
  if (dvpp_video == nullptr) {
    return ACLLITE_ERROR;
  }

  auto ret = static_cast<DvppVideo *>(dvpp_video)->Close();
  delete static_cast<DvppVideo *>(dvpp_video);
  return ret;
}

AclLiteError PluginDvppVideoDumpFrame(void *dvpp_video) {
  if (dvpp_video == nullptr) {
    return ACLLITE_ERROR;
  }

  return static_cast<DvppVideo *>(dvpp_video)->DumpFrame();
}

APP_ERROR PluginInitResource(ResourceInfo &resource_info) {
  if (ResourceManager::GetInstance() == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return ResourceManager::GetInstance()->InitResource(resource_info);
}

aclrtContext PluginGetContext(int device_id) {
  if (ResourceManager::GetInstance() == nullptr) {
    return nullptr;
  }
  return ResourceManager::GetInstance()->GetContext(device_id);
}

void PluginRelease() {
  if (ResourceManager::GetInstance() != nullptr) {
    return ResourceManager::GetInstance()->Release();
  }
}

void *PluginCreateAclProcessWithResize(uint32_t resize_width, uint32_t resize_height, uint32_t crop_width,
                                       uint32_t crop_height, aclrtContext context, bool is_crop, aclrtStream stream,
                                       const std::shared_ptr<DvppCommon> &dvpp_common) {
  return new MDAclProcess(resize_width, resize_height, crop_width, crop_height, context, is_crop, stream, dvpp_common);
}

void *PluginCreateAclProcessWithPara(uint32_t para_width, uint32_t para_height, aclrtContext context, bool is_crop,
                                     aclrtStream stream, const std::shared_ptr<DvppCommon> &dvpp_common) {
  return new MDAclProcess(para_width, para_height, context, is_crop, stream, dvpp_common);
}

void *PluginCreateAclProcess(aclrtContext context, bool is_crop, aclrtStream stream,
                             const std::shared_ptr<DvppCommon> &dvpp_common) {
  return new MDAclProcess(context, is_crop, stream, dvpp_common);
}

void PluginDestroyAclProcess(void *acl_process) {
  if (acl_process != nullptr) {
    delete static_cast<MDAclProcess *>(acl_process);
  }
}

APP_ERROR PluginReleaseAclProcess(void *acl_process) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->Release();
}

APP_ERROR PluginInitAclProcess(void *acl_process) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->InitResource();
}

aclrtContext PluginGetContextFromAclProcess(void *acl_process) {
  if (acl_process == nullptr) {
    return nullptr;
  }
  return static_cast<MDAclProcess *>(acl_process)->GetContext();
}

aclrtStream PluginGetStreamFromAclProcess(void *acl_process) {
  if (acl_process == nullptr) {
    return nullptr;
  }
  return static_cast<MDAclProcess *>(acl_process)->GetStream();
}

APP_ERROR PluginJPEG_DRC_WITH_DATA(void *acl_process, const RawData &data) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->JPEG_DRC(data);
}

APP_ERROR PluginJPEG_DR_WITH_DATA(void *acl_process, const RawData &data) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->JPEG_DR(data);
}

APP_ERROR PluginJPEG_D_WITH_DATA(void *acl_process, const RawData &data) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->JPEG_D(data);
}

APP_ERROR PluginJPEG_R_WITH_DATA(void *acl_process, const DvppDataInfo &data) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->JPEG_R(data);
}

APP_ERROR PluginJPEG_C_WITH_DATA(void *acl_process, const DvppDataInfo &data) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->JPEG_C(data);
}

APP_ERROR PluginPNG_D_WITH_DATA(void *acl_process, const RawData &data) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->PNG_D(data);
}

APP_ERROR PluginJPEG_DRC(void *acl_process) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->JPEG_DRC();
}

APP_ERROR PluginJPEG_DR(void *acl_process) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->JPEG_DR();
}

APP_ERROR PluginJPEG_D(void *acl_process) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->JPEG_D();
}

APP_ERROR PluginJPEG_R(void *acl_process, const std::string &last_step) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->JPEG_R(last_step);
}

APP_ERROR PluginJPEG_C(void *acl_process, const std::string &last_step) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->JPEG_C(last_step);
}

APP_ERROR PluginPNG_D(void *acl_process) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->PNG_D();
}

void *PluginGetMemoryData(void *acl_process) {
  if (acl_process == nullptr) {
    return nullptr;
  }
  return static_cast<MDAclProcess *>(acl_process)->Get_Memory_Data().get();
}

DvppDataInfo *PluginGetCropedDeviceData(void *acl_process) {
  if (acl_process == nullptr) {
    return nullptr;
  }
  return static_cast<MDAclProcess *>(acl_process)->Get_Croped_DeviceData().get();
}

DvppDataInfo *PluginGetResizedDeviceData(void *acl_process) {
  if (acl_process == nullptr) {
    return nullptr;
  }
  return static_cast<MDAclProcess *>(acl_process)->Get_Resized_DeviceData().get();
}

DvppDataInfo *PluginGetDecodeDeviceData(void *acl_process) {
  if (acl_process == nullptr) {
    return nullptr;
  }
  return static_cast<MDAclProcess *>(acl_process)->Get_Decode_DeviceData().get();
}

APP_ERROR PluginH2D_Sink(void *acl_process, const std::shared_ptr<mindspore::dataset::Tensor> &input,
                         std::shared_ptr<mindspore::dataset::DeviceTensor> &device_input) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->H2D_Sink(input, device_input);
}

APP_ERROR PluginD2H_Pop(void *acl_process, const std::shared_ptr<mindspore::dataset::DeviceTensor> &device_output,
                        std::shared_ptr<mindspore::dataset::Tensor> &output) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->D2H_Pop(device_output, output);
}

APP_ERROR PluginDeviceMemoryRelease(void *acl_process) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->device_memory_release();
}

APP_ERROR PluginSetResizeParas(void *acl_process, uint32_t width, uint32_t height) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->SetResizeParas(width, height);
}

APP_ERROR PluginSetCropParas(void *acl_process, uint32_t width, uint32_t height) {
  if (acl_process == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return static_cast<MDAclProcess *>(acl_process)->SetCropParas(width, height);
}

int PluginaclrtMemcpy(void *dst, size_t dest_max, const void *src, size_t count, int kind) {
  return CALL_ASCEND_API(aclrtMemcpy, dst, dest_max, src, count, static_cast<aclrtMemcpyKind>(kind));
}

#if !defined(BUILD_LITE) && defined(ENABLE_D)
// Ascend910B
APP_ERROR PluginDvppAdjustBrightness(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                                     std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output,
                                     float factor) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppAdjustBrightness(input, output, factor);
}

APP_ERROR PluginDvppAdjustContrast(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                                   std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output, float factor) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppAdjustContrast(input, output, factor);
}

APP_ERROR PluginDvppAdjustHue(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                              std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output, float factor) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppAdjustHue(input, output, factor);
}

APP_ERROR PluginDvppAdjustSaturation(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                                     std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output,
                                     float factor) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppAdjustSaturation(input, output, factor);
}

APP_ERROR PluginDvppAffine(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                           std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output,
                           const std::vector<float> &matrix, uint32_t interpolation_mode, uint32_t padding_mode,
                           const std::vector<float> &fill) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppAffine(input, output, matrix, interpolation_mode, padding_mode, fill);
}

APP_ERROR PluginDvppCrop(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                         std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output, uint32_t top,
                         uint32_t left, uint32_t height, uint32_t width) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppCrop(input, output, top, left, height, width);
}

APP_ERROR PluginDvppDecode(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                           std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppDecode(input, output);
}

APP_ERROR PluginDvppGaussianBlur(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                                 std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output,
                                 const std::vector<int64_t> &kernel_size, const std::vector<float> &sigma,
                                 uint32_t padding_mode) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppGaussianBlur(input, output, kernel_size, sigma, padding_mode);
}

APP_ERROR PluginDvppHorizontalFlip(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                                   std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppHorizontalFlip(input, output);
}

APP_ERROR PluginDvppNormalize(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                              std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output,
                              std::vector<float> mean, std::vector<float> std, bool is_hwc) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppNormalize(input, output, mean, std, is_hwc);
}

APP_ERROR PluginDvppPad(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                        std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output,
                        const std::vector<int64_t> &padding, uint32_t padding_mode, const std::vector<float> &fill) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppPad(input, output, padding, padding_mode, fill);
}

APP_ERROR PluginDvppPerspective(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                                std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output,
                                const std::vector<std::vector<int32_t>> &start_points,
                                const std::vector<std::vector<int32_t>> &end_points,
                                mindspore::dataset::InterpolationMode interpolation) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppPerspective(input, output, start_points, end_points, interpolation);
}

APP_ERROR PluginDvppResize(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                           std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output, int32_t output_height,
                           int32_t output_width, double fx, double fy, mindspore::dataset::InterpolationMode mode) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppResize(input, output, output_height, output_width, fx, fy, mode);
}

APP_ERROR PluginDvppResizedCrop(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                                std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output, int32_t top,
                                int32_t left, int32_t height, int32_t width, int32_t output_height,
                                int32_t output_width, mindspore::dataset::InterpolationMode mode) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppResizedCrop(input, output, top, left, height, width, output_height, output_width, mode);
}

APP_ERROR PluginDvppVerticalFlip(const std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> &input,
                                 std::shared_ptr<mindspore::dataset::DeviceTensorAscend910B> *output) {
  if (input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  if (output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return DvppVerticalFlip(input, output);
}

// acl
APP_ERROR PluginGetSocName(std::string *soc_name) { return mindspore::dataset::GetSocName(soc_name); }

APP_ERROR PluginCreateAclTensor(const int64_t *view_dims, uint64_t view_dims_num, mindspore::TypeId data_type,
                                const int64_t *stride, int64_t offset, const int64_t *storage_dims,
                                uint64_t storage_dims_num, void *tensor_data, bool is_hwc, void **acl_tensor) {
  if (view_dims == nullptr) {
    MS_LOG(ERROR) << "Input view_dims is null.";
    return APP_ERR_ACL_FAILURE;
  }
  if (stride == nullptr) {
    MS_LOG(ERROR) << "Input stride is null.";
    return APP_ERR_ACL_FAILURE;
  }
  if (storage_dims == nullptr) {
    MS_LOG(ERROR) << "Input storage_dims is null.";
    return APP_ERR_ACL_FAILURE;
  }
  if (tensor_data == nullptr) {
    MS_LOG(ERROR) << "Input tensor_data is null.";
    return APP_ERR_ACL_FAILURE;
  }
  if (acl_tensor == nullptr) {
    MS_LOG(ERROR) << "Input acl_tensor is null.";
    return APP_ERR_ACL_FAILURE;
  }

  return mindspore::dataset::CreateAclTensor(view_dims, view_dims_num, data_type, stride, offset, storage_dims,
                                             storage_dims_num, tensor_data, is_hwc, acl_tensor);
}

APP_ERROR PluginDestroyTensor(void *tensor) {
  if (tensor == nullptr) {
    MS_LOG(ERROR) << "Input tensor is null.";
    return APP_ERR_ACL_FAILURE;
  }

  return mindspore::dataset::DestroyTensor(tensor);
}

APP_ERROR PluginDestroyFloatArray(void *float_array) {
  if (float_array == nullptr) {
    MS_LOG(ERROR) << "Input float_array is null.";
    return APP_ERR_ACL_FAILURE;
  }

  return mindspore::dataset::DestroyFloatArray(float_array);
}

APP_ERROR PluginDestroyIntArray(void *int_array) {
  if (int_array == nullptr) {
    MS_LOG(ERROR) << "Input int_array is null.";
    return APP_ERR_ACL_FAILURE;
  }

  return mindspore::dataset::DestroyIntArray(int_array);
}
#endif
