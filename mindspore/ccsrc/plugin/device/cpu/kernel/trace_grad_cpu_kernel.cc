/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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
#include "plugin/device/cpu/kernel/trace_grad_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "kernel/common_utils.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kInputNum = 2;
constexpr size_t kOutputNum = 1;
}  // namespace

bool TraceGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  values_type_ = inputs.at(kIndex0)->dtype_id();
  return true;
}

int TraceGradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  input_shape_ = inputs.at(kIndex1)->GetDeviceShapeVector();
  const std::vector<int64_t> x_shape_ = {2};
  if (input_shape_ != x_shape_) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the shape of input[x_shape] should be " << x_shape_
                      << ", but got " << input_shape_ << ".";
  }
  return KRET_OK;
}

bool TraceGradCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                   const std::vector<kernel::KernelTensor *> &,
                                   const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kInputNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kOutputNum, kernel_name_);
  switch (values_type_) {
    case kNumberTypeInt8:
      LaunchKernel<int8_t>(inputs, outputs);
      break;
    case kNumberTypeUInt8:
      LaunchKernel<uint8_t>(inputs, outputs);
      break;
    case kNumberTypeInt16:
      LaunchKernel<int16_t>(inputs, outputs);
      break;
    case kNumberTypeUInt16:
      LaunchKernel<uint16_t>(inputs, outputs);
      break;
    case kNumberTypeFloat16:
      LaunchKernel<float16>(inputs, outputs);
      break;
    case kNumberTypeInt32:
      LaunchKernel<int32_t>(inputs, outputs);
      break;
    case kNumberTypeUInt32:
      LaunchKernel<uint32_t>(inputs, outputs);
      break;
    case kNumberTypeFloat32:
      LaunchKernel<float>(inputs, outputs);
      break;
    case kNumberTypeInt64:
      LaunchKernel<int64_t>(inputs, outputs);
      break;
    case kNumberTypeUInt64:
      LaunchKernel<uint64_t>(inputs, outputs);
      break;
    case kNumberTypeFloat64:
      LaunchKernel<double>(inputs, outputs);
      break;
    default:
      MS_LOG(EXCEPTION) << "Trace Grad Unsupported input data type.";
  }
  return true;
}

template <typename T>
void TraceGradCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                         const std::vector<KernelTensor *> &outputs) {
  T *grad = GetDeviceAddress<T>(inputs, kIndex0);
  MS_EXCEPTION_IF_NULL(grad);
  auto shape = GetDeviceAddress<int64_t>(inputs, kIndex1);
  MS_EXCEPTION_IF_NULL(shape);
  T *output_addr = GetDeviceAddress<T>(outputs, kIndex0);
  MS_EXCEPTION_IF_NULL(output_addr);

  if (memset_s(output_addr, outputs[0]->size(), 0, outputs[0]->size()) != EOK) {
    MS_LOG(EXCEPTION) << "Failed to init output memory.";
  }
  int64_t min_size = std::min(shape[0], shape[1]);
  for (int64_t i = 0; i < min_size; ++i) {
    *(output_addr + i * shape[1] + i) = *grad;
  }
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, TraceGrad, TraceGradCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
