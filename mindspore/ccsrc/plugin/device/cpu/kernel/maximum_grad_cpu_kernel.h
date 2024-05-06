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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_MAXIMUM_GRAD_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_MAXIMUM_GRAD_CPU_KERNEL_H_

#include <map>
#include <vector>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class MaximumGradCpuKernelMod : public NativeCpuKernelMod {
 public:
  MaximumGradCpuKernelMod() = default;
  ~MaximumGradCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

 private:
  template <typename T>
  void LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);

  template <typename T>
  void MaximumGradRecTask(const T *x, const T *y, const T *dout, T *dx, T *dy, size_t dim, size_t x_index,
                          size_t y_index, size_t dout_index, const std::vector<size_t> &x_cargo,
                          const std::vector<size_t> &y_cargo, const std::vector<size_t> &dout_cargo,
                          const std::vector<size_t> &x_shape, const std::vector<size_t> &y_shape,
                          const std::vector<size_t> &dout_shape);

  template <typename T>
  void MaximumGradRecTaskSerialized(const T *x, const T *y, const T *dout, T *dx, T *dy, size_t dim, size_t x_index,
                                    size_t y_index, size_t dout_index, const std::vector<size_t> &x_cargo,
                                    const std::vector<size_t> &y_cargo, const std::vector<size_t> &dout_cargo,
                                    const std::vector<size_t> &x_shape, const std::vector<size_t> &y_shape,
                                    const std::vector<size_t> &dout_shape, bool paralleled);

  ShapeVector x_shape_;
  ShapeVector y_shape_;
  ShapeVector dout_shape_;
  ShapeVector dx_shape_;
  ShapeVector dy_shape_;
  TypeId dtype_{kTypeUnknown};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_MAXIMUM_GRAD_CPU_KERNEL_H_
