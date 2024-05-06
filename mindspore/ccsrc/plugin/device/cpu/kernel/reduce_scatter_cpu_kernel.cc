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

#include "plugin/device/cpu/kernel/reduce_scatter_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "plugin/device/cpu/hal/device/mpi/mpi_interface.h"
#include "mindspore/core/ops/reduce_scatter.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr auto kOP = "op";
constexpr auto kRanksGroup = "group";
constexpr size_t kReduceScatterInputsNum = 1;
constexpr size_t kReduceScatterOutputsNum = 1;
}  // namespace

ReduceScatterCpuKernelMod::ReduceScatterCpuKernelMod() : op_type_(kMPIOpTypeSum) {}

bool ReduceScatterCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  auto op = primitive_->GetAttr(kOP);
  if (op != nullptr) {
    op_type_ = GetValue<std::string>(op);
  }
  auto ranks_group = primitive_->GetAttr(kRanksGroup);
  if (ranks_group == nullptr) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the 'group' can not be null, but got empty value.";
    return false;
  }
  ranks_group_ = GetValue<std::vector<int>>(ranks_group);
  return true;
}

bool ReduceScatterCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                       const std::vector<kernel::KernelTensor *> &,
                                       const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kReduceScatterInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kReduceScatterOutputsNum, kernel_name_);
  auto *input_addr = reinterpret_cast<float *>(inputs[0]->device_ptr());
  auto *output_addr = reinterpret_cast<float *>(outputs[0]->device_ptr());
  auto output_data_num = outputs[0]->size() / sizeof(float);
  return MPIReduceScatter(input_addr, output_addr, ranks_group_, output_data_num, op_type_);
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, _HostReduceScatter, ReduceScatterCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
