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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_MULTILABEL_MARGIN_LOSS_GRAD_GPU_KERNEL_H
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_MULTILABEL_MARGIN_LOSS_GRAD_GPU_KERNEL_H

#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>
#include <map>
#include "mindspore/core/ops/grad/multilabel_margin_loss_grad.h"
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_class/multilabel_margin_loss_grad_helper.h"
namespace mindspore {
namespace kernel {
class MultilabelMarginLossGradGpuKernelMod : public NativeGpuKernelMod {
 public:
  MultilabelMarginLossGradGpuKernelMod() { attr_ptr_ = std::make_shared<cukernel::MultilabelMarginLossGradAttr>(); }
  ~MultilabelMarginLossGradGpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  std::unique_ptr<cukernel::GpuKernelHelperBase> helper_ptr_{nullptr};
  std::shared_ptr<cukernel::MultilabelMarginLossGradAttr> attr_ptr_{nullptr};
  int64_t batch_size_{0};
  int64_t class_num_{0};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_NN_MULTILABEL_MARGIN_LOSS_GRAD_GPU_KERNEL_H
