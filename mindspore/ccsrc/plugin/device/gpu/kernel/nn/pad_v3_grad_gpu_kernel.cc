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

#include "plugin/device/gpu/kernel/nn/pad_v3_grad_gpu_kernel.h"
namespace mindspore {
namespace kernel {
namespace {
const std::vector<std::string> mode_list = {ops::kReflect, ops::kEdge, ops::kCircular};
template <typename T, typename S>
std::unique_ptr<cukernel::GpuKernelHelperBase> CreatePadV3GradKernelPtr(const std::string &kernel_name,
                                                                        const uint32_t &device_id) {
  return std::make_unique<cukernel::PadV3GradHelperGpuKernel<T, S>>(kernel_name, device_id);
}
using PadV3GradPtrCreatorFunc =
  std::function<std::unique_ptr<cukernel::GpuKernelHelperBase>(const std::string &, const uint32_t &)>;

const std::vector<std::pair<KernelAttr, PadV3GradPtrCreatorFunc>> kernel_attr = {
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   CreatePadV3GradKernelPtr<double, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   CreatePadV3GradKernelPtr<float, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   CreatePadV3GradKernelPtr<half, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   CreatePadV3GradKernelPtr<int64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   CreatePadV3GradKernelPtr<int32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   CreatePadV3GradKernelPtr<int16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   CreatePadV3GradKernelPtr<int8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   CreatePadV3GradKernelPtr<uint64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   CreatePadV3GradKernelPtr<uint32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   CreatePadV3GradKernelPtr<uint16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   CreatePadV3GradKernelPtr<uint8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   CreatePadV3GradKernelPtr<Complex<float>, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   CreatePadV3GradKernelPtr<Complex<double>, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat64),
   CreatePadV3GradKernelPtr<double, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32),
   CreatePadV3GradKernelPtr<float, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat16),
   CreatePadV3GradKernelPtr<half, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt64),
   CreatePadV3GradKernelPtr<int64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
   CreatePadV3GradKernelPtr<int32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt16),
   CreatePadV3GradKernelPtr<int16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt8),
   CreatePadV3GradKernelPtr<int8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt64),
   CreatePadV3GradKernelPtr<uint64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt32),
   CreatePadV3GradKernelPtr<uint32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt16),
   CreatePadV3GradKernelPtr<uint16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt8),
   CreatePadV3GradKernelPtr<uint8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeComplex64),
   CreatePadV3GradKernelPtr<Complex<float>, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeComplex128),
   CreatePadV3GradKernelPtr<Complex<double>, int64_t>},
};
}  // namespace

bool PadV3GradGpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &workspace,
                                   const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  std::vector<void *> input_ptrs = ConvertPtrs(inputs);
  std::vector<void *> work_ptrs = ConvertPtrs(workspace);
  std::vector<void *> output_ptrs = ConvertPtrs(outputs);

  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(
    cudaMemsetAsync(output_ptrs[0], 0, outputs[0]->size(), reinterpret_cast<cudaStream_t>(stream_ptr)),
    "failed to set cuda memory with zeros.");

  if (helper_ptr_->Process(input_ptrs, output_ptrs, work_ptrs, stream_ptr) != 0) {
    return false;
  }
  return true;
}

bool PadV3GradGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  auto tensor_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(tensor_attr, GetOpSupport());
  if (!is_match) {
    return false;
  }
  MS_ERROR_IF_NULL(attr_ptr_);
  attr_ptr_->mode = GetValue<std::string>(primitive_->GetAttr(ops::kMode));
  const bool is_mode_available = std::find(mode_list.begin(), mode_list.end(), attr_ptr_->mode) != mode_list.end();
  if (is_mode_available == false) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the 'mode' should be, 'reflect' or 'edge', but got "
                      << attr_ptr_->mode;
  }
  attr_ptr_->paddings_contiguous = GetValue<bool>(primitive_->GetAttr("paddings_contiguous"));
  helper_ptr_ = std::move(kernel_attr[index].second(kernel_name_, device_id_));
  helper_ptr_->SetKernelParam(attr_ptr_);
  return true;
}

int PadV3GradGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &outputs) {
  auto ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }

  std::vector<int64_t> paddings_val;
  auto paddings_type = inputs[kIndex1]->dtype_id();
  if (paddings_type == kNumberTypeInt32) {
    std::vector<int32_t> paddings_arg = inputs[kIndex1]->GetValueWithCheck<std::vector<int32_t>>();
    for (size_t i = 0; i < paddings_arg.size(); ++i) {
      paddings_val.push_back(static_cast<int64_t>(paddings_arg[i]));
    }
  } else if (paddings_type == kNumberTypeInt64) {
    paddings_val = inputs[kIndex1]->GetValueWithCheck<std::vector<int64_t>>();
  } else {
    MS_LOG(ERROR) << "For Padv3, the paddings value type should be int64 or int32, but got " << paddings_type;
    return KRET_RESIZE_FAILED;
  }

  int64_t paddings_size = SizeToLong(paddings_val.size());
  auto prim = primitive_;
  MS_EXCEPTION_IF_NULL(prim);
  if (!GetValue<bool>(prim->GetAttr("paddings_contiguous"))) {
    constexpr int64_t nTwo = 2;
    std::vector<int64_t> tmp = paddings_val;
    for (int64_t i = 0; i < paddings_size; ++i) {
      if (i % nTwo == 0) {
        paddings_val[LongToSize(i)] = tmp[LongToSize(i) / nTwo];
      } else {
        paddings_val[LongToSize(i)] = tmp[LongToSize((i + paddings_size) / nTwo)];
      }
    }
  }
  attr_ptr_->paddings = paddings_val;

  std::vector<std::vector<int64_t>> input_shapes;
  std::vector<std::vector<int64_t>> output_shapes;
  std::vector<int64_t> x_shape = inputs[0]->GetShapeVector();
  std::vector<int64_t> padding_shape = inputs[1]->GetShapeVector();
  std::vector<int64_t> out_shape = outputs[0]->GetShapeVector();
  input_shapes.emplace_back(x_shape);
  input_shapes.emplace_back(padding_shape);
  output_shapes.emplace_back(out_shape);
  if (helper_ptr_->CalMemSize(input_shapes, output_shapes) == -1) {
    return KRET_RESIZE_FAILED;
  }
  output_size_list_ = helper_ptr_->GetOutputSizeList();
  workspace_size_list_ = helper_ptr_->GetWorkSizeList();
  return KRET_OK;
}

std::vector<KernelAttr> PadV3GradGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(kernel_attr.begin(), kernel_attr.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, PadV3GradPtrCreatorFunc> &item) { return item.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, PadV3Grad, PadV3GradGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
