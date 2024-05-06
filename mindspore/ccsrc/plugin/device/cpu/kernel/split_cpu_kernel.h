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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SPLIT_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SPLIT_CPU_KERNEL_H_

#include <vector>
#include <memory>
#include <thread>
#include <tuple>
#include <map>

#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "plugin/device/cpu/kernel/nnacl/base/split_base.h"

namespace mindspore {
namespace kernel {
class SplitCpuKernelMod : public NativeCpuKernelMod {
 public:
  SplitCpuKernelMod() = default;
  ~SplitCpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override {
    static std::vector<KernelAttr> support_list = {KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeInt64),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeInt32)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeInt32),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeInt16)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeInt16),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeInt8)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeInt8),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeUInt32)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeUInt32),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeUInt16)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeUInt16),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeUInt8)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeUInt8),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeUInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeUInt64),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeFloat32)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeFloat32),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeFloat16)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeFloat16),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeFloat64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeFloat64),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeComplex64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeComplex64),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeComplex128)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeComplex128),
                                                   KernelAttr()
                                                     .AddAllSameAttr(true)
                                                     .AddInputAttr(kNumberTypeBool)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeBool)};
    return support_list;
  }

 private:
  void CheckParam();
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs);
  template <typename T>
  void InitIOSize();

  using SplitFunc =
    std::function<bool(SplitCpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<KernelTensor *> &, const std::vector<kernel::KernelTensor *> &)>;
  using InitIOFunc = std::function<void(SplitCpuKernelMod *)>;
  static std::vector<std::tuple<KernelAttr, SplitFunc, InitIOFunc>> func_list_;
  SplitFunc kernel_func_;
  InitIOFunc init_io_func_;

  template <typename T>
  void LaunchSplit(T *input, T **output, size_t size);

  int64_t axis_{0};
  size_t output_num_{1};
  std::vector<int> input_shape_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SPLIT_CPU_KERNEL_H_
