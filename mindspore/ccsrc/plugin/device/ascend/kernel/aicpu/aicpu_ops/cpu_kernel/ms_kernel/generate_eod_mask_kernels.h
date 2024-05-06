/**
 * Copyright 2024 Huawei Technologies Co., Ltd
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
#ifndef AICPU_KERNELS_GENERATEEODMASK_H_
#define AICPU_KERNELS_GENERATEEODMASK_H_

#include <vector>
#include "inc/ms_cpu_kernel.h"

namespace aicpu {
class GenerateEodMaskCpuKernel : public CpuKernel {
 public:
  GenerateEodMaskCpuKernel() = default;
  ~GenerateEodMaskCpuKernel() override = default;

 protected:
  uint32_t Compute(CpuKernelContext &ctx) override;

 private:
  template <typename T, typename M>
  uint32_t ComputeKernel(CpuKernelContext &ctx, const int64_t &n_pos, const int64_t &eod_token_id,
                         const std::vector<int64_t> &n_step, const int64_t &circle, const bool &enable_mask_nfirst);
  static int64_t _compute_count;
  static int64_t _skip_step;
};
int64_t GenerateEodMaskCpuKernel::_compute_count = 0;
int64_t GenerateEodMaskCpuKernel::_skip_step = 0;
}  // namespace aicpu
#endif  // AICPU_KERNELS_GENERATEEODMASK_H_
