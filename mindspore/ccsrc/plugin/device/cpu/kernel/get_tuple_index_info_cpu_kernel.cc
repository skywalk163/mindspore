/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/get_tuple_index_info_cpu_kernel.h"
#include <algorithm>
#include <utility>
#include <string>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "utils/ms_utils.h"
#include "include/common/thread_pool.h"
#include "ops/op_name.h"
#include "mindspore/core/ops/get_tuple_index_info.h"

namespace mindspore {
namespace kernel {
static const size_t max_indices_num = 8;
bool GetTupleIndexInfoCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                         const std::vector<KernelTensor *> &outputs) {
  tuple_index_types_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(kAttrTupleIndexTypes));
  if (primitive_->HasAttr(kAttrTupleIndexInfoType)) {
    tuple_index_info_type_ = GetValue<string>(primitive_->GetAttr(kAttrTupleIndexInfoType));
  }
  expand_dims_count_ = GetValue<int64_t>(primitive_->GetAttr(kAttrExpandDimsCnt));
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int GetTupleIndexInfoCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                          const std::vector<KernelTensor *> &outputs) {
  (void)KernelMod::Resize(inputs, outputs);
  data_shapes_ = GetShapes(inputs);
  return KRET_OK;
}

static inline void CheckCopy(void *dest, size_t destMax, const void *src, size_t count, const string &kernel_name) {
  if (destMax == 0) {
    if (memset_s(dest, sizeof(int64_t), 0, sizeof(int64_t)) != EOK) {
      MS_LOG(EXCEPTION) << kernel_name << " memset error";
    }
    return;
  }
  if (memcpy_s(dest, destMax, src, count) != EOK) {
    MS_LOG(EXCEPTION) << "For " << kernel_name << ", memcpy error";
  }
}

bool GetTupleIndexInfoCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                                 const std::vector<KernelTensor *> &workspace,
                                                 const std::vector<KernelTensor *> &outputs) {
  const auto *input1 = static_cast<int64_t *>(inputs[kIndex1]->device_ptr());
  ShapeVector broadcast_shape;
  ShapeVector final_shape;
  ShapeVector index_tensor_new_shape;
  std::vector<ShapeVector> tensor_indices_shapes;
  ShapeVector slice_shapes;
  size_t valid_tensor_nums = 0;
  ShapeVector data_shape = data_shapes_[kIndex0];
  for (size_t i = 0; i < tuple_index_types_.size(); i++) {
    if (tuple_index_types_[i] == kMetaTypeEllipsis) {
      valid_tensor_nums = data_shape.size() + LongToSize(expand_dims_count_);
      break;
    } else if (tuple_index_types_[i] != kTypeUnknown) {
      valid_tensor_nums += 1;
    }
  }
  for (size_t i = 0; i < valid_tensor_nums; i++) {
    ShapeVector tensor_data_shape = data_shapes_[i + kIndex2];
    (void)tensor_indices_shapes.emplace_back(tensor_data_shape);
  }
  size_t fancy_position = LongToSize(input1[0]);
  auto new_slice_shapes = ops::GetTupleIndexInfo::ConstGetTupleIndexInfo(
    data_shape, tensor_indices_shapes, tuple_index_types_, &broadcast_shape, &final_shape, &index_tensor_new_shape,
    &fancy_position, tuple_index_info_type_);

  CheckCopy(static_cast<int64_t *>(outputs[0]->device_ptr()), sizeof(int64_t), &fancy_position, sizeof(int64_t),
            kernel_name_);
  std::vector<std::vector<int64_t>> out_datas = {broadcast_shape, index_tensor_new_shape, final_shape};
  (void)out_datas.insert(out_datas.end(), new_slice_shapes.begin(), new_slice_shapes.end());
  const size_t indices_size = final_shape.size();
  for (size_t i = 0; i < max_indices_num - new_slice_shapes.size(); i++) {
    (void)out_datas.emplace_back(ShapeVector(indices_size, 1));
  }

  for (size_t i = 0; i < out_datas.size(); i++) {
    const size_t out_size = out_datas[i].size() * sizeof(int64_t);
    CheckCopy(static_cast<int64_t *>(outputs[i + 1]->device_ptr()), out_size, out_datas[i].data(), out_size,
              kernel_name_);
  }
  return true;
}

bool GetTupleIndexInfoCpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                           const std::vector<KernelTensor *> &workspace,
                                           const std::vector<KernelTensor *> &outputs) {
  return kernel_func_(this, inputs, workspace, outputs);
}

std::vector<std::pair<KernelAttr, GetTupleIndexInfoCpuKernelMod::GetTupleIndexInfoFunc>>
  GetTupleIndexInfoCpuKernelMod::func_list_ = {};

std::vector<KernelAttr> GetTupleIndexInfoCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  std::vector<TypeId> data_type_ids = {kNumberTypeFloat16,   kNumberTypeFloat32,   kNumberTypeFloat64, kNumberTypeInt8,
                                       kNumberTypeInt16,     kNumberTypeInt32,     kNumberTypeInt64,   kNumberTypeUInt8,
                                       kNumberTypeUInt16,    kNumberTypeUInt32,    kNumberTypeUInt64,  kNumberTypeBool,
                                       kNumberTypeComplex64, kNumberTypeComplex128};
  (void)std::transform(data_type_ids.begin(), data_type_ids.end(), std::back_inserter(func_list_),
                       [](TypeId data_type_id) -> std::pair<KernelAttr, GetTupleIndexInfoFunc> {
                         auto kernel_attr = KernelAttr();
                         (void)kernel_attr.AddInputAttr(data_type_id);
                         (void)kernel_attr.AddInputAttr(kObjectTypeNumber, kNumberTypeInt64);
                         for (size_t i = 0; i < max_indices_num; i++) {
                           (void)kernel_attr.AddInputAttr(kNumberTypeInt64);
                         }
                         const size_t output_size = 12;
                         kernel_attr.AddOutputAttr(kObjectTypeNumber, kNumberTypeInt64);
                         for (size_t i = 1; i < output_size; i++) {
                           (void)kernel_attr.AddOutputAttr(kObjectTypeTuple, kNumberTypeInt64);
                         }

                         return {kernel_attr, &GetTupleIndexInfoCpuKernelMod::LaunchKernel};
                       });

  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, GetTupleIndexInfoFunc> &item) { return item.first; });
  return support_list;
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, GetTupleIndexInfo, GetTupleIndexInfoCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
