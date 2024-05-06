/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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
#include "pad_v3_grad.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

#include "securec.h"
#include "context/inc/cpu_kernel_utils.h"
#include "utils/eigen_tensor.h"
#include "utils/kernel_util.h"

namespace {
const char *kPadV3Grad = "PadV3Grad";
constexpr uint32_t kInputNum = 2;
constexpr uint32_t kOutputNum = 1;
constexpr int64_t kParallelNum = 1024 * 64;
const int64_t k3DNum = 6;
const int64_t k2DNum = 4;
const int64_t k1DNum = 2;
constexpr int64_t kpad_l = 0;
constexpr int64_t kpad_t = 2;
constexpr int64_t kpad_f = 4;
constexpr int64_t kpad_r = 1;
constexpr int64_t kpad_d = 3;
constexpr int64_t kpad_b = 5;
constexpr int64_t kwidth = 1;
constexpr int64_t kheight = 2;
constexpr int64_t kchannel = 3;
constexpr int64_t kInput1Dim = 3;
constexpr int64_t kInput2Dim = 4;
constexpr int64_t kInput3Dim = 5;
constexpr int64_t k2Num = 2;
constexpr int64_t k3Num = 3;
constexpr int64_t k4Num = 4;
constexpr auto kStep2 = 2;

const std::vector<std::string> mode_list = {"reflect", "edge", "circular"};
using float16 = Eigen::half;

#define PAD_V3_GRAD_READ_PADDINGS(DTYPE, TYPE, CTX)                   \
  case (DTYPE): {                                                     \
    uint32_t result = PadV3ReadPaddingsAndSetOutputShape<TYPE>(CTX);  \
    if (result != KERNEL_STATUS_OK) {                                 \
      CUST_KERNEL_LOG_ERROR(ctx, "PadV3Grad kernel compute failed."); \
      return result;                                                  \
    }                                                                 \
    break;                                                            \
  }

#define PAD_V3_GRAD_COMPUTE_CASE(DTYPE, TYPE, CTX)                    \
  case (DTYPE): {                                                     \
    uint32_t result = PadV3GradCompute<TYPE>(CTX);                    \
    if (result != KERNEL_STATUS_OK) {                                 \
      CUST_KERNEL_LOG_ERROR(ctx, "PadV3Grad kernel compute failed."); \
      return result;                                                  \
    }                                                                 \
    break;                                                            \
  }
}  // namespace

