/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
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

#ifndef AICPU_KERNELS_NORMALIZED_SMOOTH_L1_LOSS_GRAD_V2_H_
#define AICPU_KERNELS_NORMALIZED_SMOOTH_L1_LOSS_GRAD_V2_H_

#include <string>
#include "inc/ms_cpu_kernel.h"
#include "utils/bcast.h"

namespace aicpu {
class SmoothL1LossGradV2CpuKernel : public CpuKernel {
 public:
  SmoothL1LossGradV2CpuKernel() = default;
  ~SmoothL1LossGradV2CpuKernel() = default;

 protected:
  uint32_t Compute(CpuKernelContext &ctx) override;

 private:
  uint32_t ParamCheck(CpuKernelContext &ctx);
  uint32_t AttributesCheck(CpuKernelContext &ctx);
  template <typename T>
  uint32_t ComputeMean(CpuKernelContext &ctx);
  template <typename T>
  uint32_t ComputeSum(CpuKernelContext &ctx);
  template <typename T>
  uint32_t ComputeNone(CpuKernelContext &ctx);
  std::string reduction{"mean"};
};
}  // namespace aicpu
#endif  // AICPU_KERNELS_NORMALIZED_SMOOTH_L1_LOSS_GRAD_V2_H_
