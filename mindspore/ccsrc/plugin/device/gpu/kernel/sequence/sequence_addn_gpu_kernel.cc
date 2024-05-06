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

#include "plugin/device/gpu/kernel/sequence/sequence_addn_gpu_kernel.h"
#include <algorithm>
#include <complex>
#include <functional>
#include <utility>
#include "mindspore/core/ops/math_ops.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr int kInputsNum = 1;
constexpr int kOutputsNum = 1;
}  // namespace

bool SequenceAddNGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kOutputsNum, kernel_name_);
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int SequenceAddNGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  tuple_shape_ = inputs[0]->GetShapeVector();
  if (tuple_shape_.empty()) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << " the input tuple size must greater 0";
  }
  workspace_size_list_.push_back(inputs[kIndex0]->size());
  return KRET_OK;
}

template <typename T>
bool SequenceAddNGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &workspace,
                                            const std::vector<KernelTensor *> &outputs) {
  T *output_addr = GetDeviceAddress<T>(outputs, 0);
  auto work_addr = output_addr;
  auto input_0 = GetDeviceAddress<T>(inputs, 0);
  if (output_addr == GetDeviceAddress<T>(inputs, 0)) {
    work_addr = GetDeviceAddress<T>(workspace, 0);
  }
  cudaError_t status = cudaErrorNotReady;
  size_t element_num = outputs[0]->size() / sizeof(T);
  status =
    FillDeviceArray(outputs[0]->size() / sizeof(T), output_addr, 0.0f, reinterpret_cast<cudaStream_t>(stream_ptr_));
  CHECK_CUDA_STATUS(status, kernel_name_);
  status =
    FillDeviceArray(outputs[0]->size() / sizeof(T), work_addr, 0.0f, reinterpret_cast<cudaStream_t>(stream_ptr_));
  CHECK_CUDA_STATUS(status, kernel_name_);
  std::vector<int64_t> ele_shape = {static_cast<int64_t>(element_num)};
  for (int64_t i = 0; i < tuple_shape_[0]; i++) {
    T *input_addr = element_num * i + input_0;
    status = BinaryOpWithBroadcastCudaFunc<BinaryOpType::kAdd, T, T, T>(false, ele_shape, ele_shape, ele_shape,
                                                                        input_addr, work_addr, work_addr, device_id_,
                                                                        reinterpret_cast<cudaStream_t>(stream_ptr_));
    CHECK_CUDA_STATUS(status, kernel_name_);
  }

  if (work_addr != output_addr) {
    CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(
      cudaMemcpyAsync(output_addr, work_addr, outputs[0]->size(), cudaMemcpyDeviceToDevice,
                      reinterpret_cast<cudaStream_t>(stream_ptr_)),
      "Addn cudaMemcpyAsync outputs failed");
  }

  return true;
}

#define SEQUENCE_ADDN_KERNEL_REG(ms_type, builtin_type)                                            \
  {KernelAttr().AddInputAttr(kObjectTypeTuple, ms_type).AddOutputAttr(kObjectTypeNumber, ms_type), \
   &SequenceAddNGpuKernelMod::LaunchKernel<builtin_type>},                                         \
  {                                                                                                \
    KernelAttr().AddInputAttr(kObjectTypeTuple, ms_type).AddOutputAttr(ms_type),                   \
      &SequenceAddNGpuKernelMod::LaunchKernel<builtin_type>                                        \
  }

const SequenceAddNGpuKernelMod::FuncList &SequenceAddNGpuKernelMod::GetFuncList() const {
  static const FuncList func_list = {SEQUENCE_ADDN_KERNEL_REG(kNumberTypeInt8, int8_t),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeInt16, int16_t),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeInt32, int32_t),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeInt64, int64_t),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeUInt8, uint8_t),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeUInt16, uint16_t),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeUInt32, uint32_t),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeUInt64, uint64_t),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeFloat16, half),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeFloat32, float),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeFloat64, double),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeComplex64, Complex<float>),
                                     SEQUENCE_ADDN_KERNEL_REG(kNumberTypeComplex128, Complex<double>)};
  return func_list;
}
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, SequenceAddN, SequenceAddNGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
