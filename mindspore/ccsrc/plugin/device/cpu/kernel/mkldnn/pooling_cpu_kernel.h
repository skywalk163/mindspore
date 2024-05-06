/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_POOLING_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_POOLING_CPU_KERNEL_H_

#include <vector>
#include <memory>
#include <utility>
#include <unordered_map>
#include <map>
#include <string>
#include "mindspore/core/ops/op_utils.h"
#include "plugin/device/cpu/kernel/mkldnn/mkl_cpu_kernel.h"

namespace mindspore {
namespace kernel {
constexpr auto kUnkown = "Unknown";
constexpr size_t kPoolingDilation = 1;

class PoolingCpuKernelMod : public MKLCpuKernelMod {
 public:
  PoolingCpuKernelMod() = default;
  explicit PoolingCpuKernelMod(const std::string &kernel_type) : kernel_type_(kernel_type) {}
  ~PoolingCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 protected:
  template <typename T>
  void EliminateInvalidPadding(T *output);
  template <typename T>
  void ReComputeDivisor(T *output);

  dnnl::algorithm algorithm_{dnnl::algorithm::pooling_max};
  bool ceil_mode_{false};
  int64_t divisor_override_{0};
  std::vector<int64_t> dst_shape_;
  std::vector<int64_t> kernel_;
  std::vector<int64_t> padding_invalid_;
  std::string format_;
  mindspore::PadMode pad_mode_;
  std::vector<int64_t> kernel_include_nc{};
  std::vector<int64_t> strides_include_nc{};
  std::map<uint32_t, tensor::TensorPtr> inputs_on_host_{};

 private:
  void InitPoolingFields(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);

  std::string kernel_type_{kUnkown};

  template <typename T>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &outputs);

  TypeId dtype_{kTypeUnknown};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_POOLING_CPU_KERNEL_H_
