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

#include "plugin/device/gpu/kernel/arrays/masked_select_gpu_kernel.h"
#include <functional>
#include <utility>
#include <string>
#include <algorithm>
#include "mindspore/core/abstract/utils.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/masked_select_impl.cuh"
#include "kernel/common_utils.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/complex.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr int kMaskedSelectInputsNum = 2;
constexpr int kMaskedSelectOutputsNum = 1;
}  // namespace

static std::vector<int64_t> GetBroadcastShape(const std::vector<int64_t> &x, const std::vector<int64_t> &y) {
  size_t x_len = x.size();
  size_t y_len = y.size();
  size_t length = x_len < y_len ? x_len : y_len;
  std::vector<int64_t> broadcast_shape;
  std::vector<int64_t> broadcast_shape_back;
  for (int i = -length; i < 0; ++i) {
    if (x[x_len + i] == 1) {
      broadcast_shape_back.push_back(y[y_len + i]);
    } else if (y[y_len + i] == 1) {
      broadcast_shape_back.push_back(x[x_len + i]);
    } else if (x[x_len + i] == y[y_len + i]) {
      broadcast_shape_back.push_back(x[x_len + i]);
    }
  }
  if (length == x_len) {
    for (size_t i = 0; i < y_len - length; ++i) {
      broadcast_shape.push_back(y[i]);
    }
  } else {
    for (size_t i = 0; i < x_len - length; ++i) {
      broadcast_shape.push_back(x[i]);
    }
  }
  for (size_t i = 0; i < length; ++i) {
    broadcast_shape.push_back(broadcast_shape_back[i]);
  }
  return broadcast_shape;
}

bool MaskedSelectGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kMaskedSelectInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kMaskedSelectOutputsNum, kernel_name_);
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }

  kernel_func_ = func_list_[index].second;
  input_type_size_ = abstract::TypeIdSize(kernel_attr.GetInputAttr(kIndex0).dtype);
  mask_type_size_ = abstract::TypeIdSize(kernel_attr.GetInputAttr(kIndex1).dtype);
  return true;
}

void MaskedSelectGpuKernelMod::ResetResource() noexcept {
  input_size_ = 0;
  mask_size_ = 0;
  broadcast_size_ = 0;
  real_output_size_ = 0;
  input_broadcast_ = false;
  mask_broadcast_ = false;
  for (size_t i = 0; i < MAX_DIMS; i++) {
    input_shape_[i] = 1;
    mask_shape_[i] = 1;
    broadcast_shape_[i] = 1;
  }
  output_size_list_.clear();
  workspace_size_list_.clear();
}

int MaskedSelectGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  ResetResource();
  auto x_shape = inputs[kIndex0]->GetShapeVector();
  auto y_shape = inputs[kIndex1]->GetShapeVector();
  auto it_x = std::find_if(x_shape.begin(), x_shape.end(), [](int64_t sh) { return sh <= 0; });
  auto it_y = std::find_if(y_shape.begin(), y_shape.end(), [](int64_t sh) { return sh <= 0; });
  if (it_x != x_shape.end() || it_y != y_shape.end()) {
    return KRET_UNKNOWN_SHAPE;
  }

  if (x_shape.size() > MAX_DIMS || y_shape.size() > MAX_DIMS) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dimension of input and mask cannot be greater than "
                      << MAX_DIMS << ", but got the dimension of input: " << x_shape.size()
                      << ", the dimension of mask: " << y_shape.size();
  }

  // shape
  auto broadcast_shape = GetBroadcastShape(x_shape, y_shape);
  size_t offset_x = broadcast_shape.size() - x_shape.size();
  for (size_t i = 0; i < x_shape.size(); ++i) {
    input_shape_[i + offset_x] = x_shape[i];
  }

  size_t offset_y = broadcast_shape.size() - y_shape.size();
  for (size_t j = 0; j < y_shape.size(); ++j) {
    mask_shape_[j + offset_y] = y_shape[j];
  }

  for (size_t k = 0; k < broadcast_shape.size(); ++k) {
    broadcast_shape_[k] = broadcast_shape[k];
  }

  // size and broadcast type
  input_size_ = std::accumulate(input_shape_.begin(), input_shape_.end(), size_t(1), std::multiplies{});
  mask_size_ = std::accumulate(mask_shape_.begin(), mask_shape_.end(), size_t(1), std::multiplies{});
  broadcast_size_ = std::accumulate(broadcast_shape_.begin(), broadcast_shape_.end(), size_t(1), std::multiplies{});
  if (input_size_ < broadcast_size_) {
    input_broadcast_ = true;
  }
  if (mask_size_ < broadcast_size_) {
    mask_broadcast_ = true;
  }

  workspace_size_list_.push_back(broadcast_size_ * sizeof(size_t));  // save prefix sum of mask
  if (input_broadcast_) {
    workspace_size_list_.push_back(broadcast_size_ * input_type_size_);  // save broadcast result of input
  }
  if (mask_broadcast_) {
    workspace_size_list_.push_back(broadcast_size_ * mask_type_size_);  // save broadcast result of mask
  }
  output_size_list_.push_back(broadcast_size_ * input_type_size_);
  return KRET_OK;
}

