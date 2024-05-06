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
#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_BATCH_NORM_GRAD_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_BATCH_NORM_GRAD_CPU_KERNEL_H_
#include <map>
#include <memory>
#include <vector>
#include "plugin/device/cpu/kernel/mkldnn/mkl_cpu_kernel.h"

namespace mindspore {
namespace kernel {
class BatchNormGradCpuKernelMod : public MKLCpuKernelMod {
 public:
  BatchNormGradCpuKernelMod() = default;
  ~BatchNormGradCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  void InitWorkspaceSize(const std::vector<KernelTensor *> &inputs);

  bool is_train_{false};
  float epsilon_{1e-5};
  float momentum_{0.9};
  int64_t batch_size_{0};
  int64_t channel_{0};
  int64_t hw_size_{0};
  int64_t nhw_size_{0};
  enum format_ { N, C, H, W };
  enum input_list_ { Y_BACKPROP, X, SCALE, SAVE_MEAN, SAVE_VARIANCE, RESERVE };
  enum workspace_list_ { SCALE_BIAS, DIFF_SCALE_BIAS };
  enum output_list_ { DX, DSCALE, DBIAS };
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_BATCH_NORM_GRAD_CPU_KERNEL_H_
