/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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
#include "plugin/device/cpu/kernel/grid_sampler_2d_grad_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "mindspore/core/ops/op_enum.h"

namespace {
const size_t kDataSizeThreshold = 64 * 1024;
const size_t kZero = 0;
const size_t kOne = 1;
const size_t kTwo = 2;
const size_t kThree = 3;
const size_t kFour = 4;
const size_t kFive = 5;
const size_t kInputsNum = 6;
const size_t kOutputsNum = 2;
}  // namespace

namespace mindspore {
namespace kernel {
bool GridSampler2DGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                         const std::vector<KernelTensor *> &outputs) {
  constexpr size_t input_num = kInputsNum;
  constexpr size_t output_num = kOutputsNum;
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), input_num, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), output_num, kernel_name_);
  dtype_ = inputs[kZero]->dtype_id();
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto match = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!match.first) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }
  return true;
}

int GridSampler2DGradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                          const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }
  grad_shape_ = inputs[kZero]->GetDeviceShapeVector();
  x_shape_ = inputs[kOne]->GetDeviceShapeVector();
  grid_shape_ = inputs[kTwo]->GetDeviceShapeVector();
  interpolation_mode_ = inputs[kThree]->GetValueWithCheck<int64_t>();
  padding_mode_ = inputs[kFour]->GetValueWithCheck<int64_t>();
  align_corners_ = inputs[kFive]->GetValueWithCheck<bool>();
  dx_shape_ = outputs[kZero]->GetDeviceShapeVector();
  dgrid_shape_ = outputs[kOne]->GetDeviceShapeVector();
  dx_size_ = LongToSize(dx_shape_[kZero] * dx_shape_[kOne] * dx_shape_[kTwo] * dx_shape_[kThree]);
  grid_size_ = LongToSize(grid_shape_[kZero] * grid_shape_[kOne] * grid_shape_[kTwo] * grid_shape_[kThree]);
  return ret;
}

bool GridSampler2DGradCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                           const std::vector<kernel::KernelTensor *> &,
                                           const std::vector<kernel::KernelTensor *> &outputs) {
  if (dtype_ == kNumberTypeFloat16) {
    LaunchKernel<float16>(inputs, outputs);
  } else if (dtype_ == kNumberTypeFloat32) {
    LaunchKernel<float>(inputs, outputs);
  } else if (dtype_ == kNumberTypeFloat64) {
    LaunchKernel<double>(inputs, outputs);
  } else {
    MS_EXCEPTION(TypeError) << "Input dtype only support float16, float32, float64!, but got" << dtype_;
  }
  return true;
}

