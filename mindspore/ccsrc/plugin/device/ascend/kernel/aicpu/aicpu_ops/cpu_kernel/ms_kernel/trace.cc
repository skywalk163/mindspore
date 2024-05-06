/**
 * Copyright 2021-2023 Huawei Technologies Co., Ltd
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

#include "cpu_kernel/ms_kernel/trace.h"
#include <algorithm>
#include "cstring"
#include "securec.h"
#include "context/inc/cpu_kernel_utils.h"
#include "utils/eigen_tensor.h"
#include "utils/kernel_util.h"

namespace {
const uint32_t kInputNum = 1;
const uint32_t kOutputNum = 1;
const uint32_t InputShapeDim = 2;
const uint32_t OutputShapeDim = 1;
const uint64_t OutputShapeDimSize = 1;
const char *kTrace = "Trace";

#define TRACE_COMPUTE_CASE(DTYPE, INPUT, OUTPUT, CTX, TYPE)       \
  case (DTYPE): {                                                 \
    uint32_t result = TraceCompute<TYPE>(INPUT, OUTPUT, CTX);     \
    if (result != KERNEL_STATUS_OK) {                             \
      CUST_KERNEL_LOG_ERROR(ctx, "Trace kernel compute failed."); \
      return result;                                              \
    }                                                             \
    break;                                                        \
  }
}  // namespace

namespace aicpu {
uint32_t TraceCpuKernel::Compute(CpuKernelContext &ctx) {
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, kInputNum, kOutputNum), "Trace check input and output number failed.");

  Tensor *input_tensor = ctx.Input(0);
  CUST_KERNEL_CHECK_NULLPTR(ctx, input_tensor->GetData(), KERNEL_STATUS_PARAM_INVALID, "Trace get input data failed.")
  CUST_KERNEL_CHECK_NULLPTR(ctx, input_tensor->GetTensorShape(), KERNEL_STATUS_PARAM_INVALID,
                            "Trace get input shape failed")

  if (input_tensor->GetTensorShape()->GetDims() != InputShapeDim) {
    CUST_KERNEL_LOG_ERROR(ctx, "Trace input dim must be 2!");
    return KERNEL_STATUS_PARAM_INVALID;
  }

  // check output tensor
  Tensor *output_tensor = ctx.Output(0);
  CUST_KERNEL_CHECK_NULLPTR(ctx, output_tensor, KERNEL_STATUS_PARAM_INVALID, "Trace get output failed.")
  CUST_KERNEL_CHECK_NULLPTR(ctx, output_tensor->GetData(), KERNEL_STATUS_PARAM_INVALID, "Trace get output data failed.")
  CUST_KERNEL_CHECK_NULLPTR(ctx, output_tensor->GetTensorShape(), KERNEL_STATUS_PARAM_INVALID,
                            "Trace get output shape failed")

  auto input_dtype = input_tensor->GetDataType();
  auto output_dtype = output_tensor->GetDataType();
  switch (input_dtype) {
    TRACE_COMPUTE_CASE(DT_INT8, input_tensor, output_tensor, ctx, int8_t)
    TRACE_COMPUTE_CASE(DT_UINT8, input_tensor, output_tensor, ctx, uint8_t)
    TRACE_COMPUTE_CASE(DT_INT16, input_tensor, output_tensor, ctx, int16_t)
    TRACE_COMPUTE_CASE(DT_UINT16, input_tensor, output_tensor, ctx, uint16_t)
    TRACE_COMPUTE_CASE(DT_INT32, input_tensor, output_tensor, ctx, int32_t)
    TRACE_COMPUTE_CASE(DT_UINT32, input_tensor, output_tensor, ctx, uint32_t)
    TRACE_COMPUTE_CASE(DT_INT64, input_tensor, output_tensor, ctx, int64_t)
    TRACE_COMPUTE_CASE(DT_UINT64, input_tensor, output_tensor, ctx, uint64_t)
    TRACE_COMPUTE_CASE(DT_FLOAT16, input_tensor, output_tensor, ctx, Eigen::half)
    TRACE_COMPUTE_CASE(DT_FLOAT, input_tensor, output_tensor, ctx, float)
    TRACE_COMPUTE_CASE(DT_DOUBLE, input_tensor, output_tensor, ctx, double)
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "Trace kernel data type [%u] not support", output_dtype);
      return KERNEL_STATUS_PARAM_INVALID;
  }
  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t TraceCpuKernel::TraceCompute(Tensor *input, Tensor *output, CpuKernelContext &ctx) {
  auto inputDataAddr = reinterpret_cast<T *>(input->GetData());
  auto outputDataAddr = reinterpret_cast<T *>(output->GetData());
  auto input_shape = ctx.Input(0)->GetTensorShape();
  int64_t inputLine = input_shape->GetDimSize(0);
  int64_t inputCol = input_shape->GetDimSize(1);
  auto min_shape = std::min(inputLine, inputCol);
  auto output_size = output->GetDataSize();
  auto ret = memset_s(outputDataAddr, output_size, 0, sizeof(T));
  if (ret != EOK) {
    CUST_KERNEL_LOG_ERROR(ctx, "For 'Trace', memset_s failed, ret=%d.", ret);
    return KERNEL_STATUS_INNER_ERROR;
  }
  for (int64_t i = 0; i < min_shape; i++) {
    *(outputDataAddr) += *(inputDataAddr + i * inputCol + i);
  }
  return KERNEL_STATUS_OK;
}

REGISTER_MS_CPU_KERNEL(kTrace, TraceCpuKernel);
}  // namespace aicpu
