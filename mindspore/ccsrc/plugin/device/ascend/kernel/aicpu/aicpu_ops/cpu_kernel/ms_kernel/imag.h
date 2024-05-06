/**
 * Copyright (C)  2020-2021. Huawei Technologies Co., Ltd. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the Apache License Version 2.0.You may not use this file except in compliance with the License.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Apache License for more details at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * @brief
 *
 * @version 1.0
 *
 */

#ifndef AICPU_KERNELS_NORMALIZED_REAL_H_
#define AICPU_KERNELS_NORMALIZED_REAL_H_

#include "inc/ms_cpu_kernel.h"

namespace aicpu {
class ImagCpuKernel : public CpuKernel {
 public:
  ImagCpuKernel() = default;
  ~ImagCpuKernel() override = default;

 protected:
  uint32_t Compute(CpuKernelContext &ctx) override;

 private:
  uint32_t ImagCheck(CpuKernelContext &ctx);

  template <typename T, typename t>
  uint32_t ImagCompute(CpuKernelContext &ctx);
};
}  // namespace aicpu
#endif
