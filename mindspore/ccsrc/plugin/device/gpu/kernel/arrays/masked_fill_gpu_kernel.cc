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

#include "plugin/device/gpu/kernel/arrays/masked_fill_gpu_kernel.h"
#include <functional>
#include <algorithm>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/masked_fill_impl.cuh"
#include "mindspore/core/ops/masked_fill.h"
#include "abstract/utils.h"

namespace mindspore {
namespace kernel {
constexpr int MAX_DIMS = 8;

template <typename T>
bool MaskedFillGpuKernelMod::LaunchKernel(const std::vector<AddressPtr> &inputs,
                                          const std::vector<AddressPtr> &workspace,
                                          const std::vector<AddressPtr> &outputs) {
  T *input_addr = GetDeviceAddress<T>(inputs, 0);
  bool *mask_addr = GetDeviceAddress<bool>(inputs, 1);
  T *value = GetDeviceAddress<T>(inputs, 2);
  T *output_addr = GetDeviceAddress<T>(outputs, 0);

  if (need_broadcast_) {
    BroadcastMaskedFill(inner_size_, lhs_shape_, rhs_shape_, output_shape_, input_addr, mask_addr, value, output_addr,
                        reinterpret_cast<cudaStream_t>(cuda_stream_));
  } else {
    ElewiseMaskedFill(inner_size_, output_num_, input_addr, mask_addr, value, output_addr,
                      reinterpret_cast<cudaStream_t>(cuda_stream_));
  }
  return true;
}

bool MaskedFillGpuKernelMod::Init(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
                                  const std::vector<KernelTensorPtr> &outputs) {
  kernel_name_ = base_operator->name();
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' does not support this kernel type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

void MaskedFillGpuKernelMod::BroadcastShape(const std::vector<size_t> &input_shape,
                                            const std::vector<size_t> &mask_shape,
                                            const std::vector<size_t> &output_shape) {
  lhs_shape_.resize(MAX_DIMS, 1);
  rhs_shape_.resize(MAX_DIMS, 1);
  output_shape_.resize(MAX_DIMS, 1);
  for (size_t i = 0; i < output_shape.size(); i++) {
    if (need_broadcast_) {
      if (i < MAX_DIMS) {
        output_shape_[i] = output_shape[i];
      } else {
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the index of output should be less than " << MAX_DIMS
                          << ", but got " << i;
      }
    }
    output_num_ *= output_shape[i];
  }

  size_t lhs_offset = output_shape.size() - input_shape.size();
  for (size_t j = 0; j < input_shape.size(); j++) {
    if (need_broadcast_) {
      if ((j + lhs_offset) < MAX_DIMS) {
        lhs_shape_[j + lhs_offset] = input_shape[j];
      } else {
        auto index = j + lhs_offset;
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the index of input cannot be less than 0 and greater than "
                          << MAX_DIMS << ", but got " << index;
      }
    }
  }

  size_t rhs_offset = output_shape.size() - mask_shape.size();
  for (size_t k = 0; k < mask_shape.size(); k++) {
    if (need_broadcast_) {
      if ((k + rhs_offset) < MAX_DIMS) {
        rhs_shape_[k + rhs_offset] = mask_shape[k];
      } else {
        auto index = k + rhs_offset;
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the index of mask cannot be less than 0 and greater than "
                          << MAX_DIMS << ", but got " << index;
      }
    }
  }
}

int MaskedFillGpuKernelMod::Resize(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
                                   const std::vector<KernelTensorPtr> &outputs,
                                   const std::map<uint32_t, tensor::TensorPtr> &) {
  ResetResource();
  int ret = KRET_OK;
  if ((ret = NativeGpuKernelMod::Resize(base_operator, inputs, outputs)) != 0) {
    return ret;
  }
  std::vector<int64_t> input_shape_vec = inputs.at(kIndex0)->GetShapeVector();
  std::vector<int64_t> mask_shape_vec = inputs.at(kIndex1)->GetShapeVector();
  std::vector<int64_t> value_shape_vec = inputs.at(kIndex2)->GetShapeVector();
  std::vector<int64_t> output_shape_vec = outputs.at(kIndex0)->GetShapeVector();
  std::vector<size_t> input_shape;
  std::vector<size_t> mask_shape;
  std::vector<size_t> value_shape;
  std::vector<size_t> output_shape;
  std::transform(input_shape_vec.begin(), input_shape_vec.end(), std::back_inserter(input_shape), LongToSize);
  std::transform(mask_shape_vec.begin(), mask_shape_vec.end(), std::back_inserter(mask_shape), LongToSize);
  std::transform(value_shape_vec.begin(), value_shape_vec.end(), std::back_inserter(value_shape), LongToSize);
  std::transform(output_shape_vec.begin(), output_shape_vec.end(), std::back_inserter(output_shape), LongToSize);
  is_null_input_ =
    CHECK_SHAPE_NULL(input_shape, kernel_name_, "input") || CHECK_SHAPE_NULL(mask_shape, kernel_name_, "mask") ||
    CHECK_SHAPE_NULL(value_shape, kernel_name_, "value") || CHECK_SHAPE_NULL(output_shape, kernel_name_, "output");
  if (is_null_input_) {
    return ret;
  }
  need_broadcast_ = common::AnfAlgo::IsTensorBroadcast(input_shape, mask_shape);
  if (need_broadcast_ && (input_shape.size() > MAX_DIMS || mask_shape.size() > MAX_DIMS)) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dimension of input and mask cannot be greater than "
                      << MAX_DIMS << ", but got input: " << input_shape.size() << ", mask: " << mask_shape.size();
  }
  size_t batch_size = value_shape.size();
  if (input_shape.size() < batch_size || mask_shape.size() < batch_size) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the dimension of input and mask should not be less than value's, but got input: "
                      << input_shape.size() << ", mask: " << mask_shape.size() << ", value:" << value_shape.size();
  }
  for (size_t i = 0; i < batch_size; i++) {
    if (input_shape[i] != mask_shape[i] && input_shape[i] != value_shape[i]) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the first " << batch_size
                        << " shape should be the same for input, mask and value, but got input shape: " << input_shape
                        << ", mask shape: " << mask_shape << ", value shape: " << value_shape;
    }
  }
  BroadcastShape(input_shape, mask_shape, output_shape);
  size_t rank_size = std::accumulate(value_shape.begin(), value_shape.end(), 1, std::multiplies<size_t>());
  inner_size_ = output_num_ / rank_size;
  return ret;
}

