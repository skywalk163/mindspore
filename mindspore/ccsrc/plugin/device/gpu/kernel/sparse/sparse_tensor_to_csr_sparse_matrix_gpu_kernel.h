/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_SPARSE_SPARSE_TENSOR_TO_CSR_SPARSE_MATRIX_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_SPARSE_SPARSE_TENSOR_TO_CSR_SPARSE_MATRIX_GPU_KERNEL_H_

#include <vector>
#include <utility>
#include <string>
#include <map>
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/kernel_constants.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/sparse_tensor_to_csr_sparse_matrix_impl.cuh"

namespace mindspore {
namespace kernel {
class SparseTensorToCSRSparseMatrixGpuKernelMod : public NativeGpuKernelMod {
 public:
  SparseTensorToCSRSparseMatrixGpuKernelMod() = default;
  ~SparseTensorToCSRSparseMatrixGpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *cuda_stream) override {
    stream_ = reinterpret_cast<cudaStream_t>(cuda_stream);
    return kernel_func_(this, inputs, workspace, outputs);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename IndiceType, typename DataType>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs);
  using SparseTensorToCSRSparseMatrixFunc =
    std::function<bool(SparseTensorToCSRSparseMatrixGpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &, const std::vector<kernel::KernelTensor *> &)>;

  static std::vector<std::pair<KernelAttr, SparseTensorToCSRSparseMatrixFunc>> func_list_;

 private:
  size_t unit_size_{1};
  size_t input_elements_{};
  int elements[3] = {0, 0, 0};
  cudaStream_t stream_;
  cusparseHandle_t handle_{nullptr};
  int row_num;
  int batch_size;
  int temp_nnz;
  int bapt;
  SparseTensorToCSRSparseMatrixFunc kernel_func_{};
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_SPARSE_SPARSE_TENSOR_TO_CSR_SPARSE_MATRIX_GPU_KERNEL_H_
