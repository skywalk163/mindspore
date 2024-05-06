/**
 * Copyright 2021 Huawei Technologies Co., Ltd.
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

#ifndef AICPU_KERNELS_NORMALIZED_BIAS_ADD_H_
#define AICPU_KERNELS_NORMALIZED_BIAS_ADD_H_
#define EIGEN_USE_THREADS
#define EIGEN_USE_SIMPLE_THREAD_POOL

#include "inc/ms_cpu_kernel.h"
#include "cpu_types.h"
#include "cpu_kernel/utils/bcast.h"

namespace aicpu {
class BiasAddCpuKernel : public CpuKernel {
 public:
  BiasAddCpuKernel() = default;
  ~BiasAddCpuKernel() = default;
  uint32_t Compute(CpuKernelContext &ctx) override;

 private:
  /**
   * @brief compute for all types
   * @param ctx cpu kernel context
   * @return status if success
   */
  template <typename T>
  uint32_t BiasAddCompute(CpuKernelContext &ctx);

  /**
   * @brief Check if input&output addr is aligned
   * @param calc_info data used to calculate
   * @return true: aligned, false: not aligned
   */
  bool AlignedCheck(const BCalcInfo &calc_info);

  template <int32_t RANK, typename T>
  uint32_t BiasAddCalculateWithAlignedCheck(CpuKernelContext &ctx, const BCalcInfo &calc_info);

  /**
   * @brief Eigen calculate for all types
   * @param calc_info data used to calculate
   */
  template <int32_t RANK, typename T, int32_t OPTION>
  uint32_t BiasAddCalculate(CpuKernelContext &ctx, const BCalcInfo &calc_info);
};
}  // namespace aicpu
#endif  // AICPU_KERNELS_NORMALIZED_BIAS_ADD_GRAD_H_