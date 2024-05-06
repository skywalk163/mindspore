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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_CTCLOSS_V2_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_CTCLOSS_V2_CPU_KERNEL_H_

#include <cmath>
#include <memory>
#include <map>
#include <utility>
#include <vector>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class CTCLossV2CpuKernelMod : public NativeCpuKernelMod, public MatchKernelHelper<CTCLossV2CpuKernelMod> {
 public:
  CTCLossV2CpuKernelMod() = default;
  ~CTCLossV2CpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, workspace, outputs);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  const std::vector<std::pair<KernelAttr, KernelRunFunc>> &GetFuncList() const override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override { return OpSupport(); }

 private:
  struct SoftParam {
    int64_t input_length;
    int64_t target_length;
    int64_t offset;
    int64_t batch;
  };
  template <typename target_t>
  inline int64_t GetBlankPaddedTarget(const target_t *target, int64_t offset, int64_t idx) const {
    constexpr int64_t interval = 2;
    if (idx % interval == 0) {
      return blank_;
    } else {
      return target[offset + (idx / interval)];
    }
  }
  template <typename S, typename T>
  void LossCompute(const S *log_probs_p, S *log_alpha_p, const T *tar_p, SoftParam params) const;
  template <typename T>
  bool IndexProcessing(const T *in_len_p, const T *tar_len_p, std::vector<int64_t> *target_offsets);
  // Variables for the operator itself
  int64_t blank_{0};
  // Stands for T
  int64_t time_series_{0};
  // Stands for N
  int64_t batch_sizes_{0};
  // Stands for C
  int64_t num_labels_{0};
  // Stands for S
  int64_t max_target_length_{0};
  template <typename T, typename S>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &workspace,
                    const std::vector<kernel::KernelTensor *> &outputs);
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_DROPOUT_ND_CPU_KERNEL_H_
