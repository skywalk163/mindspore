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

#include <algorithm>
#include <utility>
#include <complex>
#include <functional>
#include "plugin/device/gpu/kernel/sequence/sequence_concat_gpu_kernel.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/concat_impl.cuh"

namespace mindspore {
namespace kernel {
namespace {
constexpr int kInputsNum = 2;
constexpr int kOutputsNum = 1;
}  // namespace

bool SequenceConcatGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kOutputsNum, kernel_name_);
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int SequenceConcatGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  auto user_data = inputs[0]->user_data();
  if (user_data != nullptr && user_data->has(kRealElementsSize)) {
    auto real_elem_sizes = user_data->get<std::vector<size_t>>(kRealElementsSize);
    MS_EXCEPTION_IF_NULL(real_elem_sizes);
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << ", only support all same inner elements now, but got inner elements size: " << (*real_elem_sizes);
    return KRET_RESIZE_FAILED;
  }

  tuple_shape_ = inputs[0]->GetShapeVector();
  if (tuple_shape_.empty()) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << " the input tuple size must greater 0";
  }

  std::vector<int64_t> shape_vec_item;
  std::copy(tuple_shape_.begin() + 1, tuple_shape_.end(), std::back_inserter(shape_vec_item));

  input_num_ = tuple_shape_[0];
  axis_ = LongToInt(inputs[kIndex1]->GetValueWithCheck<int64_t>());
  inputs_shape_.clear();
  len_axis_.clear();
  for (int i = 0; i < input_num_; ++i) {
    inputs_shape_.push_back(shape_vec_item);
    len_axis_.push_back(LongToInt(shape_vec_item[axis_]));
  }
  int dims = SizeToInt(inputs_shape_[0].size());
  if (axis_ < -dims || axis_ >= dims) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the 'axis' must be in the range [-" << dims << "," << dims
                      << "), but got " << axis_;
  }
  if (axis_ < 0) {
    axis_ = axis_ + dims;
  }

  workspace_size_list_.push_back(sizeof(void *) * input_num_);
  workspace_size_list_.push_back(sizeof(int) * input_num_);
  inputs_host_.resize(input_num_);

  auto output_shape = outputs[0]->GetDeviceShapeVector();
  all_size_before_axis_ = 1;
  all_size_axis_ = 1;
  for (int i = 0; i < SizeToInt(output_shape.size()); i++) {
    if (i > axis_) {
      all_size_before_axis_ *= LongToInt(output_shape[i]);
      all_size_axis_ *= LongToInt(output_shape[i]);
    }
    if (i == axis_) {
      all_size_before_axis_ *= LongToInt(output_shape[i]);
    }
  }

  return KRET_OK;
}

template <typename T>
bool SequenceConcatGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                              const std::vector<KernelTensor *> &workspace,
                                              const std::vector<KernelTensor *> &outputs) {
  if (input_num_ == 0) {
    return true;
  }
  const auto input_addr = GetDeviceAddress<T>(inputs, 0);
  T *output = GetDeviceAddress<T>(outputs, 0);
  T **inputs_device = GetDeviceAddress<T *>(workspace, 0);
  int *len_axis_device = GetDeviceAddress<int>(workspace, 1);
  size_t element_num = outputs[0]->size() / sizeof(T) / input_num_;
  for (int i = 0; i < input_num_; i++) {
    T *tmp_addr = input_addr + i * element_num;
    inputs_host_[i] = tmp_addr;
  }
  CHECK_CUDA_RET_WITH_ERROR_NOTRACE(
    cudaMemcpyAsync(inputs_device, inputs_host_.data(), sizeof(T *) * input_num_, cudaMemcpyHostToDevice,
                    reinterpret_cast<cudaStream_t>(stream_ptr_)),
    "SequenceConcat opt cudaMemcpyAsync inputs failed");
  CHECK_CUDA_RET_WITH_ERROR_NOTRACE(
    cudaMemcpyAsync(len_axis_device, len_axis_.data(), sizeof(int) * input_num_, cudaMemcpyHostToDevice,
                    reinterpret_cast<cudaStream_t>(stream_ptr_)),
    "SequenceConcat opt cudaMemcpyAsync length on axis failed");
  output_size_ = output_size_list_[0] / sizeof(T);
  auto status = ConcatKernel(output_size_, input_num_, all_size_before_axis_, all_size_axis_, len_axis_device,
                             inputs_device, output, reinterpret_cast<cudaStream_t>(stream_ptr_));
  CHECK_CUDA_STATUS(status, kernel_name_);

  return true;
}

#define SEQUENCE_CONCAT_KERNEL_REG(ms_type, builtin_type)     \
  {                                                           \
    KernelAttr()                                              \
      .AddInputAttr(kObjectTypeTuple, ms_type)                \
      .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)      \
      .AddOutputAttr(ms_type),                                \
      &SequenceConcatGpuKernelMod::LaunchKernel<builtin_type> \
  }

const SequenceConcatGpuKernelMod::FuncList &SequenceConcatGpuKernelMod::GetFuncList() const {
  static const FuncList func_list = {SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeInt8, int8_t),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeInt16, int16_t),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeInt32, int32_t),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeInt64, int64_t),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeUInt8, uint8_t),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeUInt16, uint16_t),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeUInt32, uint32_t),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeUInt64, uint64_t),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeFloat16, half),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeFloat32, float),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeFloat64, double),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeComplex64, Complex<float>),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeComplex128, Complex<double>),
                                     SEQUENCE_CONCAT_KERNEL_REG(kNumberTypeBool, bool)};
  return func_list;
}
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, SequenceConcat, SequenceConcatGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
