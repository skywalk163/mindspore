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

#include "cpu_kernel/ms_kernel/transpose.h"

#include <securec.h>

#include "context/inc/cpu_kernel_utils.h"
#include "unsupported/Eigen/CXX11/Tensor"
#include "utils/kernel_util.h"

namespace {
const uint32_t kOutputNum = 1;
const uint32_t kInputNum = 2;
const uint32_t kDim1 = 1;
const uint32_t kMinDim = 2;
const uint32_t kMaxDim = 8;
const uint32_t kIndex0 = 0;
const uint32_t kIndex1 = 1;
const uint32_t kIndex2 = 2;
const uint32_t kIndex3 = 3;
const uint32_t kIndex4 = 4;
const uint32_t kIndex5 = 5;
const uint32_t kIndex6 = 6;
const uint32_t kIndex7 = 7;
const char *kTranspose = "Transpose";

#define TRANSPOSE_COMPUTE_CASE(DTYPE, TYPE, CTX)                      \
  case (DTYPE): {                                                     \
    uint32_t result = TransposeCompute<TYPE>(CTX);                    \
    if (result != KERNEL_STATUS_OK) {                                 \
      CUST_KERNEL_LOG_ERROR(ctx, "Transpose kernel compute failed."); \
      return result;                                                  \
    }                                                                 \
    break;                                                            \
  }
}  // namespace

namespace aicpu {
uint32_t TransposeCpuKernel::GetTransposeValue(Tensor *tensor, std::vector<int64_t> &value) {
  auto type = tensor->GetDataType();
  if (type == DT_INT32) {
    auto data = reinterpret_cast<int32_t *>(tensor->GetData());
    for (unsigned int i = 0; i < tensor->NumElements(); i++) {
      value.push_back(static_cast<int64_t>(*(data + i)));
    }
  } else if (type == DT_INT64) {
    auto data = reinterpret_cast<int64_t *>(tensor->GetData());
    for (unsigned int i = 0; i < tensor->NumElements(); i++) {
      value.push_back(*(data + i));
    }
  } else {
    return KERNEL_STATUS_PARAM_INVALID;
  }
  return KERNEL_STATUS_OK;
}

uint32_t TransposeCpuKernel::Compute(CpuKernelContext &ctx) {
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, kInputNum, kOutputNum), "[%s] check input and output failed.",
                           kTranspose);
  CUST_KERNEL_HANDLE_ERROR(ctx, TransposeParamCheck(ctx), "[%s] check params failed.", kTranspose);
  auto x_type = ctx.Input(0)->GetDataType();
  switch (x_type) {
    TRANSPOSE_COMPUTE_CASE(DT_BOOL, bool, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_DOUBLE, double, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_UINT8, uint8_t, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_UINT16, uint16_t, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_UINT32, uint32_t, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_UINT64, uint64_t, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_INT8, int8_t, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_INT16, int16_t, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_INT32, int32_t, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_INT64, int64_t, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_FLOAT, float, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_FLOAT16, Eigen::half, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_COMPLEX64, std::complex<float>, ctx)
    TRANSPOSE_COMPUTE_CASE(DT_COMPLEX128, std::complex<double>, ctx)
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "Transpose kernel data type [%s] not support.", DTypeStr(x_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }

  return KERNEL_STATUS_OK;
}

