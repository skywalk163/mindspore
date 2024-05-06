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

#include "unsorted_segment_prod.h"

#include <string>
#include "context/inc/cpu_kernel_utils.h"
#include "cpu_types.h"
#include "utils/eigen_tensor.h"
#include "utils/kernel_util.h"

namespace {
const char *kUnsortedSegmentProd = "UnsortedSegmentProd";
const uint32_t input_num = 3;
const uint32_t output_num = 1;
constexpr int64_t kParallelDataNums = 64 * 1024;
}  // namespace

namespace aicpu {
template <typename input_t, typename segment_ids_t, typename num_segments_t>
uint32_t UnsortedSegmentProdCpuKernel::UnsortedSegmentProdComputeTemplate(CpuKernelContext &ctx) {
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, input_num, output_num),
                           " node input size should be [%llu],  get [%llu]", input_num, ctx.GetInputsSize(),
                           " node output size should be [%llu],  get [%llu]", output_num, ctx.GetOutputsSize());
  if (ctx.Input(0)->GetDataType() != ctx.Output(0)->GetDataType()) {
    CUST_KERNEL_LOG_ERROR(ctx, "The data type of the input [%s] need be the same as the output [%s]",
                          DTypeStr(ctx.Input(0)->GetDataType()).c_str(),
                          DTypeStr(ctx.Output(0)->GetDataType()).c_str());
    return KERNEL_STATUS_PARAM_INVALID;
  }
  if (ctx.Input(0)->GetDataSize() != ctx.Output(0)->GetDataSize()) {
    CUST_KERNEL_LOG_ERROR(ctx,
                          "The data size of the input [%llu] need be the same as the output "
                          "[%llu]",
                          ctx.Input(0)->GetDataSize(), ctx.Output(0)->GetDataSize());
    return KERNEL_STATUS_PARAM_INVALID;
  }
  int64_t data_size = ctx.Input(0)->NumElements();
  int64_t id_size = ctx.Input(1)->NumElements();

  auto input_x = reinterpret_cast<input_t *>(ctx.Input(0)->GetData());
  CUST_KERNEL_CHECK_NULLPTR(ctx, input_x, KERNEL_STATUS_PARAM_INVALID, "Get input data failed")
  auto output_y = reinterpret_cast<input_t *>(ctx.Output(0)->GetData());
  CUST_KERNEL_CHECK_NULLPTR(ctx, output_y, KERNEL_STATUS_PARAM_INVALID, "Get output data failed")
  auto segmentids = reinterpret_cast<segment_ids_t *>(ctx.Input(1)->GetData());
  CUST_KERNEL_CHECK_NULLPTR(ctx, segmentids, KERNEL_STATUS_PARAM_INVALID, "Get segment_ids failed")
  auto numsegments = reinterpret_cast<num_segments_t *>(ctx.Input(2)->GetData());
  CUST_KERNEL_CHECK_NULLPTR(ctx, numsegments, KERNEL_STATUS_PARAM_INVALID, "Get num_segments failed")

  if (id_size <= 0) {
    CUST_KERNEL_LOG_ERROR(ctx, "segment_ids num elements should great than 0");
    return KERNEL_STATUS_PARAM_INVALID;
  }

  int64_t reshapesize = data_size / id_size;
  // Initialized to 1
  for (int64_t k = 0; k < data_size; k++) {
    *(output_y + k) = static_cast<input_t>(1);
  }
  if (data_size <= kParallelDataNums) {
    // calculation process
    for (int64_t i = 0; i < id_size; i++) {
      if (*(segmentids + i) < *numsegments) {
        for (int64_t j = 0; j < reshapesize; j++) {
          *(output_y + *(segmentids + i) * reshapesize + j) *= *(input_x + i * reshapesize + j);
        }
      }
    }
  } else {
    uint32_t min_core_num = 1;
    uint32_t max_core_num = std::max(min_core_num, aicpu::CpuKernelUtils::GetCPUNum(ctx) - 2);
    if (max_core_num > reshapesize) {
      max_core_num = reshapesize;
    }
    // calculation process
    auto shard_unsorted_segment_prod = [&](int64_t start, int64_t end) {
      for (int64_t i = 0; i < id_size; i++) {
        if (*(segmentids + i) < *numsegments) {
          for (int64_t j = start; j < end; j++) {
            *(output_y + *(segmentids + i) * reshapesize + j) *= *(input_x + i * reshapesize + j);
          }
        }
      }
    };
    CUST_KERNEL_HANDLE_ERROR(
      ctx, CpuKernelUtils::ParallelFor(ctx, reshapesize, reshapesize / max_core_num, shard_unsorted_segment_prod),
      "CpuKernelUtils::ParallelFor failed.");
  }
  return KERNEL_STATUS_OK;
}

