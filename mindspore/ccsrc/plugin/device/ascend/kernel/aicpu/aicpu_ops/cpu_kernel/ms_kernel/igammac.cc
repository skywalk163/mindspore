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

#include "cpu_kernel/ms_kernel/igammac.h"
#include <algorithm>

#include "cpu_kernel/utils/igamma_utils.h"
#include "context/inc/cpu_kernel_utils.h"
#include "utils/eigen_tensor.h"
#include "utils/kernel_util.h"

namespace {
const uint32_t kOutputNum = 1;
const uint32_t kInputNum = 2;
const char *kigammac = "Igammac";
constexpr size_t kParallelDataNums = 128;

#define SWITCH_PARALLEL(ctx, SHARD, end_num)                                                                      \
  if (data_num <= kParallelDataNums) {                                                                            \
    SHARD(0, end_num);                                                                                            \
  } else {                                                                                                        \
    CUST_KERNEL_HANDLE_ERROR(ctx, CpuKernelUtils::ParallelFor(ctx, (end_num), (end_num) / (max_core_num), SHARD), \
                             "Igammac SHARD Compute failed.");                                                    \
  }

#define IGAMMAC_COMPUTE_CASE(DTYPE, TYPE, CTX, CALCINFO)            \
  case (DTYPE): {                                                   \
    uint32_t result = IgammacCompute<TYPE>(CTX, CALCINFO);          \
    if (result != KERNEL_STATUS_OK) {                               \
      CUST_KERNEL_LOG_ERROR(ctx, "Igammac kernel compute failed."); \
      return result;                                                \
    }                                                               \
    break;                                                          \
  }

}  // namespace

namespace aicpu {
uint32_t IgammacCpuKernel::Compute(CpuKernelContext &ctx) {
  // check param number
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, kInputNum, kOutputNum),
                           "Igammac check input and output number failed.");
  BCalcInfo calc_info;
  CUST_KERNEL_HANDLE_ERROR(ctx, IgammacCheckAndBroadCast(ctx, &calc_info), "Igammac check params or bcast failed.");

  auto data_type = ctx.Input(0)->GetDataType();
  switch (data_type) {
    IGAMMAC_COMPUTE_CASE(DT_FLOAT, float, ctx, calc_info)
    IGAMMAC_COMPUTE_CASE(DT_DOUBLE, double, ctx, calc_info)
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "Igammac kernel data type [%s] not support.", DTypeStr(data_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }

  return KERNEL_STATUS_OK;
}

uint32_t IgammacCpuKernel::IgammacCheckAndBroadCast(CpuKernelContext &ctx, BCalcInfo *calc_info) {
  (*calc_info).input_0 = ctx.Input(kFirstInputIndex);
  (*calc_info).input_1 = ctx.Input(kSecondInputIndex);
  (*calc_info).output = ctx.Output(0);

  // check input datatype
  DataType input0_datatype = (*calc_info).input_0->GetDataType();
  CUST_KERNEL_CHECK_FALSE(ctx, (input0_datatype == DT_DOUBLE || input0_datatype == DT_FLOAT),
                          KERNEL_STATUS_PARAM_INVALID,
                          "Input[0] data type must DT_FLOAT or DT_DOUBLE,"
                          "but got data type[%s].",
                          DTypeStr(input0_datatype).c_str());

  DataType input1_datatype = (*calc_info).input_1->GetDataType();
  CUST_KERNEL_CHECK_FALSE(ctx, (input0_datatype == input1_datatype), KERNEL_STATUS_PARAM_INVALID,
                          "The data type of input1 [%s] need be same with "
                          "input0 [%s].",
                          DTypeStr(input1_datatype).c_str(), DTypeStr(input0_datatype).c_str())

  // check output dtype
  DataType output_datatype = (*calc_info).output->GetDataType();
  CUST_KERNEL_CHECK_FALSE(ctx, (input0_datatype == output_datatype), KERNEL_STATUS_PARAM_INVALID,
                          "The data type of output [%s] need be same with "
                          "input0 [%s].",
                          DTypeStr(output_datatype).c_str(), DTypeStr(input0_datatype).c_str())

  CUST_KERNEL_LOG_DEBUG(ctx,
                        "IgammacCpuKernel[%s], input0: size[%llu];"
                        "input1: size[%llu], output: size[%llu].",
                        ctx.GetOpType().c_str(), (*calc_info).input_0->GetDataSize(),
                        (*calc_info).input_1->GetDataSize(), (*calc_info).output->GetDataSize());

  Bcast bcast(ctx);
  CUST_KERNEL_HANDLE_ERROR(ctx, bcast.GenerateBcastInfo((*calc_info)), "Generate broadcast info failed.");
  (void)bcast.BCastIndexes((*calc_info).x_indexes, (*calc_info).y_indexes);
  (void)bcast.GetBcastVec((*calc_info));

  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t IgammacCpuKernel::IgammacCompute(CpuKernelContext &ctx, const BCalcInfo &calc_info) {
  auto input_x1 = reinterpret_cast<T *>(calc_info.input_0->GetData());
  auto input_x2 = reinterpret_cast<T *>(calc_info.input_1->GetData());
  auto output_y = reinterpret_cast<T *>(calc_info.output->GetData());

  size_t data_num = calc_info.x_indexes.size();
  uint32_t min_core_num = 1;
  size_t max_core_num = std::max(min_core_num, aicpu::CpuKernelUtils::GetCPUNum(ctx) - 2);
  if (max_core_num > data_num) {
    max_core_num = data_num;
  }

  if (max_core_num == 0) {
    max_core_num = 1;
  }

  auto shard_igammac = [&](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      T *x1_index = input_x1 + calc_info.x_indexes[i];  // i-th value of input0
      T *x2_index = input_x2 + calc_info.y_indexes[i];  // i-th value of input1
      *(output_y + i) = IgammacSingle<T>(*x1_index, *x2_index);
    }
  };

  SWITCH_PARALLEL(ctx, shard_igammac, data_num);

  return KERNEL_STATUS_OK;
}

REGISTER_MS_CPU_KERNEL(kigammac, IgammacCpuKernel);
}  // namespace aicpu
