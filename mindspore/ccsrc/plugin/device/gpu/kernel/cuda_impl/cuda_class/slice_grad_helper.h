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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_CLASS_SLICE_GRAD_HELPER_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_CLASS_SLICE_GRAD_HELPER_H_
#include <memory>
#include <string>
#include <vector>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_class/helper_base.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/slice_impl.cuh"

namespace mindspore {
namespace cukernel {
constexpr size_t kSliceGradDefaultInputShapeSize = 4;
constexpr size_t kSliceGradMaxInputShapeSize = 7;
constexpr size_t kDim2 = 2;
constexpr size_t kDim3 = 3;
constexpr size_t kDim4 = 4;
constexpr size_t kDim5 = 5;
constexpr size_t kDim6 = 6;
constexpr size_t kDim7 = 7;
class SliceGradAttr : public GpuKernelAttrBase {
 public:
  SliceGradAttr() = default;
  ~SliceGradAttr() override = default;
  std::vector<int64_t> begin;
  std::vector<int64_t> size;
  std::vector<int64_t> input_shape;
  int64_t output_num;
};

template <typename T, typename S>
class SliceGradHelperGpuKernel : public GpuKernelHelperBase {
 public:
  explicit SliceGradHelperGpuKernel(const std::string &kernel_name, const uint32_t &device_id)
      : GpuKernelHelperBase(kernel_name, device_id) {}

  virtual ~SliceGradHelperGpuKernel() = default;
  int CalMemSize(const std::vector<std::vector<int64_t>> &input_shapes,
                 const std::vector<std::vector<int64_t>> &output_shapes) {
    ResetResource();
    input_size_ = sizeof(T);
    for (auto shape : attr_ptr_->input_shape) {
      input_size_ = input_size_ * static_cast<size_t>(shape);
    }

    output_size_list_.push_back(input_size_);
    return 0;
  }

  int Process(const std::vector<void *> &input_ptrs, const std::vector<void *> &output_ptrs,
              const std::vector<void *> &work_ptrs, void *stream_ptr) override {
    T *dy = nullptr;
    T *dx = nullptr;
    int flag = GetDeviceAddress<T>(input_ptrs, 0, kernel_name_, &dy);
    if (flag != 0) {
      return flag;
    }

    flag = GetDeviceAddress<T>(output_ptrs, 0, kernel_name_, &dx);
    if (flag != 0) {
      return flag;
    }

    auto status = FillDeviceArray(input_size_ / sizeof(T), dx, 0.f, reinterpret_cast<cudaStream_t>(stream_ptr));
    CHECK_CUDA_STATUS(status, kernel_name_);
    auto &input_shape = attr_ptr_->input_shape;
    auto &begin = attr_ptr_->begin;
    auto &size = attr_ptr_->size;
    if (input_shape.size() <= kSliceGradDefaultInputShapeSize) {
      status = CalSlice4DGrad(begin[0], begin[1], begin[kDim2], begin[kDim3], size[0], size[1], size[kDim2],
                              size[kDim3], input_shape[0], input_shape[1], input_shape[kDim2], input_shape[kDim3], dy,
                              dx, reinterpret_cast<cudaStream_t>(stream_ptr));
      CHECK_CUDA_STATUS(status, kernel_name_);
    } else {
      status =
        CalSlice7DGrad(begin[0], begin[1], begin[kDim2], begin[kDim3], begin[kDim4], begin[kDim5], begin[kDim6],
                       size[0], size[1], size[kDim2], size[kDim3], size[kDim4], size[kDim5], size[kDim6],
                       input_shape[0], input_shape[1], input_shape[kDim2], input_shape[kDim3], input_shape[kDim4],
                       input_shape[kDim5], input_shape[kDim6], dy, dx, reinterpret_cast<cudaStream_t>(stream_ptr));
      CHECK_CUDA_STATUS(status, kernel_name_);
    }
    return 0;
  }

  void SetKernelParam(const GpuKernelAttrBasePtr &kernel_attr) override {
    attr_ptr_ = std::dynamic_pointer_cast<SliceGradAttr>(kernel_attr);
  }

 private:
  size_t input_size_;
  std::shared_ptr<SliceGradAttr> attr_ptr_{nullptr};
};
}  // namespace cukernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_CLASS_SLICE_GRAD_HELPER_H_