template <typename input_t, typename segment_ids_t>
uint32_t UnsortedSegmentProdCpuKernel::DoComputeWithNumSegmentsType(CpuKernelContext &ctx, DataType num_segments_type) {
  switch (num_segments_type) {
    case DT_INT32:
      return UnsortedSegmentProdComputeTemplate<input_t, segment_ids_t, int32_t>(ctx);
    case DT_INT64:
      return UnsortedSegmentProdComputeTemplate<input_t, segment_ids_t, int64_t>(ctx);

    default:
      CUST_KERNEL_LOG_ERROR(ctx, "UnsortedSegmentProd invalid num_segments_type type [%s]",
                            DTypeStr(num_segments_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }
}

template <typename input_t>
uint32_t UnsortedSegmentProdCpuKernel::DoComputeWithSegmentIdsType(CpuKernelContext &ctx, DataType segment_ids_type) {
  auto num_segments_type = ctx.Input(2)->GetDataType();
  switch (segment_ids_type) {
    case DT_INT32:
      return DoComputeWithNumSegmentsType<input_t, int32_t>(ctx, num_segments_type);
    case DT_INT64:
      return DoComputeWithNumSegmentsType<input_t, int64_t>(ctx, num_segments_type);

    default:
      CUST_KERNEL_LOG_ERROR(ctx, "UnsortedSegmentProd invalid segment_ids_type type [%s]",
                            DTypeStr(segment_ids_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }
}

uint32_t UnsortedSegmentProdCpuKernel::Compute(CpuKernelContext &ctx) {
  auto input_type = ctx.Input(0)->GetDataType();
  auto segment_ids_type = ctx.Input(1)->GetDataType();
  switch (input_type) {
    case DT_INT32:
      return DoComputeWithSegmentIdsType<int32_t>(ctx, segment_ids_type);
    case DT_INT16:
      return DoComputeWithSegmentIdsType<int16_t>(ctx, segment_ids_type);
    case DT_FLOAT:
      return DoComputeWithSegmentIdsType<float>(ctx, segment_ids_type);
    case DT_DOUBLE:
      return DoComputeWithSegmentIdsType<double>(ctx, segment_ids_type);
    case DT_FLOAT16:
      return DoComputeWithSegmentIdsType<Eigen::half>(ctx, segment_ids_type);
    case DT_INT8:
      return DoComputeWithSegmentIdsType<int8_t>(ctx, segment_ids_type);
    case DT_INT64:
      return DoComputeWithSegmentIdsType<int64_t>(ctx, segment_ids_type);
    case DT_UINT8:
      return DoComputeWithSegmentIdsType<uint8_t>(ctx, segment_ids_type);
    case DT_UINT16:
      return DoComputeWithSegmentIdsType<uint16_t>(ctx, segment_ids_type);
    case DT_UINT32:
      return DoComputeWithSegmentIdsType<uint32_t>(ctx, segment_ids_type);
    case DT_UINT64:
      return DoComputeWithSegmentIdsType<uint64_t>(ctx, segment_ids_type);
    case DT_COMPLEX64:
      return DoComputeWithSegmentIdsType<std::complex<float>>(ctx, segment_ids_type);
    case DT_COMPLEX128:
      return DoComputeWithSegmentIdsType<std::complex<double>>(ctx, segment_ids_type);
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "UnsortedSegmentProd invalid input type [%s]", DTypeStr(input_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }
  return KERNEL_STATUS_OK;
}

REGISTER_MS_CPU_KERNEL(kUnsortedSegmentProd, UnsortedSegmentProdCpuKernel);
}  // namespace aicpu
