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
#include "minddata/dataset/kernels/image/dvpp/acl_adapter.h"

#if !defined(_WIN32) && !defined(_WIN64)
#include <libgen.h>
#endif

#include <algorithm>

#include "utils/ms_context.h"

namespace mindspore {
namespace dataset {
namespace {
#if defined(BUILD_LITE)
constexpr auto kAclPluginRelatedPath = "./libdvpp_utils.so";
#else
constexpr auto kAclPluginRelatedPath = "./lib/plugin/ascend/libdvpp_utils.so";
#endif
}  // namespace
AclAdapter &AclAdapter::GetInstance() {
  static AclAdapter instance{};
  static std::once_flag flag;
  std::call_once(flag, []() { instance.InitPlugin(); });
  return instance;
}

void AclAdapter::InitPlugin() {
  if (plugin_handle_ != nullptr) {
    return;
  }
#if !defined(ENABLE_ACL) || defined(ENABLE_D)
  // 310.tar.gz skip this check
  if (MsContext::GetInstance() != nullptr && !MsContext::GetInstance()->IsAscendPluginLoaded()) {
    return;
  }
#endif
#if !defined(_WIN32) && !defined(_WIN64)
  Dl_info dl_info;
  if (dladdr(reinterpret_cast<void *>(AclAdapter::GetInstance), &dl_info) == 0) {
    MS_LOG(INFO) << "Get dladdr error";
    return;
  }
  std::string cur_so_path = dl_info.dli_fname;
  std::string acl_plugin_path = std::string(dirname(cur_so_path.data())) + "/" + kAclPluginRelatedPath;

  plugin_handle_ = dlopen(acl_plugin_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
  if (plugin_handle_ == nullptr) {
    MS_LOG(INFO) << "Cannot dlopen " << acl_plugin_path << ", result = " << GetDlErrorMsg()
                 << ", it can be ignored if not running on ascend.";
    return;
  }

  create_dvpp_video_fun_obj_ = DlsymFuncObj(CreateDvppVideo, plugin_handle_);
  init_dvpp_video_fun_obj_ = DlsymFuncObj(InitDvppVideo, plugin_handle_);
  close_dvpp_video_fun_obj_ = DlsymFuncObj(CloseDvppVideo, plugin_handle_);
  dvpp_video_dump_frame_fun_obj_ = DlsymFuncObj(DvppVideoDumpFrame, plugin_handle_);
  init_resource_fun_obj_ = DlsymFuncObj(InitResource, plugin_handle_);
  get_context_fun_obj_ = DlsymFuncObj(GetContext, plugin_handle_);
  release_fun_obj_ = DlsymFuncObj(Release, plugin_handle_);
  create_acl_process_with_resize_fun_obj_ = DlsymFuncObj(CreateAclProcessWithResize, plugin_handle_);
  create_acl_process_with_para_fun_obj_ = DlsymFuncObj(CreateAclProcessWithPara, plugin_handle_);
  create_acl_process_fun_obj_ = DlsymFuncObj(CreateAclProcess, plugin_handle_);
  destroy_acl_process_fun_obj_ = DlsymFuncObj(DestroyAclProcess, plugin_handle_);
  release_acl_process_fun_obj_ = DlsymFuncObj(ReleaseAclProcess, plugin_handle_);
  init_acl_process_fun_obj_ = DlsymFuncObj(InitAclProcess, plugin_handle_);
  get_context_from_acl_process_fun_obj_ = DlsymFuncObj(GetContextFromAclProcess, plugin_handle_);
  get_stream_from_acl_process_fun_obj_ = DlsymFuncObj(GetStreamFromAclProcess, plugin_handle_);
  jpeg_drc_with_data_fun_obj_ = DlsymFuncObj(JPEG_DRC_WITH_DATA, plugin_handle_);
  jpeg_dr_with_data_fun_obj_ = DlsymFuncObj(JPEG_DR_WITH_DATA, plugin_handle_);
  jpeg_d_with_data_fun_obj_ = DlsymFuncObj(JPEG_D_WITH_DATA, plugin_handle_);
  jpeg_r_with_data_fun_obj_ = DlsymFuncObj(JPEG_R_WITH_DATA, plugin_handle_);
  jpeg_c_with_data_fun_obj_ = DlsymFuncObj(JPEG_C_WITH_DATA, plugin_handle_);
  png_d_with_data_fun_obj_ = DlsymFuncObj(PNG_D_WITH_DATA, plugin_handle_);
  jpeg_drc_fun_obj_ = DlsymFuncObj(JPEG_DRC, plugin_handle_);
  jpeg_dr_fun_obj_ = DlsymFuncObj(JPEG_DR, plugin_handle_);
  jpeg_d_fun_obj_ = DlsymFuncObj(JPEG_D, plugin_handle_);
  jpeg_r_fun_obj_ = DlsymFuncObj(JPEG_R, plugin_handle_);
  jpeg_c_fun_obj_ = DlsymFuncObj(JPEG_C, plugin_handle_);
  png_d_fun_obj_ = DlsymFuncObj(PNG_D, plugin_handle_);
  get_memory_data_fun_obj_ = DlsymFuncObj(GetMemoryData, plugin_handle_);
  get_croped_device_data_fun_obj_ = DlsymFuncObj(GetCropedDeviceData, plugin_handle_);
  get_resized_device_data_fun_obj_ = DlsymFuncObj(GetResizedDeviceData, plugin_handle_);
  get_decode_device_data_fun_obj_ = DlsymFuncObj(GetDecodeDeviceData, plugin_handle_);
  h_2_d_sink_fun_obj_ = DlsymFuncObj(H2D_Sink, plugin_handle_);
  d_2_h_pop_fun_obj_ = DlsymFuncObj(D2H_Pop, plugin_handle_);
  device_memory_release_fun_obj_ = DlsymFuncObj(DeviceMemoryRelease, plugin_handle_);
  set_resize_paras_fun_obj_ = DlsymFuncObj(SetResizeParas, plugin_handle_);
  set_crop_paras_fun_obj_ = DlsymFuncObj(SetCropParas, plugin_handle_);
  aclrt_malloc_host_fun_obj_ = DlsymFuncObj(aclrtMallocHost, plugin_handle_);
  aclrt_free_host_fun_obj_ = DlsymFuncObj(aclrtFreeHost, plugin_handle_);
  aclrt_memcpy_fun_obj_ = DlsymFuncObj(aclrtMemcpy, plugin_handle_);
#if !defined(BUILD_LITE) && defined(ENABLE_D)
  // Ascend910B
  dvpp_affine_fun_obj_ = DlsymFuncObj(DvppAffine, plugin_handle_);
  dvpp_brightness_fun_obj_ = DlsymFuncObj(DvppAdjustBrightness, plugin_handle_);
  dvpp_contrast_fun_obj_ = DlsymFuncObj(DvppAdjustContrast, plugin_handle_);
  dvpp_crop_fun_obj_ = DlsymFuncObj(DvppCrop, plugin_handle_);
  dvpp_decode_fun_obj_ = DlsymFuncObj(DvppDecode, plugin_handle_);
  dvpp_gaussian_blur_fun_obj_ = DlsymFuncObj(DvppGaussianBlur, plugin_handle_);
  dvpp_horizontal_flip_fun_obj_ = DlsymFuncObj(DvppHorizontalFlip, plugin_handle_);
  dvpp_hue_fun_obj_ = DlsymFuncObj(DvppAdjustHue, plugin_handle_);
  dvpp_normalize_fun_obj_ = DlsymFuncObj(DvppNormalize, plugin_handle_);
  dvpp_pad_fun_obj_ = DlsymFuncObj(DvppPad, plugin_handle_);
  dvpp_perspective_fun_obj_ = DlsymFuncObj(DvppPerspective, plugin_handle_);
  dvpp_resize_fun_obj_ = DlsymFuncObj(DvppResize, plugin_handle_);
  dvpp_resized_crop_fun_obj_ = DlsymFuncObj(DvppResizedCrop, plugin_handle_);
  dvpp_saturation_fun_obj_ = DlsymFuncObj(DvppAdjustSaturation, plugin_handle_);
  dvpp_vertical_flip_fun_obj_ = DlsymFuncObj(DvppVerticalFlip, plugin_handle_);

  // acl
  get_soc_name_fun_obj_ = DlsymFuncObj(GetSocName, plugin_handle_);
  create_acl_tensor_fun_obj_ = DlsymFuncObj(CreateAclTensor, plugin_handle_);
  destroy_tensor_fun_obj_ = DlsymFuncObj(DestroyTensor, plugin_handle_);
  destroy_float_array_fun_obj_ = DlsymFuncObj(DestroyFloatArray, plugin_handle_);
  destroy_int_array_fun_obj_ = DlsymFuncObj(DestroyIntArray, plugin_handle_);
#endif
#endif
}

void AclAdapter::FinalizePlugin() {
  if (plugin_handle_ == nullptr) {
    return;
  }

  create_dvpp_video_fun_obj_ = nullptr;
  init_dvpp_video_fun_obj_ = nullptr;
  close_dvpp_video_fun_obj_ = nullptr;
  dvpp_video_dump_frame_fun_obj_ = nullptr;
  init_resource_fun_obj_ = nullptr;
  get_context_fun_obj_ = nullptr;
  release_fun_obj_ = nullptr;
  create_acl_process_with_resize_fun_obj_ = nullptr;
  create_acl_process_with_para_fun_obj_ = nullptr;
  create_acl_process_fun_obj_ = nullptr;
  destroy_acl_process_fun_obj_ = nullptr;
  release_acl_process_fun_obj_ = nullptr;
  init_acl_process_fun_obj_ = nullptr;
  get_context_from_acl_process_fun_obj_ = nullptr;
  get_stream_from_acl_process_fun_obj_ = nullptr;
  jpeg_drc_with_data_fun_obj_ = nullptr;
  jpeg_dr_with_data_fun_obj_ = nullptr;
  jpeg_d_with_data_fun_obj_ = nullptr;
  jpeg_r_with_data_fun_obj_ = nullptr;
  jpeg_c_with_data_fun_obj_ = nullptr;
  png_d_with_data_fun_obj_ = nullptr;
  jpeg_drc_fun_obj_ = nullptr;
  jpeg_dr_fun_obj_ = nullptr;
  jpeg_d_fun_obj_ = nullptr;
  jpeg_r_fun_obj_ = nullptr;
  jpeg_c_fun_obj_ = nullptr;
  png_d_fun_obj_ = nullptr;
  get_memory_data_fun_obj_ = nullptr;
  get_croped_device_data_fun_obj_ = nullptr;
  get_resized_device_data_fun_obj_ = nullptr;
  get_decode_device_data_fun_obj_ = nullptr;
  h_2_d_sink_fun_obj_ = nullptr;
  d_2_h_pop_fun_obj_ = nullptr;
  device_memory_release_fun_obj_ = nullptr;
  set_resize_paras_fun_obj_ = nullptr;
  set_crop_paras_fun_obj_ = nullptr;
  aclrt_malloc_host_fun_obj_ = nullptr;
  aclrt_free_host_fun_obj_ = nullptr;
  aclrt_memcpy_fun_obj_ = nullptr;
#if !defined(BUILD_LITE) && defined(ENABLE_D)
  // Ascend910B
  dvpp_affine_fun_obj_ = nullptr;
  dvpp_brightness_fun_obj_ = nullptr;
  dvpp_contrast_fun_obj_ = nullptr;
  dvpp_crop_fun_obj_ = nullptr;
  dvpp_decode_fun_obj_ = nullptr;
  dvpp_gaussian_blur_fun_obj_ = nullptr;
  dvpp_horizontal_flip_fun_obj_ = nullptr;
  dvpp_hue_fun_obj_ = nullptr;
  dvpp_normalize_fun_obj_ = nullptr;
  dvpp_pad_fun_obj_ = nullptr;
  dvpp_perspective_fun_obj_ = nullptr;
  dvpp_resize_fun_obj_ = nullptr;
  dvpp_resized_crop_fun_obj_ = nullptr;
  dvpp_saturation_fun_obj_ = nullptr;
  dvpp_vertical_flip_fun_obj_ = nullptr;

  // acl
  get_soc_name_fun_obj_ = nullptr;
  create_acl_tensor_fun_obj_ = nullptr;
  destroy_tensor_fun_obj_ = nullptr;
  destroy_float_array_fun_obj_ = nullptr;
  destroy_int_array_fun_obj_ = nullptr;
#endif
#if !defined(_WIN32) && !defined(_WIN64)
  (void)dlclose(plugin_handle_);
#endif
  plugin_handle_ = nullptr;
}

void *AclAdapter::CreateDvppVideo(void *context, uint8_t *data, uint32_t size, uint32_t width, uint32_t height,
                                  uint32_t type, uint32_t out_format, const std::string &output) const {
  if (!HasAclPlugin() || create_dvpp_video_fun_obj_ == nullptr) {
    return nullptr;
  }
  return create_dvpp_video_fun_obj_(context, data, size, width, height, type, out_format, output);
}

AclLiteError AclAdapter::InitDvppVideo(void *dvpp_video) const {
  if (!HasAclPlugin() || init_dvpp_video_fun_obj_ == nullptr) {
    return ACLLITE_ERROR;
  }
  return init_dvpp_video_fun_obj_(dvpp_video);
}

AclLiteError AclAdapter::CloseDvppVideo(void *dvpp_video) const {
  if (!HasAclPlugin() || close_dvpp_video_fun_obj_ == nullptr) {
    return ACLLITE_ERROR;
  }
  return close_dvpp_video_fun_obj_(dvpp_video);
}

AclLiteError AclAdapter::DvppVideoDumpFrame(void *dvpp_video) const {
  if (!HasAclPlugin() || dvpp_video_dump_frame_fun_obj_ == nullptr) {
    return ACLLITE_ERROR;
  }
  return dvpp_video_dump_frame_fun_obj_(dvpp_video);
}

APP_ERROR AclAdapter::InitResource(ResourceInfo *resource_info) const {
  if (!HasAclPlugin() || init_resource_fun_obj_ == nullptr || resource_info == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return init_resource_fun_obj_(*resource_info);
}

void *AclAdapter::GetContext(int device_id) const {
  if (!HasAclPlugin() || get_context_fun_obj_ == nullptr) {
    return nullptr;
  }
  return get_context_fun_obj_(device_id);
}

void AclAdapter::Release() const {
  if (!HasAclPlugin() || release_fun_obj_ == nullptr) {
    return;
  }
  release_fun_obj_();
}

void *AclAdapter::CreateAclProcessWithResize(uint32_t resize_width, uint32_t resize_height, uint32_t crop_width,
                                             uint32_t crop_height, void *context, bool is_crop, void *stream,
                                             const std::shared_ptr<DvppCommon> &dvpp_common) const {
  if (!HasAclPlugin() || create_acl_process_with_resize_fun_obj_ == nullptr) {
    return nullptr;
  }
  return create_acl_process_with_resize_fun_obj_(resize_width, resize_height, crop_width, crop_height, context, is_crop,
                                                 stream, dvpp_common);
}

void *AclAdapter::CreateAclProcessWithPara(uint32_t para_width, uint32_t para_height, void *context, bool is_crop,
                                           void *stream, const std::shared_ptr<DvppCommon> &dvpp_common) const {
  if (!HasAclPlugin() || create_acl_process_with_para_fun_obj_ == nullptr) {
    return nullptr;
  }
  return create_acl_process_with_para_fun_obj_(para_width, para_height, context, is_crop, stream, dvpp_common);
}

void *AclAdapter::CreateAclProcess(void *context, bool is_crop, void *stream,
                                   const std::shared_ptr<DvppCommon> &dvpp_common) const {
  if (!HasAclPlugin() || create_acl_process_fun_obj_ == nullptr) {
    return nullptr;
  }
  return create_acl_process_fun_obj_(context, is_crop, stream, dvpp_common);
}

void AclAdapter::DestroyAclProcess(void *acl_process) const {
  if (!HasAclPlugin() || destroy_acl_process_fun_obj_ == nullptr) {
    return;
  }
  destroy_acl_process_fun_obj_(acl_process);
}

APP_ERROR AclAdapter::ReleaseAclProcess(void *acl_process) const {
  if (!HasAclPlugin() || release_acl_process_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return release_acl_process_fun_obj_(acl_process);
}

APP_ERROR AclAdapter::InitAclProcess(void *acl_process) const {
  if (!HasAclPlugin() || init_acl_process_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return init_acl_process_fun_obj_(acl_process);
}

void *AclAdapter::GetContextFromAclProcess(void *acl_process) const {
  if (!HasAclPlugin() || get_context_from_acl_process_fun_obj_ == nullptr) {
    return nullptr;
  }
  return get_context_from_acl_process_fun_obj_(acl_process);
}

void *AclAdapter::GetStreamFromAclProcess(void *acl_process) const {
  if (!HasAclPlugin() || get_stream_from_acl_process_fun_obj_ == nullptr) {
    return nullptr;
  }
  return get_stream_from_acl_process_fun_obj_(acl_process);
}

APP_ERROR AclAdapter::JPEG_DRC_WITH_DATA(void *acl_process, const RawData &data) const {
  if (!HasAclPlugin() || jpeg_drc_with_data_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return jpeg_drc_with_data_fun_obj_(acl_process, data);
}

APP_ERROR AclAdapter::JPEG_DR_WITH_DATA(void *acl_process, const RawData &data) const {
  if (!HasAclPlugin() || jpeg_dr_with_data_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return jpeg_dr_with_data_fun_obj_(acl_process, data);
}

APP_ERROR AclAdapter::JPEG_D_WITH_DATA(void *acl_process, const RawData &data) const {
  if (!HasAclPlugin() || jpeg_d_with_data_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return jpeg_d_with_data_fun_obj_(acl_process, data);
}

APP_ERROR AclAdapter::JPEG_R_WITH_DATA(void *acl_process, const DvppDataInfo &data) const {
  if (!HasAclPlugin() || jpeg_r_with_data_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return jpeg_r_with_data_fun_obj_(acl_process, data);
}

APP_ERROR AclAdapter::JPEG_C_WITH_DATA(void *acl_process, const DvppDataInfo &data) const {
  if (!HasAclPlugin() || jpeg_c_with_data_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return jpeg_c_with_data_fun_obj_(acl_process, data);
}

APP_ERROR AclAdapter::PNG_D_WITH_DATA(void *acl_process, const RawData &data) const {
  if (!HasAclPlugin() || png_d_with_data_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return png_d_with_data_fun_obj_(acl_process, data);
}

APP_ERROR AclAdapter::JPEG_DRC(void *acl_process) const {
  if (!HasAclPlugin() || jpeg_drc_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return jpeg_drc_fun_obj_(acl_process);
}

APP_ERROR AclAdapter::JPEG_DR(void *acl_process) const {
  if (!HasAclPlugin() || jpeg_dr_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return jpeg_dr_fun_obj_(acl_process);
}

APP_ERROR AclAdapter::JPEG_D(void *acl_process) const {
  if (!HasAclPlugin() || jpeg_d_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return jpeg_d_fun_obj_(acl_process);
}

APP_ERROR AclAdapter::JPEG_R(void *acl_process, const std::string &last_step) const {
  if (!HasAclPlugin() || jpeg_r_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return jpeg_r_fun_obj_(acl_process, last_step);
}

APP_ERROR AclAdapter::JPEG_C(void *acl_process, const std::string &last_step) const {
  if (!HasAclPlugin() || jpeg_c_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return jpeg_c_fun_obj_(acl_process, last_step);
}

APP_ERROR AclAdapter::PNG_D(void *acl_process) const {
  if (!HasAclPlugin() || png_d_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return png_d_fun_obj_(acl_process);
}

void *AclAdapter::GetMemoryData(void *acl_process) const {
  if (!HasAclPlugin() || get_memory_data_fun_obj_ == nullptr) {
    return nullptr;
  }
  return get_memory_data_fun_obj_(acl_process);
}

DvppDataInfo *AclAdapter::GetCropedDeviceData(void *acl_process) const {
  if (!HasAclPlugin() || get_croped_device_data_fun_obj_ == nullptr) {
    return nullptr;
  }
  return get_croped_device_data_fun_obj_(acl_process);
}

DvppDataInfo *AclAdapter::GetResizedDeviceData(void *acl_process) const {
  if (!HasAclPlugin() || get_resized_device_data_fun_obj_ == nullptr) {
    return nullptr;
  }
  return get_resized_device_data_fun_obj_(acl_process);
}

DvppDataInfo *AclAdapter::GetDecodeDeviceData(void *acl_process) const {
  if (!HasAclPlugin() || get_decode_device_data_fun_obj_ == nullptr) {
    return nullptr;
  }
  return get_decode_device_data_fun_obj_(acl_process);
}

APP_ERROR AclAdapter::H2D_Sink(void *acl_process, const std::shared_ptr<mindspore::dataset::Tensor> &input,
                               std::shared_ptr<mindspore::dataset::DeviceTensor> *device_input) const {
  if (!HasAclPlugin() || h_2_d_sink_fun_obj_ == nullptr || device_input == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return h_2_d_sink_fun_obj_(acl_process, input, *device_input);
}

APP_ERROR AclAdapter::D2H_Pop(void *acl_process, const std::shared_ptr<mindspore::dataset::DeviceTensor> &device_output,
                              std::shared_ptr<mindspore::dataset::Tensor> *output) const {
  if (!HasAclPlugin() || d_2_h_pop_fun_obj_ == nullptr || output == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return d_2_h_pop_fun_obj_(acl_process, device_output, *output);
}

APP_ERROR AclAdapter::DeviceMemoryRelease(void *acl_process) const {
  if (!HasAclPlugin() || device_memory_release_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return device_memory_release_fun_obj_(acl_process);
}

APP_ERROR AclAdapter::SetResizeParas(void *acl_process, uint32_t width, uint32_t height) const {
  if (!HasAclPlugin() || set_resize_paras_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return set_resize_paras_fun_obj_(acl_process, width, height);
}

APP_ERROR AclAdapter::SetCropParas(void *acl_process, uint32_t width, uint32_t height) const {
  if (!HasAclPlugin() || set_crop_paras_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return set_crop_paras_fun_obj_(acl_process, width, height);
}

int AclAdapter::Memcpy(void *dst, size_t dest_max, const void *src, size_t count, int kind) const {
  if (!HasAclPlugin() || aclrt_memcpy_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return aclrt_memcpy_fun_obj_(dst, dest_max, src, count, kind);
}

int AclAdapter::MallocHost(void **host_ptr, size_t size) const {
  if (!HasAclPlugin() || aclrt_malloc_host_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return aclrt_malloc_host_fun_obj_(host_ptr, size);
}

int AclAdapter::FreeHost(void *host_ptr) const {
  if (!HasAclPlugin() || aclrt_free_host_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return aclrt_free_host_fun_obj_(host_ptr);
}

#if !defined(BUILD_LITE) && defined(ENABLE_D)
// Ascend910B
APP_ERROR AclAdapter::DvppAdjustBrightness(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                           std::shared_ptr<DeviceTensorAscend910B> *output, float factor) {
  if (!HasAclPlugin() || dvpp_brightness_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_brightness_fun_obj_(input, output, factor);
}

APP_ERROR AclAdapter::DvppAdjustContrast(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                         std::shared_ptr<DeviceTensorAscend910B> *output, float factor) {
  if (!HasAclPlugin() || dvpp_contrast_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_contrast_fun_obj_(input, output, factor);
}

APP_ERROR AclAdapter::DvppAdjustHue(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                    std::shared_ptr<DeviceTensorAscend910B> *output, float factor) {
  if (!HasAclPlugin() || dvpp_hue_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_hue_fun_obj_(input, output, factor);
}

APP_ERROR AclAdapter::DvppAdjustSaturation(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                           std::shared_ptr<DeviceTensorAscend910B> *output, float factor) {
  if (!HasAclPlugin() || dvpp_saturation_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_saturation_fun_obj_(input, output, factor);
}

APP_ERROR AclAdapter::DvppAffine(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                 std::shared_ptr<DeviceTensorAscend910B> *output, const std::vector<float> &matrix,
                                 uint32_t interpolation_mode, uint32_t padding_mode, const std::vector<float> &fill) {
  if (!HasAclPlugin() || dvpp_affine_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_affine_fun_obj_(input, output, matrix, interpolation_mode, padding_mode, fill);
}

APP_ERROR AclAdapter::DvppCrop(const std::shared_ptr<DeviceTensorAscend910B> &input,
                               std::shared_ptr<DeviceTensorAscend910B> *output, uint32_t top, uint32_t left,
                               uint32_t height, uint32_t width) {
  if (!HasAclPlugin() || dvpp_crop_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_crop_fun_obj_(input, output, top, left, height, width);
}

APP_ERROR AclAdapter::DvppDecode(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                 std::shared_ptr<DeviceTensorAscend910B> *output) {
  if (!HasAclPlugin() || dvpp_decode_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_decode_fun_obj_(input, output);
}

APP_ERROR AclAdapter::DvppGaussianBlur(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                       std::shared_ptr<DeviceTensorAscend910B> *output,
                                       const std::vector<int64_t> &kernel_size, const std::vector<float> &sigma,
                                       uint32_t padding_mode) {
  if (!HasAclPlugin() || dvpp_gaussian_blur_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_gaussian_blur_fun_obj_(input, output, kernel_size, sigma, padding_mode);
}

APP_ERROR AclAdapter::DvppHorizontalFlip(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                         std::shared_ptr<DeviceTensorAscend910B> *output) {
  if (!HasAclPlugin() || dvpp_horizontal_flip_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_horizontal_flip_fun_obj_(input, output);
}

APP_ERROR AclAdapter::DvppNormalize(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                    std::shared_ptr<DeviceTensorAscend910B> *output, std::vector<float> mean,
                                    std::vector<float> std, bool is_hwc) {
  if (!HasAclPlugin() || dvpp_normalize_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_normalize_fun_obj_(input, output, mean, std, is_hwc);
}

APP_ERROR AclAdapter::DvppPad(const std::shared_ptr<DeviceTensorAscend910B> &input,
                              std::shared_ptr<DeviceTensorAscend910B> *output, const std::vector<int64_t> &padding,
                              uint32_t padding_mode, const std::vector<float> &fill) {
  if (!HasAclPlugin() || dvpp_pad_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_pad_fun_obj_(input, output, padding, padding_mode, fill);
}

APP_ERROR AclAdapter::DvppPerspective(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                      std::shared_ptr<DeviceTensorAscend910B> *output,
                                      const std::vector<std::vector<int32_t>> &start_points,
                                      const std::vector<std::vector<int32_t>> &end_points,
                                      InterpolationMode interpolation) {
  if (!HasAclPlugin() || dvpp_perspective_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_perspective_fun_obj_(input, output, start_points, end_points, interpolation);
}

APP_ERROR AclAdapter::DvppResize(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                 std::shared_ptr<DeviceTensorAscend910B> *output, int32_t output_height,
                                 int32_t output_width, double fx, double fy, InterpolationMode mode) {
  if (!HasAclPlugin() || dvpp_resize_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_resize_fun_obj_(input, output, output_height, output_width, fx, fy, mode);
}

APP_ERROR AclAdapter::DvppResizedCrop(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                      std::shared_ptr<DeviceTensorAscend910B> *output, int32_t top, int32_t left,
                                      int32_t height, int32_t width, int32_t output_height, int32_t output_width,
                                      InterpolationMode interpolation) {
  if (!HasAclPlugin() || dvpp_resized_crop_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_resized_crop_fun_obj_(input, output, top, left, height, width, output_height, output_width,
                                    interpolation);
}

APP_ERROR AclAdapter::DvppVerticalFlip(const std::shared_ptr<DeviceTensorAscend910B> &input,
                                       std::shared_ptr<DeviceTensorAscend910B> *output) {
  if (!HasAclPlugin() || dvpp_vertical_flip_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return dvpp_vertical_flip_fun_obj_(input, output);
}

// acl
APP_ERROR AclAdapter::GetSocName(std::string *soc_name) {
  if (!HasAclPlugin() || get_soc_name_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return get_soc_name_fun_obj_(soc_name);
}

APP_ERROR AclAdapter::CreateAclTensor(const int64_t *view_dims, uint64_t view_dims_num, mindspore::TypeId data_type,
                                      const int64_t *stride, int64_t offset, const int64_t *storage_dims,
                                      uint64_t storage_dims_num, void *tensor_data, bool is_hwc, void **acl_tensor) {
  if (!HasAclPlugin() || create_acl_tensor_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return create_acl_tensor_fun_obj_(view_dims, view_dims_num, data_type, stride, offset, storage_dims, storage_dims_num,
                                    tensor_data, is_hwc, acl_tensor);
}

APP_ERROR AclAdapter::DestroyTensor(void *tensor) {
  if (!HasAclPlugin() || destroy_tensor_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return destroy_tensor_fun_obj_(tensor);
}

APP_ERROR AclAdapter::DestroyFloatArray(void *float_array) {
  if (!HasAclPlugin() || destroy_float_array_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return destroy_float_array_fun_obj_(float_array);
}

APP_ERROR AclAdapter::DestroyIntArray(void *int_array) {
  if (!HasAclPlugin() || destroy_int_array_fun_obj_ == nullptr) {
    return APP_ERR_ACL_FAILURE;
  }
  return destroy_int_array_fun_obj_(int_array);
}
#endif
}  // namespace dataset
}  // namespace mindspore
