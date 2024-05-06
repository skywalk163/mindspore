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

#include "cpu_kernel/ms_kernel/segment_min.h"
#include <utility>
#include <algorithm>
#include <iostream>
#include <vector>
#include <unordered_map>
#include "context/inc/cpu_kernel_utils.h"
#include "utils/eigen_tensor.h"
#include "utils/kernel_util.h"
namespace aicpu {
namespace {
constexpr uint32_t kOutputNum = 1;
constexpr uint32_t kInputNum = 2;
constexpr uint64_t _8k = 8 * 1024;
constexpr uint64_t _2k = 2 * 1024;
const char *kSegmentMin = "SegmentMin";

#define SEGMENT_MIN_COMPUTE_CASE(DTYPE, TYPE, CTX, STYPE)                                                            \
  case (DTYPE): {                                                                                                    \
    uint32_t res;                                                                                                    \
    switch (STYPE) {                                                                                                 \
      case DT_INT32:                                                                                                 \
        res = SegmentMinCompute<TYPE, int32_t>(CTX);                                                                 \
        break;                                                                                                       \
      case DT_INT64:                                                                                                 \
        res = SegmentMinCompute<TYPE, int64_t>(CTX);                                                                 \
        break;                                                                                                       \
      default:                                                                                                       \
        CUST_KERNEL_LOG_ERROR(ctx, "SegmentMin kernel segment_ids type [%s] not support.", DTypeStr(STYPE).c_str()); \
        return KERNEL_STATUS_PARAM_INVALID;                                                                          \
    }                                                                                                                \
    if (res != KERNEL_STATUS_OK) {                                                                                   \
      CUST_KERNEL_LOG_ERROR(ctx, "SegmentMin kernel compute failed.");                                               \
      return res;                                                                                                    \
    }                                                                                                                \
    break;                                                                                                           \
  }

template <typename T1, typename T2>
void InnerCompute(const uint64_t start_len, const uint64_t end_len, const uint64_t st, const uint64_t ed,
                  const uint64_t len2, const T1 *const data_data, T2 *const output_data, const uint64_t output_start) {
  for (uint64_t k = start_len; k < end_len; k++) {
    for (uint64_t j = st; j <= ed; j++) {
      uint64_t data_start = j * len2;
      T1 &u = output_data[output_start + k];
      const T1 v = data_data[data_start + k];
      if (j == st) {
        u = v;
      } else {
        u = std::min(u, v);
      }
    }
  }
}
}  // namespace
uint32_t SegmentMinCpuKernel::Compute(CpuKernelContext &ctx) {
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, kInputNum, kOutputNum),
                           "SegmentMin check input and output number failed.");
  CUST_KERNEL_HANDLE_ERROR(ctx, SegmentMinCheck(ctx), "SegmentMin check params failed.");
  auto type_data = ctx.Input(0)->GetDataType();
  auto type_seg = ctx.Input(1)->GetDataType();
  switch (type_data) {
    SEGMENT_MIN_COMPUTE_CASE(DT_INT8, int8_t, ctx, type_seg)
    SEGMENT_MIN_COMPUTE_CASE(DT_INT16, int16_t, ctx, type_seg)
    SEGMENT_MIN_COMPUTE_CASE(DT_INT32, int32_t, ctx, type_seg)
    SEGMENT_MIN_COMPUTE_CASE(DT_INT64, int64_t, ctx, type_seg)
    SEGMENT_MIN_COMPUTE_CASE(DT_UINT8, uint8_t, ctx, type_seg)
    SEGMENT_MIN_COMPUTE_CASE(DT_UINT32, uint32_t, ctx, type_seg)
    SEGMENT_MIN_COMPUTE_CASE(DT_UINT16, uint16_t, ctx, type_seg)
    SEGMENT_MIN_COMPUTE_CASE(DT_UINT64, uint64_t, ctx, type_seg)
    SEGMENT_MIN_COMPUTE_CASE(DT_FLOAT16, Eigen::half, ctx, type_seg)
    SEGMENT_MIN_COMPUTE_CASE(DT_FLOAT, float, ctx, type_seg)
    SEGMENT_MIN_COMPUTE_CASE(DT_DOUBLE, double, ctx, type_seg)
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "SegmentMin kernel data type [%s] not support.", DTypeStr(type_data).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }
  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t SegmentMinCpuKernel::OutputInit(CpuKernelContext &ctx, const uint64_t output_len, T *const output_data) {
  // 输出初始化为0
  if (output_len <= _8k) {
    for (uint64_t i = 0; i < output_len; i++) output_data[i] = static_cast<T>(0);
    return KERNEL_STATUS_OK;
  }
  uint32_t min_core = 1;
  uint64_t max_core = std::max(min_core, aicpu::CpuKernelUtils::GetCPUNum(ctx) / 2);
  if (max_core > output_len) {
    max_core = output_len;
  }
  auto init = [&](size_t start, size_t end) {
    for (auto i = start; i < end; i++) output_data[i] = static_cast<T>(0);
  };
  CUST_KERNEL_HANDLE_ERROR(ctx, CpuKernelUtils::ParallelFor(ctx, output_len, output_len / max_core, init),
                           "Initialize value of output failed.");
  return KERNEL_STATUS_OK;
}

