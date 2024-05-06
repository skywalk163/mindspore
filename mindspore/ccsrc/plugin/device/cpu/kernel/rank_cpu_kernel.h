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
#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RANK_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RANK_CPU_KERNEL_H_

#include <vector>
#include <string>
#include <limits>
#include <tuple>
#include <map>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "plugin/device/cpu/kernel/nnacl/op_base.h"

namespace mindspore {
namespace kernel {
class RankCpuKernelMod : public NativeCpuKernelMod {
 public:
  RankCpuKernelMod() = default;
  ~RankCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
              const std::vector<KernelTensor *> &outputs) override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  int64_t input_shape_size;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RANK_CPU_KERNEL_H_
