/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
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
#include "cpu_kernel/ms_kernel/index_fill.h"

#include <securec.h>
#include <map>
#include <utility>
#include <algorithm>

#include "Eigen/Core"
#include "context/inc/cpu_kernel_utils.h"
#include "cpu_types.h"
#include "inc/kernel_log.h"
#include "context/common/status.h"
#include "utils/kernel_util.h"

namespace {
const uint32_t kNumInput = 4;
const uint32_t kNumOutput = 1;
const char *kIndexFill = "IndexFill";

// when input data size is more than kParallelDataNum, use Parallel func
const uint32_t kParallelDataNum = 16 * 1024;
const uint32_t kParallelDataNumMid = 128 * 1024;

#define INDEXFILL_COMPUTE_CASE(DTYPE, TYPE, CTX)                      \
  case (DTYPE): {                                                     \
    uint32_t result = DoCompute<TYPE>(CTX);                           \
    if (result != KERNEL_STATUS_OK) {                                 \
      CUST_KERNEL_LOG_ERROR(ctx, "IndexFill kernel compute failed."); \
      return result;                                                  \
    }                                                                 \
    break;                                                            \
  }
}  // namespace

namespace aicpu {
uint32_t IndexFillCpuKernel::GetInputAndCheck(CpuKernelContext &ctx) {
  // check params
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, kNumInput, kNumOutput),
                           "IndexFill check input and output number failed.");
  // get input Tensors
  for (uint32_t i = 0; i < kNumInput; ++i) {
    Tensor *tensor = ctx.Input(i);
    inputs_.push_back(tensor);
  }
  // get output Tensors
  Tensor *tensor = ctx.Output(0);
  outputs_.push_back(tensor);

  dim_data_type_ = inputs_[1]->GetDataType();
  DataType index_type = inputs_[2]->GetDataType();

  if (dim_data_type_ != DT_INT32 && dim_data_type_ != DT_INT64) {
    CUST_KERNEL_LOG_ERROR(ctx, "IndexFill: Expected dtype int32 or int64 for dim.");
    return KERNEL_STATUS_PARAM_INVALID;
  }
  if (index_type != DT_INT32) {
    CUST_KERNEL_LOG_ERROR(ctx, "IndexFill: Expected dtype int32 for index.");
    return KERNEL_STATUS_PARAM_INVALID;
  }

  return KERNEL_STATUS_OK;
}

template <typename T>
void IndexFillCpuKernel::SpecialCompute(int64_t start, int64_t end, const int32_t input_dim,
                                        std::map<int32_t, bool> &index_dict) {
  auto *input_x = reinterpret_cast<T *>(inputs_[0]->GetData());
  auto *input_value = reinterpret_cast<T *>(inputs_[3]->GetData());
  auto *output_y = reinterpret_cast<T *>(outputs_[0]->GetData());
  int32_t x_dim_nums = inputs_[0]->GetTensorShape()->GetDims();
  auto x_dims = inputs_[0]->GetTensorShape()->GetDimSizes();

  int32_t dim_flag;
  if (x_dim_nums != 0) {
    dim_flag = input_dim % x_dim_nums + 1;
  } else {
    dim_flag = 0;
  }

  int32_t remain_dims = 1;
  if (dim_flag == x_dim_nums) {
    if (dim_flag != 0) {
      remain_dims = x_dims[input_dim];
    }
    for (int64_t i = start; i < end; i++) {
      int32_t index_flag = i % remain_dims;
      std::map<int32_t, bool>::iterator f = index_dict.find(index_flag);
      if (f != index_dict.end()) {
        output_y[i] = *input_value;
      } else {
        output_y[i] = input_x[i];
      }
    }
  } else {
    for (int32_t i = input_dim + 1; i < x_dim_nums; i++) {
      remain_dims *= x_dims[i];
    }
    for (int64_t i = start; i < end; i++) {
      int32_t index_flag = (i / remain_dims) % x_dims[input_dim];
      std::map<int32_t, bool>::iterator f = index_dict.find(index_flag);
      if (f != index_dict.end()) {
        output_y[i] = *input_value;
      } else {
        output_y[i] = input_x[i];
      }
    }
  }
}

