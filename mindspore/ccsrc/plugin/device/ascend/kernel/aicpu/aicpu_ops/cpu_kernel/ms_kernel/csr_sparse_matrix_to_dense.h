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
#ifndef AICPU_KERNELS_NORMALIZED_CSR_SPARSE_MATRIX_TO_DENSE_H_
#define AICPU_KERNELS_NORMALIZED_CSR_SPARSE_MATRIX_TO_DENSE_H_

#include "Eigen/Core"
#include "inc/ms_cpu_kernel.h"

namespace aicpu {

class CSRSparseMatrixToDenseCpuKernel : public CpuKernel {
 public:
  ~CSRSparseMatrixToDenseCpuKernel() = default;
  uint32_t Compute(CpuKernelContext &ctx) override;

 private:
  template <typename indiceT, typename valueT>
  uint32_t DoCompute(CpuKernelContext &ctx);
};

}  // namespace aicpu
#endif
