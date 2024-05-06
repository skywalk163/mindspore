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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_CONV_GRAD_FILTER_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_CONV_GRAD_FILTER_CPU_KERNEL_H_

#include <vector>
#include <memory>
#include <string>
#include <map>

#include "plugin/device/cpu/kernel/mkldnn/mkl_cpu_kernel.h"

namespace mindspore {
namespace kernel {
class ConvGradFilterCpuKernelMod : public MKLCpuKernelMod {
 public:
  ConvGradFilterCpuKernelMod() = default;
  explicit ConvGradFilterCpuKernelMod(const std::string &kernel_type) : kernel_type_(kernel_type) {}
  ~ConvGradFilterCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

  std::vector<size_t> GetLaunchIgnoredInputAddressIdx() const override { return {filter_size_index_}; }

 private:
  size_t src_index_{0};
  size_t diff_dst_index_{1};
  const size_t filter_size_index_{2};
  std::string kernel_type_;
  std::string format_;
  int64_t group_;
  mindspore::PadMode pad_mode_;
  std::vector<int64_t> strides_include_nc_;
  std::vector<int64_t> dilation_include_nc_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_CONV_GRAD_FILTER_CPU_KERNEL_H_
