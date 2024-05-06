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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_SEQUENCE_ADD_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_SEQUENCE_ADD_CPU_KERNEL_H_
#include <vector>
#include <memory>
#include <utility>
#include <map>
#include <string>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "mindspore/core/ops/sequence_add.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class SequenceAddCpuKernelMod : public NativeCpuKernelMod {
 public:
  SequenceAddCpuKernelMod() = default;
  explicit SequenceAddCpuKernelMod(const std::string &kernel_type) : kernel_type_(kernel_type) {}
  ~SequenceAddCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);

  template <typename T>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &workspace,
                    const std::vector<kernel::KernelTensor *> &outputs);

 protected:
  std::vector<KernelAttr> GetOpSupport() override;

  using SequenceAddFunc =
    std::function<bool(SequenceAddCpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &, const std::vector<kernel::KernelTensor *> &)>;

  static std::vector<std::pair<KernelAttr, SequenceAddFunc>> func_list_;
  SequenceAddFunc kernel_func_;

 private:
  std::string kernel_type_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_SEQUENCE_ADD_CPU_KERNEL_H_