namespace aicpu {
uint32_t PadV3GradCpuKernel::Compute(CpuKernelContext &ctx) {
  CUST_KERNEL_HANDLE_ERROR(ctx, PadV3GradCheck(ctx), "PadV3Grad check params failed.");
  auto paddings_type = ctx.Input(1)->GetDataType();
  switch (paddings_type) {
    PAD_V3_GRAD_READ_PADDINGS(DT_INT32, int32_t, ctx)
    PAD_V3_GRAD_READ_PADDINGS(DT_INT64, int64_t, ctx)
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "PadV3Grad paddings data type [%s] not support.", DTypeStr(paddings_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }
  auto data_type = ctx.Output(0)->GetDataType();
  switch (data_type) {
    PAD_V3_GRAD_COMPUTE_CASE(DT_INT8, int8_t, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_INT16, int16_t, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_INT32, int32_t, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_INT64, int64_t, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_UINT8, uint8_t, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_UINT16, uint16_t, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_UINT32, uint32_t, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_UINT64, uint64_t, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_FLOAT16, float16, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_FLOAT, float, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_DOUBLE, double, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_COMPLEX64, std::complex<float>, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_COMPLEX128, std::complex<double>, ctx)
    PAD_V3_GRAD_COMPUTE_CASE(DT_BOOL, bool, ctx)
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "PadV3Grad kernel data type [%s] not support.", DTypeStr(data_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }
  return KERNEL_STATUS_OK;
}

uint32_t PadV3GradCpuKernel::PadV3GradCheck(CpuKernelContext &ctx) {
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, kInputNum, kOutputNum), "PadV3Grad check failed.");
  if (ctx.GetAttr("paddings_contiguous") == nullptr) {
    padding_contiguous = true;
    CUST_KERNEL_LOG_DEBUG(ctx, "Get attr [paddings_contiguous] failed, use default value [true]");
  } else {
    padding_contiguous = ctx.GetAttr("paddings_contiguous")->GetBool();
  }
  if (ctx.GetAttr("mode") == nullptr) {
    mode = "reflect";
    CUST_KERNEL_LOG_DEBUG(ctx, "Get attr [mode] failed, use default value [reflect]");
  } else {
    mode = ctx.GetAttr("mode")->GetString();
    const bool is_mode_available = std::find(mode_list.begin(), mode_list.end(), mode) != mode_list.end();
    if (!is_mode_available) {
      CUST_KERNEL_LOG_ERROR(ctx, "Attr [mode] must be included in [reflect, edge], but got [%s]", mode.c_str());
      return KERNEL_STATUS_PARAM_INVALID;
    }
  }

  if (ctx.Input(0)->GetDataType() != ctx.Output(0)->GetDataType()) {
    CUST_KERNEL_LOG_ERROR(ctx, "Tensor y dtype[%s] must be same with x dtype[%s]",
                          DTypeStr(ctx.Output(0)->GetDataType()).c_str(),
                          DTypeStr(ctx.Input(0)->GetDataType()).c_str());
    return KERNEL_STATUS_PARAM_INVALID;
  }
  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t PadV3GradCpuKernel::PadV3ReadPaddingsAndSetOutputShape(CpuKernelContext &ctx) {
  num_elem = ctx.Input(1)->NumElements();
  input_dim = ctx.Input(0)->GetTensorShape()->GetDims();
  const std::vector<int64_t> input_shape = ctx.Input(0)->GetTensorShape()->GetDimSizes();
  auto paddings_ptr = reinterpret_cast<T *>(ctx.Input(1)->GetData());
  paddings = std::vector<int64_t>(num_elem, 0);

  if (num_elem == 1) {
    num_elem = k2Num * (input_dim - k2Num);
    for (int64_t i = 0; i < num_elem; ++i) {
      paddings[i] = static_cast<int64_t>(paddings_ptr[0]);
    }
  } else {
    for (int64_t i = 0; i < num_elem; ++i) {
      paddings[i] = static_cast<int64_t>(paddings_ptr[i]);
    }
  }
  // Find redundancy index in paddings
  const int64_t remaining_paddings_num = 2;
  auto redundancy_paddings_num = num_elem - remaining_paddings_num;
  for (int64_t i = remaining_paddings_num; i <= num_elem - remaining_paddings_num; i += kStep2) {
    if (std::any_of(paddings.begin(), paddings.begin() + i, [](const int64_t &val) { return val != 0; })) {
      redundancy_paddings_num = i - remaining_paddings_num;
      break;
    }
  }
  num_elem -= redundancy_paddings_num;

  // (0, 0, 0, 0, 1, 2, 3, 4) -> (3, 4, 1, 2, 0, 0, 0, 0)
  std::reverse(paddings.begin(), paddings.end());
  for (size_t i = 1; i < paddings.size(); i += kStep2) {
    std::swap(paddings[i - 1], paddings[i]);
  }

  parallelSliceNum = 1;
  for (int64_t i = 0; i < input_dim - num_elem / k2Num; i++) {
    parallelSliceNum *= input_shape[i];
  }

  if (!padding_contiguous && num_elem == k3DNum) {
    std::vector<int64_t> tmp = paddings;
    paddings[1] = tmp[k3Num];
    paddings[k2Num] = tmp[1];
    paddings[k3Num] = tmp[k4Num];
    paddings[k4Num] = tmp[k2Num];
  }

  if (!padding_contiguous && num_elem == k2DNum) {
    std::vector<int64_t> tmp = paddings;
    paddings[1] = tmp[k2Num];
    paddings[k2Num] = tmp[1];
  }
  return KERNEL_STATUS_OK;
}

int64_t PadV3GradCpuKernel::IndexCaculate(int64_t pad_value, int64_t pad_end, int64_t now, int64_t output_value,
                                          int64_t o_start, int64_t i_start) {
  int64_t ip = 0;
  if (now < pad_value) {
    if (mode == "reflect") {
      ip = pad_value + pad_value - now;
    } else if (mode == "edge") {
      ip = pad_value;
    } else if (mode == "circular") {
      ip = output_value + now + std::min(int64_t(0), pad_end);
    }
  } else if (now >= pad_value && now < output_value + pad_value) {
    ip = now;
  } else {
    if (mode == "reflect") {
      ip = (output_value + pad_value - 1) + (output_value + pad_value - 1) - now;
    } else if (mode == "edge") {
      ip = output_value + pad_value - 1;
    } else if (mode == "circular") {
      ip = now - output_value - std::min(int64_t(0), pad_value);
    }
  }
  ip = ip - o_start + i_start;
  return ip;
}

template <typename T>
uint32_t PadV3GradCpuKernel::PadV3GradCompute1(T *input, T *output, int64_t p) {
  if (num_elem == k1DNum) {
    PadV3GradCompute1D<T>(input, output, p);
  } else if (num_elem == k2DNum) {
    for (int i = 0; i < input_h; i++) {
      PadV3GradCompute2D<T>(input, output, p, i);
    }
  } else if (num_elem == k3DNum) {
    for (int z = 0; z < input_c; z++) {
      PadV3GradCompute3D<T>(input, output, p, z);
    }
  }
  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t PadV3GradCpuKernel::PadV3GradCompute1D(T *input, T *output, int64_t p) {
  int ip_x;
  for (int j = 0; j < input_w; j++) {
    ip_x = IndexCaculate(pad_l, pad_r, j, output_w, o_start_x, i_start_x);
    T *src_p = input + p * input_w + j;
    T *dest_p = output + p * output_w + ip_x;
    *dest_p += *src_p;
  }
  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t PadV3GradCpuKernel::PadV3GradCompute2D(T *input, T *output, int64_t p, int64_t i) {
  for (int j = 0; j < input_w; j++) {
    int ip_x = IndexCaculate(pad_l, pad_r, j, output_w, o_start_x, i_start_x);
    int ip_y = IndexCaculate(pad_t, pad_d, i, output_h, o_start_y, i_start_y);
    T *src_p = input + p * input_w * input_h + i * input_w + j;
    T *dest_p = output + p * output_w * output_h + ip_y * output_w + ip_x;
    *dest_p += *src_p;
  }
  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t PadV3GradCpuKernel::PadV3GradCompute3D(T *input, T *output, int64_t p, int64_t z) {
  for (int i = 0; i < input_h; i++) {
    for (int j = 0; j < input_w; j++) {
      int ip_x = IndexCaculate(pad_l, pad_r, j, output_w, o_start_x, i_start_x);
      int ip_y = IndexCaculate(pad_t, pad_d, i, output_h, o_start_y, i_start_y);
      int ip_z = IndexCaculate(pad_f, pad_b, z, output_c, o_start_z, i_start_z);
      T *src_p = input + p * input_w * input_h * input_c + z * input_w * input_h + i * input_w + j;
      T *dest_p = output + p * output_w * output_h * output_c + ip_z * output_w * output_h + ip_y * output_w + ip_x;
      *dest_p += *src_p;
    }
  }
  return KERNEL_STATUS_OK;
}

template <typename T>
uint32_t PadV3GradCpuKernel::PadV3GradCompute(CpuKernelContext &ctx) {
  const std::vector<int64_t> input_shape = ctx.Input(0)->GetTensorShape()->GetDimSizes();
  std::vector<int64_t> output_shape = ctx.Output(0)->GetTensorShape()->GetDimSizes();

  // For GE graph, the output rank will be decreased if the last dim is 1, so the rank between output and input would be
  // different, which causing accuracy error.
  for (size_t i = 0; i < input_shape.size() - output_shape.size(); i++) {
    output_shape.emplace_back(1);
  }
  T *input = reinterpret_cast<T *>(ctx.Input(0)->GetData());
  T *output = reinterpret_cast<T *>(ctx.Output(0)->GetData());

  output_w = output_shape.end()[-kwidth];
  output_h = output_shape.end()[-kheight];
  output_c = output_shape.end()[-kchannel];
  input_w = input_shape.end()[-kwidth];
  input_h = input_shape.end()[-kheight];
  input_c = input_shape.end()[-kchannel];
  i_start_x = std::max(int64_t(0), -paddings[kpad_l]);
  i_start_y = std::max(int64_t(0), -paddings[kpad_t]);
  i_start_z = std::max(int64_t(0), -paddings[kpad_f]);
  o_start_x = std::max(int64_t(0), paddings[kpad_l]);
  o_start_y = std::max(int64_t(0), paddings[kpad_t]);
  o_start_z = std::max(int64_t(0), paddings[kpad_f]);
  pad_l = paddings[kpad_l];
  pad_t = paddings[kpad_t];
  pad_f = paddings[kpad_f];
  pad_r = paddings[kpad_r];
  pad_d = paddings[kpad_d];
  pad_b = paddings[kpad_b];

  int64_t output_num_ = 1;
  for (int64_t i = 0; i < input_dim; i++) {
    output_num_ *= output_shape[i];
  }
  auto ret = memset_s(output, sizeof(T) * output_num_, 0, sizeof(T) * output_num_);
  if (ret != EOK) {
    CUST_KERNEL_LOG_ERROR(ctx, "memset_s error, ret=%d", ret);
    return KERNEL_STATUS_INNER_ERROR;
  }
  auto shard_padv3_grad = [&](int64_t start, int64_t end) {
    for (int p = start; p < end; p++) {
      PadV3GradCompute1<T>(input, output, p);
    }
  };
  const int64_t data_num = parallelSliceNum;
  const bool enable_parallel = parallelSliceNum > kParallelNum;
  if (enable_parallel) {
    uint32_t min_core_num = 1;
    uint32_t max_core_num = std::max(min_core_num, aicpu::CpuKernelUtils::GetCPUNum(ctx) - kResvCpuNum);
    CUST_KERNEL_HANDLE_ERROR(ctx, CpuKernelUtils::ParallelFor(ctx, data_num, data_num / max_core_num, shard_padv3_grad),
                             "PadV3Grad Compute failed.");
  } else {
    for (int p = 0; p < data_num; p++) {
      PadV3GradCompute1<T>(input, output, p);
    }
  }
  return KERNEL_STATUS_OK;
}

REGISTER_MS_CPU_KERNEL(kPadV3Grad, PadV3GradCpuKernel);
}  // namespace aicpu
