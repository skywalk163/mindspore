/**
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/gpu/kernel/nn/activation_grad_kernel.h"
#include <memory>
#include "mindspore/core/ops/nn_optimizer_ops.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/complex.h"
#include "ops/auto_generate/gen_ops_name.h"

namespace mindspore {
namespace kernel {
std::map<std::string, std::vector<std::pair<KernelAttr, ActivationGradGpuKernelMod::ActivationGradFunc>>>
  ActivationGradGpuKernelMod::kernel_attr_map_ = {
    {ops::kNameReLU6Grad,
     {{KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
       &ActivationGradGpuKernelMod::LaunchEluRelu<float>},
      {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
       &ActivationGradGpuKernelMod::LaunchEluRelu<half>}}},
    {ops::kNameEluGrad,
     {{KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
       &ActivationGradGpuKernelMod::LaunchEluRelu<float>},
      {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
       &ActivationGradGpuKernelMod::LaunchEluRelu<half>}}},
};

bool ActivationGradGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &outputs) {
  cudnn_handle_ = device::gpu::GPUDeviceManager::GetInstance().GetCudnnHandle();

  auto iter = kernel_attr_map_.find(kernel_name_);
  if (iter == kernel_attr_map_.end()) {
    MS_LOG(ERROR)
      << "For 'ActivationGrad', the kernel name must be in "
      << kernel::Map2Str<std::map, std::vector<std::pair<KernelAttr, ActivationGradGpuKernelMod::ActivationGradFunc>>>(
           kernel_attr_map_)
      << ", but got " << kernel_name_;
    return false;
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }
  kernel_func_ = kernel_attr_map_.at(kernel_name_)[index].second;

  static const std::map<std::string, cudnnActivationMode_t> activation_mode_map = {
    {ops::kNameReLU6Grad, CUDNN_ACTIVATION_CLIPPED_RELU}, {ops::kNameEluGrad, CUDNN_ACTIVATION_ELU}};
  auto mode_iter = activation_mode_map.find(kernel_name_);
  if (mode_iter == activation_mode_map.end()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', only support these activations: "
                  << kernel::Map2Str<std::map, cudnnActivationMode_t>(activation_mode_map) << ", but got "
                  << kernel_name_;
    return KRET_RESIZE_FAILED;
  }
  mode_ = mode_iter->second;

  dtype_ = inputs[kIndex0]->dtype_id();
  return true;
}

int ActivationGradGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  input_shape_ = inputs[kIndex0]->GetShapeVector();
  is_null_input_ = CHECK_NULL_INPUT(input_shape_);
  if (is_null_input_) {
    return KRET_OK;
  }

  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateTensorDescriptor(&data_descriptor_),
                                      "For 'ActivationGrad', cudnnCreateTensorDescriptor failed.");
  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateActivationDescriptor(&activation_desc_),
                                      "For 'ActivationGrad', cudnnCreateActivationDescriptor failed.");
  cudnn_data_type_ = GetCudnnDataType(TypeIdLabel(inputs[kIndex0]->dtype_id()));
  CheckTensorSize({input_shape_});
  ShapeVector shape;
  double coef = (mode_ == CUDNN_ACTIVATION_CLIPPED_RELU) ? ReLU6_UP_TURNING_POINT : 0.0;
  if (mode_ == CUDNN_ACTIVATION_ELU) coef = 1.0;
  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnSetActivationDescriptor(activation_desc_, mode_, CUDNN_PROPAGATE_NAN, coef),
                                      "For 'ActivationGrad', cudnnSetActivationDescriptor failed.");

  const int split_dim = 4;
  if (input_shape_.size() <= split_dim) {
    ShapeNdTo4d(input_shape_, &shape);
    if (inputs[kIndex0]->format() == mindspore::Format::NHWC) {
      CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
        cudnnSetTensor4dDescriptor(data_descriptor_, CUDNN_TENSOR_NHWC, cudnn_data_type_, LongToInt(shape[0]),
                                   LongToInt(shape[3]), LongToInt(shape[1]), LongToInt(shape[2])),
        "For 'ActivationGrad', cudnnSetTensor4dDescriptor failed.");
    } else {
      CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
        cudnnSetTensor4dDescriptor(data_descriptor_, CUDNN_TENSOR_NCHW, cudnn_data_type_, LongToInt(shape[0]),
                                   LongToInt(shape[1]), LongToInt(shape[2]), LongToInt(shape[3])),
        "For 'ActivationGrad', cudnnSetTensor4dDescriptor failed.");
    }
  } else {
    CudnnSetTensorNdDescriptor(input_shape_, data_descriptor_, cudnn_data_type_, kernel_name_);
  }
  return KRET_OK;
}

std::vector<KernelAttr> ActivationGradGpuKernelMod::GetOpSupport() {
  auto iter = kernel_attr_map_.find(kernel_name_);
  if (iter == kernel_attr_map_.end()) {
    MS_LOG(ERROR)
      << "For 'ActivationGrad', the kernel name must be in "
      << kernel::Map2Str<std::map, std::vector<std::pair<KernelAttr, ActivationGradGpuKernelMod::ActivationGradFunc>>>(
           kernel_attr_map_)
      << ", but got " << kernel_name_;
    return std::vector<KernelAttr>{};
  }
  std::vector<KernelAttr> support_list;
  (void)std::transform(iter->second.begin(), iter->second.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, ActivationGradFunc> &item) { return item.first; });
  return support_list;
}

template <typename T>
bool ActivationGradGpuKernelMod::LaunchEluRelu(const std::vector<kernel::KernelTensor *> &inputs,
                                               const std::vector<kernel::KernelTensor *> &outputs) {
  T *dy = GetDeviceAddress<T>(inputs, kIndex0);
  T *y = GetDeviceAddress<T>(inputs, kIndex1);
  T *dx = GetDeviceAddress<T>(outputs, kIndex0);
  const float alpha = 1;
  const float beta = 0;
  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
    cudnnActivationBackward(cudnn_handle_, activation_desc_, &alpha, data_descriptor_, y, data_descriptor_, dy,
                            data_descriptor_, y, &beta, data_descriptor_, dx),
    "For 'ActivationGrad', cudnnActivationBackward failed.");

  return true;
}
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeGpuKernelMod, ReLU6Grad,
                                 []() { return std::make_shared<ActivationGradGpuKernelMod>(ops::kNameReLU6Grad); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeGpuKernelMod, EluGrad,
                                 []() { return std::make_shared<ActivationGradGpuKernelMod>(ops::kNameEluGrad); });
}  // namespace kernel
}  // namespace mindspore
