/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2022.All rights reserved.
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

#include "cpu_kernel/ms_kernel/heaviside.h"

#include <algorithm>

#include "context/inc/cpu_kernel_utils.h"
#include "utils/eigen_tensor.h"
#include "utils/kernel_util.h"

namespace {
const uint32_t kOutputNum = 1;
const uint32_t kInputNum = 2;
const char *kHeaviside = "Heaviside";
const int64_t kParallelDataNum = 2 * 1024;
const int64_t kParallelDataNumMid = 16 * 1024;
const int64_t kParallelDataNumSameShape = 7 * 1024;
const int64_t kParallelDataNumSameShapeMid = 35 * 1024;

#define HEAVISIDE_COMPUTE_CASE(DTYPE, TYPE, CTX)                      \
  case (DTYPE): {                                                     \
    uint32_t result = HeavisideCompute<TYPE>(CTX);                    \
    if (result != KERNEL_STATUS_OK) {                                 \
      CUST_KERNEL_LOG_ERROR(ctx, "Heaviside kernel compute failed."); \
      return result;                                                  \
    }                                                                 \
    break;                                                            \
  }
}  // namespace

namespace aicpu {
template <typename T>
T heaviside(T a, T b) {
  return a == static_cast<T>(0) ? b : static_cast<T>(a > static_cast<T>(0));
}

uint32_t HeavisideCpuKernel::Compute(CpuKernelContext &ctx) {
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, kInputNum, kOutputNum),
                           "Heaviside check input and output number failed.");
  CUST_KERNEL_HANDLE_ERROR(ctx, HeavisideParamCheck(ctx), "Heaviside check params failed.");
  auto data_type = ctx.Input(0)->GetDataType();
  switch (data_type) {
    HEAVISIDE_COMPUTE_CASE(DT_DOUBLE, double, ctx)
    HEAVISIDE_COMPUTE_CASE(DT_FLOAT, float, ctx)
    HEAVISIDE_COMPUTE_CASE(DT_FLOAT16, Eigen::half, ctx)
    HEAVISIDE_COMPUTE_CASE(DT_INT16, int16_t, ctx)
    HEAVISIDE_COMPUTE_CASE(DT_INT32, int32_t, ctx)
    HEAVISIDE_COMPUTE_CASE(DT_INT64, int64_t, ctx)
    HEAVISIDE_COMPUTE_CASE(DT_INT8, int8_t, ctx)
    HEAVISIDE_COMPUTE_CASE(DT_UINT16, uint16_t, ctx)
    HEAVISIDE_COMPUTE_CASE(DT_UINT32, uint32_t, ctx)
    HEAVISIDE_COMPUTE_CASE(DT_UINT64, uint64_t, ctx)
    HEAVISIDE_COMPUTE_CASE(DT_UINT8, uint8_t, ctx)
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "Heaviside kernel data type [%s] not support.", DTypeStr(data_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }

  return KERNEL_STATUS_OK;
}

