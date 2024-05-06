/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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
#include "plugin/device/ascend/kernel/aicpu/aicpu_ops/uniform_real_kernels.h"

#include <stdlib.h>
#include <memory.h>
#include <cfloat>
#include <string>

#include "aicpu_sharder/aicpu_sharder.h"
#include "common/kernel_errcode.h"
#include "common/kernel_log.h"
#include "common/random_utils.h"

namespace aicpu {
namespace {
const uint32_t kCountsIndex = 1;
const uint32_t kStatesIndex = 2;
}  // namespace
uint32_t UniformRealKernel::DoCompute() {
  float *tmp_out;
  if (out_count_ > 0) {
    tmp_out = reinterpret_cast<float *>(malloc(out_count_ * sizeof(float)));
    if (tmp_out == NULL) {
      return kAicpuKernelStateInvalid;
    }
  } else {
    return kAicpuKernelStateInvalid;
  }

  // get random generator seed
  uint32_t kernel_ret = 0;
  uint64_t rng_seed =
    random::GetKernelBaseRandomStates(io_addrs_, kCountsIndex, kStatesIndex, seed_, seed2_, "UniformReal", &kernel_ret);
  if (kernel_ret != kAicpuKernelStateSucess) {
    free(tmp_out);
    tmp_out = NULL;
    return kAicpuKernelStateFailed;
  }
  rng_.seed(rng_seed);

  for (uint64_t i = 0; i < out_count_; ++i) {
    std::uniform_real_distribution<float> uni_real(0.0, 1.0);
    tmp_out[i] = uni_real(rng_);
  }

  int ret =
    memcpy_s(reinterpret_cast<void *>(io_addrs_[3]), out_count_ * sizeof(float), tmp_out, out_count_ * sizeof(float));
  free(tmp_out);
  tmp_out = NULL;
  if (ret != EOK) {
    KERNEL_LOG_ERROR("memcpy_s failed, ret=%d.", ret)
    return KERNEL_STATUS_INNNER_ERROR;
  }
  return kAicpuKernelStateSucess;
}

uint32_t UniformRealKernel::ParseKernelParam() {
  ::google::protobuf::Map<::std::string, ::aicpuops::AttrValue> attrs = node_def_.attrs();
  seed_ = static_cast<uint64_t>(attrs["seed"].i());
  seed2_ = static_cast<uint64_t>(attrs["seed2"].i());

  // get inputs
  // shape of one batch
  uint64_t tmp_count = 1;
  aicpuops::Tensor shape_tensor = node_def_.inputs(0);
  aicpuops::TensorShape input_shape = shape_tensor.tensor_shape();
  ::aicpuops::DataType shape_dt = static_cast<::aicpuops::DataType>(shape_tensor.tensor_type());
  for (int i = 0; i < input_shape.dim_size(); ++i) {
    tmp_count *= input_shape.dim(i).size();
  }
  if (shape_dt == aicpuops::DataType::MS_INT32) {
    auto input0 = reinterpret_cast<int32_t *>(io_addrs_[0]);
    for (uint64_t index = 0; index < tmp_count; index++) {
      out_count_ *= input0[index];
    }
  } else {
    auto input0 = reinterpret_cast<int64_t *>(io_addrs_[0]);
    for (uint64_t index = 0; index < tmp_count; index++) {
      out_count_ *= input0[index];
    }
  }

  return kAicpuKernelStateSucess;
}
}  // namespace aicpu

extern "C" {
__attribute__((visibility("default"))) uint32_t UniformReal(void *param) {
  aicpu::UniformRealKernel uniform_realKernel;
  return uniform_realKernel.Compute(param);
}
}
