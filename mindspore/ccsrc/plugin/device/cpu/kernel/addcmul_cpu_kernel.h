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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_ADDCMUL_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_ADDCMUL_CPU_KERNEL_H_

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "plugin/device/cpu/kernel/nnacl/arithmetic_parameter.h"

namespace mindspore {
namespace kernel {
class AddcmulCpuKernelMod : public NativeCpuKernelMod {
 public:
  AddcmulCpuKernelMod() = default;
  ~AddcmulCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  TypeId dtype_{kTypeUnknown};
  TypeId dtype_value_{kTypeUnknown};
  std::vector<int64_t> input_shape0_;
  std::vector<int64_t> input_shape1_;
  std::vector<int64_t> input_shape2_;
  std::vector<int64_t> input_shape3_;
  std::vector<int64_t> output_shape_;
  size_t output_size_{1};
  size_t data_shape_size_{0};
  size_t inputx_shape_size_{0};
  size_t inputy_shape_size_{0};
  size_t value_shape_size_{0};
  ArithmeticParameter mul_para_{};

  template <typename T>
  bool AddcmulCheck(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);

  template <typename T1, typename T2>
  bool AddcmulCompute(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_ADDCMUL_CPU_KERNEL_H_
