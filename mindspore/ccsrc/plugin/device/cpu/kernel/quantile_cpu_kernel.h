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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_QUANTILE_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_QUANTILE_CPU_KERNEL_H_

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class QuantileCpuKernelMod : public NativeCpuKernelMod, public MatchKernelHelper<QuantileCpuKernelMod> {
 public:
  QuantileCpuKernelMod() = default;
  ~QuantileCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, workspace, outputs);
  }
  uint32_t MaybeWrapDim(int dim, int dim_post_expr) const;
  template <typename T>
  void ParallelRun(int64_t last_shape_size, uint64_t q_size, const std::vector<T> &sorted, T *output_addr,
                   T *q_addrs) const;
  template <typename T>
  void DoQuantile(int64_t last_shape_size, uint64_t q_size, const std::vector<T> &sorted, T *output_addr, T *q_addrs,
                  uint64_t start, uint64_t end) const;
  const std::vector<std::pair<KernelAttr, KernelRunFunc>> &GetFuncList() const override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override { return OpSupport(); }

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &workspace,
                    const std::vector<kernel::KernelTensor *> &outputs);
  int dim_ = 0;
  bool keep_dims_ = false;
  std::vector<int64_t> input_shape_;
  std::vector<int64_t> q_shape_;
  size_t total_ = 0;
  bool ignore_nan_ = false;
  bool has_nan_ = false;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_QUANTILE_CPU_KERNEL_H_
