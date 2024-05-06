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

#include "plugin/device/cpu/kernel/tensor_copy_cpu_kernel.h"
#include <algorithm>
#include <complex>
#include <functional>
#include <map>

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t ktInput = 0;
constexpr size_t ktOutput = 0;
using complex64 = std::complex<float>;
using complex128 = std::complex<double>;
}  // namespace

bool TensorCopyCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &outputs) {
  auto input_shape = inputs[ktInput]->GetShapeVector();
  auto output_shape = outputs[ktOutput]->GetShapeVector();
  auto input_type = inputs[ktInput]->dtype_id();
  auto output_type = inputs[ktOutput]->dtype_id();

  auto copy_size = GetTypeByte(TypeIdToType(input_type));
  copy_size = std::accumulate(input_shape.begin(), input_shape.end(), copy_size, std::multiplies<size_t>());
  output_size_list_.push_back(copy_size);
  if (input_type != output_type) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the type of 'input' and the type of 'output' should be same, but 'input' type is "
                  << input_type << "while 'output' type is " << output_type;
    return false;
  }
  if (input_shape != output_shape) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the shape of 'input' and the shape of 'output' should be same, but 'input' shape is "
                  << input_shape << "while 'output' shape is " << output_shape;
    return false;
  }
  return true;
}

int TensorCopyCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  return 0;
}

bool TensorCopyCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                    const std::vector<kernel::KernelTensor *> & /* workspace */,
                                    const std::vector<kernel::KernelTensor *> &outputs) {
  auto input = GetDeviceAddress<void>(inputs, 0);
  auto output = GetDeviceAddress<void>(outputs, 0);

  auto ret = memcpy_s(output, outputs[0]->size(), input, inputs[0]->size());
  if (ret != EOK) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', memory copy failed. Error no: " << ret << "Copy input:" << input
                  << " size=" << inputs[0]->size() << " ,To output:" << output << " size=" << outputs[0]->size();
    return false;
  }
  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, TensorMove, TensorCopyCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
