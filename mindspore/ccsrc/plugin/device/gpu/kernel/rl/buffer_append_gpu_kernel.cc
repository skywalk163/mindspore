/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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
#include "plugin/device/gpu/kernel/rl/buffer_append_gpu_kernel.h"

#include <memory>
#include <algorithm>

#include "kernel/common_utils.h"
#include "plugin/device/gpu/kernel/cuda_impl/rl/rl_buffer_impl.cuh"
#include "plugin/device/gpu/hal/device/gpu_common.h"

namespace mindspore {
namespace kernel {
constexpr size_t kDouble = 2;

BufferAppendKernelMod::BufferAppendKernelMod() : element_nums_(0), exp_batch_(0), capacity_(0) {}

BufferAppendKernelMod::~BufferAppendKernelMod() {}

bool BufferAppendKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  return true;
}

int BufferAppendKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &outputs) {
  for (const auto &input : inputs) {
    auto input_shape = input->GetShapeVector();
    if (!IsValidShape(input_shape)) {
      return KRET_UNKNOWN_SHAPE;
    }
  }
  workspace_size_list_.clear();
  output_size_list_.clear();
  auto shapes = GetValue<std::vector<int64_t>>(primitive_->GetAttr("buffer_elements"));
  auto types = GetValue<std::vector<TypePtr>>(primitive_->GetAttr("buffer_dtype"));
  capacity_ = GetValue<int64_t>(primitive_->GetAttr("capacity"));
  exp_batch_ = GetValue<int64_t>(primitive_->GetAttr("exp_batch"));
  element_nums_ = shapes.size();
  for (size_t i = 0; i < element_nums_; i++) {
    exp_element_list.push_back(shapes[i] * UnitSizeInBytes(types[i]->type_id()));
  }
  output_size_list_.push_back(0);
  workspace_size_list_.push_back(sizeof(int));
  return KRET_OK;
}

bool BufferAppendKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &workspace, const std::vector<KernelTensor *> &,
                                   void *stream) {
  int *count_addr = GetDeviceAddress<int>(inputs, kDouble * element_nums_);
  int *head_addr = GetDeviceAddress<int>(inputs, kDouble * element_nums_ + 1);
  int *index_addr = GetDeviceAddress<int>(workspace, 0);
  auto cuda_stream = reinterpret_cast<cudaStream_t>(stream);
  auto status = IncreaseCount(capacity_, LongToInt(exp_batch_), count_addr, head_addr, index_addr, cuda_stream);
  CHECK_CUDA_STATUS(status, kernel_name_);
  for (size_t i = 0; i < element_nums_; i++) {
    auto buffer_addr = GetDeviceAddress<unsigned char>(inputs, i);
    auto exp_addr = GetDeviceAddress<unsigned char>(inputs, i + element_nums_);
    size_t one_exp_len = inputs[i + element_nums_]->size();
    status =
      BufferAppend(capacity_, one_exp_len, index_addr, LongToInt(exp_batch_), buffer_addr, exp_addr, cuda_stream);
    CHECK_CUDA_STATUS(status, kernel_name_);
  }
  return true;
}
std::vector<KernelAttr> BufferAppendKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> support_list = {KernelAttr().AddSkipCheckAttr(true)};
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, BufferAppend, BufferAppendKernelMod);
}  // namespace kernel
}  // namespace mindspore
