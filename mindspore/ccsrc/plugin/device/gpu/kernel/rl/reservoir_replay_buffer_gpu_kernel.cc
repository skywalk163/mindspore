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

#include "plugin/device/gpu/kernel/rl/reservoir_replay_buffer_gpu_kernel.h"
#include <vector>
#include <random>
#include <algorithm>
#include <functional>
#include "mindspore/core/ops/reservoir_replay_buffer.h"

namespace mindspore {
namespace kernel {
using ReservoirReplayBufferFactory = ReplayBufferFactory<ReservoirReplayBuffer>;

ReservoirReplayBufferCreateGpuKernel::~ReservoirReplayBufferCreateGpuKernel() {
  auto &allocator = device::gpu::GPUMemoryAllocator::GetInstance();
  if (handle_device_) {
    allocator.FreeTensorMem(handle_device_);
  }
}

bool ReservoirReplayBufferCreateGpuKernel::Init(const std::vector<KernelTensor *> &inputs,
                                                const std::vector<KernelTensor *> &outputs) {
  const std::vector<int64_t> &schema = GetValue<std::vector<int64_t>>(primitive_->GetAttr("schema"));
  const int64_t &seed0 = GetValue<int64_t>(primitive_->GetAttr("seed0"));
  const int64_t &seed1 = GetValue<int64_t>(primitive_->GetAttr("seed1"));

  unsigned int seed = 0;
  std::random_device rd;
  if (seed1 != 0) {
    seed = static_cast<unsigned int>(seed1);
  } else if (seed0 != 0) {
    seed = static_cast<unsigned int>(seed0);
  } else {
    seed = rd();
  }

  std::vector<size_t> schema_in_size;
  std::transform(schema.begin(), schema.end(), std::back_inserter(schema_in_size),
                 [](const int64_t &arg) -> size_t { return LongToSize(arg); });

  const int64_t &capacity = GetValue<int64_t>(primitive_->GetAttr("capacity"));
  auto &factory = ReservoirReplayBufferFactory::GetInstance();
  std::tie(handle_, reservoir_replay_buffer_) = factory.Create(seed, capacity, schema_in_size);
  MS_EXCEPTION_IF_NULL(reservoir_replay_buffer_);

  auto &allocator = device::gpu::GPUMemoryAllocator::GetInstance();
  handle_device_ = static_cast<int64_t *>(allocator.AllocTensorMem(sizeof(handle_)));
  CHECK_CUDA_RET_WITH_ERROR_NOTRACE(cudaMemcpy(handle_device_, &handle_, sizeof(handle_), cudaMemcpyHostToDevice),
                                    "cudaMemcpy failed.");

  output_size_list_.push_back(sizeof(handle_));
  return true;
}

std::vector<KernelAttr> ReservoirReplayBufferCreateGpuKernel::GetOpSupport() {
  std::vector<KernelAttr> support_list = {KernelAttr().AddOutputAttr(kNumberTypeInt64)};
  return support_list;
}

bool ReservoirReplayBufferCreateGpuKernel::Launch(const std::vector<KernelTensor *> &,
                                                  const std::vector<KernelTensor *> &,
                                                  const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  auto handle = GetDeviceAddress<int64_t>(outputs, 0);
  auto stream = reinterpret_cast<cudaStream_t>(stream_ptr);
  CHECK_CUDA_RET_WITH_ERROR_NOTRACE(
    cudaMemcpyAsync(handle, handle_device_, sizeof(handle_), cudaMemcpyDeviceToDevice, stream), "cudaMemcpy failed.");
  return true;
}

ReservoirReplayBufferPushGpuKernel::~ReservoirReplayBufferPushGpuKernel() {
  auto &allocator = device::gpu::GPUMemoryAllocator::GetInstance();
  if (handle_device_) {
    allocator.FreeTensorMem(handle_device_);
  }
}

bool ReservoirReplayBufferPushGpuKernel::Init(const std::vector<KernelTensor *> &inputs,
                                              const std::vector<KernelTensor *> &outputs) {
  handle_ = GetValue<int64_t>(primitive_->GetAttr("handle"));
  reservior_replay_buffer_ = ReservoirReplayBufferFactory::GetInstance().GetByHandle(handle_);
  MS_EXCEPTION_IF_NULL(reservior_replay_buffer_);

  auto &allocator = device::gpu::GPUMemoryAllocator::GetInstance();
  handle_device_ = static_cast<int64_t *>(allocator.AllocTensorMem(sizeof(handle_)));
  CHECK_CUDA_RET_WITH_ERROR_NOTRACE(cudaMemcpy(handle_device_, &handle_, sizeof(handle_), cudaMemcpyHostToDevice),
                                    "cudaMemcpy failed.");

  output_size_list_.push_back(sizeof(handle_));
  return true;
}

bool ReservoirReplayBufferPushGpuKernel::Launch(const std::vector<KernelTensor *> &inputs,
                                                const std::vector<KernelTensor *> &,
                                                const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  auto stream = reinterpret_cast<cudaStream_t>(stream_ptr);
  // Return a placeholder in case of dead code eliminate optimization.
  auto handle = GetDeviceAddress<int64_t>(outputs, 0);
  CHECK_CUDA_RET_WITH_ERROR_NOTRACE(
    cudaMemcpyAsync(handle, handle_device_, sizeof(handle_), cudaMemcpyDeviceToDevice, stream), "cudaMemcpy failed.");
  return reservior_replay_buffer_->Push(inputs, stream);
}

std::vector<KernelAttr> ReservoirReplayBufferPushGpuKernel::GetOpSupport() {
  std::vector<KernelAttr> support_list = {KernelAttr().AddSkipCheckAttr(true)};
  return support_list;
}

bool ReservoirReplayBufferSampleGpuKernel::Init(const std::vector<KernelTensor *> &inputs,
                                                const std::vector<KernelTensor *> &outputs) {
  handle_ = GetValue<int64_t>(primitive_->GetAttr("handle"));
  batch_size_ = GetValue<int64_t>(primitive_->GetAttr("batch_size"));
  reservior_replay_buffer_ = ReservoirReplayBufferFactory::GetInstance().GetByHandle(handle_);
  MS_EXCEPTION_IF_NULL(reservior_replay_buffer_);

  for (size_t i = 0; i < outputs.size(); i++) {
    TypeId type_id = outputs[i]->dtype_id();
    size_t type_size = GetTypeByte(TypeIdToType(type_id));
    const std::vector<int64_t> &shape = outputs[i]->GetShapeVector();
    size_t tensor_size = std::accumulate(shape.begin(), shape.end(), type_size, std::multiplies<size_t>());
    output_size_list_.push_back(tensor_size);
  }

  return true;
}

bool ReservoirReplayBufferSampleGpuKernel::Launch(const std::vector<KernelTensor *> &,
                                                  const std::vector<KernelTensor *> &,
                                                  const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  return reservior_replay_buffer_->Sample(batch_size_, outputs, reinterpret_cast<cudaStream_t>(stream_ptr));
}

std::vector<KernelAttr> ReservoirReplayBufferSampleGpuKernel::GetOpSupport() {
  std::vector<KernelAttr> support_list = {KernelAttr().AddSkipCheckAttr(true)};
  return support_list;
}

bool ReservoirReplayBufferDestroyGpuKernel::Init(const std::vector<KernelTensor *> &inputs,
                                                 const std::vector<KernelTensor *> &outputs) {
  handle_ = GetValue<int64_t>(primitive_->GetAttr("handle"));
  output_size_list_.push_back(sizeof(handle_));
  return true;
}

bool ReservoirReplayBufferDestroyGpuKernel::Launch(const std::vector<KernelTensor *> &inputs,
                                                   const std::vector<KernelTensor *> &,
                                                   const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  ReservoirReplayBufferFactory::GetInstance().Delete(handle_);

  // Apply host to device memory copy since it is not performance critical path.
  auto handle = GetDeviceAddress<float>(outputs, 0);
  CHECK_CUDA_RET_WITH_ERROR_NOTRACE(cudaMemcpyAsync(handle, &handle_, sizeof(handle_), cudaMemcpyHostToDevice,
                                                    reinterpret_cast<cudaStream_t>(stream_ptr)),
                                    "cudaMemcpy failed.");
  return true;
}

std::vector<KernelAttr> ReservoirReplayBufferDestroyGpuKernel::GetOpSupport() {
  std::vector<KernelAttr> support_list = {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64)};
  return support_list;
}
}  // namespace kernel
}  // namespace mindspore
