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

#include "cpu_kernel/ms_kernel/sparse_dense_cwise_div.h"
#include <iostream>
#include "utils/kernel_util.h"
#include "utils/sparse_dense_cwise_utils.h"

namespace aicpu {
namespace {
const char *kSparseDenseCwiseDiv = "SparseDenseCwiseDiv";

#define SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DTYPE, TYPE, CTX)                   \
  case (DTYPE): {                                                               \
    uint32_t result = SparseDenseCwiseOpCompute<TYPE>(CTX);                     \
    if (result != KERNEL_STATUS_OK) {                                           \
      CUST_KERNEL_LOG_ERROR(ctx, "SparseDenseCwiseDiv kernel compute failed."); \
      return result;                                                            \
    }                                                                           \
    break;                                                                      \
  }
}  // namespace

uint32_t SparseDenseCwiseDivKernel::Compute(CpuKernelContext &ctx) {
  CUST_KERNEL_HANDLE_ERROR(ctx, CheckParams(ctx), "SparseDenseCwiseADiv check params failed.");

  auto data_type = ctx.Input(1)->GetDataType();
  switch (data_type) {
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_INT8, int8_t, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_INT16, int16_t, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_INT32, int32_t, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_INT64, int64_t, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_UINT8, uint8_t, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_UINT16, uint16_t, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_UINT32, uint32_t, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_UINT64, uint64_t, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_FLOAT16, Eigen::half, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_DOUBLE, double, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_FLOAT, float, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_COMPLEX64, std::complex<float>, ctx)
    SPARSE_DENSE_CWISE_DIV_COMPUTE_CASE(DT_COMPLEX128, std::complex<double>, ctx)
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "SparseDenseCwiseDiv kernel data type %s not support.", DTypeStr(data_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }

  return KERNEL_STATUS_OK;
}

REGISTER_MS_CPU_KERNEL(kSparseDenseCwiseDiv, SparseDenseCwiseDivKernel);
}  // namespace aicpu
