/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#ifndef AICPU_KERNELS_NORMALIZED_MATRIX_SOLVE_LS_H_
#define AICPU_KERNELS_NORMALIZED_MATRIX_SOLVE_LS_H_

#include <complex>
#include "inc/ms_cpu_kernel.h"
#include "cpu_types.h"
#include "utils/bcast.h"

namespace aicpu {
class MatrixSolveLsCpuKernel : public CpuKernel {
 public:
  MatrixSolveLsCpuKernel() = default;
  ~MatrixSolveLsCpuKernel() override = default;

 protected:
  uint32_t Compute(CpuKernelContext &ctx) override;

 private:
  template <typename T>
  void RealCholeskySingleCompute(T *aptr, T *bptr, T *xptr, double *l2, int64_t m, int64_t k, int64_t n);

  template <typename T>
  uint32_t RealCholesky(CpuKernelContext &ctx);

  template <typename T>
  void RealQrSingleCompute(T *aptr, T *bptr, T *xptr, int64_t m, int64_t k, int64_t n);

  template <typename T>
  uint32_t RealQr(CpuKernelContext &ctx);

  template <typename T>
  void ComplexCholeskySingleCompute(std::complex<T> *aptr, std::complex<T> *bptr, std::complex<T> *xptr, double *l2,
                                    int64_t m, int64_t k, int64_t n);

  template <typename T>
  uint32_t ComplexCholesky(CpuKernelContext &ctx);

  template <typename T>
  void ComplexQrSingleCompute(std::complex<T> *aptr, std::complex<T> *bptr, std::complex<T> *xptr, int64_t m, int64_t k,
                              int64_t n);

  template <typename T>
  uint32_t ComplexQr(CpuKernelContext &ctx);
};
}  // namespace aicpu
#endif
