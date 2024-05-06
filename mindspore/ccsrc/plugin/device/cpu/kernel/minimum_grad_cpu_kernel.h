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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MINIMUMGRAD_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MINIMUMGRAD_CPU_KERNEL_H_

#include <map>
#include <utility>
#include <vector>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "include/common/utils/convert_utils.h"

namespace mindspore {
namespace kernel {
class MinimumGradCpuKernelMod : public NativeCpuKernelMod, public MatchKernelHelper<MinimumGradCpuKernelMod> {
 public:
  MinimumGradCpuKernelMod() = default;
  ~MinimumGradCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    MS_EXCEPTION_IF_NULL(kernel_func_);
    return kernel_func_(this, inputs, workspace, outputs);
  }

  const std::vector<std::pair<KernelAttr, KernelRunFunc>> &GetFuncList() const override;

 protected:
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                    const std::vector<KernelTensor *> &outputs);

  std::vector<KernelAttr> GetOpSupport() override { return OpSupport(); }

  template <typename T>
  void MinimumGradRecTask(const T *x, const T *y, const T *dout, T *dx, T *dy, const size_t dim, const size_t x_index,
                          const size_t y_index, const size_t dout_index, const std::vector<size_t> &x_cargo,
                          const std::vector<size_t> &y_cargo, const std::vector<size_t> &dout_cargo,
                          const std::vector<size_t> &x_shape, const std::vector<size_t> &y_shape,
                          const std::vector<size_t> &dout_shape);

  template <typename T>
  void MinimumGradRecTaskSerialized(const T *x, const T *y, const T *dout, T *dx, T *dy, const size_t dim,
                                    const size_t x_index, const size_t y_index, const size_t dout_index,
                                    const std::vector<size_t> &x_cargo, const std::vector<size_t> &y_cargo,
                                    const std::vector<size_t> &dout_cargo, const std::vector<size_t> &x_shape,
                                    const std::vector<size_t> &y_shape, const std::vector<size_t> &dout_shape,
                                    bool paralleled);

  ShapeVector x_shape_;
  ShapeVector y_shape_;
  ShapeVector dout_shape_;
  TypeId dtype_{kTypeUnknown};
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MINIMUMGRAD_CPU_KERNEL_H_
