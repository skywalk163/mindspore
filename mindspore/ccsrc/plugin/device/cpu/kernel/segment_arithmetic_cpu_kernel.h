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
#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SEGMENT_ARITHMETIC_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SEGMENT_ARITHMETIC_CPU_KERNEL_H_

#include <map>
#include <complex>
#include <vector>
#include <memory>
#include <string>
#include <utility>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class SegmentArithmeticCPUKernelMod : public NativeCpuKernelMod {
 public:
  SegmentArithmeticCPUKernelMod() = default;
  ~SegmentArithmeticCPUKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, workspace, outputs);
  }

 protected:
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T1, typename T2>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &workspace,
                    const std::vector<kernel::KernelTensor *> &outputs);

  template <typename T>
  T GetInitValue() const;

  template <typename T>
  bool GetComputeFunc();

  using SegmentArithmeticFunc =
    std::function<bool(SegmentArithmeticCPUKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &, const std::vector<kernel::KernelTensor *> &)>;
  static std::vector<std::pair<KernelAttr, SegmentArithmeticFunc>> func_list_;
  using SegmentComputeFunc = std::function<void(void *output_addr, void *input_addr)>;
  SegmentComputeFunc compute_func_;
  SegmentArithmeticFunc kernel_func_;

  ShapeVector input_x_shape_;
  ShapeVector segment_ids_shape_;
  ShapeVector output_shape_;
  size_t input_x_num_{0};
  size_t segment_ids_num_{0};
  size_t output_num_{0};
  TypeId input_x_dtype_{kTypeUnknown};
  TypeId segment_ids_dtype_{kTypeUnknown};
  TypeId output_dtype_{kTypeUnknown};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SEGMENT_ARITHMETIC_CPU_KERNEL_H_
