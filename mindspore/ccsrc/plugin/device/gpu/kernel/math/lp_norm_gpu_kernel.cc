/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/gpu/kernel/math/lp_norm_gpu_kernel.h"
#include <functional>
#include <utility>
#include <string>
#include <algorithm>
#include <set>
#include "mindspore/core/ops/math_ops.h"
#include "abstract/utils.h"
#include "kernel/common_utils.h"
#include "mindspore/core/ops/lp_norm.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/lp_norm_impl.cuh"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/elementwise/eltwise_ops_impl.cuh"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/elementwise/eltwise_ops_type.cuh"
namespace mindspore {
namespace kernel {
bool LpNormGpuKernelMod::GetLpNormAttr() {
  if (kernel_name_ != prim::kPrimLpNorm->name()) {
    MS_LOG(ERROR) << "For '" << prim::kPrimLpNorm->name() << "' , it's kernel name must be equal to LpNorm, but got "
                  << kernel_name_;
    return false;
  }

  axis_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kAxis));
  p_ = GetValue<int64_t>(primitive_->GetAttr(ops::kP));
  epsilon_ = GetValue<float>(primitive_->GetAttr(ops::kEpsilon));
  return true;
}

void LpNormGpuKernelMod::InitWorkSpaceSizeList() {
  // The workspace for device input shape.
  const size_t device_input_shape_size = input_shape_.size() * sizeof(size_t);
  // The workspace for device output axis.
  const size_t device_axis_shape_size = output_axis_.size() * sizeof(size_t);
  // The workspace for device output stride.
  const size_t device_output_stride_size = output_stride_.size() * sizeof(size_t);

  workspace_size_list_.clear();
  workspace_size_list_ = {device_input_shape_size, device_axis_shape_size, device_output_stride_size};
  // If in half, ms need high precision, so malloc device middle output.
  if (data_type_ == kNumberTypeFloat16) {
    constexpr auto high_precision_unit = 2;
    const size_t device_middle_output = output_elements_ * sizeof(half) * high_precision_unit;
    workspace_size_list_.emplace_back(device_middle_output);
  }
}

bool LpNormGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (inputs.empty() || outputs.empty()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it got empty inputs or outputs, which is invalid.";
    return false;
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return GetLpNormAttr();
}

int LpNormGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  data_type_ = inputs.at(kIndex0)->dtype_id();
  input_shape_.clear();
  auto input_shape = inputs.at(kIndex0)->GetShapeVector();
  if (input_shape.empty()) {
    is_scalar_input_ = true;
    return KRET_OK;
  }
  (void)std::transform(input_shape.begin(), input_shape.end(), std::back_inserter(input_shape_), LongToSize);
  input_elements_ = std::accumulate(input_shape_.begin(), input_shape_.end(), size_t(1), std::multiplies<size_t>());
  is_null_input_ = CHECK_SHAPE_NULL(input_shape_, kernel_name_, "input shape");
  if (is_null_input_) {
    return KRET_OK;
  }

  output_shape_.clear();
  if (axis_.size() == input_shape.size()) {
    output_shape_ = {1};
    output_elements_ = 1;
    InitWorkSpaceSizeList();
    return KRET_OK;
  }
  auto output_shape = outputs.at(kIndex0)->GetShapeVector();
  (void)std::transform(output_shape.begin(), output_shape.end(), std::back_inserter(output_shape_), LongToSize);
  output_axis_.clear();
  std::vector<size_t> axis;
  int64_t input_rank = SizeToLong(input_shape_.size());
  (void)std::transform(axis_.begin(), axis_.end(), std::back_inserter(axis), [&input_rank](const int64_t &dim) {
    return dim < 0 ? LongToSize(dim + input_rank) : LongToSize(dim);
  });
  std::set<size_t> axis_set(axis.begin(), axis.end());
  for (size_t i = 0; i < input_shape_.size(); ++i) {
    if (!axis_set.count(i)) {
      output_axis_.emplace_back(i);
    }
  }
  output_stride_.clear();
  output_stride_.resize(output_axis_.size());
  output_stride_[output_stride_.size() - 1] = 1;
  for (int i = static_cast<int>(output_stride_.size() - 2); i >= 0; --i) {
    output_stride_[i] = output_stride_[i + 1] * input_shape_[output_axis_[i + 1]];
  }
  output_elements_ = std::accumulate(output_shape_.begin(), output_shape_.end(), size_t(1), std::multiplies<size_t>());
  InitWorkSpaceSizeList();
  return KRET_OK;
}

