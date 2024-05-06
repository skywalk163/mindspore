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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_FRACTIONAL_AVG_POOL_GRAD_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_FRACTIONAL_AVG_POOL_GRAD_CPU_KERNEL_H_

#include <memory>
#include <unordered_map>
#include <vector>
#include <random>
#include <algorithm>
#include <utility>
#include <map>
#include "Eigen/Core"
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class FractionalAvgPoolGradCpuKernelMod : public NativeCpuKernelMod {
 public:
  FractionalAvgPoolGradCpuKernelMod() : kernel_func_(nullptr) {}
  ~FractionalAvgPoolGradCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, outputs);
  }

 protected:
  bool IsNeedUpdateOutputShapeAndSize() override { return true; }
  void UpdateOutputShapeAndSize(const std::vector<KernelTensor *> &inputs,
                                const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool FractionalAvgPoolGradLaunch(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs);
  template <typename T>
  void FractionalAvgPoolGradCompute(
    const int64_t out_cols, const int64_t *col_seq, const int64_t height_start, int64_t height_end, int64_t b,
    int64_t hs, const int64_t out_rows, const int64_t out_depth, const int64_t in_rows, const int64_t in_cols,
    const Eigen::Map<const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>> &out_backprop_mat,
    Eigen::Map<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>> in_backprop_tensor_temp_mat);
  using FractionalAvgPoolGradFunc =
    std::function<bool(FractionalAvgPoolGradCpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &)>;
  static std::vector<std::pair<KernelAttr, FractionalAvgPoolGradFunc>> func_list_;
  FractionalAvgPoolGradFunc kernel_func_;
  std::vector<int64_t> orig_input_shape_;
  std::vector<int64_t> out_backprop_shape_;
  bool overlapping_{false};
  ShapeVector out_shape_{};
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_FRACTIONAL_AVG_POOL_GRAD_CPU_KERNEL_H_
