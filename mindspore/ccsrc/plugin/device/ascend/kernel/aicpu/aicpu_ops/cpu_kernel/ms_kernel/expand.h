/**
 * Copyright (c) 2022-2022 Huawei Technologies Co., Ltd.  All rights reserved.
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

#ifndef AICPU_KERNELS_NORMALIZED_EXPAND_H_
#define AICPU_KERNELS_NORMALIZED_EXPAND_H_

#include "inc/ms_cpu_kernel.h"
#include "unsupported/Eigen/CXX11/Tensor"
#include "utils/bcast.h"

namespace aicpu {
class ExpandCpuKernel : public CpuKernel {
 public:
  ExpandCpuKernel() = default;
  ~ExpandCpuKernel() override = default;

 protected:
  uint32_t Compute(CpuKernelContext &ctx) override;

 private:
  template <int32_t RANK, typename T, int32_t OPTION>
  uint32_t BroadcastCompute(CpuKernelContext &ctx, BCalcInfo &calc_info);
  bool AlignedCheck(const BCalcInfo &calc_info);
  template <int32_t RANK, typename T>
  uint32_t ExpandCalculateWithAlignedCheck(CpuKernelContext &ctx, BCalcInfo &calc_info);

  template <typename T>
  uint32_t ExpandCompute(CpuKernelContext &ctx);
};
}  // namespace aicpu
#endif