template <typename T>
bool LpNormGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &workspace,
                                      const std::vector<KernelTensor *> &outputs) {
  auto input = GetDeviceAddress<T>(inputs, kIndex0);
  auto output = GetDeviceAddress<T>(outputs, kIndex0);
  if (is_scalar_input_) {
    UnaryOpsCudaFunc<ElwiseOpType::kAbs, T, T>(outputs.at(kIndex0)->size() / sizeof(T), input, output,
                                               reinterpret_cast<cudaStream_t>(cuda_stream_));
    return true;
  }
  auto device_input_shape = GetDeviceAddress<size_t>(workspace, kIndex0);
  auto device_axis_output = GetDeviceAddress<size_t>(workspace, kIndex1);
  auto device_output_stride = GetDeviceAddress<size_t>(workspace, kIndex2);
  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(
    cudaMemcpyAsync(device_input_shape, input_shape_.data(), input_shape_.size() * sizeof(size_t),
                    cudaMemcpyHostToDevice, reinterpret_cast<cudaStream_t>(cuda_stream_)),
    "LpNormGpuKernelMod cudaMemcpyAsync input_shape_ failed");

  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(
    cudaMemcpyAsync(device_axis_output, output_axis_.data(), output_axis_.size() * sizeof(size_t),
                    cudaMemcpyHostToDevice, reinterpret_cast<cudaStream_t>(cuda_stream_)),
    "LpNormGpuKernelMod cudaMemcpyAsync output_axis_ failed");

  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(
    cudaMemcpyAsync(device_output_stride, output_stride_.data(), output_stride_.size() * sizeof(size_t),
                    cudaMemcpyHostToDevice, reinterpret_cast<cudaStream_t>(cuda_stream_)),
    "LpNormGpuKernelMod cudaMemcpyAsync output_shape_ failed");

  CHECK_CUDA_RET_WITH_ERROR_NOTRACE(
    cudaMemsetAsync(output, 0, output_elements_ * sizeof(T), reinterpret_cast<cudaStream_t>(cuda_stream_)),
    "LpNormGpuKernelMod failed  to set output cuda memory to zeros.");

  // The workspace for device output high precision.
  if constexpr (std::is_same_v<T, half>) {
    auto middle_output = GetDeviceAddress<float>(workspace, kIndex3);
    auto middle_output_size = output_elements_ * sizeof(float);
    CHECK_CUDA_RET_WITH_ERROR_NOTRACE(cudaMemset(middle_output, 0, middle_output_size),
                                      "LpNormGpuKernelMod failed  to set middle output cuda memory to zeros.");
    auto status = CalLpNorm(input, device_input_shape, input_shape_.size(), input_elements_, device_axis_output,
                            device_output_stride, output_axis_.size(), output_elements_, p_, epsilon_, middle_output,
                            output, device_id_, reinterpret_cast<cudaStream_t>(cuda_stream_));
    CHECK_CUDA_STATUS(status, kernel_name_);
  } else {
    auto status = CalLpNorm(input, device_input_shape, input_shape_.size(), input_elements_, device_axis_output,
                            device_output_stride, output_axis_.size(), output_elements_, p_, epsilon_, nullptr, output,
                            device_id_, reinterpret_cast<cudaStream_t>(cuda_stream_));
    CHECK_CUDA_STATUS(status, kernel_name_);
  }
  return true;
}

std::vector<std::pair<KernelAttr, LpNormGpuKernelMod::LpNormFunc>> LpNormGpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
   &LpNormGpuKernelMod::LaunchKernel<half>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   &LpNormGpuKernelMod::LaunchKernel<float>}};

std::vector<KernelAttr> LpNormGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, LpNormFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, LpNorm, LpNormGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