void MaskedFillGpuKernelMod::ResetResource() noexcept {
  need_broadcast_ = false;
  is_null_input_ = false;
  output_num_ = 1;
  inner_size_ = 1;
  lhs_shape_.clear();
  rhs_shape_.clear();
  output_shape_.clear();
  input_size_list_.clear();
  output_size_list_.clear();
  workspace_size_list_.clear();
}

std::vector<std::pair<KernelAttr, MaskedFillGpuKernelMod::MaskedFillFunc>> MaskedFillGpuKernelMod::func_list_ = {
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeBool)
     .AddInputAttr(kNumberTypeFloat16)
     .AddOutputAttr(kNumberTypeFloat16),
   &MaskedFillGpuKernelMod::LaunchKernel<half>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeBool)
     .AddInputAttr(kNumberTypeFloat32)
     .AddOutputAttr(kNumberTypeFloat32),
   &MaskedFillGpuKernelMod::LaunchKernel<float>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeInt8)
     .AddInputAttr(kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt8)
     .AddOutputAttr(kNumberTypeInt8),
   &MaskedFillGpuKernelMod::LaunchKernel<int8_t>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeInt32)
     .AddInputAttr(kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt32)
     .AddOutputAttr(kNumberTypeInt32),
   &MaskedFillGpuKernelMod::LaunchKernel<int32_t>}};

std::vector<KernelAttr> MaskedFillGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, MaskedFillFunc> &pair) { return pair.first; });
  return support_list;
}
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, MaskedFill, MaskedFillGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
