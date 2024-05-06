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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RMSPROP_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RMSPROP_CPU_KERNEL_H_

#include <map>
#include <string>
#include <vector>
#include <utility>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
class RMSPropCpuKernelMod : public NativeCpuKernelMod {
 public:
  RMSPropCpuKernelMod() = default;
  explicit RMSPropCpuKernelMod(const std::string &kernel_type) : kernel_type_(kernel_type) {}
  ~RMSPropCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, workspace, outputs);
  }

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  void LaunchRMSPropUnuseCenter(T *variable, T *mean_square, T *moment, T *gradients, float *learning_rate,
                                float *decay, float *momentum, float *epsilon);
  template <typename T>
  void LaunchRMSPropUseCenter(T *variable, T *mean_square, T *moment, T *gradients, T *mean_gradients, T *momentum,
                              T *learning_rate, T *decay, T *epsilon);
  template <typename T>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &workspace,
                    const std::vector<kernel::KernelTensor *> &outputs);
  int CalElements(std::vector<int64_t> var_shape, std::vector<int64_t> lr_shape, int ret);
  using RMSPropFunc =
    std::function<bool(RMSPropCpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &, const std::vector<kernel::KernelTensor *> &)>;
  static std::map<std::string, std::vector<std::pair<KernelAttr, RMSPropFunc>>> func_list_;
  RMSPropFunc kernel_func_;

  size_t size_{1};
  bool use_center_{false};
  int64_t batch_size_{1};
  int64_t batch_rank_{0};
  int64_t input_elements_{0};
  TypeId dtype_{kTypeUnknown};
  std::string kernel_type_{"Unknown"};
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RMSPROP_CPU_KERNEL_H_
