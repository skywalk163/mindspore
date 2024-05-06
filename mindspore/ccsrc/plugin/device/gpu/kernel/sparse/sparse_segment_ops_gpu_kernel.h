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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_SPARSE_SPARSE_SEGMENT_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_SPARSE_SPARSE_SEGMENT_GPU_KERNEL_H_

#include <vector>
#include <utility>
#include <string>
#include <memory>
#include <map>
#include <algorithm>
#include <functional>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cuda_common.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_class/cuda_class_common.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/sparse_segment_impl.cuh"
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"

namespace mindspore {
namespace kernel {
class SparseSegmentOpsGpuKernelMod : public NativeGpuKernelMod {
 public:
  explicit SparseSegmentOpsGpuKernelMod(const std::string &kernel_type) : kernel_type_(kernel_type) {}
  ~SparseSegmentOpsGpuKernelMod() override = default;

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

 protected:
  void ResetResource() noexcept {
    outer_size_ = 0;
    inner_size_ = 0;
    x_elements_ = 0;
    x_shape_0_ = 0;
    idx_seg_elements_ = 0;
    output_dim0_ = 0;
    output_elements_ = 0;
    is_null_input_ = false;
    output_size_list_.clear();
    workspace_size_list_.clear();
  }

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename R, typename S>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs);

  using SSLaunchFunc =
    std::function<bool(SparseSegmentOpsGpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &, const std::vector<kernel::KernelTensor *> &)>;

 private:
  size_t outer_size_{0};
  size_t inner_size_{0};
  size_t x_elements_{0};
  size_t x_shape_0_{0};
  size_t idx_seg_elements_{0};
  size_t output_dim0_{0};
  size_t output_elements_{0};
  size_t unit_x_size_{1};
  size_t unit_idx_seg_size_{1};
  std::string kernel_type_{"Unknown"};
  bool is_null_input_{false};
  size_t flag_{0};
  void *cuda_stream_{nullptr};
  SSLaunchFunc kernel_func_{};
  static std::map<std::string, std::vector<std::pair<KernelAttr, SSLaunchFunc>>> kernel_attr_map_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_SPARSE_SPARSE_SEGMENT_GPU_KERNEL_H_