template <typename T>
uint32_t IndexFillCpuKernel::SpecialComputeParallel(CpuKernelContext &ctx, const uint32_t &data_num,
                                                    const int32_t input_dim, std::map<int32_t, bool> &index_dict) {
  uint32_t min_core_num = 1;
  uint32_t max_core_num = std::max(min_core_num, aicpu::CpuKernelUtils::GetCPUNum(ctx) - kResvCpuNum);

  if (data_num <= kParallelDataNumMid) {
    max_core_num = std::min(max_core_num, 4U);  // up to 4 cpu cores
  }
  if (max_core_num > data_num) {
    max_core_num = data_num;
  }
  if (max_core_num == 0) {
    CUST_KERNEL_LOG_ERROR(ctx, "The number of available CPU cores must be greater than 0!");
    return KERNEL_STATUS_INNER_ERROR;
  }

  auto sharder_index_fill = [&](int64_t start, int64_t end) { SpecialCompute<T>(start, end, input_dim, index_dict); };

  CUST_KERNEL_HANDLE_ERROR(ctx, CpuKernelUtils::ParallelFor(ctx, data_num, data_num / max_core_num, sharder_index_fill),
                           "IndexFill Compute failed.");
  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t IndexFillCpuKernel::DoCompute(CpuKernelContext &ctx) {
  int32_t input_dim;

  if (dim_data_type_ == DT_INT32) {
    input_dim = *reinterpret_cast<int32_t *>(inputs_[1]->GetData());
  } else {
    input_dim = static_cast<int32_t>(*reinterpret_cast<int64_t *>(inputs_[1]->GetData()));
  }
  int32_t *input_2 = reinterpret_cast<int32_t *>(inputs_[2]->GetData());

  int32_t x_dim_nums = inputs_[0]->GetTensorShape()->GetDims();
  auto x_dims = inputs_[0]->GetTensorShape()->GetDimSizes();

  uint32_t data_num = outputs_[0]->NumElements();
  int64_t index_num = inputs_[2]->GetTensorShape()->NumElements();

  int32_t real_input_dim = input_dim;
  if (input_dim < 0) {
    real_input_dim = input_dim + x_dim_nums;
  }

  std::map<int32_t, bool> index_dict;
  if (x_dim_nums == 0) {
    for (int32_t i = 0; i < index_num; i++) {
      if (input_2[i] < -1 || input_2[i] > 0) {
        CUST_KERNEL_LOG_ERROR(ctx, "Invalid argument 3: out of range.");
        return KERNEL_STATUS_PARAM_INVALID;
      } else {
        index_dict.insert(std::pair<int32_t, bool>(0, true));
      }
    }
  } else if (input_dim < -x_dim_nums || input_dim >= x_dim_nums) {
    CUST_KERNEL_LOG_ERROR(ctx,
                          "Dimension out of range (expected to be in range of "
                          "[%d, %d], but got %d).",
                          0 - x_dim_nums, x_dim_nums - 1, input_dim);
    return KERNEL_STATUS_PARAM_INVALID;
  } else {
    for (int32_t i = 0; i < index_num; i++) {
      if (input_2[i] < -x_dims[real_input_dim] || input_2[i] >= x_dims[real_input_dim]) {
        CUST_KERNEL_LOG_ERROR(ctx, "Invalid argument 3: out of range.");
        return KERNEL_STATUS_PARAM_INVALID;
      } else {
        input_2[i] = (input_2[i] < 0) ? (input_2[i] + x_dims[real_input_dim]) : input_2[i];
        index_dict.insert(std::pair<int32_t, bool>(input_2[i], true));
      }
    }
  }

  if (data_num >= kParallelDataNum) {
    uint32_t res = SpecialComputeParallel<T>(ctx, data_num, real_input_dim, index_dict);
    if (res != static_cast<uint32_t>(KERNEL_STATUS_OK)) {
      CUST_KERNEL_LOG_ERROR(ctx, "IndexFill kernel SpecialComputeParallel failed.");
      return res;
    }
  } else {
    SpecialCompute<T>(0, data_num, real_input_dim, index_dict);
  }
  return KERNEL_STATUS_OK;
}

uint32_t IndexFillCpuKernel::Compute(CpuKernelContext &ctx) {
  uint32_t res = GetInputAndCheck(ctx);
  if (res != KERNEL_STATUS_OK) {
    return res;
  }

  DataType input_type{ctx.Input(0)->GetDataType()};
  switch (input_type) {
    INDEXFILL_COMPUTE_CASE(DT_INT8, int8_t, ctx)
    INDEXFILL_COMPUTE_CASE(DT_INT16, int16_t, ctx)
    INDEXFILL_COMPUTE_CASE(DT_INT32, int32_t, ctx)
    INDEXFILL_COMPUTE_CASE(DT_INT64, int64_t, ctx)
    INDEXFILL_COMPUTE_CASE(DT_UINT8, uint8_t, ctx)
    INDEXFILL_COMPUTE_CASE(DT_UINT16, uint16_t, ctx)
    INDEXFILL_COMPUTE_CASE(DT_UINT32, uint32_t, ctx)
    INDEXFILL_COMPUTE_CASE(DT_UINT64, uint64_t, ctx)
    INDEXFILL_COMPUTE_CASE(DT_FLOAT16, Eigen::half, ctx)
    INDEXFILL_COMPUTE_CASE(DT_FLOAT, float, ctx)
    INDEXFILL_COMPUTE_CASE(DT_DOUBLE, double, ctx)
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "[%s] Data type of input is not support, input data type is [%s].",
                            ctx.GetOpType().c_str(), DTypeStr(input_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }
  return KERNEL_STATUS_OK;
}
REGISTER_MS_CPU_KERNEL(kIndexFill, IndexFillCpuKernel);
}  // namespace aicpu