template <typename T>
bool MaskedSelectGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &workspace,
                                            const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  cuda_stream_ = reinterpret_cast<cudaStream_t>(stream_ptr);
  if (broadcast_size_ == 0) {
    return true;
  }

  auto input_ptr = GetDeviceAddress<T>(inputs, kIndex0);
  MS_EXCEPTION_IF_NULL(input_ptr);
  auto mask_ptr = GetDeviceAddress<bool>(inputs, kIndex1);
  MS_EXCEPTION_IF_NULL(mask_ptr);
  auto index_ptr = GetDeviceAddress<size_t>(workspace, kIndex0);  // save prefix sum of mask
  MS_EXCEPTION_IF_NULL(index_ptr);

  T *input_broadcast_ptr = nullptr;    // save broadcast result of input
  bool *mask_broadcast_ptr = nullptr;  // save broadcast result of mask
  if (input_broadcast_) {
    input_broadcast_ptr = GetDeviceAddress<T>(workspace, kIndex1);
    MS_EXCEPTION_IF_NULL(input_broadcast_ptr);
    if (mask_broadcast_) {
      mask_broadcast_ptr = GetDeviceAddress<bool>(workspace, kIndex2);
      MS_EXCEPTION_IF_NULL(mask_broadcast_ptr);
    }
  } else if (mask_broadcast_) {
    mask_broadcast_ptr = GetDeviceAddress<bool>(workspace, kIndex1);
    MS_EXCEPTION_IF_NULL(mask_broadcast_ptr);
  }
  auto output_ptr = GetDeviceAddress<T>(outputs, kIndex0);
  MS_EXCEPTION_IF_NULL(output_ptr);

  // kernel
  auto status = MaskedSelect(input_ptr, mask_ptr, index_ptr, input_shape_, mask_shape_, broadcast_shape_,
                             input_broadcast_ptr, mask_broadcast_ptr, output_ptr, device_id_, cuda_stream_);
  CHECK_CUDA_STATUS(status, kernel_name_);

  // The last element of index_ptr is the final output size of MaskedSelect.
  // e.g., the index(prefix sum) is [0, 0, 1, 2, 2], so the real_output_size_ is 2
  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(cudaMemcpyAsync(&real_output_size_, index_ptr + broadcast_size_ - 1,
                                                     sizeof(size_t), cudaMemcpyDeviceToHost, cuda_stream_),
                                     "MaskedSelect cudaMemcpyAsync failed.");
  return true;
}

void MaskedSelectGpuKernelMod::UpdateOutputShapeAndSize(const std::vector<KernelTensor *> &inputs,
                                                        const std::vector<KernelTensor *> &outputs) {
  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(cudaStreamSynchronize(cuda_stream_), "MaskedSelect cudaStreamSynchronize failed");
  std::vector<int64_t> new_output_shape = {SizeToLong(real_output_size_)};
  outputs[kIndex0]->SetShapeVector(new_output_shape);
  outputs[kIndex0]->set_size(real_output_size_ * UnitSizeInBytes(outputs[kIndex0]->dtype_id()));
}

std::vector<std::pair<KernelAttr, MaskedSelectGpuKernelMod::MaskedSelectFunc>> MaskedSelectGpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt8),
   &MaskedSelectGpuKernelMod::LaunchKernel<int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt16),
   &MaskedSelectGpuKernelMod::LaunchKernel<int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt32),
   &MaskedSelectGpuKernelMod::LaunchKernel<int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt64),
   &MaskedSelectGpuKernelMod::LaunchKernel<int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeFloat16),
   &MaskedSelectGpuKernelMod::LaunchKernel<half>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeFloat32),
   &MaskedSelectGpuKernelMod::LaunchKernel<float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeFloat64),
   &MaskedSelectGpuKernelMod::LaunchKernel<double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt8),
   &MaskedSelectGpuKernelMod::LaunchKernel<uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt16),
   &MaskedSelectGpuKernelMod::LaunchKernel<uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt32),
   &MaskedSelectGpuKernelMod::LaunchKernel<uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt64),
   &MaskedSelectGpuKernelMod::LaunchKernel<uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeBool),
   &MaskedSelectGpuKernelMod::LaunchKernel<bool>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeComplex64),
   &MaskedSelectGpuKernelMod::LaunchKernel<mindspore::utils::Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeComplex128),
   &MaskedSelectGpuKernelMod::LaunchKernel<mindspore::utils::Complex<double>>}};

std::vector<KernelAttr> MaskedSelectGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(
    func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
    [](const std::pair<KernelAttr, MaskedSelectGpuKernelMod::MaskedSelectFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, MaskedSelect, MaskedSelectGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
