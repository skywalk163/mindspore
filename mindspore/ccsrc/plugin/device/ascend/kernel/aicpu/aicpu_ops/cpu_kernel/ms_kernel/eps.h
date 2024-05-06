/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
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
#ifndef AICPU_KERNELS_EPS_H
#define AICPU_KERNELS_EPS_H

#include "inc/ms_cpu_kernel.h"

namespace aicpu {
class EpsCpuKernel : public CpuKernel {
 public:
  EpsCpuKernel() = default;
  ~EpsCpuKernel() override = default;

  uint32_t Compute(CpuKernelContext &ctx) override;

 private:
  template <typename T>
  uint32_t EpsPartCompute(CpuKernelContext &ctx);

  template <typename T>
  void SpecialEpsOutput(int64_t start, int64_t end, T *output_data, T value);
};
}  // namespace aicpu
#endif