template <typename T>
void GridSampler2DGradCpuKernelMod::ComputeTask(const std::vector<KernelTensor *> &inputs,
                                                const std::vector<KernelTensor *> &outputs) {
  GridSamplerInterpolation interp = GridSamplerInterpolation::Bilinear;
  GridSamplerPadding padding;
  bool align_corners = align_corners_;

  auto grad_data_addr = static_cast<T *>(inputs[kZero]->device_ptr());
  auto x_data_addr = static_cast<T *>(inputs[kOne]->device_ptr());
  auto grid_data_addr = static_cast<T *>(inputs[kTwo]->device_ptr());
  auto dx_data_addr = static_cast<T *>(outputs[kZero]->device_ptr());
  auto dgrid_data_addr = static_cast<T *>(outputs[kOne]->device_ptr());
  if (interpolation_mode_ == static_cast<int64_t>(ops::InterpolationMode::BILINEAR)) {
    interp = GridSamplerInterpolation::Bilinear;
  } else if (interpolation_mode_ == static_cast<int64_t>(ops::InterpolationMode::NEAREST)) {
    interp = GridSamplerInterpolation::Nearest;
  }
  if (padding_mode_ == static_cast<int64_t>(ops::GridSamplerPaddingMode::ZEROS)) {
    padding = GridSamplerPadding::Zeros;
  } else if (padding_mode_ == static_cast<int64_t>(ops::GridSamplerPaddingMode::BORDER)) {
    padding = GridSamplerPadding::Border;
  } else {
    padding = GridSamplerPadding::Reflection;
  }

  int64_t N = x_shape_[kZero];

#define VARDEF                                                                                                       \
  for (int64_t n = 0; n < N; ++n) {                                                                                  \
    auto DXSlice = DXAcc[n];                                                                                         \
    auto DGridSlice = DGridAcc[n];                                                                                   \
    auto GradSlice = GradAcc[n];                                                                                     \
    auto XSlice = XAcc[n];                                                                                           \
    GridSampler2DGridSliceIterator(GridAcc[n], [&](const vec256::Vec256<T> &grid_x, const vec256::Vec256<T> &grid_y, \
                                                   int64_t spatial_offset, int64_t len) {                            \
      grid_sample.Backward(&DXSlice, &DGridSlice, GradSlice, XSlice, spatial_offset, grid_x, grid_y, len);           \
    });                                                                                                              \
  }
#define PROCESS_CASE(interp, padding, align_corners)                                \
  do {                                                                              \
    case padding: {                                                                 \
      ApplyGridSample2D<T, kTwo, interp, padding, align_corners> grid_sample(XAcc); \
      VARDEF;                                                                       \
      return;                                                                       \
    }                                                                               \
  } while (0);

#define PROCESS_INTERP(interp, align_corners)                                \
  do {                                                                       \
    case interp: {                                                           \
      switch (static_cast<GridSamplerPadding>(padding)) {                    \
        PROCESS_CASE(interp, GridSamplerPadding::Zeros, align_corners);      \
        PROCESS_CASE(interp, GridSamplerPadding::Border, align_corners);     \
        PROCESS_CASE(interp, GridSamplerPadding::Reflection, align_corners); \
      }                                                                      \
      return;                                                                \
    }                                                                        \
  } while (0);

  auto DXAcc = accessor<T, 4>(dx_data_addr, dx_shape_);
  auto DGridAcc = accessor<T, 4>(dgrid_data_addr, dgrid_shape_);
  auto XAcc = accessor<T, 4>(x_data_addr, x_shape_);
  auto GridAcc = accessor<T, 4>(grid_data_addr, grid_shape_);
  auto GradAcc = accessor<T, 4>(grad_data_addr, grad_shape_);
  if (align_corners) {
    switch (static_cast<GridSamplerInterpolation>(interp)) {
      PROCESS_INTERP(GridSamplerInterpolation::Bilinear, true);
      PROCESS_INTERP(GridSamplerInterpolation::Nearest, true);
    }
  } else {
    switch (static_cast<GridSamplerInterpolation>(interp)) {
      PROCESS_INTERP(GridSamplerInterpolation::Bilinear, false);
      PROCESS_INTERP(GridSamplerInterpolation::Nearest, false);
    }
  }

#undef PROCESS_CASE
#undef PROCESS_INTERP
}

template <typename T>
void GridSampler2DGradCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                                 const std::vector<KernelTensor *> &outputs) {
  auto dx_data_addr = static_cast<T *>(outputs[kZero]->device_ptr());
  auto dgrid_data_addr = static_cast<T *>(outputs[kOne]->device_ptr());
  for (size_t i = kZero; i < dx_size_; i++) {
    dx_data_addr[i] = static_cast<T>(kZero);
  }
  for (size_t i = kZero; i < grid_size_; i++) {
    dgrid_data_addr[i] = static_cast<T>(kZero);
  }
  if (dtype_ == kNumberTypeFloat16) {
    //
  } else if (dtype_ == kNumberTypeFloat32) {
    ComputeTask<float>(inputs, outputs);
  } else if (dtype_ == kNumberTypeFloat64) {
    ComputeTask<double>(inputs, outputs);
  }
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, GridSampler2DGrad, GridSampler2DGradCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
