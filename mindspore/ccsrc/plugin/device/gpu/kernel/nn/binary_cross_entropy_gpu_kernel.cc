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
#include "plugin/device/gpu/kernel/nn/binary_cross_entropy_gpu_kernel.h"
#include <map>
#include "mindspore/core/ops/binary_cross_entropy.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/loss_with_reduction_impl.cuh"
#include "ops/binary_cross_entropy.h"
#include "ops/op_name.h"

namespace mindspore {
namespace kernel {
bool BinaryCrossEntropyGpuKernelMod::BinaryCrossEntropyGpuKernelMod::Launch(
  const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
  const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  if (dtype_ == kNumberTypeFloat16) {
    LaunchKernel<half>(inputs, workspace, outputs, stream_ptr);
  } else if (dtype_ == kNumberTypeFloat32) {
    LaunchKernel<float>(inputs, workspace, outputs, stream_ptr);
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dtype of input must be float16 or float32, but got "
                      << TypeIdToType(dtype_)->ToString();
  }
  return true;
}

template <typename T>
void BinaryCrossEntropyGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                                  const std::vector<KernelTensor *> &workspace,
                                                  const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  T *input_x = GetDeviceAddress<T>(inputs, kIndex0);
  T *input_y = GetDeviceAddress<T>(inputs, kIndex1);
  T *weight = nullptr;
  if (inputs[kIndex2]->type_id() != kMetaTypeNone) {
    weight = GetDeviceAddress<T>(inputs, kIndex2);
  }
  T *loss = GetDeviceAddress<T>(outputs, kIndex0);
  T *tmp_loss = GetDeviceAddress<T>(workspace, kIndex0);
  if (input_size_ > 0) {
    auto status = BinaryCrossEntropyLoss(input_size_, reduction_, input_x, input_y, weight, loss, tmp_loss,
                                         reinterpret_cast<cudaStream_t>(stream_ptr));
    CHECK_CUDA_STATUS(status, kernel_name_);
  }
}

bool BinaryCrossEntropyGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                          const std::vector<KernelTensor *> &outputs) {
  const auto reduction = ops::BinaryCrossEntropy::get_reduction(primitive_->GetAttr(ops::kReduction));
  if (reduction == Reduction::NONE) {
    reduction_ = ReductionMode::kNone;
  } else if (reduction == Reduction::MEAN) {
    reduction_ = ReductionMode::kMean;
  } else {
    reduction_ = ReductionMode::kSum;
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto match = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!match.first) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it does not support this kernel type: " << kernel_attr;
  }

  dtype_ = inputs[kIndex0]->dtype_id();
  return true;
}

int BinaryCrossEntropyGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                           const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  auto input_shape = inputs[kIndex0]->GetShapeVector();
  input_size_ = SizeOf(input_shape);
  workspace_size_ = sizeof(TypeIdToType(inputs[kIndex0]->dtype_id()));
  if (reduction_ != ReductionMode::kNone) {
    workspace_size_ *= input_size_;
  }
  workspace_size_list_.push_back(workspace_size_);
  return KRET_OK;
}

std::vector<KernelAttr> BinaryCrossEntropyGpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> kernel_attr_list = {KernelAttr()
                                                       .AddInputAttr(kNumberTypeFloat16)
                                                       .AddInputAttr(kNumberTypeFloat16)
                                                       .AddOptionalInputAttr(kNumberTypeFloat16)
                                                       .AddOutputAttr(kNumberTypeFloat16),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeFloat32)
                                                       .AddInputAttr(kNumberTypeFloat32)
                                                       .AddOptionalInputAttr(kNumberTypeFloat32)
                                                       .AddOutputAttr(kNumberTypeFloat32)};

  return kernel_attr_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, BinaryCrossEntropy, BinaryCrossEntropyGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
