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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_TENSOR_TO_SEQUENCE_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_TENSOR_TO_SEQUENCE_CPU_KERNEL_H_
#include <vector>
#include <string>
#include <memory>
#include <map>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class TensorToSeqCpuKernelMod : public NativeCpuKernelMod {
 public:
  TensorToSeqCpuKernelMod() = default;
  explicit TensorToSeqCpuKernelMod(const std::string &kernel_type) : kernel_type_(kernel_type) {}
  ~TensorToSeqCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 protected:
  static std::map<std::string, std::vector<KernelAttr>> kernel_attr_lists_;
  static std::vector<KernelAttr> sequence_list_;
  static std::vector<KernelAttr> scalar_list_;

 private:
  std::string kernel_type_;
  bool is_empty_tensor_{false};
  bool is_sequence_input_{false};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_TENSOR_TO_SEQUENCE_CPU_KERNEL_H_
