/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All right reserved.
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
#ifndef AICPU_KERNELS_NORMALIZED_HARD_SIGMOID_H
#define AICPU_KERNELS_NORMALIZED_HARD_SIGMOID_H

#include "inc/ms_cpu_kernel.h"
#include "utils/bcast.h"

namespace aicpu {
class HardSigmoidCpuKernel : public CpuKernel {
 public:
  HardSigmoidCpuKernel() = default;
  ~HardSigmoidCpuKernel() override = default;

  uint32_t Compute(CpuKernelContext &ctx) override;

 private:
  template <typename T>
  uint32_t HardSigmoidCompute(CpuKernelContext &ctx);
};
}  // namespace aicpu
#endif
