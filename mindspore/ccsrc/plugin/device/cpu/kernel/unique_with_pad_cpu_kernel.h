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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_UNIQUE_WITH_PAD_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_UNIQUE_WITH_PAD_CPU_KERNEL_H_

#include <memory>
#include <unordered_map>
#include <vector>
#include <map>
#include <functional>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "plugin/device/cpu/kernel/unique_cpu_kernel.h"
#include "ops/op_utils.h"

namespace mindspore {
namespace kernel {
inline static constexpr size_t kUniqueWithPadInputsNum = 2;
inline static constexpr size_t kUniqueWithPadOutputsNum = 2;
inline static constexpr size_t kPadNumIndex = 1;
inline static constexpr size_t kInputIndex = 0;
class UniqueWithPadCpuKernelMod : public UniqueCpuKernelMod {
 public:
  UniqueWithPadCpuKernelMod() = default;
  ~UniqueWithPadCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override {
    dtype_ = inputs[0]->dtype_id();
    auto batch_rank = ops::get_batch_rank(primitive_);
    if (batch_rank < 0) {
      return false;
    }
    batch_rank_ = static_cast<size_t>(batch_rank);
    return true;
  }

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override {
    static std::vector<KernelAttr> support_list = {KernelAttr()
                                                     .AddInputAttr(kNumberTypeInt32)
                                                     .AddInputAttr(kNumberTypeInt32)
                                                     .AddOutputAttr(kNumberTypeInt32)
                                                     .AddOutputAttr(kNumberTypeInt32),
                                                   KernelAttr()
                                                     .AddInputAttr(kNumberTypeInt64)
                                                     .AddInputAttr(kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeInt64)
                                                     .AddOutputAttr(kNumberTypeInt64),
                                                   KernelAttr()
                                                     .AddInputAttr(kNumberTypeFloat32)
                                                     .AddInputAttr(kNumberTypeFloat32)
                                                     .AddOutputAttr(kNumberTypeFloat32)
                                                     .AddOutputAttr(kNumberTypeInt32)};
    return support_list;
  }

 private:
  template <typename T>
  void PadOutput(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs,
                 const std::vector<size_t> &start);
  // Disable update output shape because parent class 'UniqueCpuKernelMod'(for Unique op) need update output shape, but
  // UniqueWithPad doesn't need.
  bool IsNeedUpdateOutputShapeAndSize() override { return false; }
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_UNIQUE_WITH_PAD_CPU_KERNEL_H_
