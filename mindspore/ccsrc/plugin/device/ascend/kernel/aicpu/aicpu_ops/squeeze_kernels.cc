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

#include "plugin/device/ascend/kernel/aicpu/aicpu_ops/squeeze_kernels.h"
#include <memory.h>
#include <map>
#include <thread>
#include "ops/squeeze.h"
#include "cpu_kernel/common/status.h"
#include "./kernel_log.h"
#include "./kernel_errcode.h"
#include "proto/node_def.pb.h"
#include "common/tensor.h"
#include "proto/attr.pb.h"
#include "aicpu_sharder/aicpu_sharder.h"
namespace aicpu {
namespace dataset {
uint32_t SqueezeKernel::DoCompute() {
  size_t type_size = GetDataTypeSize(matrix_info_.matrix_type);
  if (type_size < 1) {
    CUST_AICPU_LOGE(workspace_info_, "don't support input tensor types");
    return kAicpuKernelStateFailed;
  }
  int ret = memcpy_s(reinterpret_cast<void *>(io_addrs_[1]), input_size_ * type_size,
                     reinterpret_cast<void *>(io_addrs_[0]), input_size_ * type_size);
  if (ret != EOK) {
    KERNEL_LOG_ERROR("For 'Squeeze', memcpy_s failed, ret=%d.", ret);
    return KERNEL_STATUS_INNER_ERROR;
  }
  return kAicpuKernelStateSucess;
}

uint32_t SqueezeKernel::ParseKernelParam() {
  CUST_AICPU_LOGI(workspace_info_, "aicpu SqueezeKernel");
  aicpuops::Tensor input_tensor = node_def_.inputs(0);
  aicpuops::TensorShape input_shape = input_tensor.tensor_shape();
  input_size_ = 1;

  matrix_info_.matrix_type = static_cast<::aicpuops::DataType>(input_tensor.tensor_type());
  for (int i = 0; i < input_shape.dim_size(); ++i) {
    matrix_info_.matrix_shape.push_back(input_shape.dim(i).size());
    input_size_ *= input_shape.dim(i).size();
  }

  return kAicpuKernelStateSucess;
}
}  // namespace dataset
}  // namespace aicpu

extern "C" {
__attribute__((visibility("default"))) uint32_t Squeeze(void *param) {
  aicpu::dataset::SqueezeKernel squeezeKernel;
  return squeezeKernel.Compute(param);
}
}