uint32_t HeavisideCpuKernel::HeavisideParamCheck(CpuKernelContext &ctx) {
  Tensor *input_0 = ctx.Input(0);
  Tensor *input_1 = ctx.Input(1);
  Tensor *output = ctx.Output(0);
  DataType input0_type = input_0->GetDataType();
  DataType input1_type = input_1->GetDataType();
  CUST_KERNEL_CHECK_FALSE(ctx, (input0_type == input1_type), KERNEL_STATUS_PARAM_INVALID,
                          "The data type of input0 [%s] need be same with "
                          "input1 [%s].",
                          DTypeStr(input0_type).c_str(), DTypeStr(input1_type).c_str())
  CUST_KERNEL_LOG_DEBUG(ctx,
                        "HeavisideCpuKernel[%s], input0: size[%llu];"
                        "input1: size[%llu], output: size[%llu].",
                        ctx.GetOpType().c_str(), input_0->GetDataSize(), input_1->GetDataSize(), output->GetDataSize());

  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t NoBcastComputeParallel(CpuKernelContext &ctx, const BcastShapeType &type, T *in0, T *in1, T *out,
                                const int64_t &data_num) {
  uint32_t min_core_num = 1;
  uint32_t max_core_num = std::max(min_core_num, aicpu::CpuKernelUtils::GetCPUNum(ctx) - kResvCpuNum);

  if (data_num <= kParallelDataNumSameShapeMid) {
    max_core_num = std::min(max_core_num, 4U);  // up to 4 cpu cores
  }

  if (max_core_num > data_num) {
    max_core_num = data_num;
  }

  auto sharder_heaviside = [&](int64_t start, int64_t end) {
    switch (type) {
      case BcastShapeType::SAME_SHAPE:
        for (int64_t i = start; i < end; ++i) {
          *(out + i) = heaviside<T>(*(in0 + i), *(in1 + i));
        }
        break;
      case BcastShapeType::X_ONE_ELEMENT:
        for (int64_t i = start; i < end; ++i) {
          *(out + i) = heaviside<T>(*in0, *(in1 + i));
        }
        break;
      case BcastShapeType::Y_ONE_ELEMENT:
        for (int64_t i = start; i < end; ++i) {
          *(out + i) = heaviside<T>(*(in0 + i), *in1);
        }
        break;
      default:
        CUST_KERNEL_LOG_ERROR(ctx, "Invalid type [%d]", static_cast<int32_t>(type));
        break;
    }
  };

  if (max_core_num == 0) {
    CUST_KERNEL_LOG_ERROR(ctx, "max_core_num could not be 0");
  }
  CUST_KERNEL_HANDLE_ERROR(ctx, CpuKernelUtils::ParallelFor(ctx, data_num, data_num / max_core_num, sharder_heaviside),
                           "Heaviside Compute failed.");
  return KERNEL_STATUS_OK;
}

template <typename T>
void NoBcastComputeSingle(CpuKernelContext &ctx, const BcastShapeType &type, T *in0, T *in1, T *out,
                          const int64_t &data_num) {
  switch (type) {
    case BcastShapeType::SAME_SHAPE:
      for (int64_t i = static_cast<int64_t>(0); i < data_num; ++i) {
        *(out + i) = heaviside<T>(*(in0 + i), *(in1 + i));
      }
      break;
    case BcastShapeType::X_ONE_ELEMENT:
      for (int64_t i = static_cast<int64_t>(0); i < data_num; ++i) {
        *(out + i) = heaviside<T>(*in0, *(in1 + i));
      }
      break;
    case BcastShapeType::Y_ONE_ELEMENT:
      for (int64_t i = static_cast<int64_t>(0); i < data_num; ++i) {
        *(out + i) = heaviside<T>(*(in0 + i), *in1);
      }
      break;
    default:
      CUST_KERNEL_LOG_WARN(ctx, "Invalid type [%d]", static_cast<int32_t>(type));
      break;
  }
}

template <typename T>
uint32_t HeavisideCpuKernel::NoBcastCompute(CpuKernelContext &ctx) {
  auto in0 = reinterpret_cast<T *>(ctx.Input(0)->GetData());
  auto in1 = reinterpret_cast<T *>(ctx.Input(1)->GetData());
  auto out = reinterpret_cast<T *>(ctx.Output(0)->GetData());
  int64_t in0_elements_nums = ctx.Input(0)->NumElements();
  int64_t in1_elements_nums = ctx.Input(1)->NumElements();
  int64_t data_num = ctx.Output(0)->NumElements();

  BcastShapeType type;
  if (in0_elements_nums == in1_elements_nums) {
    type = BcastShapeType::SAME_SHAPE;
  } else {
    type = (in0_elements_nums == 1 ? BcastShapeType::X_ONE_ELEMENT : BcastShapeType::Y_ONE_ELEMENT);
  }

  if (data_num >= kParallelDataNumSameShape) {
    uint32_t res = NoBcastComputeParallel<T>(ctx, type, in0, in1, out, data_num);
    if (res != static_cast<uint32_t>(KERNEL_STATUS_OK)) {
      CUST_KERNEL_LOG_ERROR(ctx, "Heaviside kernel NoBcastComputeParallel failed.");
      return res;
    }
  } else {
    NoBcastComputeSingle<T>(ctx, type, in0, in1, out, data_num);
  }
  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t HeavisideCpuKernel::BcastCompute(CpuKernelContext &ctx, const Bcast &bcast) {
  T *in0 = reinterpret_cast<T *>(ctx.Input(0)->GetData());
  T *in1 = reinterpret_cast<T *>(ctx.Input(1)->GetData());
  T *out = reinterpret_cast<T *>(ctx.Output(0)->GetData());
  int64_t data_num = ctx.Output(0)->NumElements();
  if (data_num >= kParallelDataNum) {
    uint32_t min_core_num = 1;
    uint32_t max_core_num = std::max(min_core_num, aicpu::CpuKernelUtils::GetCPUNum(ctx) - kResvCpuNum);

    if (data_num <= kParallelDataNumMid) {
      max_core_num = std::min(max_core_num, 4U);  // up to 4 cpu cores
    }

    if (max_core_num > data_num) {
      max_core_num = data_num;
    }

    auto sharder_heaviside = [&](int64_t start, int64_t end) {
      for (int64_t i = start; i < end; ++i) {
        *(out + i) = heaviside<T>(*(in0 + bcast.GetBroadcastXIndex(i)), *(in1 + bcast.GetBroadcastYIndex(i)));
      }
    };

    if (max_core_num == 0) {
      CUST_KERNEL_LOG_ERROR(ctx, "max_core_num could not be 0");
    }
    CUST_KERNEL_HANDLE_ERROR(ctx,
                             CpuKernelUtils::ParallelFor(ctx, data_num, data_num / max_core_num, sharder_heaviside),
                             "Heaviside Compute failed.");
  } else {
    for (int64_t i = 0; i < data_num; ++i) {
      *(out + i) = heaviside<T>(*(in0 + bcast.GetBroadcastXIndex(i)), *(in1 + bcast.GetBroadcastYIndex(i)));
    }
  }
  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t HeavisideCpuKernel::HeavisideCompute(CpuKernelContext &ctx) {
  Tensor *input0_tensor = ctx.Input(0);
  auto input0_shape = input0_tensor->GetTensorShape()->GetDimSizes();
  int64_t input0_elements_nums = input0_tensor->NumElements();

  Tensor *input1_tensor = ctx.Input(1);
  auto input1_shape = input1_tensor->GetTensorShape()->GetDimSizes();
  int64_t input1_elements_nums = input1_tensor->NumElements();

  bool isNeedBcast = (input0_shape == input1_shape) || (input0_elements_nums == 1) || (input1_elements_nums == 1);
  if (isNeedBcast) {
    return NoBcastCompute<T>(ctx);
  } else {
    Bcast bcast(ctx, input0_shape, input1_shape);
    if (!bcast.IsValid()) {
      CUST_KERNEL_LOG_ERROR(ctx, "[%s] broadcast failed.", ctx.GetOpType().c_str());
      return KERNEL_STATUS_PARAM_INVALID;
    }

    return BcastCompute<T>(ctx, bcast);
  }

  return KERNEL_STATUS_OK;
}

REGISTER_MS_CPU_KERNEL(kHeaviside, HeavisideCpuKernel);
}  // namespace aicpu
