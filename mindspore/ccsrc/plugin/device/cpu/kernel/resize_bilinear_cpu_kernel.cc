/**
 * Copyright 2020-2024 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/resize_bilinear_cpu_kernel.h"
#include <functional>
#include <map>
#include "kernel/ops_utils.h"
#include "ops/auto_generate/gen_ops_primitive.h"
#include "ops/ops_func_impl/resize_bilinear_v2.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kResizeBilinearV2InputsNum = 4;
constexpr size_t kResizeBilinearOutputsNum = 1;
constexpr size_t kResizeBilinearExpectedRank = 4;
}  // namespace

using FuncVec = const std::vector<std::pair<KernelAttr, ResizeBilinearCpuKernelMod::KernelRunFunc>>;

bool ResizeBilinearCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &outputs) {
  if (inputs.size() != kResizeBilinearV2InputsNum) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the number of inputs must be" << kResizeBilinearV2InputsNum
                  << ", but got " << inputs.size();
    return false;
  }
  if (outputs.size() != kResizeBilinearOutputsNum) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the number of outputs must be" << kResizeBilinearOutputsNum
                  << ", but got " << outputs.size();
    return false;
  }
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int ResizeBilinearCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  align_corners_ = inputs.at(kIndex2)->GetValueWithCheck<bool>();
  half_pixel_centers_ = inputs.at(kIndex3)->GetValueWithCheck<bool>();
  if (half_pixel_centers_ == true && align_corners_ == true) {
    MS_LOG(ERROR) << "align_corners and half_pixel_centers cannot be True at the same time.";
    return false;
  }

  shape_ = Convert2SizeTClipNeg(inputs.at(kIndex0)->GetShapeVector());
  output_shape_ = Convert2SizeTClipNeg(outputs.at(kIndex0)->GetShapeVector());
  if (shape_.size() != kResizeBilinearExpectedRank || output_shape_.size() != kResizeBilinearExpectedRank) {
    MS_LOG(EXCEPTION) << "For " << kernel_name_ << ", input and output should be 4-D Tensor.";
  }

  is_null_input_ = (std::accumulate(shape_.begin(), shape_.end(), size_t(1), std::multiplies<size_t>()) == 0);
  is_null_input_ = is_null_input_ || (std::accumulate(output_shape_.begin(), output_shape_.end(), size_t(1),
                                                      std::multiplies<size_t>()) == 0);
  if (is_null_input_) {
    return static_cast<int>(KRET_OK);
  }

  size_t in_height = shape_[2];
  size_t in_width = shape_[3];
  size_t out_height = output_shape_[2];
  size_t out_width = output_shape_[3];
  height_scale = Scaling(in_height, out_height, align_corners_);
  width_scale = Scaling(in_width, out_width, align_corners_);

  return static_cast<int>(KRET_OK);
}

bool ResizeBilinearCpuKernelMod::LaunchFloat16Kernel(const std::vector<KernelTensor *> &inputs,
                                                     const std::vector<KernelTensor *> &,
                                                     const std::vector<KernelTensor *> &outputs) const {
  auto output_addr = GetDeviceAddress<float16>(outputs, kIndex0);
  auto input_addr = GetDeviceAddress<float16>(inputs, kIndex0);
  MS_EXCEPTION_IF_NULL(output_addr);
  MS_EXCEPTION_IF_NULL(input_addr);

  float *float_input_addr = nullptr;
  float *float_output_addr = nullptr;
  size_t input_mem_size = inputs[0]->size() / sizeof(float16) * sizeof(float);
  float_input_addr = reinterpret_cast<float *>(malloc(input_mem_size));
  if (float_input_addr == nullptr) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', malloc memory failed.";
    return false;
  }
  for (size_t i = 0; i < ((inputs[0]->size()) / sizeof(float16)); ++i) {
    float_input_addr[i] = static_cast<float>(input_addr[i]);
  }

  size_t output_mem_size = outputs[0]->size() / sizeof(float16) * sizeof(float);
  float_output_addr = reinterpret_cast<float *>(malloc(output_mem_size));
  if (float_output_addr == nullptr) {
    free(float_input_addr);
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', malloc memory failed.";
    return false;
  }

  size_t batch_size = shape_[0];
  size_t channel = shape_[1];
  size_t in_height = shape_[2];
  size_t in_width = shape_[3];
  size_t out_height = output_shape_[2];
  size_t out_width = output_shape_[3];
  size_t out_hw_size = out_height * out_width;
  size_t in_hw_size = in_height * in_width;
  size_t bhwc_size = in_hw_size * channel * batch_size;

  if (out_height == in_height && out_width == in_width) {
    for (size_t i = 0; i < bhwc_size; ++i) {
      float_output_addr[i] = static_cast<float>(float_input_addr[i]);
    }
  }

  std::vector<CachedInterpolation> ys(out_height + 1);
  std::vector<CachedInterpolation> xs(out_width + 1);
  ComputeInterpolationWeights(out_height, in_height, height_scale, ys.data(), half_pixel_centers_);
  ComputeInterpolationWeights(out_width, in_width, width_scale, xs.data(), half_pixel_centers_);

  float *cur_input_addr = float_input_addr;
  float *cur_output_addr = float_output_addr;
  for (size_t b = 0; b < batch_size; ++b) {
    for (size_t c = 0; c < channel; ++c) {
      for (size_t h = 0; h < out_height; ++h) {
        const float *ys_input_lower_ptr = cur_input_addr + ys[h].lower * in_width;
        const float *ys_input_upper_ptr = cur_input_addr + ys[h].upper * in_width;
        const float ys_lerp = static_cast<float>(ys[h].lerp);
        for (size_t w = 0; w < out_width; ++w) {
          const size_t xs_lower = xs[w].lower;
          const size_t xs_upper = xs[w].upper;
          const float xs_lerp = static_cast<float>(xs[w].lerp);
          const float top_left(ys_input_lower_ptr[xs_lower]);
          const float top_right(ys_input_lower_ptr[xs_upper]);
          const float bottom_left(ys_input_upper_ptr[xs_lower]);
          const float bottom_right(ys_input_upper_ptr[xs_upper]);
          cur_output_addr[h * out_width + w] =
            ComputeLerp(top_left, top_right, bottom_left, bottom_right, xs_lerp, ys_lerp);
          output_addr[h * out_width + w] = static_cast<float16>(cur_output_addr[h * out_width + w]);
        }
      }
      output_addr += out_hw_size;
      cur_input_addr += in_hw_size;
      cur_output_addr += out_hw_size;
    }
  }
  free(float_input_addr);
  free(float_output_addr);
  return true;
}

template <typename T>
bool ResizeBilinearCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                              const std::vector<KernelTensor *> &,
                                              const std::vector<KernelTensor *> &outputs) const {
  auto output_addr = GetDeviceAddress<T>(outputs, kIndex0);
  auto float_input_addr = GetDeviceAddress<T>(inputs, kIndex0);
  auto float_output_addr = GetDeviceAddress<T>(outputs, kIndex0);
  MS_EXCEPTION_IF_NULL(output_addr);
  MS_EXCEPTION_IF_NULL(float_input_addr);
  MS_EXCEPTION_IF_NULL(float_output_addr);

  size_t batch_size = shape_[0];
  size_t channel = shape_[1];
  size_t in_height = shape_[2];
  size_t in_width = shape_[3];
  size_t out_height = output_shape_[2];
  size_t out_width = output_shape_[3];
  size_t out_hw_size = out_height * out_width;
  size_t in_hw_size = in_height * in_width;
  size_t bhwc_size = in_hw_size * channel * batch_size;

  if (out_height == in_height && out_width == in_width) {
    for (size_t i = 0; i < bhwc_size; ++i) {
      float_output_addr[i] = static_cast<T>(float_input_addr[i]);
    }
  }

  std::vector<CachedInterpolation> ys(out_height + 1);
  std::vector<CachedInterpolation> xs(out_width + 1);
  ComputeInterpolationWeights(out_height, in_height, height_scale, ys.data(), half_pixel_centers_);
  ComputeInterpolationWeights(out_width, in_width, width_scale, xs.data(), half_pixel_centers_);

  T *cur_input_addr = float_input_addr;
  T *cur_output_addr = float_output_addr;
  for (size_t b = 0; b < batch_size; ++b) {
    for (size_t c = 0; c < channel; ++c) {
      for (size_t h = 0; h < out_height; ++h) {
        const T *ys_input_lower_ptr = cur_input_addr + ys[h].lower * in_width;
        const T *ys_input_upper_ptr = cur_input_addr + ys[h].upper * in_width;
        const T ys_lerp = static_cast<T>(ys[h].lerp);
        for (size_t w = 0; w < out_width; ++w) {
          const size_t xs_lower = xs[w].lower;
          const size_t xs_upper = xs[w].upper;
          const T xs_lerp = static_cast<T>(xs[w].lerp);
          const T top_left(ys_input_lower_ptr[xs_lower]);
          const T top_right(ys_input_lower_ptr[xs_upper]);
          const T bottom_left(ys_input_upper_ptr[xs_lower]);
          const T bottom_right(ys_input_upper_ptr[xs_upper]);
          cur_output_addr[h * out_width + w] =
            ComputeLerp(top_left, top_right, bottom_left, bottom_right, xs_lerp, ys_lerp);
          output_addr[h * out_width + w] = static_cast<T>(cur_output_addr[h * out_width + w]);
        }
      }
      output_addr += out_hw_size;
      cur_input_addr += in_hw_size;
      cur_output_addr += out_hw_size;
    }
  }
  return true;
}

FuncVec &ResizeBilinearCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, ResizeBilinearCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
     &ResizeBilinearCpuKernelMod::LaunchFloat16Kernel},
    {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
     &ResizeBilinearCpuKernelMod::LaunchKernel<float>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
     &ResizeBilinearCpuKernelMod::LaunchKernel<double>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat16)
       .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddOutputAttr(kNumberTypeFloat16),
     &ResizeBilinearCpuKernelMod::LaunchFloat16Kernel},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddOutputAttr(kNumberTypeFloat32),
     &ResizeBilinearCpuKernelMod::LaunchKernel<float>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat64)
       .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
       .AddOutputAttr(kNumberTypeFloat64),
     &ResizeBilinearCpuKernelMod::LaunchKernel<double>},
  };
  return func_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, ResizeBilinearV2, ResizeBilinearCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
