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
#include "minimum_grad_grad.h"

#include <fstream>
#include <iostream>

#include "context/inc/cpu_kernel_utils.h"
#include "utils/eigen_tensor.h"
#include "utils/kernel_util.h"

namespace {
constexpr uint32_t kMinimumGradGradInputNum = 4;
constexpr uint32_t kMinimumGradGradOutputNum = 3;
const char *kMinimumGradGrad = "MinimumGradGrad";

#define MINIMUMGRADGRAD_COMPUTE_CASE(DTYPE, TYPE, CTX)                      \
  case (DTYPE): {                                                           \
    uint32_t result = MinimumGradGradCompute<TYPE>(CTX);                    \
    if (result != KERNEL_STATUS_OK) {                                       \
      CUST_KERNEL_LOG_ERROR(ctx, "MinimumGradGrad kernel compute failed."); \
      return result;                                                        \
    }                                                                       \
    break;                                                                  \
  }
}  // namespace

namespace aicpu {
uint32_t MinimumGradGradCpuKernel::Compute(CpuKernelContext &ctx) {
  // check params
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, kMinimumGradGradInputNum, kMinimumGradGradOutputNum),
                           "MinimumGradGrad check input and output number failed.");
  CUST_KERNEL_HANDLE_ERROR(ctx, MinimumGradGradParamCheck(ctx), "MinimumGradGrad check params failed.");
  auto data_type = ctx.Input(0)->GetDataType();
  switch (data_type) {
    MINIMUMGRADGRAD_COMPUTE_CASE(DT_INT32, int32_t, ctx)
    MINIMUMGRADGRAD_COMPUTE_CASE(DT_FLOAT, float, ctx)
    MINIMUMGRADGRAD_COMPUTE_CASE(DT_FLOAT16, Eigen::half, ctx)
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "The data type of input is not support, input data type is [%s].",
                            DTypeStr(data_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }
  return KERNEL_STATUS_OK;
}

uint32_t MinimumGradGradCpuKernel::MinimumGradGradParamCheck(CpuKernelContext &ctx) {
  // the non null of inputs and outputs has been verified in
  // NormalCheck
  Tensor *x1 = ctx.Input(0);
  Tensor *x2 = ctx.Input(1);
  Tensor *grad_y1 = ctx.Input(2);
  Tensor *grad_y2 = ctx.Input(3);

  // type check
  DataType grad_y1_type = grad_y1->GetDataType();
  DataType grad_y2_type = grad_y2->GetDataType();
  DataType x1_type = x1->GetDataType();
  DataType x2_type = x2->GetDataType();
  CUST_KERNEL_CHECK_FALSE(ctx, ((grad_y1_type == grad_y2_type) && (grad_y2_type == x1_type) && (x1_type == x2_type)),
                          KERNEL_STATUS_PARAM_INVALID,
                          "The data type of grad_y1 [%s], grad_y2 [%s], x1 [%s] and "
                          "x2 [%s] need to be same.",
                          DTypeStr(grad_y1_type).c_str(), DTypeStr(grad_y2_type).c_str(), DTypeStr(x1_type).c_str(),
                          DTypeStr(x2_type).c_str())
  // shape check
  auto grad_y1_shape = grad_y1->GetTensorShape()->GetDimSizes();
  auto grad_y2_shape = grad_y2->GetTensorShape()->GetDimSizes();
  auto x1_shape = x1->GetTensorShape()->GetDimSizes();
  auto x2_shape = x2->GetTensorShape()->GetDimSizes();
  CUST_KERNEL_CHECK_FALSE(ctx, grad_y1_shape == x1_shape, KERNEL_STATUS_PARAM_INVALID,
                          "Mismatch in shape of grad_y1 and x1.");
  CUST_KERNEL_CHECK_FALSE(ctx, grad_y2_shape == x2_shape, KERNEL_STATUS_PARAM_INVALID,
                          "Mismatch in shape of grad_y2 and x2.");
  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t MinimumGradGradCpuKernel::MinimumGradGradCompute(CpuKernelContext &ctx) {
  Tensor *input0_tensor = ctx.Input(0);
  Tensor *input1_tensor = ctx.Input(1);

  auto input0_shape = input0_tensor->GetTensorShape()->GetDimSizes();
  auto input1_shape = input1_tensor->GetTensorShape()->GetDimSizes();

  Bcast bcast(ctx, input0_shape, input1_shape);
  if (!bcast.IsValid()) {
    CUST_KERNEL_LOG_ERROR(ctx, "[%s] broadcast failed.", ctx.GetOpType().c_str());
    return KERNEL_STATUS_PARAM_INVALID;
  }
  return BcastCompute<T>(ctx, bcast);
}

template <typename T>
uint32_t MinimumGradGradCpuKernel::BcastCompute(CpuKernelContext &ctx, Bcast &bcast) {
  auto in0 = reinterpret_cast<T *>(ctx.Input(0)->GetData());
  auto in1 = reinterpret_cast<T *>(ctx.Input(1)->GetData());
  auto in2 = reinterpret_cast<T *>(ctx.Input(2)->GetData());
  auto in3 = reinterpret_cast<T *>(ctx.Input(3)->GetData());
  auto out0 = reinterpret_cast<T *>(ctx.Output(0)->GetData());
  auto out1 = reinterpret_cast<T *>(ctx.Output(1)->GetData());
  auto out2 = reinterpret_cast<T *>(ctx.Output(2)->GetData());
  *out0 = static_cast<T>(0);
  *out1 = static_cast<T>(0);
  int64_t data_num = ctx.Output(2)->NumElements();

  for (int64_t i = 0; i < data_num; ++i) {
    if (*(in0 + bcast.GetBroadcastXIndex(i)) <= *(in1 + bcast.GetBroadcastYIndex(i))) {
      *(out2 + i) = *(in2 + bcast.GetBroadcastXIndex(i));
    } else {
      *(out2 + i) = *(in3 + bcast.GetBroadcastYIndex(i));
    }
  }

  return KERNEL_STATUS_OK;
}

REGISTER_MS_CPU_KERNEL(kMinimumGradGrad, MinimumGradGradCpuKernel);
}  // namespace aicpu
