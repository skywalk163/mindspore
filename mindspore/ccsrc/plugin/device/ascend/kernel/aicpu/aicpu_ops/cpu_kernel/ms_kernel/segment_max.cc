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
#include "cpu_kernel/ms_kernel/segment_max.h"
#include <algorithm>
#include <vector>
#include "context/inc/cpu_kernel_utils.h"
#include "utils/eigen_tensor.h"
#include "utils/kernel_util.h"

namespace aicpu {
namespace {
const uint32_t kInputNum = 2;
const uint32_t kOutputNum = 1;
const char *kSegmentMax = "SegmentMax";
const int64_t kDataSize = 2 * 1024;

#define SEGMENTMAX_COMPUTE_CASE(DTYPE, TYPE1, TYPE2, CTX)              \
  case (DTYPE): {                                                      \
    uint32_t result = SegmentMaxCompute<TYPE1, TYPE2>(CTX);            \
    if (result != KERNEL_STATUS_OK) {                                  \
      CUST_KERNEL_LOG_ERROR(ctx, "SegmentMax kernel compute failed."); \
      return result;                                                   \
    }                                                                  \
    break;                                                             \
  }

#define SEGMENTMAX_COMPUTE_CASE_ALL(TYPE, CTX)                \
  SEGMENTMAX_COMPUTE_CASE(DT_INT8, int8_t, TYPE, CTX)         \
  SEGMENTMAX_COMPUTE_CASE(DT_INT16, int16_t, TYPE, CTX)       \
  SEGMENTMAX_COMPUTE_CASE(DT_INT32, int32_t, TYPE, CTX)       \
  SEGMENTMAX_COMPUTE_CASE(DT_INT64, int64_t, TYPE, CTX)       \
  SEGMENTMAX_COMPUTE_CASE(DT_UINT8, uint8_t, TYPE, CTX)       \
  SEGMENTMAX_COMPUTE_CASE(DT_UINT16, uint16_t, TYPE, CTX)     \
  SEGMENTMAX_COMPUTE_CASE(DT_UINT32, uint32_t, TYPE, CTX)     \
  SEGMENTMAX_COMPUTE_CASE(DT_UINT64, uint64_t, TYPE, CTX)     \
  SEGMENTMAX_COMPUTE_CASE(DT_FLOAT16, Eigen::half, TYPE, CTX) \
  SEGMENTMAX_COMPUTE_CASE(DT_FLOAT, float, TYPE, CTX)         \
  SEGMENTMAX_COMPUTE_CASE(DT_DOUBLE, double, TYPE, CTX)

template <typename T>
uint32_t SegmentIdsCompute(CpuKernelContext &ctx, const T *segment_ids_data_addr, const int64_t segment_ids_data_num,
                           std::vector<int64_t> *const segments_segment_ids) {
  if (segment_ids_data_addr[0] < 0) {
    CUST_KERNEL_LOG_ERROR(ctx, "Input[1] must be nonnegative data.");
    return aicpu::KERNEL_STATUS_PARAM_INVALID;
  }
  int64_t seg_tmp = 1;
  for (int64_t i = 0; i < segment_ids_data_num - 1; i++) {
    if (segment_ids_data_addr[i] > segment_ids_data_addr[i + 1]) {
      CUST_KERNEL_LOG_ERROR(ctx, "Input[1] must be an ascending ordered sequence.");
      return aicpu::KERNEL_STATUS_PARAM_INVALID;
    }
    if (segment_ids_data_addr[i] == segment_ids_data_addr[i + 1]) {
      seg_tmp++;
    } else {
      segments_segment_ids->push_back(seg_tmp);
      seg_tmp = 1;
    }
    if (i == segment_ids_data_num - kInputNum) {
      segments_segment_ids->push_back(seg_tmp);
    }
  }
  return aicpu::KERNEL_STATUS_OK;
}

template <typename T1, typename T2>
void InnerCompute(const int64_t start, const int64_t end, const int64_t input_addr_base, const T1 *input_x_addr,
                  T1 *const output_data_addr, const T2 *segment_ids_data_addr, const int64_t count,
                  const int64_t num_compare_per, const int64_t count_no) {
  for (int64_t j = start; j < end; j++) {
    int64_t max_init_addr = input_addr_base + j;
    T1 max_value = input_x_addr[max_init_addr];
    for (int64_t k = 1; k < count; k++) {
      int64_t cmp_addr = max_init_addr + k * num_compare_per;
      if (max_value < input_x_addr[cmp_addr]) {
        max_value = input_x_addr[cmp_addr];
      }
    }
    output_data_addr[segment_ids_data_addr[count_no] * num_compare_per + j] = max_value;
  }
}
}  // namespace

uint32_t SegmentMaxCpuKernel::Compute(CpuKernelContext &ctx) {
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, kInputNum, kOutputNum),
                           "SegmentMax check input and output number failed.");
  auto data_type = ctx.Input(0)->GetDataType();
  auto segment_ids_type = ctx.Input(1)->GetDataType();
  switch (segment_ids_type) {
    case DT_INT32: {
      switch (data_type) {
        SEGMENTMAX_COMPUTE_CASE_ALL(int32_t, ctx)
        default:
          CUST_KERNEL_LOG_ERROR(ctx, "Input[0] data type[%s] not supported.", DTypeStr(data_type).c_str());
          return KERNEL_STATUS_PARAM_INVALID;
      }
      break;
    }
    case DT_INT64: {
      switch (data_type) {
        SEGMENTMAX_COMPUTE_CASE_ALL(int64_t, ctx)
        default:
          CUST_KERNEL_LOG_ERROR(ctx, "Input[0] data type[%s] not supported.", DTypeStr(data_type).c_str());
          return KERNEL_STATUS_PARAM_INVALID;
      }
      break;
    }
    default: {
      CUST_KERNEL_LOG_ERROR(ctx, "Input[1] data type[%s] not supported.", DTypeStr(segment_ids_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
    }
  }
  return KERNEL_STATUS_OK;
}

template <typename T1, typename T2>
uint32_t SegmentMaxCpuKernel::SegmentMaxCompute(CpuKernelContext &ctx) {
  Tensor *input_x_data = ctx.Input(0);
  auto input_x_addr = reinterpret_cast<T1 *>(input_x_data->GetData());
  auto input_x_shape = input_x_data->GetTensorShape();
  auto input_x_dims = input_x_shape->GetDimSizes();
  int64_t input_x_num = input_x_data->NumElements();
  Tensor *segment_ids_data = ctx.Input(1);
  auto segment_ids_data_addr = reinterpret_cast<T2 *>(segment_ids_data->GetData());
  auto segment_ids_len = segment_ids_data->NumElements();
  int64_t segment_ids_data_num = segment_ids_data->NumElements();
  input_x_dims[0] = segment_ids_data_addr[segment_ids_data_num - 1] + 1;
  Tensor *output_data = ctx.Output(0);
  auto output_data_addr = reinterpret_cast<T1 *>(output_data->GetData());
  auto output_data_shape_sizes = input_x_data->GetTensorShape()->GetDimSizes();
  output_data_shape_sizes[0] = segment_ids_data_addr[segment_ids_len - 1] + 1;
  output_data->GetTensorShape()->SetDimSizes(output_data_shape_sizes);
  auto output_data_shape = output_data->GetTensorShape();
  if (output_data_shape->GetDimSize(0) < input_x_dims[0]) {
    CUST_KERNEL_LOG_ERROR(ctx, "The number of segments of the segmentation result of segment_ids is too large.");
    return KERNEL_STATUS_PARAM_INVALID;
  }
  output_data_shape->SetDimSizes(input_x_dims);
  if (!output_data->SetTensorShape(output_data_shape.get())) {
    CUST_KERNEL_LOG_ERROR(ctx, "Set output shape failed.");
    return KERNEL_STATUS_INNER_ERROR;
  }
  int64_t output_data_num = output_data->NumElements();
  for (int64_t i = 0; i < output_data_num; i++) {
    output_data_addr[i] = static_cast<T1>(0);
  }
  std::vector<int64_t> segments_segment_ids;
  if (segment_ids_data_num != (input_x_data->GetTensorShape()->GetDimSize(0))) {
    CUST_KERNEL_LOG_ERROR(ctx, "The amount of data for input[1] must be equal to the first dimension of input[0].");
    return KERNEL_STATUS_PARAM_INVALID;
  }
  if (auto status = SegmentIdsCompute(ctx, segment_ids_data_addr, segment_ids_data_num, &segments_segment_ids);
      status != KERNEL_STATUS_OK) {
    return status;
  }
  const int64_t num_compare_per = input_x_num / (input_x_shape->GetDimSize(0));
  const int64_t num_segments_segment_ids = segments_segment_ids.size();
  if (num_segments_segment_ids < kDataSize) {
    for (int64_t i = 0; i < num_segments_segment_ids; i++) {
      int64_t count = segments_segment_ids[i];
      int64_t count_no = 0;
      for (int64_t j = 0; j < i; j++) {
        count_no += segments_segment_ids[j];
      }
      int64_t input_addr_base = count_no * num_compare_per;
      if (num_compare_per < kDataSize) {
        InnerCompute(0, num_compare_per, input_addr_base, input_x_addr, output_data_addr, segment_ids_data_addr, count,
                     num_compare_per, count_no);
      } else {
        uint32_t min_core_num = 1;
        int64_t max_core_num = std::max(min_core_num, aicpu::CpuKernelUtils::GetCPUNum(ctx) - 2);
        if (max_core_num > num_compare_per) {
          max_core_num = num_compare_per;
        }
        auto shard_compute = [&](size_t start, size_t end) {
          InnerCompute(static_cast<int64_t>(start), static_cast<int64_t>(end), input_addr_base, input_x_addr,
                       output_data_addr, segment_ids_data_addr, count, num_compare_per, count_no);
        };
        CUST_KERNEL_HANDLE_ERROR(
          ctx, CpuKernelUtils::ParallelFor(ctx, num_compare_per, num_compare_per / max_core_num, shard_compute),
          "SegmentMax Compute failed.");
      }
    }
  } else {
    uint32_t min_core_num_seg = 1;
    int64_t max_core_num_seg = std::max(min_core_num_seg, aicpu::CpuKernelUtils::GetCPUNum(ctx) - 2);
    if (max_core_num_seg > num_segments_segment_ids) {
      max_core_num_seg = num_segments_segment_ids;
    }
    auto shard_compute_seg = [&](size_t start_seg, size_t end_seg) {
      for (size_t i = start_seg; i < end_seg; i++) {
        int64_t count = segments_segment_ids[i];
        int64_t count_no = 0;
        for (size_t j = 0; j < i; j++) {
          count_no += segments_segment_ids[j];
        }
        int64_t input_addr_base = count_no * num_compare_per;
        InnerCompute(0, num_compare_per, input_addr_base, input_x_addr, output_data_addr, segment_ids_data_addr, count,
                     num_compare_per, count_no);
      }
    };
    CUST_KERNEL_HANDLE_ERROR(
      ctx,
      CpuKernelUtils::ParallelFor(ctx, num_segments_segment_ids, num_segments_segment_ids / max_core_num_seg,
                                  shard_compute_seg),
      "SegmentMax Compute failed.");
  }
  return KERNEL_STATUS_OK;
}
REGISTER_MS_CPU_KERNEL(kSegmentMax, SegmentMaxCpuKernel);
}  // namespace aicpu
