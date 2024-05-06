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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_SPARSE_SOFTMAX_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_SPARSE_SOFTMAX_GPU_KERNEL_H_

#include <vector>
#include <numeric>
#include <algorithm>
#include <functional>
#include <string>
#include <memory>
#include <map>
#include <utility>
#include "mindspore/core/ops/sparse_softmax.h"
#include "abstract/utils.h"
#include "plugin/factory/ms_factory.h"
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/sparse_softmax_impl.cuh"

namespace mindspore {
namespace kernel {
class SparseSoftmaxGpuKernelMod : public NativeGpuKernelMod {
 public:
  SparseSoftmaxGpuKernelMod() { ResetResource(); }
  ~SparseSoftmaxGpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *cuda_stream) override {
    if (is_null_input_) {
      return true;
    }
    cuda_stream_ = cuda_stream;
    return kernel_func_(this, inputs, workspace, outputs);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 protected:
  void ResetResource() noexcept {
    indices_elements_ = 0;
    indice_number_ = 0;
    indice_dims_ = 0;
    values_elements_ = 0;
    shape_elements_ = 0;
    is_null_input_ = false;
    output_size_list_.clear();
    workspace_size_list_.clear();
  }

  void InitSizeLists() {
    size_t output_size = values_elements_ * output_unit_size_;
    output_size_list_.emplace_back(output_size);
    workspace_size_list_.emplace_back(values_elements_ * sizeof(int32_t));
    workspace_size_list_.emplace_back(values_elements_ * sizeof(int64_t));
  }

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs);
  using SparseSoftmaxFunc =
    std::function<bool(SparseSoftmaxGpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &, const std::vector<kernel::KernelTensor *> &)>;

 private:
  size_t indices_unit_size_{1};
  size_t values_unit_size_{1};
  size_t shape_unit_size_{1};
  size_t output_unit_size_{1};
  size_t indices_elements_;
  size_t indice_number_;
  size_t indice_dims_;
  size_t values_elements_;
  size_t shape_elements_;
  SparseSoftmaxFunc kernel_func_{};
  bool is_null_input_{false};
  void *cuda_stream_{nullptr};
  static std::vector<std::pair<KernelAttr, SparseSoftmaxFunc>> func_list_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_SPARSE_SOFTMAX_GPU_KERNEL_H_
