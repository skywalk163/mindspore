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

#include "plugin/device/gpu/kernel/quant/minmax_update_perlayer_gpu_kernel.h"
#include <thrust/extrema.h>
#include <thrust/pair.h>
#include <thrust/device_vector.h>
#include <cuda_runtime_api.h>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/minmax_update_impl.cuh"
#include "plugin/device/gpu/kernel/quant/quant_op_const.h"

namespace mindspore {
namespace kernel {
MinMaxUpdatePerLayerGpuKernelMod::MinMaxUpdatePerLayerGpuKernelMod()
    : input_size_(0), quant_num_(1), ema_(false), is_null_input_(false), ema_decay_(0) {}

bool MinMaxUpdatePerLayerGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &outputs) {
  ema_ = GetValue<bool>(primitive_->GetAttr("ema"));
  ema_decay_ = GetValue<float>(primitive_->GetAttr("ema_decay"));
  return true;
}

int MinMaxUpdatePerLayerGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                             const std::vector<KernelTensor *> &outputs) {
  output_size_list_.clear();
  workspace_size_list_.clear();
  // init size
  auto input_shape = inputs[kIndex0]->GetShapeVector();
  is_null_input_ = CHECK_SHAPE_NULL(input_shape, kernel_name_, "input");
  if (is_null_input_) {
    output_size_list_.push_back(sizeof(float));  // output min
    output_size_list_.push_back(sizeof(float));  // output max
    return KRET_UNKNOWN_SHAPE;
  }
  auto size = SizeOf(input_shape);
  quant_num_ = SizeToInt(size);
  input_size_ = sizeof(float) * size;
  output_size_list_.push_back(sizeof(float));  // output min
  output_size_list_.push_back(sizeof(float));  // output max
  return KRET_OK;
}

bool MinMaxUpdatePerLayerGpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                              const std::vector<KernelTensor *> &,
                                              const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  if (is_null_input_) {
    return true;
  }
  float *output_min = GetDeviceAddress<float>(outputs, kIndex0);
  float *output_max = GetDeviceAddress<float>(outputs, kIndex1);
  float *input = GetDeviceAddress<float>(inputs, kIndex0);
  float *input_min = GetDeviceAddress<float>(inputs, kIndex1);
  float *input_max = GetDeviceAddress<float>(inputs, kIndex2);

  auto status = CalMinMaxPerLayer(input, input_min, input_max, output_min, output_max, quant_num_, ema_decay_, ema_,
                                  reinterpret_cast<cudaStream_t>(stream_ptr));
  CHECK_CUDA_STATUS(status, kernel_name_);
  return true;
}

MS_REG_GPU_KERNEL(MinMaxUpdatePerLayer, MinMaxUpdatePerLayerGpuKernelMod)
}  // namespace kernel
}  // namespace mindspore
