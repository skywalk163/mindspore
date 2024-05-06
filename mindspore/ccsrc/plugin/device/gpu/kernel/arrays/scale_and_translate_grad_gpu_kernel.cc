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

#include "plugin/device/gpu/kernel/arrays/scale_and_translate_grad_gpu_kernel.h"
#include <utility>

namespace mindspore {
namespace kernel {
namespace {
template <typename T>
std::unique_ptr<cukernel::GpuKernelHelperBase> CreateScaleAndTranslateGradKernelPtr(const std::string &kernel_name,
                                                                                    const uint32_t &device_id) {
  return std::make_unique<cukernel::ScaleAndTranslateGradHelperGpuKernel<T>>(kernel_name, device_id);
}
using ScaleAndTranslateGradPtrCreatorFunc =
  std::function<std::unique_ptr<cukernel::GpuKernelHelperBase>(const std::string &, const uint32_t &)>;

const std::vector<std::pair<KernelAttr, ScaleAndTranslateGradPtrCreatorFunc>> kernel_attr = {
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddOutputAttr(kNumberTypeFloat32),
   CreateScaleAndTranslateGradKernelPtr<float>}};
}  // namespace

bool ScaleAndTranslateGradGpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
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

bool ScaleAndTranslateGradGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                             const std::vector<KernelTensor *> &outputs) {
  auto tensor_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(tensor_attr, GetOpSupport());
  if (!is_match) {
    return false;
  }
  attr_ptr_->kernel_type = GetValue<std::string>(primitive_->GetAttr("kernel_type"));
  attr_ptr_->antialias = GetValue<bool>(primitive_->GetAttr("antialias"));
  helper_ptr_ = std::move(kernel_attr[index].second(kernel_name_, device_id_));
  helper_ptr_->SetKernelParam(attr_ptr_);
  return true;
}

int ScaleAndTranslateGradGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                              const std::vector<KernelTensor *> &outputs) {
  std::vector<std::vector<int64_t>> input_shapes;
  std::vector<std::vector<int64_t>> output_shapes;
  std::vector<int64_t> inp_shape_grads = inputs[0]->GetShapeVector();
  std::vector<int64_t> inp_shape_images = inputs[1]->GetShapeVector();
  std::vector<int64_t> inp_shape_scale = inputs[kIndex2]->GetShapeVector();
  std::vector<int64_t> inp_shape_translation = inputs[kIndex3]->GetShapeVector();
  std::vector<int64_t> out_shape = outputs[0]->GetShapeVector();
  for (const auto &inputtest : inputs) {
    auto inshape = inputtest->GetShapeVector();
    if (!IsValidShape(inshape)) {
      return KRET_UNKNOWN_SHAPE;
    }
  }
  input_shapes.emplace_back(inp_shape_grads);
  input_shapes.emplace_back(inp_shape_images);
  input_shapes.emplace_back(inp_shape_scale);
  input_shapes.emplace_back(inp_shape_translation);
  output_shapes.emplace_back(out_shape);
  if (helper_ptr_->CalMemSize(input_shapes, output_shapes) == -1) {
    return KRET_RESIZE_FAILED;
  }
  output_size_list_ = helper_ptr_->GetOutputSizeList();
  workspace_size_list_ = helper_ptr_->GetWorkSizeList();
  return KRET_OK;
}

std::vector<KernelAttr> ScaleAndTranslateGradGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(
    kernel_attr.begin(), kernel_attr.end(), std::back_inserter(support_list),
    [](const std::pair<KernelAttr, ScaleAndTranslateGradPtrCreatorFunc> &item) { return item.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, ScaleAndTranslateGrad, ScaleAndTranslateGradGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
