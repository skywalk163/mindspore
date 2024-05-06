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

#include "plugin/device/gpu/kernel/quant/fake_quant_perchannel_gpu_kernel.h"
#include <thrust/extrema.h>
#include <thrust/pair.h>
#include <thrust/device_vector.h>
#include <cuda_runtime_api.h>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/fake_quant_perchannel_impl.cuh"
#include "plugin/device/gpu/kernel/quant/quant_op_const.h"

namespace mindspore {
namespace kernel {
FakeQuantPerChannelGpuKernelMod::FakeQuantPerChannelGpuKernelMod()
    : input_size_(0),
      num_channels_(0),
      num_bits_(0),
      training_(false),
      symmetric_(false),
      narrow_range_(false),
      is_null_input_(false),
      quant_delay_(0),
      quant_min_(0),
      quant_max_(0),
      global_step_(0) {}

bool FakeQuantPerChannelGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                           const std::vector<KernelTensor *> &outputs) {
  // get attribute
  num_bits_ = static_cast<unsigned int>(GetValue<int64_t>(primitive_->GetAttr("num_bits")));
  training_ = GetValue<bool>(primitive_->GetAttr("training"));
  symmetric_ = GetValue<bool>(primitive_->GetAttr("symmetric"));
  narrow_range_ = GetValue<bool>(primitive_->GetAttr("narrow_range"));
  quant_delay_ = static_cast<int>(GetValue<int64_t>(primitive_->GetAttr("quant_delay")));

  if (num_bits_ <= kMinQuantBit || num_bits_ >= kMaxQuantBit) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the value of num_bits should be in (2, 16), but got "
                      << num_bits_;
  }

  if (quant_delay_ < 0) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the value of quant_delay_ cannot be less than 0, but got "
                      << quant_delay_;
  }

  // quant min and max value
  quant_min_ = 0;
  quant_max_ = (1 << num_bits_) - 1;
  if (narrow_range_) {
    quant_min_++;
  }
  return true;
}

void FakeQuantPerChannelGpuKernelMod::SetSizeLists() {
  output_size_list_.push_back(input_size_);                       // output in tensor
  workspace_size_list_.push_back(sizeof(float) * num_channels_);  // scale in channel
  workspace_size_list_.push_back(sizeof(float) * num_channels_);  // min in channel
  workspace_size_list_.push_back(sizeof(float) * num_channels_);  // max in channel
}

int FakeQuantPerChannelGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &outputs) {
  output_size_list_.clear();
  workspace_size_list_.clear();
  // shape info for gpu
  auto input_shape = inputs[kIndex0]->GetShapeVector();
  is_null_input_ = CHECK_SHAPE_NULL(input_shape, kernel_name_, "input");
  if (is_null_input_) {
    SetSizeLists();
    return KRET_UNKNOWN_SHAPE;
  }
  if (input_shape.empty()) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', input cannot be empty, but got empty";
  }
  num_channels_ = LongToInt(input_shape[0]);
  input_size_ = sizeof(float) * SizeOf(input_shape);
  SetSizeLists();
  return KRET_OK;
}

void FakeQuantPerChannelGpuKernelMod::CalFakeQuantize(const float *input, float *output, float *input_min,
                                                      float *input_max, float *nudge_min, float *nudge_max,
                                                      float *scale, void *stream_ptr) {
  auto status = CalNudgePerChannel(input_min, input_max, quant_min_, quant_max_, nudge_min, nudge_max, scale,
                                   num_channels_, symmetric_, reinterpret_cast<cudaStream_t>(stream_ptr));
  CHECK_CUDA_STATUS(status, "CalNudgePerChannel called by " + kernel_name_);
  status = CalFakeQuantPerChannel(input, output, input_size_ / sizeof(float), num_channels_, nudge_min, nudge_max,
                                  scale, reinterpret_cast<cudaStream_t>(stream_ptr));
  CHECK_CUDA_STATUS(status, kernel_name_);
}

bool FakeQuantPerChannelGpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                             const std::vector<KernelTensor *> &workspace,
                                             const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  if (is_null_input_) {
    return true;
  }
  (void)workspace;
  float *output = GetDeviceAddress<float>(outputs, kIndex0);
  float *input = GetDeviceAddress<float>(inputs, kIndex0);
  float *input_min = GetDeviceAddress<float>(inputs, kIndex1);
  float *input_max = GetDeviceAddress<float>(inputs, kIndex2);
  float *scale = GetDeviceAddress<float>(workspace, kIndex0);
  float *nudge_min = GetDeviceAddress<float>(workspace, kIndex1);
  float *nudge_max = GetDeviceAddress<float>(workspace, kIndex2);

  if (training_) {
    if (global_step_ >= quant_delay_) {
      CalFakeQuantize(input, output, input_min, input_max, nudge_min, nudge_max, scale, stream_ptr);
    } else {
      CHECK_CUDA_RET_WITH_ERROR_NOTRACE(cudaMemcpyAsync(output, input, input_size_, cudaMemcpyDeviceToDevice,
                                                        reinterpret_cast<cudaStream_t>(stream_ptr)),
                                        "Copy gpu memory failed.");
    }
    global_step_++;
  } else {
    CalFakeQuantize(input, output, input_min, input_max, nudge_min, nudge_max, scale, stream_ptr);
  }

  return true;
}

MS_REG_GPU_KERNEL(FakeQuantPerChannel, FakeQuantPerChannelGpuKernelMod)
}  // namespace kernel
}  // namespace mindspore