template <class T1, class T2>
uint32_t SegmentMinCpuKernel::SegmentMinCompute(CpuKernelContext &ctx) {
  auto data = ctx.Input(0);  // tensor*
  auto segment_ids = ctx.Input(1);
  auto output = ctx.Output(0);
  auto data_data = reinterpret_cast<T1 *>(data->GetData());
  auto segment_ids_data = reinterpret_cast<T2 *>(segment_ids->GetData());
  auto segment_ids_len = segment_ids->NumElements();
  auto data_len = data->NumElements();
  auto data_shape = data->GetTensorShape();
  auto output_data = reinterpret_cast<T1 *>(output->GetData());
  auto output_data_shape_sizes = data_shape->GetDimSizes();
  output_data_shape_sizes[0] = segment_ids_data[segment_ids_len - 1] + 1;
  output->GetTensorShape()->SetDimSizes(output_data_shape_sizes);
  uint64_t output_len = output->NumElements();
  uint64_t len2 = data_len / data_shape->GetDimSize(0);
  OutputInit(ctx, output_len, output_data);
  std::vector<T2> nums;
  std::vector<std::pair<uint64_t, uint64_t>> ranges;
  for (int64_t i = 0; i < segment_ids_len; ++i) {
    if (i) {
      if (segment_ids_data[i] == nums.back()) {
        ++ranges.back().second;
      } else {
        nums.push_back(segment_ids_data[i]), ranges.push_back({i, i});
      }
    } else {
      nums.push_back(segment_ids_data[0]), ranges.push_back(std::make_pair(0, 0));
    }
  }
  uint64_t nums_len = nums.size();
  if (nums_len > _8k) {
    uint32_t min_core = 1;
    uint64_t max_core = std::max(min_core, aicpu::CpuKernelUtils::GetCPUNum(ctx) - 2);
    max_core = std::min(max_core, nums_len);
    auto mt_for_nums = [&](size_t start_num, size_t end_num) {
      for (auto i = start_num; i < end_num; ++i) {
        uint64_t st = ranges[i].first;
        uint64_t ed = ranges[i].second;
        uint64_t output_start = nums[i] * len2;
        InnerCompute(0, len2, st, ed, len2, data_data, output_data, output_start);
      }
    };
    CUST_KERNEL_HANDLE_ERROR(ctx, CpuKernelUtils::ParallelFor(ctx, nums_len, nums_len / max_core, mt_for_nums),
                             "SegmentMin Compute failed.");
  } else {
    for (uint64_t i = 0; i < nums_len; ++i) {
      uint64_t st = ranges[i].first;
      uint64_t ed = ranges[i].second;
      uint64_t output_start = nums[i] * len2;
      if (len2 < _2k) {
        InnerCompute(0, len2, st, ed, len2, data_data, output_data, output_start);
      } else {
        uint32_t min_core = 1;
        uint64_t max_core = std::max(min_core, aicpu::CpuKernelUtils::GetCPUNum(ctx) - 2);
        max_core = std::min(max_core, len2);
        auto mt_for_len2 = [&](size_t start_len, size_t end_len) {
          InnerCompute(start_len, end_len, st, ed, len2, data_data, output_data, output_start);
        };
        CUST_KERNEL_HANDLE_ERROR(ctx, CpuKernelUtils::ParallelFor(ctx, len2, len2 / max_core, mt_for_len2),
                                 "SegmentMin Compute failed.");
      }
    }
  }
  return KERNEL_STATUS_OK;
}
uint32_t SegmentMinCpuKernel::SegmentMinCheck(CpuKernelContext &ctx) {
  // inspect the input & output pointer
  CUST_KERNEL_CHECK_NULLPTR(ctx, ctx.Input(0), KERNEL_STATUS_PARAM_INVALID, "Get input 0 failed.")
  CUST_KERNEL_CHECK_NULLPTR(ctx, ctx.Input(1), KERNEL_STATUS_PARAM_INVALID, "Get input 1 failed.")
  CUST_KERNEL_CHECK_NULLPTR(ctx, ctx.Output(0), KERNEL_STATUS_PARAM_INVALID, "Get output failed.")
  // inspect data in input & output
  CUST_KERNEL_CHECK_NULLPTR(ctx, ctx.Input(0)->GetData(), KERNEL_STATUS_PARAM_INVALID, "Get input 0 data failed.")
  CUST_KERNEL_CHECK_NULLPTR(ctx, ctx.Input(1)->GetData(), KERNEL_STATUS_PARAM_INVALID, "Get input 1 data failed.")
  CUST_KERNEL_CHECK_NULLPTR(ctx, ctx.Output(0)->GetData(), KERNEL_STATUS_PARAM_INVALID, "Get output 0 data failed.")
  // regular test
  CUST_KERNEL_CHECK_FALSE(ctx, CheckType(ctx.Input(1)), KERNEL_STATUS_PARAM_INVALID,
                          "The data type of segment_ids should be DT_INT32 or DT_INT64.")
  CUST_KERNEL_CHECK_FALSE(ctx, CheckDim(ctx.Input(1)), KERNEL_STATUS_PARAM_INVALID,
                          "The dimension of segment_ids should be 1.")
  CUST_KERNEL_CHECK_FALSE(ctx, CheckSorted(ctx.Input(1)), KERNEL_STATUS_PARAM_INVALID,
                          "segment_ids should be ascending and no negative number in it.")
  CUST_KERNEL_CHECK_FALSE(ctx, CheckLength(ctx.Input(1), ctx.Input(0)), KERNEL_STATUS_PARAM_INVALID,
                          "The length of segment_ids should be equal to the length "
                          "of the first dimension of the data")
  CUST_KERNEL_LOG_DEBUG(ctx,
                        "SegmentMinCpuKernel[%s], input0: size[%llu];"
                        "input1: size[%llu], output: size[%llu].",
                        ctx.GetOpType().c_str(), ctx.Input(0)->GetDataSize(), ctx.Input(1)->GetDataSize(),
                        ctx.Output(0)->GetDataSize());
  return KERNEL_STATUS_OK;
}
bool SegmentMinCpuKernel::CheckType(Tensor *t) {
  DataType type = t->GetDataType();
  return type == DT_INT32 || type == DT_INT64;
}
bool SegmentMinCpuKernel::CheckDim(Tensor *t) {
  auto dims = t->GetTensorShape()->GetDims();
  return dims == 1;
}
bool SegmentMinCpuKernel::CheckSorted(Tensor *tensor) {
  DataType type = tensor->GetDataType();
  auto len = tensor->NumElements();
  switch (type) {
    case DT_INT32: {
      auto data = reinterpret_cast<int32_t *>(tensor->GetData());
      for (int64_t i = 0; i < len; i++)
        if ((i && data[i] < data[i - 1]) || data[i] < 0) {
          return false;
        }
      break;
    }
    case DT_INT64: {
      auto data = reinterpret_cast<int64_t *>(tensor->GetData());
      for (int64_t i = 0; i < len; i++)
        if ((i && data[i] < data[i - 1]) || data[i] < 0) {
          return false;
        }
      break;
    }
    default:
      return true;
  }
  return true;
}
bool SegmentMinCpuKernel::CheckLength(Tensor *seg, Tensor *data) {
  auto len1 = seg->NumElements();
  auto len2 = data->GetTensorShape()->GetDimSize(0);
  return len1 == len2;
}
REGISTER_MS_CPU_KERNEL(kSegmentMin, SegmentMinCpuKernel);
}  // namespace aicpu
