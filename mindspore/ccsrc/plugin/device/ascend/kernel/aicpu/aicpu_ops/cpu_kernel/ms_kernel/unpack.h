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
#ifndef AICPU_KERNELS_NORMALIZED_UNPACK_H_
#define AICPU_KERNELS_NORMALIZED_UNPACK_H_

#include <memory>
#include <vector>
#include "cpu_types.h"
#include "utils/bcast.h"
#include "unsupported/Eigen/CXX11/Tensor"
#include "securec.h"
#include "inc/ms_cpu_kernel.h"
#include "inc/ms_cpu_kernel.h"
#include "inc/kernel_log.h"
#include "context/common/status.h"

namespace aicpu {
class UnpackCpuKernel : public CpuKernel {
 public:
  uint32_t Compute(CpuKernelContext &ctx) override;

 private:
  uint32_t CheckAndInitParams(CpuKernelContext &ctx);

  template <typename T>
  uint32_t UnpackWithOneOutput(CpuKernelContext &ctx, T *input_data_ptr, std::vector<T *> output_data_vec);

  template <typename T>
  uint32_t UnpackWithDimZero(CpuKernelContext &ctx, T *input_data_ptr, std::vector<T *> output_data_vec);

  template <typename T>
  uint32_t UnpackCompute(T *input_data_ptr, std::vector<T *> output_data_vec, CpuKernelContext &ctx);

  template <typename T>
  uint32_t DoCompute(CpuKernelContext &ctx);

 private:
  DataType data_type;
  uint64_t unpack_axis;
  int64_t unpack_num;
  int64_t value_num;
  void *value_data_ptr;
  std::vector<void *> output_ptr_vec;
  std::vector<int64_t> value_shape_vec;
};
}  // namespace aicpu
#endif