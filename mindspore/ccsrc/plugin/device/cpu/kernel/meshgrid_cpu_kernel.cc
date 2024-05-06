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

#include "plugin/device/cpu/kernel/meshgrid_cpu_kernel.h"
#include <algorithm>
#include <functional>
#include <string>
#include "mindspore/core/ops/meshgrid.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
bool MeshgridCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (inputs.size() != outputs.size()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', input and output size must be equal, but get " << inputs.size()
                  << " and " << outputs.size();
    return false;
  }
  if (inputs.size() <= 1) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', input size must greater than 1, but get " << inputs.size();
    return false;
  }

  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto pair = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!pair.first) {
    MS_LOG(ERROR) << "'" << kernel_name_ << "' does not support this kernel data type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[pair.second].second;

  auto indexing = GetValue<std::string>(primitive_->GetAttr(ops::kIndexing));
  if (indexing == "xy") {
    swap_indexing_ = true;
  } else if (indexing == "ij") {
    swap_indexing_ = false;
  } else {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the value of 'indexing' must be \"xy\" or \"ij\", but get "
                  << indexing;
    return false;
  }
  unit_size_ = abstract::TypeIdSize(inputs[kIndex0]->dtype_id());
  return true;
}

int MeshgridCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }

  input_shape_.clear();
  output_shape_.clear();

  // The input tensor must be 1-D tensor.
  for (auto &input : inputs) {
    auto shape = input->GetShapeVector();
    if (shape.size() != 1) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', each input tensor shape size must be 1, but get " << shape.size();
      return static_cast<int>(KRET_RESIZE_FAILED);
    }
    input_shape_.push_back(1);
    output_shape_.push_back(shape[0]);
  }
  if (swap_indexing_) {
    std::swap(output_shape_[0], output_shape_[1]);
  }

  for (auto &output : outputs) {
    auto shape = output->GetShapeVector();
    if (shape != output_shape_) {
      MS_LOG(ERROR) << "For '" << kernel_name_
                    << "', each output tensor shape should be the combination of all input tensor shape. But get the "
                       "shape of all inputs tensor shape: "
                    << output_shape_ << ", and the shape of output: " << shape;
      return static_cast<int>(KRET_RESIZE_FAILED);
    }
  }
  output_element_ = std::accumulate(output_shape_.begin(), output_shape_.end(), 1, std::multiplies<size_t>());
  workspace_size_list_.push_back(output_element_ * unit_size_);
  return static_cast<int>(KRET_OK);
}

template <typename T>
void MeshgridCpuKernelMod::Mul(const T *input1, const T *input2, T *out) {
  BroadcastIterator base_iter(input_shape_, output_shape_, output_shape_);
  auto task = [&input1, &input2, &out, &base_iter](size_t start, size_t end) {
    auto iter = base_iter;
    iter.SetPos(start);
    for (size_t i = start; i < end; i++) {
      if constexpr (std::is_same_v<T, bool>) {
        out[i] = static_cast<T>(input1[iter.GetInputPosA()] && input2[iter.GetInputPosB()]);
      } else {
        out[i] = static_cast<T>(input1[iter.GetInputPosA()] * input2[iter.GetInputPosB()]);
      }
      iter.GenNextPos();
    }
  };
  ParallelLaunchAutoSearch(task, output_element_, this, &parallel_search_info_);
}

template <typename T>
bool MeshgridCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &workspace,
                                        const std::vector<KernelTensor *> &outputs) {
  auto *ones_addr = reinterpret_cast<T *>(workspace[kIndex0]->device_ptr());
  MS_ERROR_IF_NULL_W_RET_VAL(ones_addr, false);
  auto task = [&ones_addr](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      ones_addr[i] = T(1);
    }
  };
  ParallelLaunchAutoSearch(task, output_element_, this, &parallel_search_info_);

  if (inputs.size() != outputs.size()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', input and output size must be equal, but get " << inputs.size()
                  << " and " << outputs.size();
    return false;
  }
  for (size_t i = 0; i < inputs.size(); i++) {
    auto input_index = (i <= 1 && swap_indexing_ == true) ? 1 - i : i;
    input_shape_[input_index] = output_shape_[input_index];
    auto *input = reinterpret_cast<T *>(inputs[i]->device_ptr());
    MS_ERROR_IF_NULL_W_RET_VAL(input, false);
    auto *output = reinterpret_cast<T *>(outputs[i]->device_ptr());
    MS_ERROR_IF_NULL_W_RET_VAL(output, false);
    Mul<T>(input, ones_addr, output);
    input_shape_[input_index] = 1;
  }
  return true;
}

std::vector<std::pair<KernelAttr, MeshgridCpuKernelMod::MeshgridFunc>> MeshgridCpuKernelMod::func_list_ = {
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeBool),
   &MeshgridCpuKernelMod::LaunchKernel<bool>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8),
   &MeshgridCpuKernelMod::LaunchKernel<uint8_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16),
   &MeshgridCpuKernelMod::LaunchKernel<uint16_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32),
   &MeshgridCpuKernelMod::LaunchKernel<uint32_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64),
   &MeshgridCpuKernelMod::LaunchKernel<uint64_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8),
   &MeshgridCpuKernelMod::LaunchKernel<int8_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16),
   &MeshgridCpuKernelMod::LaunchKernel<int16_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
   &MeshgridCpuKernelMod::LaunchKernel<int32_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &MeshgridCpuKernelMod::LaunchKernel<int64_t>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
   &MeshgridCpuKernelMod::LaunchKernel<float16>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   &MeshgridCpuKernelMod::LaunchKernel<float>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
   &MeshgridCpuKernelMod::LaunchKernel<double>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeComplex64),
   &MeshgridCpuKernelMod::LaunchKernel<complex64>},
  {KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeComplex128),
   &MeshgridCpuKernelMod::LaunchKernel<complex128>}};

std::vector<KernelAttr> MeshgridCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(
    func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
    [](const std::pair<KernelAttr, MeshgridCpuKernelMod::MeshgridFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Meshgrid, MeshgridCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