uint32_t TransposeCpuKernel::TransposeParamCheck(CpuKernelContext &ctx) {
  std::vector<int64_t> shape_x = ctx.Input(kIndex0)->GetTensorShape()->GetDimSizes();
  std::vector<int64_t> shape_perm = ctx.Input(kIndex1)->GetTensorShape()->GetDimSizes();

  auto perm_tensor = ctx.Input(kIndex1);
  auto y_tensor = ctx.Output(kIndex0);

  CUST_KERNEL_CHECK_FALSE(ctx, (shape_perm.size() == kDim1), KERNEL_STATUS_PARAM_INVALID,
                          "Expected perm to be 1-D tensors , but got [%zu]-D tensors.", shape_x.size())
  CUST_KERNEL_CHECK_FALSE(ctx, (perm_tensor->NumElements() == (unsigned int)shape_x.size()),
                          KERNEL_STATUS_PARAM_INVALID, "Expected the size of perm to be [%zu], but got [%zu].",
                          shape_x.size(), perm_tensor->NumElements())
  CUST_KERNEL_CHECK_FALSE(ctx, (GetTransposeValue(perm_tensor, perm) == KERNEL_STATUS_OK), KERNEL_STATUS_PARAM_INVALID,
                          "perm must be either int32 or int64, but got [%s].",
                          DTypeStr(perm_tensor->GetDataType()).c_str())
  CUST_KERNEL_CHECK_FALSE(ctx, (shape_x.size() > kDim1), KERNEL_STATUS_PARAM_INVALID,
                          "Expected the dimension of x to be greater than 1-D, but got [%zu].", shape_x.size())

  std::vector<int64_t> shape_y;
  for (size_t i = 0; i < shape_x.size(); ++i) {
    int64_t perm_value = perm.at(i);
    if (shape_x.at(i) == 0) {
      CUST_KERNEL_CHECK_FALSE(ctx, (perm_value == 0), KERNEL_STATUS_PARAM_INVALID,
                              "Expected perm[%zu] == 0 (got %zu), when x shape[%zu] == 0.", i, perm_value, i)
    } else {
      CUST_KERNEL_CHECK_FALSE(ctx, (0 <= perm_value && perm_value <= (unsigned int)shape_x.size() - 1),
                              KERNEL_STATUS_PARAM_INVALID, "Expected perm[%zu] in [0, %zu], but got %zu.", i,
                              shape_x.size(), perm_value)
    }
    int64_t temp_value = 0;
    for (size_t j = 0; j < shape_x.size(); ++j) {
      if ((unsigned int)perm.at(j) == i) {
        break;
      } else {
        temp_value = j + 1;
        CUST_KERNEL_CHECK_FALSE(ctx, (temp_value < (unsigned int)shape_x.size()), KERNEL_STATUS_PARAM_INVALID,
                                "Expected perm value is unique.")
      }
    }
    shape_y.push_back(shape_x.at(perm_value));
  }
  y_tensor->GetTensorShape()->SetDimSizes(shape_y);

  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t TransposeCpuKernel::TransposeCompute(CpuKernelContext &ctx) {
  auto x_data = ctx.Input(kIndex0)->GetData();
  auto y_data = ctx.Output(kIndex0)->GetData();
  std::vector<int64_t> shape_x = ctx.Input(kIndex0)->GetTensorShape()->GetDimSizes();
  std::vector<int64_t> shape_y = ctx.Output(kIndex0)->GetTensorShape()->GetDimSizes();
  auto input_data = reinterpret_cast<T *>(x_data);
  auto output_data = reinterpret_cast<T *>(y_data);
  size_t input_dims = shape_x.size();
  if (input_dims < kMinDim || input_dims > kMaxDim) {
    CUST_KERNEL_LOG_ERROR(ctx, "[%s] : Unhandled input dimensions [%zu].", kTranspose, input_dims);
    return KERNEL_STATUS_INNER_ERROR;
  }
  size_t offset = kMaxDim - input_dims;
  std::vector<int64_t> bcast_shape_x(kMaxDim, kDim1);
  std::vector<int64_t> bcast_shape_y(kMaxDim, kDim1);
  for (size_t i = 0; i < input_dims; ++i) {
    bcast_shape_x[i + offset] = shape_x[i];
    bcast_shape_y[i + offset] = shape_y[i];
  }
  using Eigen_Tensor = Eigen::TensorMap<Eigen::Tensor<T, kMaxDim, Eigen::RowMajor>, Eigen::Aligned>;
  Eigen_Tensor input(input_data, bcast_shape_x.at(kIndex0), bcast_shape_x.at(kIndex1), bcast_shape_x.at(kIndex2),
                     bcast_shape_x.at(kIndex3), bcast_shape_x.at(kIndex4), bcast_shape_x.at(kIndex5),
                     bcast_shape_x.at(kIndex6), bcast_shape_x.at(kIndex7));
  Eigen_Tensor output(output_data, bcast_shape_y.at(kIndex0), bcast_shape_y.at(kIndex1), bcast_shape_y.at(kIndex2),
                      bcast_shape_y.at(kIndex3), bcast_shape_y.at(kIndex4), bcast_shape_y.at(kIndex5),
                      bcast_shape_y.at(kIndex6), bcast_shape_y.at(kIndex7));
  Eigen::array<Eigen::DenseIndex, kMaxDim> perm_compute;
  for (size_t j = 0; j < kMaxDim; ++j) {
    if (j < offset) {
      perm_compute[j] = j;
    } else {
      perm_compute[j] = perm.at(j - offset) + offset;
    }
  }
  output = input.shuffle(perm_compute);
  return KERNEL_STATUS_OK;
}

REGISTER_MS_CPU_KERNEL(kTranspose, TransposeCpuKernel);
}  // namespace aicpu
