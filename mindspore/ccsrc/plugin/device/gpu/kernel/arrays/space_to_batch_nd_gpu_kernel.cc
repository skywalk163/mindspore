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

#include "plugin/device/gpu/kernel/arrays/space_to_batch_nd_gpu_kernel.h"
#include <utility>

namespace mindspore {
namespace kernel {
namespace {
template <typename T>
std::unique_ptr<cukernel::GpuKernelHelperBase> CreateSpaceToBatchNDKernelPtr(const std::string &kernel_name,
                                                                             const uint32_t &device_id) {
  return std::make_unique<cukernel::SpaceToBatchNDHelperGpuKernel<T>>(kernel_name, device_id);
}
using SpaceToBatchNDPtrCreatorFunc =
  std::function<std::unique_ptr<cukernel::GpuKernelHelperBase>(const std::string &, const uint32_t &)>;

const std::vector<std::pair<KernelAttr, SpaceToBatchNDPtrCreatorFunc>> kernel_attr = {
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8), CreateSpaceToBatchNDKernelPtr<int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16), CreateSpaceToBatchNDKernelPtr<int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32), CreateSpaceToBatchNDKernelPtr<int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64), CreateSpaceToBatchNDKernelPtr<int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8), CreateSpaceToBatchNDKernelPtr<uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16),
   CreateSpaceToBatchNDKernelPtr<uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32),
   CreateSpaceToBatchNDKernelPtr<uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64),
   CreateSpaceToBatchNDKernelPtr<uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
   CreateSpaceToBatchNDKernelPtr<half>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   CreateSpaceToBatchNDKernelPtr<float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
   CreateSpaceToBatchNDKernelPtr<double>}};
}  // namespace

bool SpaceToBatchNDGpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &workspace,
                                        const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  std::vector<void *> input_ptrs = ConvertPtrs(inputs);
  std::vector<void *> work_ptrs = ConvertPtrs(workspace);
  std::vector<void *> output_ptrs = ConvertPtrs(outputs);
  if (helper_ptr_->Process(input_ptrs, output_ptrs, work_ptrs, stream_ptr) != 0) {
    return false;
  }
  return true;
}

bool SpaceToBatchNDGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &outputs) {
  auto tensor_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(tensor_attr, GetOpSupport());
  if (!is_match) {
    return false;
  }
  attr_ptr_->block_shape = GetValue<std::vector<int64_t>>(primitive_->GetAttr("block_shape"));
  attr_ptr_->paddings = GetValue<std::vector<std::vector<int64_t>>>(primitive_->GetAttr("paddings"));
  helper_ptr_ = std::move(kernel_attr[index].second(kernel_name_, device_id_));
  helper_ptr_->SetKernelParam(attr_ptr_);

  return true;
}

int SpaceToBatchNDGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  for (const auto &input : inputs) {
    // If any input shape contains -1, means input shape is dynamic, so just return do nothing.
    auto input_shape = input->GetShapeVector();
    if (!IsValidShape(input_shape)) {
      return KRET_UNKNOWN_SHAPE;
    }
  }
  std::vector<std::vector<int64_t>> input_shapes;
  std::vector<std::vector<int64_t>> output_shapes;
  std::vector<int64_t> inp_shape = inputs[kIndex0]->GetShapeVector();
  std::vector<int64_t> out_shape = outputs[kIndex0]->GetShapeVector();
  input_shapes.emplace_back(inp_shape);
  output_shapes.emplace_back(out_shape);
  if (helper_ptr_->CalMemSize(input_shapes, output_shapes) == -1) {
    return KRET_RESIZE_FAILED;
  }
  output_size_list_ = helper_ptr_->GetOutputSizeList();
  workspace_size_list_ = helper_ptr_->GetWorkSizeList();
  return KRET_OK;
}

std::vector<KernelAttr> SpaceToBatchNDGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(kernel_attr.begin(), kernel_attr.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, SpaceToBatchNDPtrCreatorFunc> &item) { return item.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, SpaceToBatchND, SpaceToBatchNDGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
