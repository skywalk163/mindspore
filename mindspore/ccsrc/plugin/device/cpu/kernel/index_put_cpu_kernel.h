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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_INDEX_PUT_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_INDEX_PUT_CPU_KERNEL_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class IndexPutCpuKernelMod : public NativeCpuKernelMod, public MatchKernelHelper<IndexPutCpuKernelMod> {
 public:
  IndexPutCpuKernelMod() = default;
  ~IndexPutCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  const std::vector<std::pair<KernelAttr, KernelRunFunc>> &GetFuncList() const override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override { return OpSupport(); }

 private:
  void CheckParams();
  std::vector<std::vector<int64_t>> Transpose(const std::vector<std::vector<int64_t>> &A) const;
  int64_t Multiplicative(const std::vector<int64_t> &tensorshapes, int64_t start, int64_t end) const;
  template <typename T, typename T0>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &outputs);
  template <typename T>
  void ComputeNospecial(T *x2, size_t x2_nums, std::vector<std::vector<int64_t>> indices_value, T *y, int accumulate);
  template <typename T>
  void ComputeSpecial(T *x2, size_t x2_nums, std::vector<std::vector<int64_t>> indices_value, T *y, int accumulate);
  std::vector<int64_t> x1_shape_;
  std::vector<int64_t> x2_shape_;
  std::vector<std::vector<int64_t>> indices_shape_;
  size_t inputs_nums = 0;
  int64_t accumulate{0};
  std::vector<TypeId> input_info_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_INDEX_PUT_CPU_KERNEL_H_
