/**
 * Copyright 2022-2024 Huawei Technologies Co., Ltd
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

#include "cpu_kernel/ms_kernel/histogram.h"

#include <securec.h>
#include <algorithm>
#include <functional>
#include <mutex>
#include <vector>

#include "context/inc/cpu_kernel_utils.h"
#include "cpu_types.h"
#include "inc/kernel_log.h"
#include "context/common/status.h"
#include "unsupported/Eigen/CXX11/Tensor"
#include "utils/eigen_tensor.h"
#include "utils/kernel_util.h"

namespace {
const char *kHistogram = "Histogram";
constexpr uint32_t kHistogramInputNum = 1;
constexpr uint32_t kHistogramOutputNum = 1;
// when input data size is more than kParallelDataNum, use Parallel func
const int64_t kParallelDataNum = 7 * 1024;
const int64_t kParallelDataNumMid = 35 * 1024;

#define HISTOGRAM_COMPUTE_CASE(DTYPE, TYPE, TYPE_C, CTX)              \
  case (DTYPE): {                                                     \
    uint32_t result = DoCompute<TYPE, TYPE_C>(CTX);                   \
    if (result != KERNEL_STATUS_OK) {                                 \
      CUST_KERNEL_LOG_ERROR(ctx, "Histogram kernel compute failed."); \
      return result;                                                  \
    }                                                                 \
    break;                                                            \
  }

#define HISTOGRAM_SINGLE_COMPUTE(begin, end, y_data)                                                             \
  for (int64_t i = (begin); i < (end); ++i) {                                                                    \
    auto elt = static_cast<InterType>(x_data[i]);                                                                \
    if (elt < static_cast<InterType>(leftmost_edge) || elt > static_cast<InterType>(rightmost_edge)) {           \
      continue;                                                                                                  \
    }                                                                                                            \
    int64_t pos =                                                                                                \
      static_cast<int64_t>((elt - static_cast<InterType>(leftmost_edge)) / step * static_cast<InterType>(bins)); \
    pos = std::min(pos, nbins_minus_1);                                                                          \
    (y_data)[pos] += 1;                                                                                          \
  }
}  // namespace

namespace aicpu {
uint32_t HistogramCpuKernel::ParamCheck(CpuKernelContext &ctx) {
  // check input number and output number
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, kHistogramInputNum, kHistogramOutputNum), "[%s] check params failed.",
                           kHistogram);
  const Tensor *x = ctx.Input(0);
  const Tensor *y = ctx.Output(0);
  CUST_KERNEL_LOG_DEBUG(ctx, "HistogramCpuKernel[%s], input x: size[%llu]; output y: size[%llu].",
                        ctx.GetOpType().c_str(), x->GetDataSize(), y->GetDataSize());
  return KERNEL_STATUS_OK;
}

template <typename T, typename InterType>
uint32_t HistogramCpuKernel::DoCompute(CpuKernelContext &ctx) {
  Tensor *x = ctx.Input(0);
  Tensor *y = ctx.Output(0);
  auto x_data = reinterpret_cast<T *>(x->GetData());
  auto y_data = reinterpret_cast<int32_t *>(y->GetData());
  int64_t x_num = x->NumElements();
  int32_t y_num = y->NumElements();

  if (ctx.GetAttr("min")) {
    min_attr = ctx.GetAttr("min")->GetFloat();
  }
  if (ctx.GetAttr("max")) {
    max_attr = ctx.GetAttr("max")->GetFloat();
  }
  if (ctx.GetAttr("bins")) {
    bins = ctx.GetAttr("bins")->GetInt();
    CUST_KERNEL_CHECK_FALSE(ctx, (bins > 0), KERNEL_STATUS_PARAM_INVALID,
                            "The attr value 'bins' should greater than 0.");
  }

  CUST_KERNEL_CHECK_FALSE(ctx, (bins == y_num), KERNEL_STATUS_PARAM_INVALID,
                          "The attr value 'bins' should equal to the shape of 'y'.");

  // initial y as all zero
  std::fill(y_data, y_data + y_num, 0);
  // calculate left and right of input
  double leftmost_edge = static_cast<double>(min_attr);
  double rightmost_edge = static_cast<double>(max_attr);
  // min and max attr check
  CUST_KERNEL_CHECK_FALSE(ctx, (leftmost_edge <= rightmost_edge), KERNEL_STATUS_PARAM_INVALID,
                          "The attr value 'max' should greater or equal 'min'.");

  auto min_max = std::minmax_element(x_data, x_data + x_num);
  auto x_min = *min_max.first;
  auto x_max = *min_max.second;

  if (leftmost_edge == rightmost_edge && x_num > 0) {
    leftmost_edge = static_cast<double>(x_min);
    rightmost_edge = static_cast<double>(x_max);
  } else if (static_cast<double>(x_min) > rightmost_edge || static_cast<double>(x_max) < leftmost_edge) {
    return KERNEL_STATUS_OK;
  }
  if (leftmost_edge == rightmost_edge) {
    leftmost_edge -= 1;
    rightmost_edge += 1;
  }
  if (std::isinf(leftmost_edge) || std::isinf(rightmost_edge) || std::isnan(leftmost_edge) ||
      std::isnan(rightmost_edge)) {
    CUST_KERNEL_LOG_ERROR(ctx, "For Histogram, range of [%lf, %lf] is not finite.", leftmost_edge, rightmost_edge);
  }

  const InterType step = static_cast<InterType>(rightmost_edge) - static_cast<InterType>(leftmost_edge);
  const int64_t nbins_minus_1 = bins - 1;
  // paraller for calculate
  if (x_num >= kParallelDataNum) {
    uint32_t min_core_num = 1;
    uint32_t max_core_num = std::max(min_core_num, aicpu::CpuKernelUtils::GetCPUNum(ctx));
    if (x_num <= kParallelDataNumMid) {
      max_core_num = std::min(max_core_num, 4U);
    }
    std::mutex hist_mutex;
    CUST_KERNEL_HANDLE_ERROR(ctx,
                             CpuKernelUtils::ParallelFor(ctx, x_num, x_num / max_core_num,
                                                         [&](int64_t start, int64_t end) {
                                                           // Allocates a tensor for the thread's local results
                                                           std::vector<int32_t> hist_local(y_num, 0);
                                                           HISTOGRAM_SINGLE_COMPUTE(start, end, hist_local)
                                                           // Locks and updates the common output
                                                           const std::lock_guard<std::mutex> lock(hist_mutex);
                                                           std::transform(hist_local.begin(), hist_local.end(), y_data,
                                                                          y_data, std::plus<int32_t>());
                                                         }),
                             "Histogram Parallel Compute failed.");
  } else {
    HISTOGRAM_SINGLE_COMPUTE(0, x_num, y_data)
  }
  return KERNEL_STATUS_OK;
}

uint32_t HistogramCpuKernel::Compute(CpuKernelContext &ctx) {
  CUST_KERNEL_HANDLE_ERROR(ctx, ParamCheck(ctx), "HistogramCpuKernel check params failed.");
  auto data_type = ctx.Input(0)->GetDataType();
  switch (data_type) {
    HISTOGRAM_COMPUTE_CASE(DT_FLOAT16, Eigen::half, float, ctx)
    HISTOGRAM_COMPUTE_CASE(DT_FLOAT, float, float, ctx)
    HISTOGRAM_COMPUTE_CASE(DT_INT32, int32_t, float, ctx)
    HISTOGRAM_COMPUTE_CASE(DT_INT64, int64_t, double, ctx)
    HISTOGRAM_COMPUTE_CASE(DT_DOUBLE, double, double, ctx)
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "Histogram kernel data type [%s] not support.", DTypeStr(data_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }
  return KERNEL_STATUS_OK;
}

REGISTER_MS_CPU_KERNEL(kHistogram, HistogramCpuKernel);
}  // namespace aicpu
