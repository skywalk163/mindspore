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
#include "mindspore/core/abstract/utils.h"
#include "kernel/common_utils.h"

#include "plugin/device/cpu/hal/device/cpu_hash_table.h"
#include "plugin/device/cpu/kernel/map_tensor/map_tensor_erase_cpu_kernel.h"

namespace mindspore {
namespace kernel {
std::vector<std::pair<KernelAttr, MapTensorEraseCpuKernelMod::MapTensorEraseLaunchFunc>>
  MapTensorEraseCpuKernelMod::map_tensor_erase_func_list_ = {{KernelAttr()
                                                                .AddInputAttr(kObjectTypeMapTensorType)
                                                                .AddInputAttr(kNumberTypeInt32)
                                                                .AddOutputAttr(kObjectTypeMapTensorType),
                                                              &MapTensorEraseCpuKernelMod::LaunchKernel<int32_t>},
                                                             {KernelAttr()
                                                                .AddInputAttr(kObjectTypeMapTensorType)
                                                                .AddInputAttr(kNumberTypeInt64)
                                                                .AddOutputAttr(kObjectTypeMapTensorType),
                                                              &MapTensorEraseCpuKernelMod::LaunchKernel<int64_t>}};

std::vector<KernelAttr> MapTensorEraseCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(
    map_tensor_erase_func_list_.begin(), map_tensor_erase_func_list_.end(), std::back_inserter(support_list),
    [](const std::pair<KernelAttr, MapTensorEraseCpuKernelMod::MapTensorEraseLaunchFunc> &pair) { return pair.first; });
  return support_list;
}

bool MapTensorEraseCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &outputs) {
  // Check the inputs and outputs num.
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kMapTensorEraseInputNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kMapTensorEraseOutputNum, kernel_name_);

  // Check the kernel attr.
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }

  // Get kernel launch function.
  kernel_launch_func_ = map_tensor_erase_func_list_[index].second;
  input_key_type_size_ = abstract::TypeIdSize(kernel_attr.GetInputAttr(kIndex1).dtype);
  return true;
}

int MapTensorEraseCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  ResetResource();

  MS_EXCEPTION_IF_NULL(inputs.at(kIndex1));
  const auto &keys_shape = inputs.at(kIndex1)->GetShapeVector();
  if (IsDynamic(keys_shape)) {
    return KRET_UNKNOWN_SHAPE;
  }

  InitSizeLists(keys_shape);
  return KRET_OK;
}

template <typename KeyType>
bool MapTensorEraseCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                              const std::vector<KernelTensor *> &,
                                              const std::vector<KernelTensor *> &outputs) {
  // Check the inputs and outputs num.
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kMapTensorEraseInputNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kMapTensorEraseOutputNum, kernel_name_);

  // The real hash table should be accessed by user data.
  auto user_data = inputs[kIndex0]->user_data();
  MS_EXCEPTION_IF_NULL(user_data);

  auto hash_table_value_type = user_data->get<TypeId>(kHashTableValueType);
  TypeId value_type = *hash_table_value_type;
  if (value_type == kNumberTypeFloat32) {
    auto hash_table_ptr = user_data->get<device::cpu::CPUHashTable<KeyType, float>>(kUserDataData);
    if (hash_table_ptr == nullptr) {
      MS_LOG(EXCEPTION) << "Failed to get gpu hash table pointer with value type:" << value_type;
    }

    return hash_table_ptr->Erase(static_cast<KeyType *>(inputs.at(kIndex1)->device_ptr()),
                                 inputs.at(kIndex1)->size() / sizeof(KeyType), nullptr);
  } else {
    MS_LOG(EXCEPTION) << "GPU hash table does not support value type:" << value_type;
  }
  return false;
}

void MapTensorEraseCpuKernelMod::InitSizeLists(const ShapeVector &keys_shape) {
  // Return size 1 as the first input size and the output size for MapTensorErase. Real map tensor is assigned by
  // framework.
  output_size_list_.push_back(kSizeOne);

  auto keys_size = std::accumulate(keys_shape.begin(), keys_shape.end(), 1, std::multiplies{});
  MS_EXCEPTION_IF_ZERO("keys size", keys_size);
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, MapTensorErase, MapTensorEraseCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
