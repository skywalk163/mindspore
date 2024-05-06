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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_CORRECTIONMULGRAD_GPUKERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_CORRECTIONMULGRAD_GPUKERNEL_H_

#include <vector>
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/correction_mul_impl.cuh"
#include "plugin/device/gpu/kernel/quant/quant_op_const.h"

namespace mindspore {
namespace kernel {
template <typename T>
class CorrectionMulGradGpuKernelMod : public NativeGpuKernelMod {
 public:
  CorrectionMulGradGpuKernelMod() : is_null_input_(false), batch_size_(0), channel_(0), height_(0), width_(0) {}
  ~CorrectionMulGradGpuKernelMod() override { DestroyResource(); }

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    if (is_null_input_) {
      return true;
    }
    auto *d_out = GetDeviceAddress<T>(inputs, kIndex0);
    auto *weight = GetDeviceAddress<T>(inputs, kIndex1);
    auto *gamma = GetDeviceAddress<T>(inputs, kIndex2);
    auto *running_std = GetDeviceAddress<T>(inputs, kIndex3);
    auto *d_weight = GetDeviceAddress<T>(outputs, kIndex0);
    auto *d_gamma = GetDeviceAddress<T>(outputs, kIndex1);
    auto *tmp = GetDeviceAddress<T>(workspace, kIndex0);

    auto status = CalCorrectionMul(d_out, gamma, running_std, batch_size_, channel_, height_, width_, d_weight,
                                   reinterpret_cast<cudaStream_t>(stream_ptr));
    CHECK_CUDA_STATUS(status, kernel_name_);
    status = CalCorrectionMulGrad(d_out, weight, running_std, batch_size_, channel_, height_, width_, d_gamma, tmp,
                                  reinterpret_cast<cudaStream_t>(stream_ptr));
    CHECK_CUDA_STATUS(status, kernel_name_);
    return true;
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override {
    return true;
  }

  void SetSizeLists() {
    size_t input_size = batch_size_ * channel_ * height_ * width_ * sizeof(T);
    size_t weight_size = batch_size_ * sizeof(T);
    output_size_list_.push_back(input_size);     // d_weight
    output_size_list_.push_back(weight_size);    // d_gamma
    workspace_size_list_.push_back(input_size);  // tmp d_out * weight
  }

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override {
    output_size_list_.clear();
    workspace_size_list_.clear();
    auto shape_signed = inputs[kIndex0]->GetShapeVector();
    if (IsDynamic(shape_signed)) {
      return KRET_UNKNOWN_SHAPE;
    }
    auto input_shape = Convert2SizeTClipNeg(shape_signed);
    is_null_input_ = CHECK_SHAPE_NULL(input_shape, kernel_name_, "input");
    if (is_null_input_) {
      SetSizeLists();
      return KRET_UNKNOWN_SHAPE;
    }
    if (input_shape.size() != kSize4) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dimension of input should be 4, but got "
                        << input_shape.size();
    }
    batch_size_ = input_shape[kIndex0];
    channel_ = input_shape[kIndex1];
    height_ = input_shape[kIndex2];
    width_ = input_shape[kIndex3];

    SetSizeLists();
    return KRET_OK;
  }

 private:
  bool is_null_input_;
  size_t batch_size_;
  size_t channel_;
  size_t height_;
  size_t width_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_CORRECTIONMULGRAD_GPUKERNEL_H_
