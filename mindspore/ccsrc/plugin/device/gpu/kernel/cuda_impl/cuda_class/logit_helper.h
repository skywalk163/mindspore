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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_CLASS_LOGIT_HELPER_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_CLASS_LOGIT_HELPER_H_

#include <memory>
#include <string>
#include <vector>
#include "plugin/device/gpu/hal/device/gpu_common.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_class/helper_base.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/logit_impl.cuh"

namespace mindspore {
namespace cukernel {
class LogitAttr : public GpuKernelAttrBase {
 public:
  LogitAttr() = default;
  ~LogitAttr() override = default;
  float eps;
};

template <typename T, typename S>
class LogitHelperGpuKernel : public GpuKernelHelperBase {
 public:
  explicit LogitHelperGpuKernel(const std::string &kernel_name, const uint32_t &device_id)
      : GpuKernelHelperBase(kernel_name, device_id) {
    eps_ = -1.0;
    is_null_input_ = false;
  }
  virtual ~LogitHelperGpuKernel() = default;

  int CalMemSize(const std::vector<std::vector<int64_t>> &input_shapes,
                 const std::vector<std::vector<int64_t>> &output_shapes) override {
    constexpr size_t INPUT_NUM = 1;
    constexpr size_t OUTPUT_NUM = 1;
    ResetResource();
    int inp_flag = CalShapesSizeInBytes<T>(input_shapes, INPUT_NUM, kernel_name_, "input_shapes", &input_size_list_);
    if (inp_flag == -1) {
      return inp_flag;
    }
    int out_flag =
      CalShapesSizeInBytes<T>(output_shapes, OUTPUT_NUM, kernel_name_, "output_shapes", &output_size_list_);
    if (out_flag != 0) {
      return out_flag;
    }
    is_null_input_ = (inp_flag == 1 || out_flag == 1);
    eps_ = attr_ptr_->eps;
    return 0;
  }

  int Process(const std::vector<void *> &input_ptrs, const std::vector<void *> &output_ptrs,
              const std::vector<void *> &work_ptrs, void *cuda_stream) override {
    if (is_null_input_) {
      return 0;
    }

    T *input_ptr = nullptr;
    T *output_ptr = nullptr;
    int flag = GetDeviceAddress<T>(input_ptrs, 0, kernel_name_, &input_ptr);
    if (flag != 0) {
      return flag;
    }

    flag = GetDeviceAddress<T>(output_ptrs, 0, kernel_name_, &output_ptr);
    if (flag != 0) {
      return flag;
    }

    // call cuda kernel
    auto status = CalLogit(input_ptr, static_cast<T>(static_cast<T>(1.0) - static_cast<T>(eps_)), eps_, output_ptr,
                           input_size_list_[0] / sizeof(T), device_id_, reinterpret_cast<cudaStream_t>(cuda_stream));
    CHECK_CUDA_STATUS(status, kernel_name_);
    return 0;
  }

  void SetKernelParam(const GpuKernelAttrBasePtr &kernel_attr) override {
    attr_ptr_ = std::dynamic_pointer_cast<LogitAttr>(kernel_attr);
  }

 private:
  std::shared_ptr<LogitAttr> attr_ptr_;
  float eps_;
  T up_bound_;
  bool is_null_input_;
};
}  // namespace cukernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_CLASS_LOGIT_HELPER_H_
