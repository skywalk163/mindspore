/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/resize_bilinear_grad_cpu_kernel.h"
#include <functional>
#include <utility>
#include "kernel/ops_utils.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kResizeBilinearGradInputsNum = 4;
constexpr size_t kResizeBilinearGradOutputNum = 1;
constexpr size_t kResizeBilinearGradInputsDoutShapeSize = 4;
constexpr size_t kResizeBilinearGradNumZero = 0;
constexpr size_t kResizeBilinearGradNumOne = 1;
constexpr size_t kResizeBilinearGradNumTwo = 2;
constexpr size_t kResizeBilinearGradNumThree = 3;
}  // namespace

using FuncVec = const std::vector<std::pair<KernelAttr, ResizeBilinearGradCpuKernelMod::KernelRunFunc>>;

void ResizeBilinearGradFP16(float *float_dloss_addr, float *float_output_addr, float16 *output_addr,
                            const size_t *shape, const size_t *size, float height_scale, float width_scale) {
  size_t batch_size = shape[kResizeBilinearGradNumZero];
  size_t channel = shape[kResizeBilinearGradNumOne];
  size_t in_height = shape[kResizeBilinearGradNumTwo];
  size_t in_width = shape[kResizeBilinearGradNumThree];
  size_t out_height = size[kResizeBilinearGradNumTwo];
  size_t out_width = size[kResizeBilinearGradNumThree];
  size_t out_hw_size = out_height * out_width;
  size_t in_hw_size = in_height * in_width;

  float *cur_dloss_addr = float_dloss_addr;
  float *cur_output_addr = float_output_addr;
  for (size_t b = 0; b < batch_size; ++b) {
    for (size_t c = 0; c < channel; ++c) {
      for (size_t h = 0; h < in_height; ++h) {
        const float in_y = static_cast<float>(h) * height_scale;
        const size_t top_y_index = std::max(static_cast<int>(floorf(in_y)), static_cast<int>(0));
        const size_t bottom_y_index = std::min(static_cast<int>(ceilf(in_y)), static_cast<int>(out_height) - 1);
        const float y_lerp = in_y - floorf(in_y);
        const float inverse_y_lerp = 1.0 - y_lerp;
        for (size_t w = 0; w < in_width; ++w) {
          const float in_x = static_cast<float>(w) * width_scale;
          const size_t left_x_index = std::max(static_cast<int>(floorf(in_x)), static_cast<int>(0));
          const size_t right_x_index = std::min(static_cast<int>(ceilf(in_x)), static_cast<int>(out_width) - 1);
          const float x_lerp = in_x - floorf(in_x);
          const float inverse_x_lerp = 1.0 - x_lerp;
          cur_output_addr[top_y_index * out_width + left_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<float>(inverse_y_lerp * inverse_x_lerp);
          cur_output_addr[top_y_index * out_width + right_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<float>(inverse_y_lerp * x_lerp);
          cur_output_addr[bottom_y_index * out_width + left_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<float>(y_lerp * inverse_x_lerp);
          cur_output_addr[bottom_y_index * out_width + right_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<float>(y_lerp * x_lerp);

          output_addr[top_y_index * out_width + left_x_index] =
            static_cast<float16>(cur_output_addr[top_y_index * out_width + left_x_index]);
          output_addr[top_y_index * out_width + right_x_index] =
            static_cast<float16>(cur_output_addr[top_y_index * out_width + right_x_index]);
          output_addr[bottom_y_index * out_width + left_x_index] =
            static_cast<float16>(cur_output_addr[bottom_y_index * out_width + left_x_index]);
          output_addr[bottom_y_index * out_width + right_x_index] =
            static_cast<float16>(cur_output_addr[bottom_y_index * out_width + right_x_index]);
        }
      }
      output_addr += out_hw_size;
      cur_dloss_addr += in_hw_size;
      cur_output_addr += out_hw_size;
    }
  }
}
void ResizeBilinearGradFP16_HPC(float *float_dloss_addr, float *float_output_addr, float16 *output_addr,
                                const size_t *shape, const size_t *size, float height_scale, float width_scale) {
  size_t batch_size = shape[kResizeBilinearGradNumZero];
  size_t channel = shape[kResizeBilinearGradNumOne];
  size_t in_height = shape[kResizeBilinearGradNumTwo];
  size_t in_width = shape[kResizeBilinearGradNumThree];
  size_t out_height = size[kResizeBilinearGradNumTwo];
  size_t out_width = size[kResizeBilinearGradNumThree];
  size_t out_hw_size = out_height * out_width;
  size_t in_hw_size = in_height * in_width;

  float *cur_dloss_addr = float_dloss_addr;
  float *cur_output_addr = float_output_addr;
  for (size_t b = 0; b < batch_size; ++b) {
    for (size_t c = 0; c < channel; ++c) {
      for (size_t h = 0; h < in_height; ++h) {
        const float in_y = (static_cast<float>(h) + 0.5f) * height_scale - 0.5f;
        const size_t top_y_index = std::max(static_cast<int>(floorf(in_y)), static_cast<int>(0));
        const size_t bottom_y_index = std::min(static_cast<int>(ceilf(in_y)), static_cast<int>(out_height) - 1);
        const float y_lerp = in_y - floorf(in_y);
        const float inverse_y_lerp = 1.0 - y_lerp;
        for (size_t w = 0; w < in_width; ++w) {
          const float in_x = (static_cast<float>(w) + 0.5f) * width_scale - 0.5f;
          const size_t left_x_index = std::max(static_cast<int>(floorf(in_x)), static_cast<int>(0));
          const size_t right_x_index = std::min(static_cast<int>(ceilf(in_x)), static_cast<int>(out_width) - 1);
          const float x_lerp = in_x - floorf(in_x);
          const float inverse_x_lerp = 1.0 - x_lerp;
          cur_output_addr[top_y_index * out_width + left_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<float>(inverse_y_lerp * inverse_x_lerp);
          cur_output_addr[top_y_index * out_width + right_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<float>(inverse_y_lerp * x_lerp);
          cur_output_addr[bottom_y_index * out_width + left_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<float>(y_lerp * inverse_x_lerp);
          cur_output_addr[bottom_y_index * out_width + right_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<float>(y_lerp * x_lerp);

          output_addr[top_y_index * out_width + left_x_index] =
            static_cast<float16>(cur_output_addr[top_y_index * out_width + left_x_index]);
          output_addr[top_y_index * out_width + right_x_index] =
            static_cast<float16>(cur_output_addr[top_y_index * out_width + right_x_index]);
          output_addr[bottom_y_index * out_width + left_x_index] =
            static_cast<float16>(cur_output_addr[bottom_y_index * out_width + left_x_index]);
          output_addr[bottom_y_index * out_width + right_x_index] =
            static_cast<float16>(cur_output_addr[bottom_y_index * out_width + right_x_index]);
        }
      }
      output_addr += out_hw_size;
      cur_dloss_addr += in_hw_size;
      cur_output_addr += out_hw_size;
    }
  }
}
template <typename T>
void ResizeBilinearGrad(T *float_dloss_addr, T *float_output_addr, T *output_addr, const size_t *shape,
                        const size_t *size, float height_scale, float width_scale) {
  size_t batch_size = shape[kResizeBilinearGradNumZero];
  size_t channel = shape[kResizeBilinearGradNumOne];
  size_t in_height = shape[kResizeBilinearGradNumTwo];
  size_t in_width = shape[kResizeBilinearGradNumThree];
  size_t out_height = size[kResizeBilinearGradNumTwo];
  size_t out_width = size[kResizeBilinearGradNumThree];
  size_t out_hw_size = out_height * out_width;
  size_t in_hw_size = in_height * in_width;

  T *cur_dloss_addr = float_dloss_addr;
  T *cur_output_addr = float_output_addr;
  for (size_t b = 0; b < batch_size; ++b) {
    for (size_t c = 0; c < channel; ++c) {
      for (size_t h = 0; h < in_height; ++h) {
        const T in_y = static_cast<T>(h) * height_scale;
        const size_t top_y_index = std::max(static_cast<int>(floorf(in_y)), static_cast<int>(0));
        const size_t bottom_y_index = std::min(static_cast<int>(ceilf(in_y)), static_cast<int>(out_height) - 1);
        const T y_lerp = in_y - floorf(in_y);
        const T inverse_y_lerp = 1.0 - y_lerp;
        for (size_t w = 0; w < in_width; ++w) {
          const T in_x = static_cast<T>(w) * width_scale;
          const size_t left_x_index = std::max(static_cast<int>(floorf(in_x)), static_cast<int>(0));
          const size_t right_x_index = std::min(static_cast<int>(ceilf(in_x)), static_cast<int>(out_width) - 1);
          const T x_lerp = in_x - floorf(in_x);
          const T inverse_x_lerp = 1.0 - x_lerp;
          cur_output_addr[top_y_index * out_width + left_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<T>(inverse_y_lerp * inverse_x_lerp);
          cur_output_addr[top_y_index * out_width + right_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<T>(inverse_y_lerp * x_lerp);
          cur_output_addr[bottom_y_index * out_width + left_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<T>(y_lerp * inverse_x_lerp);
          cur_output_addr[bottom_y_index * out_width + right_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<T>(y_lerp * x_lerp);

          output_addr[top_y_index * out_width + left_x_index] =
            static_cast<T>(cur_output_addr[top_y_index * out_width + left_x_index]);
          output_addr[top_y_index * out_width + right_x_index] =
            static_cast<T>(cur_output_addr[top_y_index * out_width + right_x_index]);
          output_addr[bottom_y_index * out_width + left_x_index] =
            static_cast<T>(cur_output_addr[bottom_y_index * out_width + left_x_index]);
          output_addr[bottom_y_index * out_width + right_x_index] =
            static_cast<T>(cur_output_addr[bottom_y_index * out_width + right_x_index]);
        }
      }
      output_addr += out_hw_size;
      cur_dloss_addr += in_hw_size;
      cur_output_addr += out_hw_size;
    }
  }
}
template <typename T>
void ResizeBilinearGrad_HPC(T *float_dloss_addr, T *float_output_addr, T *output_addr, const size_t *shape,
                            const size_t *size, float height_scale, float width_scale) {
  size_t batch_size = shape[kResizeBilinearGradNumZero];
  size_t channel = shape[kResizeBilinearGradNumOne];
  size_t in_height = shape[kResizeBilinearGradNumTwo];
  size_t in_width = shape[kResizeBilinearGradNumThree];
  size_t out_height = size[kResizeBilinearGradNumTwo];
  size_t out_width = size[kResizeBilinearGradNumThree];
  size_t out_hw_size = out_height * out_width;
  size_t in_hw_size = in_height * in_width;

  T *cur_dloss_addr = float_dloss_addr;
  T *cur_output_addr = float_output_addr;
  for (size_t b = 0; b < batch_size; ++b) {
    for (size_t c = 0; c < channel; ++c) {
      for (size_t h = 0; h < in_height; ++h) {
        const T in_y = (static_cast<float>(h) + 0.5f) * height_scale - 0.5f;
        const size_t top_y_index = std::max(static_cast<int>(floorf(in_y)), static_cast<int>(0));
        const size_t bottom_y_index = std::min(static_cast<int>(ceilf(in_y)), static_cast<int>(out_height) - 1);
        const T y_lerp = in_y - floorf(in_y);
        const T inverse_y_lerp = 1.0 - y_lerp;
        for (size_t w = 0; w < in_width; ++w) {
          const T in_x = (static_cast<float>(w) + 0.5f) * width_scale - 0.5f;
          const size_t left_x_index = std::max(static_cast<int>(floorf(in_x)), static_cast<int>(0));
          const size_t right_x_index = std::min(static_cast<int>(ceilf(in_x)), static_cast<int>(out_width) - 1);
          const T x_lerp = in_x - floorf(in_x);
          const T inverse_x_lerp = 1.0 - x_lerp;
          cur_output_addr[top_y_index * out_width + left_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<T>(inverse_y_lerp * inverse_x_lerp);
          cur_output_addr[top_y_index * out_width + right_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<T>(inverse_y_lerp * x_lerp);
          cur_output_addr[bottom_y_index * out_width + left_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<T>(y_lerp * inverse_x_lerp);
          cur_output_addr[bottom_y_index * out_width + right_x_index] +=
            cur_dloss_addr[h * in_width + w] * static_cast<T>(y_lerp * x_lerp);

          output_addr[top_y_index * out_width + left_x_index] =
            static_cast<T>(cur_output_addr[top_y_index * out_width + left_x_index]);
          output_addr[top_y_index * out_width + right_x_index] =
            static_cast<T>(cur_output_addr[top_y_index * out_width + right_x_index]);
          output_addr[bottom_y_index * out_width + left_x_index] =
            static_cast<T>(cur_output_addr[bottom_y_index * out_width + left_x_index]);
          output_addr[bottom_y_index * out_width + right_x_index] =
            static_cast<T>(cur_output_addr[bottom_y_index * out_width + right_x_index]);
        }
      }
      output_addr += out_hw_size;
      cur_dloss_addr += in_hw_size;
      cur_output_addr += out_hw_size;
    }
  }
}

bool ResizeBilinearGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                          const std::vector<KernelTensor *> &outputs) {
  if (inputs.size() != kResizeBilinearGradInputsNum || outputs.size() != kResizeBilinearGradOutputNum) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', input and output tensor number must be "
                  << kResizeBilinearGradInputsNum << " and " << kResizeBilinearGradOutputNum << ", but got "
                  << inputs.size() << " and " << outputs.size();
    return false;
  }
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int ResizeBilinearGradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                           const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  shape_ = Convert2SizeTClipNeg(inputs.at(kIndex0)->GetShapeVector());
  size_ = Convert2SizeTClipNeg(inputs.at(kIndex1)->GetShapeVector());
  is_null_input_ = (std::accumulate(shape_.begin(), shape_.end(), size_t(1), std::multiplies<size_t>()) == 0);
  if (is_null_input_) {
    return static_cast<int>(KRET_OK);
  }
  size_t in_height = shape_[2];
  size_t in_width = shape_[3];
  size_t out_height = size_[2];
  size_t out_width = size_[3];
  align_corners_ = inputs.at(kIndex2)->GetValueWithCheck<bool>();
  half_pixel_centers_ = inputs.at(kIndex3)->GetValueWithCheck<bool>();
  height_scale = Scaling(out_height, in_height, align_corners_);
  width_scale = Scaling(out_width, in_width, align_corners_);
  return static_cast<int>(KRET_OK);
}

template <typename T>
bool ResizeBilinearGradCpuKernelMod::LaunchFloat16Kernel(const std::vector<kernel::KernelTensor *> &inputs,
                                                         const std::vector<KernelTensor *> &,
                                                         const std::vector<kernel::KernelTensor *> &outputs) {
  auto output_addr = GetDeviceAddress<float16>(outputs, kIndex0);
  MS_EXCEPTION_IF_NULL(output_addr);
  if (memset_s(output_addr, outputs[0]->size(), 0, outputs[0]->size()) != EOK) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', output buffer memset failed.";
  }

  auto input_addr_T = GetDeviceAddress<float16>(inputs, kIndex0);
  MS_EXCEPTION_IF_NULL(input_addr_T);

  size_t input_mem_size = inputs[0]->size() / sizeof(float16) * sizeof(float);
  float *float_dloss_addr = reinterpret_cast<float *>(malloc(input_mem_size));
  if (float_dloss_addr == NULL) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', malloc memory failed.";
    return false;
  }
  for (size_t i = 0; i < ((inputs[0]->size()) / sizeof(float16)); ++i) {
    float_dloss_addr[i] = static_cast<float>(input_addr_T[i]);
  }

  size_t output_mem_size = outputs[0]->size() / sizeof(float16) * sizeof(float);
  float *float_output_addr = reinterpret_cast<float *>(malloc(output_mem_size));
  if (float_output_addr == NULL) {
    free(float_dloss_addr);
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', malloc memory failed.";
    return false;
  }
  size_t memset_size = outputs[0]->size() / sizeof(float16) * sizeof(float);
  if (memset_s(float_output_addr, memset_size, 0, memset_size) != EOK) {
    free(float_dloss_addr);
    free(float_output_addr);
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', output buffer memset failed.";
  }

  if (half_pixel_centers_) {
    ResizeBilinearGradFP16_HPC(float_dloss_addr, float_output_addr, output_addr, shape_.data(), size_.data(),
                               height_scale, width_scale);
  } else {
    ResizeBilinearGradFP16(float_dloss_addr, float_output_addr, output_addr, shape_.data(), size_.data(), height_scale,
                           width_scale);
  }

  free(float_dloss_addr);
  free(float_output_addr);
  return true;
}

template <typename T>
bool ResizeBilinearGradCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                                  const std::vector<KernelTensor *> &,
                                                  const std::vector<kernel::KernelTensor *> &outputs) {
  auto output_addr = GetDeviceAddress<T>(outputs, kIndex0);
  MS_EXCEPTION_IF_NULL(output_addr);
  if (memset_s(output_addr, outputs[0]->size(), 0, outputs[0]->size()) != EOK) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', output buffer memset failed.";
  }
  auto float_dloss_addr = GetDeviceAddress<T>(inputs, kIndex0);
  auto float_output_addr = GetDeviceAddress<T>(outputs, kIndex0);
  MS_EXCEPTION_IF_NULL(float_dloss_addr);
  MS_EXCEPTION_IF_NULL(float_output_addr);

  if (half_pixel_centers_) {
    ResizeBilinearGrad_HPC<T>(float_dloss_addr, float_output_addr, output_addr, shape_.data(), size_.data(),
                              height_scale, width_scale);
  } else {
    ResizeBilinearGrad<T>(float_dloss_addr, float_output_addr, output_addr, shape_.data(), size_.data(), height_scale,
                          width_scale);
  }

  return true;
}

FuncVec &ResizeBilinearGradCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, ResizeBilinearGradCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat16)
       .AddInputAttr(kNumberTypeFloat16)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddOutputAttr(kNumberTypeFloat16),
     &ResizeBilinearGradCpuKernelMod::LaunchFloat16Kernel<float16>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddOutputAttr(kNumberTypeFloat32),
     &ResizeBilinearGradCpuKernelMod::LaunchKernel<float>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat64)
       .AddInputAttr(kNumberTypeFloat64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddOutputAttr(kNumberTypeFloat64),
     &ResizeBilinearGradCpuKernelMod::LaunchKernel<double>},
  };
  return func_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, ResizeBilinearGrad, ResizeBilinearGradCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
