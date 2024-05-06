/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/ascend/kernel/bisheng/add_bisheng_kernel.h"
#include <algorithm>
#include <functional>
#include "plugin/device/ascend/kernel/bisheng/bisheng_op_info.h"
#include "plugin/device/ascend/kernel/bisheng/impl/add.h"

namespace mindspore::kernel {
namespace {
constexpr size_t kAddInputsNum = 2;
constexpr size_t kAddOutputsNum = 1;
}  // namespace

bool AddBishengKernel::Init(const std::vector<kernel::KernelTensor *> &inputs,
                            const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kAddOutputsNum, kernel_name_);
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  func_name_ = func_name_list_.at(index);
  return true;
}

template <typename T>
bool AddBishengKernel::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                    const std::vector<kernel::KernelTensor *> &workspace,
                                    const std::vector<kernel::KernelTensor *> &outputs, void *stream) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kAddInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kAddOutputsNum, kernel_name_);
  MS_EXCEPTION_IF_NULL(stream);
  if (inputs[0]->size() != inputs[1]->size()) {
    MS_LOG(EXCEPTION) << "Input memory size of first input " << inputs[0]->size()
                      << " must be equal to memory size of second input " << inputs[1]->size();
  }
  if (inputs[0]->size() != outputs[0]->size()) {
    MS_LOG(EXCEPTION) << "Input memory size of input " << inputs[0]->size()
                      << " must be equal to memory size of output " << outputs[0]->size();
  }
  bisheng::Add<T>(inputs[0]->device_ptr(), inputs[1]->device_ptr(), outputs[0]->device_ptr(),
                  workspace[0]->device_ptr(), stream);
  return true;
}

int AddTilingFunc(const BiShengKernelArgs &args, std::vector<uint8_t> *tiling_data) {
  MS_EXCEPTION_IF_NULL(tiling_data);
  const auto &output_shapes = args.output_shapes;
  if (output_shapes.empty()) {
    MS_LOG(EXCEPTION) << "Add op must have output shapes.";
  }
  uint64_t size = std::accumulate(output_shapes[0].begin(), output_shapes[0].end(), 1, std::multiplies<uint64_t>());
  TilingPacking::PackTiling(tiling_data, size);
  return 0;
}

REG(AddBishengKernel)
  .OpName("BSAdd")
  .Input(0, "x1")
  .Input(1, "x2")
  .Output(0, "y")
  .DataTypeFormat({I8_Default, I8_Default, I8_Default}, &AddBishengKernel::LaunchKernel<int8_t>)
  .DataTypeFormat({I16_Default, I16_Default, I16_Default}, &AddBishengKernel::LaunchKernel<int16_t>)
  .DataTypeFormat({I32_Default, I32_Default, I32_Default}, &AddBishengKernel::LaunchKernel<int32_t>)
  .DataTypeFormat({I64_Default, I64_Default, I64_Default}, &AddBishengKernel::LaunchKernel<int64_t>)
  .DataTypeFormat({U8_Default, U8_Default, U8_Default}, &AddBishengKernel::LaunchKernel<uint8_t>)
  .DataTypeFormat({U16_Default, U16_Default, U16_Default}, &AddBishengKernel::LaunchKernel<uint16_t>)
  .DataTypeFormat({U32_Default, U32_Default, U32_Default}, &AddBishengKernel::LaunchKernel<uint32_t>)
  .DataTypeFormat({U64_Default, U64_Default, U64_Default}, &AddBishengKernel::LaunchKernel<uint64_t>)
  .DataTypeFormat({F16_Default, F16_Default, F16_Default}, &AddBishengKernel::LaunchKernel<half>)
  .DataTypeFormat({F32_Default, F32_Default, F32_Default}, &AddBishengKernel::LaunchKernel<float>,
                  "_ZTSN9mindspore6kernel7bisheng9AddKernelIaEE")
  .Tiling(&AddTilingFunc)
  .End();
}  // namespace mindspore::kernel
