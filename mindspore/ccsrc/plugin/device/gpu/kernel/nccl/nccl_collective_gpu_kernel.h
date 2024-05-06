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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NCCL_COLLECTIVE_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NCCL_COLLECTIVE_GPU_KERNEL_H_

#include <dlfcn.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <map>
#include "plugin/device/gpu/kernel/nccl/nccl_gpu_kernel.h"

namespace mindspore {
namespace kernel {
enum NcclKernelType {
  NCCL_ALL_REDUCE = 0,
  NCCL_ALL_GATHER,
  NCCL_REDUCE_SCATTER,
  NCCL_BROADCAST,
  NCCL_INVALID_TYPE = 255
};
const std::map<std::string, NcclKernelType> kNcclTypeMap = {{"AllReduce", NCCL_ALL_REDUCE},
                                                            {"AllGather", NCCL_ALL_GATHER},
                                                            {"ReduceScatter", NCCL_REDUCE_SCATTER},
                                                            {"Broadcast", NCCL_BROADCAST}};

template <typename T>
class NcclCollectiveGpuKernel : public NcclGpuKernelMod {
 public:
  NcclCollectiveGpuKernel() { ResetResource(); }
  ~NcclCollectiveGpuKernel() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    if (is_null_input_) {
      return true;
    }
    switch (nccl_kernel_type_) {
      case NCCL_ALL_REDUCE: {
        LaunchAllReduce(inputs, outputs, stream_ptr);
        break;
      }
      case NCCL_ALL_GATHER: {
        LaunchAllGather(inputs, outputs, stream_ptr);
        break;
      }
      case NCCL_REDUCE_SCATTER: {
        LaunchReduceScatter(inputs, outputs, stream_ptr);
        break;
      }
      case NCCL_BROADCAST: {
        LaunchBroadcast(inputs, outputs, stream_ptr);
        break;
      }
      default: {
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << ", only support these types: AllReduce, AllGather, Broadcast, "
                          << "ReduceScatter currently, but got " << nccl_kernel_type_;
      }
    }
    return true;
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override {
    nccl_data_type_ = nccl_dtype(inputs[0]->dtype_id());
    InferCommType(kernel_name_, primitive_);

    SelectCollectiveHandle();
    return true;
  }

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override {
    size_t input_num = inputs.size();
    size_t output_num = outputs.size();
    output_size_list_.clear();
    for (size_t i = 0; i < input_num; ++i) {
      auto shape = inputs[i]->GetDeviceShapeVector();
      is_null_input_ = CHECK_SHAPE_NULL(shape, kernel_name_, "input");
      if (is_null_input_) {
        return true;
      }
      size_t size = sizeof(T);
      for (size_t j = 0; j < shape.size(); j++) {
        size *= LongToSizeClipNeg(shape[j]);
      }
      // Framework memory allocation ensures memory alignment, but AllGather/ReduceScatter calculation cann‘t have
      // aligned gaps in single input scenarios.
      if (input_num > 1) {
        size = device::gpu::GPUMemoryAllocator::GetInstance().AlignMemorySize(size);
      }
      input_size_ += size;
    }
    for (size_t i = 0; i < output_num; ++i) {
      auto shape = outputs[i]->GetDeviceShapeVector();
      is_null_input_ = CHECK_SHAPE_NULL(shape, kernel_name_, "output");
      if (is_null_input_) {
        return true;
      }
      size_t size = sizeof(T);
      for (size_t j = 0; j < shape.size(); j++) {
        size *= LongToSizeClipNeg(shape[j]);
      }
      output_size_list_.push_back(size);
      // Framework memory allocation ensures memory alignment, but AllGather/ReduceScatter calculation cann‘t have
      // aligned gaps in single output scenarios.
      if (output_num > 1) {
        size = device::gpu::GPUMemoryAllocator::GetInstance().AlignMemorySize(size);
      }
      output_size_ += size;
    }

    group_name_ = GetValue<std::string>(primitive_->GetAttr(kAttrGroup));
    MS_LOG(INFO) << kernel_name_ << " for group " << group_name_;
    return KRET_OK;
  }

