/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_ACTIVATION_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_ACTIVATION_GPU_KERNEL_H_

#include <functional>
#include <vector>
#include <string>
#include <map>
#include <utility>
#include <algorithm>
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/kernel_constants.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/relu_impl.cuh"

namespace mindspore {
namespace kernel {
constexpr auto kUnKnown = "UnKnown";

class ActivationFwdGpuKernelMod : public NativeGpuKernelMod {
 public:
  explicit ActivationFwdGpuKernelMod(const std::string &kernel_name) : kernel_name_(kernel_name) {}
  ~ActivationFwdGpuKernelMod() override { DestroyResource(); };

  bool Init(const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
            const std::vector<KernelTensorPtr> &outputs) override;

  int Resize(
    const BaseOperatorPtr &base_operator, const std::vector<KernelTensorPtr> &inputs,
    const std::vector<KernelTensorPtr> &outputs,
    const std::map<uint32_t, tensor::TensorPtr> &inputsOnHost = std::map<uint32_t, tensor::TensorPtr>()) override;

  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
              const std::vector<AddressPtr> &outputs, void *) override {
    if (is_null_input_) {
      return true;
    }

    return kernel_func_(this, inputs, outputs);
  }

  void DestroyResource() noexcept override {
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnDestroyActivationDescriptor(activation_desc_),
                                        "For 'Activation', cudnnDestroyActivationDescriptor failed.");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnDestroyTensorDescriptor(data_descriptor_),
                                        "For 'Activation', cudnnDestroyTensorDescriptor failed.");
  }

 protected:
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<kernel::AddressPtr> &inputs, const std::vector<kernel::AddressPtr> &outputs);
  using ActivationFunc = std::function<bool(ActivationFwdGpuKernelMod *, const std::vector<kernel::AddressPtr> &,
                                            const std::vector<kernel::AddressPtr> &)>;
  static std::map<std::string, std::vector<std::pair<KernelAttr, ActivationFwdGpuKernelMod::ActivationFunc>>>
    kernel_attr_map_;
  std::string kernel_name_{kUnKnown};
  ActivationFunc kernel_func_;
  ShapeVector input_shape_{};
  bool is_null_input_{true};
  cudnnHandle_t cudnn_handle_{nullptr};
  cudnnActivationDescriptor_t activation_desc_{nullptr};
  cudnnActivationMode_t mode_{CUDNN_ACTIVATION_SIGMOID};
  cudnnTensorDescriptor_t data_descriptor_{nullptr};
  cudnnDataType_t cudnn_data_type_{CUDNN_DATA_FLOAT};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_ACTIVATION_GPU_KERNEL_H_
