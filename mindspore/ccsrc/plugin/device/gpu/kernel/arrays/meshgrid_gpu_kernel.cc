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
#include "plugin/device/gpu/kernel/arrays/meshgrid_gpu_kernel.h"
#include <algorithm>
#include "mindspore/core/ops/math_ops.h"
#include "mindspore/core/ops/meshgrid.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/binary_ops_impl.cuh"
#include "plugin/device/gpu/kernel/math/broadcast_public.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/elementwise/eltwise_ops_impl.cuh"

namespace mindspore {
namespace kernel {
bool MeshgridGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  std::string indexing = GetValue<std::string>(primitive_->GetAttr("indexing"));
  if (indexing == "xy") {
    swap_indexing_ = true;
  } else if (indexing == "ij") {
    swap_indexing_ = false;
  } else {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the value of 'indexing' must be \"xy\" or \"ij\", but got "
                  << indexing;
    return false;
  }
  auto data_type = inputs.at(kIndex0)->dtype_id();
  data_size_ = GetTypeByte(TypeIdToType(data_type));

  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto pair = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!pair.first) {
    MS_LOG(ERROR) << "'" << kernel_name_ << "' does not support this kernel data type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[pair.second].second;
  return true;
}

int MeshgridGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  int ret = KRET_OK;
  if ((ret = KernelMod::Resize(inputs, outputs)) != 0) {
    return ret;
  }

  input_shapes_.clear();
  input_size_ = 1;
  input_count_ = static_cast<size_t>(inputs.size());
  for (size_t i = 0; i < input_count_; i++) {
    auto input_shape = inputs[i]->GetShapeVector();
    if (input_shape.size() < 1) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of input[" << i << "] cannot be less than 1, "
                    << "but got " << input_shape.size();
      return KRET_RESIZE_FAILED;
    }
    size_t input_size = input_shape[0];
    input_shapes_.push_back(input_size);
    input_size_ *= input_size;
  }

  output_size_ = 1;
  output_count_ = static_cast<size_t>(outputs.size());

  // inferred shape swaps output shape for us if needed
  output_shape_ = outputs[kIndex0]->GetShapeVector();
  is_null_input_ = CHECK_SHAPE_NULL(output_shape_, kernel_name_, "output");
  if (is_null_input_) {
    workspace_size_list_.push_back(output_size_ * data_size_);
    return KRET_OK;
  }

  if (output_count_ != input_count_) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the number of inputs and outputs must be the same, but got the number of inputs: "
                  << input_count_ << ", the number of outputs: " << output_count_;
    return KRET_RESIZE_FAILED;
  }

  output_size_ = SizeOf(output_shape_);
  workspace_size_list_.push_back(output_size_ * data_size_);
  return KRET_OK;
}

template <typename T>
bool MeshgridGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &workspace,
                                        const std::vector<KernelTensor *> &outputs) {
  if (is_null_input_) {
    return true;
  }
  T *ones_device = GetDeviceAddress<T>(workspace, 0);
  UnaryOpsCudaFunc<ElwiseOpType::kOnesLike, T, T>(output_size_, static_cast<T *>(nullptr), ones_device,
                                                  reinterpret_cast<cudaStream_t>(cuda_stream_));
  std::vector<int64_t> simplified_in0_shape;
  std::vector<int64_t> simplified_in1_shape;
  std::vector<int64_t> simplified_out_shape;
  for (size_t i = 0; i < outputs.size(); i++) {
    T *input_device = GetDeviceAddress<T>(inputs, i);
    T *output_device = GetDeviceAddress<T>(outputs, i);
    std::vector<int64_t> broadcasted_input_shape(input_shapes_.size(), 1);
    broadcasted_input_shape[i] = input_shapes_[i];
    if (swap_indexing_ && i <= 1) {
      std::swap(broadcasted_input_shape[0], broadcasted_input_shape[1]);
    }
    SimplifyBinaryBroadcastShape(broadcasted_input_shape, output_shape_, output_shape_, &simplified_in0_shape,
                                 &simplified_in1_shape, &simplified_out_shape);
    bool is_broadcast = IsBinaryBroadcast(simplified_in0_shape, simplified_in1_shape);
    BinaryOpWithBroadcastCudaFunc<BinaryOpType::kMul, T, T, T>(
      is_broadcast, simplified_in0_shape, simplified_in1_shape, simplified_out_shape, input_device, ones_device,
      output_device, device_id_, reinterpret_cast<cudaStream_t>(cuda_stream_));
  }
  return true;
}

template <typename T>
using Complex = mindspore::utils::Complex<T>;

std::vector<std::pair<KernelAttr, MeshgridGpuKernelMod::MeshgridFunc>> MeshgridGpuKernelMod::func_list_ = {
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeBool),
   &MeshgridGpuKernelMod::LaunchKernel<bool>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
   &MeshgridGpuKernelMod::LaunchKernel<half>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   &MeshgridGpuKernelMod::LaunchKernel<float>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
   &MeshgridGpuKernelMod::LaunchKernel<double>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8),
   &MeshgridGpuKernelMod::LaunchKernel<uint8_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16),
   &MeshgridGpuKernelMod::LaunchKernel<uint16_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32),
   &MeshgridGpuKernelMod::LaunchKernel<uint32_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64),
   &MeshgridGpuKernelMod::LaunchKernel<uint64_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8),
   &MeshgridGpuKernelMod::LaunchKernel<int8_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16),
   &MeshgridGpuKernelMod::LaunchKernel<int16_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
   &MeshgridGpuKernelMod::LaunchKernel<int32_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &MeshgridGpuKernelMod::LaunchKernel<int64_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeComplex64),
   &MeshgridGpuKernelMod::LaunchKernel<Complex<float>>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeComplex128),
   &MeshgridGpuKernelMod::LaunchKernel<Complex<double>>},
};

std::vector<KernelAttr> MeshgridGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, MeshgridFunc> &item) { return item.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, Meshgrid, MeshgridGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