  void ResetResource() noexcept {
    nccl_kernel_type_ = NCCL_INVALID_TYPE;
    nccl_reduce_type_ = ncclSum;
    input_size_ = 0;
    output_size_ = 0;
    root_ = 0;
    is_null_input_ = false;
    collective_handle_ = nullptr;
    output_size_list_.clear();
    workspace_size_list_.clear();
  }

 private:
  void LaunchAllReduce(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs,
                       void *stream_ptr) {
    T *input_addr = GetDeviceAddress<T>(inputs, 0);
    T *output_addr = GetDeviceAddress<T>(outputs, 0);
    (void)AllReduce(input_addr, output_addr, output_size_ / sizeof(T), nccl_data_type_, nccl_reduce_type_,
                    reinterpret_cast<cudaStream_t>(stream_ptr), group_name_);
  }

  void LaunchAllGather(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs,
                       void *stream_ptr) {
    T *input_addr = GetDeviceAddress<T>(inputs, 0);
    T *output_addr = GetDeviceAddress<T>(outputs, 0);
    (void)AllGather(input_addr, output_addr, input_size_ / sizeof(T), nccl_data_type_,
                    reinterpret_cast<cudaStream_t>(stream_ptr), group_name_);
  }

  void LaunchReduceScatter(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs,
                           void *stream_ptr) {
    T *input_addr = GetDeviceAddress<T>(inputs, 0);
    T *output_addr = GetDeviceAddress<T>(outputs, 0);
    (void)ReduceScatter(input_addr, output_addr, output_size_ / sizeof(T), nccl_data_type_, nccl_reduce_type_,
                        reinterpret_cast<cudaStream_t>(stream_ptr), group_name_);
  }

  void LaunchBroadcast(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs,
                       void *stream_ptr) {
    T *input_addr = nullptr;
    T *output_addr = nullptr;
    for (int i = 0; i < SizeToInt(inputs.size()); ++i) {
      input_addr = GetDeviceAddress<T>(inputs, i);
      output_addr = GetDeviceAddress<T>(outputs, i);
      (void)Broadcast(input_addr, output_addr, output_size_list_[i] / sizeof(T), nccl_data_type_, root_,
                      reinterpret_cast<cudaStream_t>(stream_ptr), group_name_);
    }
  }

  void InferCommType(const std::string &kernel_name, const PrimitivePtr &prim) {
    auto iter = kNcclTypeMap.find(kernel_name);
    if (iter == kNcclTypeMap.end()) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << ", only support these types: AllReduce, AllGather, Broadcast, "
                        << "ReduceScatter currently, but got " << kernel_name;
    } else {
      nccl_kernel_type_ = iter->second;
    }

    MS_EXCEPTION_IF_NULL(prim);
    auto reduce_op = prim->GetAttr(kAttrOp);
    if (reduce_op) {
      std::string type = GetValue<std::string>(reduce_op);
      if (type == "sum") {
        nccl_reduce_type_ = ncclSum;
      } else if (type == "max") {
        nccl_reduce_type_ = ncclMax;
      } else if (type == "min") {
        nccl_reduce_type_ = ncclMin;
      } else if (type == "prod") {
        nccl_reduce_type_ = ncclProd;
      } else {
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << ", only support these types: sum, max, min, prod currently, "
                          << "but got " << type;
      }
    }

    // The boolean is not supported by NCCL. Convert boolean + reducesum to uint8_t + reducemax will get same result.
    if ((nccl_reduce_type_ == ncclSum && std::is_same<T, bool>::value)) {
      nccl_reduce_type_ = ncclMax;
      nccl_data_type_ = ncclUint8;
    }

    auto root_rank = prim->GetAttr(kAttrRootRank);
    if (root_rank) {
      root_ = static_cast<int>(GetValue<int64_t>(root_rank));
    }
    return;
  }

  NcclKernelType nccl_kernel_type_;
  ncclRedOp_t nccl_reduce_type_;
  size_t input_size_;
  size_t output_size_;
  int root_;
  bool is_null_input_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NCCL_COLLECTIVE_GPU_KERNEL_H_
