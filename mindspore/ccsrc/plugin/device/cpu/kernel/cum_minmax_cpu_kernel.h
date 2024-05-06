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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_CUM_MINMAX_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_CUM_MINMAX_CPU_KERNEL_H_

#include <vector>
#include <utility>
#include <string>
#include <memory>
#include <map>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

enum CumOpType { CUMMIN = 0, CUMMAX, CUM_OP_INVALID_TYPE = 255 };

namespace mindspore {
namespace kernel {
class CumMinMaxCpuKernelMod : public NativeCpuKernelMod {
 public:
  CumMinMaxCpuKernelMod() = default;
  explicit CumMinMaxCpuKernelMod(const CumOpType &cum_op_type) : cum_op_type_(cum_op_type) {}
  ~CumMinMaxCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, outputs);
  }

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T, typename S>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &outputs);

  using CumMinMaxLaunchFunc = std::function<bool(CumMinMaxCpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                                                 const std::vector<kernel::KernelTensor *> &)>;
  static std::map<CumOpType, std::vector<std::pair<KernelAttr, CumMinMaxLaunchFunc>>> func_list_;
  CumMinMaxLaunchFunc kernel_func_;
  CumOpType cum_op_type_;
  int64_t axis_{0};
  size_t inner_size_{1};
  size_t outer_size_{1};
  size_t axis_size_{1};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_CUM_MINMAX_CPU_KERNEL_H_
