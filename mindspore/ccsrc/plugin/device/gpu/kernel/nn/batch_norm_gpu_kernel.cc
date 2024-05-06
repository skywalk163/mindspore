/**
 * Copyright 2021-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/gpu/kernel/nn/batch_norm_gpu_kernel.h"
#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include "ops/math_op_name.h"
#include "ops/nn_op_name.h"
#include "ops/op_name.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/elementwise/eltwise_ops_impl.cuh"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kBatchNormInputShapeMaxSize = 4;
constexpr size_t kBatchNormInputShapeMinSize = 2;
float kExpAvgFactorDefault = 0.1;
}  // namespace
template <typename T>
bool BatchNormGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                         const std::vector<KernelTensor *> &workspace,
                                         const std::vector<KernelTensor *> &outputs) {
  VARIABLE_NOT_USED(workspace);
  if (is_null_input_) {
    return true;
  }
  auto x = GetDeviceAddress<T>(inputs, kIndex0);
  auto scale = GetDeviceAddress<float>(inputs, kIndex1);
  auto bias = GetDeviceAddress<float>(inputs, kIndex2);
  auto running_mean = GetDeviceAddress<float>(inputs, kIndex3);
  auto running_variance = GetDeviceAddress<float>(inputs, kIndex4);
  T *z = nullptr;
  if (bn_ops_ == CUDNN_BATCHNORM_OPS_BN_ADD_ACTIVATION) {
    z = GetPossiblyNullDeviceAddress<T>(inputs, kIndex5);
  }

  auto y = GetDeviceAddress<T>(outputs, kIndex0);
  T *workspace_addr = GetPossiblyNullDeviceAddress<T>(workspace, kIndex0);

  const float alpha = 1;
  const float beta = 0;
  if (is_train_) {
    auto reserve_addr = GetPossiblyNullDeviceAddress<float>(outputs, kIndex2);
    auto save_mean = GetDeviceAddress<float>(outputs, kIndex3);
    auto save_variance = GetDeviceAddress<float>(outputs, kIndex4);
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
      cudnnBatchNormalizationForwardTrainingEx(handle_, mode_, bn_ops_, &alpha, &beta, x_desc_, x, z_desc_, z, y_desc_,
                                               y, scale_bias_mean_var_desc_, scale, bias, exp_avg_factor_, running_mean,
                                               running_variance, epsilon_, save_mean, save_variance, activation_desc_,
                                               workspace_addr, workspace_size_, reserve_addr, reserve_size_),
      "Kernel launch failed");
  } else {
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
      cudnnBatchNormalizationForwardInference(handle_, mode_, &alpha, &beta, x_desc_, x, y_desc_, y,
                                              scale_bias_mean_var_desc_, scale, bias, running_mean, running_variance,
                                              epsilon_),
      "Kernel launch failed");
  }
  if (kernel_name_ == kBatchNormWithActivationOpName && activation_type_ == mindspore::ActivationType::SWISH) {
    UnaryOpsCudaFunc<ElwiseOpType::kSiLU, T, T>(output_size_ / sizeof(T), y, y,
                                                reinterpret_cast<cudaStream_t>(cuda_stream_));
  }
  return true;
}

bool BatchNormGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  if (kernel_name_ == kBatchNormOpName) {
    bn_ops_ = CUDNN_BATCHNORM_OPS_BN;
  } else {
    auto activation_type_attr = primitive_->GetAttr(mindspore::ops::kActivationType);
    if (activation_type_attr != nullptr) {
      activation_type_ = ActivationType(GetValue<int64_t>(activation_type_attr));
    }
    if (kernel_name_ == kBatchNormWithActivationOpName && activation_type_ == mindspore::ActivationType::RELU) {
      bn_ops_ = CUDNN_BATCHNORM_OPS_BN_ACTIVATION;
    } else if (kernel_name_ == kBatchNormWithActivationOpName && activation_type_ == mindspore::ActivationType::SWISH) {
      // batch_norm + silu cuda kernel fusion
      bn_ops_ = CUDNN_BATCHNORM_OPS_BN;
    } else if (kernel_name_ == kBatchNormWithAddAndActivationOpName) {
      bn_ops_ = CUDNN_BATCHNORM_OPS_BN_ADD_ACTIVATION;
    } else {
      MS_LOG(EXCEPTION) << "Only support these kernel names: " << kBatchNormOpName << ", "
                        << kBatchNormWithActivationOpName << ", " << kBatchNormWithAddAndActivationOpName
                        << ", but got " << kernel_name_;
    }
  }
  attr_pos0_ = kernel_name_ == kBatchNormWithAddAndActivationOpName ? 6 : 5;

  auto iter = kernel_attr_map_.find(kernel_name_);
  if (iter == kernel_attr_map_.end()) {
    MS_LOG(ERROR)
      << "For 'BatchNorm', the kernel name must be in "
      << kernel::Map2Str<std::map, std::vector<std::pair<KernelAttr, BatchNormGpuKernelMod::BatchNormFunc>>>(
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

  InitResource();
  cudnn_data_type_ = GetCudnnDataType(TypeIdLabel(inputs[kIndex0]->dtype_id()));
  return true;
}

int BatchNormGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }

  is_train_ = inputs[attr_pos0_]->GetValueWithCheck<bool>();
  epsilon_ = inputs[attr_pos0_ + kIndex1]->GetValueWithCheck<float>();
  exp_avg_factor_ = inputs[attr_pos0_ + kIndex2]->GetValueWithCheck<float>();
  format_ = static_cast<mindspore::Format>(inputs[attr_pos0_ + kIndex3]->GetValueWithCheck<int64_t>());

  auto x_shape = inputs[kIndex0]->GetDeviceShapeVector();
  const size_t x_shape_size = x_shape.size();

  auto format = inputs[kIndex0]->format();
  if (x_shape_size == kBatchNormInputShapeMinSize) {
    format = Format::NCHW;
  } else if (format_ == Format::NHWC) {
    format = Format::NHWC;
  }

  (void)x_shape.insert(x_shape.begin() + (format == Format::NHWC ? kIndex1 : x_shape_size),
                       kBatchNormInputShapeMaxSize - x_shape_size, 1);

  if (x_shape_size == kBatchNormInputShapeMinSize) {
    mode_ = CUDNN_BATCHNORM_PER_ACTIVATION;
  } else if (is_train_) {
    mode_ = CUDNN_BATCHNORM_SPATIAL_PERSISTENT;
  } else {
    mode_ = CUDNN_BATCHNORM_SPATIAL;
  }

  CheckTensorSize({x_shape});
  SetTensorDescriptor(format, x_shape);
  InitSizeLists();
  return KRET_OK;
}

void BatchNormGpuKernelMod::ResetResource() noexcept {
  input_x_size_ = 0;
  input_z_size_ = 0;
  para_size_ = 0;
  output_size_ = 0;
  workspace_size_ = 0;
  reserve_size_ = 0;
  mode_ = CUDNN_BATCHNORM_SPATIAL;
  bn_ops_ = CUDNN_BATCHNORM_OPS_BN;
  epsilon_ = 10e-5;
  exp_avg_factor_ = kExpAvgFactorDefault;
  is_train_ = false;
  is_null_input_ = false;
  x_desc_ = nullptr;
  y_desc_ = nullptr;
  z_desc_ = nullptr;
  scale_bias_mean_var_desc_ = nullptr;
  activation_desc_ = nullptr;
  handle_ = nullptr;
  cudnn_data_type_ = CUDNN_DATA_FLOAT;
}

void BatchNormGpuKernelMod::DestroyResource() noexcept {
  CHECK_CUDNN_RET_WITH_ERROR_NOTRACE(cudnnDestroyTensorDescriptor(x_desc_), "Destroy x desc failed");
  CHECK_CUDNN_RET_WITH_ERROR_NOTRACE(cudnnDestroyTensorDescriptor(y_desc_), "Destroy y desc failed");
  CHECK_CUDNN_RET_WITH_ERROR_NOTRACE(cudnnDestroyTensorDescriptor(scale_bias_mean_var_desc_),
                                     "Destroy para desc failed");
  if (bn_ops_ == CUDNN_BATCHNORM_OPS_BN_ADD_ACTIVATION) {
    CHECK_CUDNN_RET_WITH_ERROR_NOTRACE(cudnnDestroyTensorDescriptor(z_desc_), "Destroy z desc failed");
  }

  if (bn_ops_ != CUDNN_BATCHNORM_OPS_BN) {
    CHECK_CUDNN_RET_WITH_ERROR_NOTRACE(cudnnDestroyActivationDescriptor(activation_desc_),
                                       "Destroy activation descriptor failed");
  }
}

void BatchNormGpuKernelMod::InitResource() {
  handle_ = device::gpu::GPUDeviceManager::GetInstance().GetCudnnHandle();
  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateTensorDescriptor(&x_desc_), "Create x desc failed");
  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateTensorDescriptor(&y_desc_), "Create y desc failed");
  if (bn_ops_ == CUDNN_BATCHNORM_OPS_BN_ADD_ACTIVATION) {
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateTensorDescriptor(&z_desc_), "Create z desc failed");
  }
  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateTensorDescriptor(&scale_bias_mean_var_desc_),
                                      "Create para desc failed");

  if (bn_ops_ != CUDNN_BATCHNORM_OPS_BN) {
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateActivationDescriptor(&activation_desc_),
                                        "Create activation descriptor failed");
  }
}

void BatchNormGpuKernelMod::InitSizeLists() {
  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnGetTensorSizeInBytes(x_desc_, &input_x_size_), "Get input x size failed");
  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnGetTensorSizeInBytes(scale_bias_mean_var_desc_, &para_size_),
                                      "Get para size failed");
  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnGetTensorSizeInBytes(y_desc_, &output_size_), "Get output size failed");

  if (bn_ops_ == CUDNN_BATCHNORM_OPS_BN_ADD_ACTIVATION) {
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnGetTensorSizeInBytes(z_desc_, &input_z_size_), "Get input z size failed");
  }

  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnGetBatchNormalizationForwardTrainingExWorkspaceSize(
                                        handle_, mode_, bn_ops_, x_desc_, z_desc_, y_desc_, scale_bias_mean_var_desc_,
                                        activation_desc_, &workspace_size_),
                                      "cudnnGetBatchNormalizationForwardTrainingExWorkspaceSize failed");

  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnGetBatchNormalizationTrainingExReserveSpaceSize(
                                        handle_, mode_, bn_ops_, activation_desc_, x_desc_, &reserve_size_),
                                      "Get reserve size failed");

  output_size_list_.clear();
  output_size_list_.push_back(output_size_);   // output
  output_size_list_.push_back(para_size_);     // save scale
  output_size_list_.push_back(reserve_size_);  // reserve space
  output_size_list_.push_back(para_size_);     // save mean
  output_size_list_.push_back(para_size_);     // save variance

  workspace_size_list_.push_back(workspace_size_);
}

void BatchNormGpuKernelMod::SetTensorDescriptor(const Format &format, const ShapeVector &shape) {
  cudnnTensorFormat_t cudnn_format;
  int batch;
  int channel;
  int height;
  int width;
  if (format == Format::NHWC) {
    batch = LongToInt(shape[kIndex0]);
    height = LongToInt(shape[kIndex1]);
    width = LongToInt(shape[kIndex2]);
    channel = LongToInt(shape[kIndex3]);
    cudnn_format = CUDNN_TENSOR_NHWC;
  } else {
    batch = LongToInt(shape[kIndex0]);
    channel = LongToInt(shape[kIndex1]);
    height = LongToInt(shape[kIndex2]);
    width = LongToInt(shape[kIndex3]);
    cudnn_format = CUDNN_TENSOR_NCHW;
  }
  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
    cudnnSetTensor4dDescriptor(x_desc_, cudnn_format, cudnn_data_type_, batch, channel, height, width),
    "Set x desc failed");

  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
    cudnnSetTensor4dDescriptor(y_desc_, cudnn_format, cudnn_data_type_, batch, channel, height, width),
    "Set y desc failed");

  if (bn_ops_ == CUDNN_BATCHNORM_OPS_BN_ADD_ACTIVATION) {
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
      cudnnSetTensor4dDescriptor(z_desc_, cudnn_format, cudnn_data_type_, batch, channel, height, width),
      "Set z desc failed");
  }

  CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
    cudnnSetTensor4dDescriptor(scale_bias_mean_var_desc_, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, 1, channel, 1, 1),
    "Set para desc failed");

  if (bn_ops_ != CUDNN_BATCHNORM_OPS_BN) {
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
      cudnnSetActivationDescriptor(activation_desc_, CUDNN_ACTIVATION_RELU, CUDNN_NOT_PROPAGATE_NAN, 0.0),
      "cudnnSetActivationDescriptor failed");
  }
}

#define BATCH_NORM_GPU_REG(MS, S)                        \
  KernelAttr()                                           \
    .AddInputAttr(MS)                                    \
    .AddInputAttr(kNumberTypeFloat32)                    \
    .AddInputAttr(kNumberTypeFloat32)                    \
    .AddInputAttr(kNumberTypeFloat32)                    \
    .AddInputAttr(kNumberTypeFloat32)                    \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)    \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32) \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32) \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)   \
    .AddOutputAttr(MS)                                   \
    .AddOutputAttr(kNumberTypeFloat32)                   \
    .AddOutputAttr(kNumberTypeFloat32)                   \
    .AddOutputAttr(kNumberTypeFloat32)                   \
    .AddOutputAttr(kNumberTypeFloat32),                  \
    &BatchNormGpuKernelMod::LaunchKernel<S>

#define BATCH_NORM_WITH_ADD_AND_ACTIVATION_GPU_REG(MS, S) \
  KernelAttr()                                            \
    .AddInputAttr(MS)                                     \
    .AddInputAttr(kNumberTypeFloat32)                     \
    .AddInputAttr(kNumberTypeFloat32)                     \
    .AddInputAttr(kNumberTypeFloat32)                     \
    .AddInputAttr(kNumberTypeFloat32)                     \
    .AddInputAttr(MS)                                     \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)     \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)  \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)  \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)    \
    .AddOutputAttr(MS)                                    \
    .AddOutputAttr(kNumberTypeFloat32)                    \
    .AddOutputAttr(kNumberTypeFloat32)                    \
    .AddOutputAttr(kNumberTypeFloat32)                    \
    .AddOutputAttr(kNumberTypeFloat32),                   \
    &BatchNormGpuKernelMod::LaunchKernel<S>

std::map<std::string, std::vector<std::pair<KernelAttr, BatchNormGpuKernelMod::BatchNormFunc>>>
  BatchNormGpuKernelMod::kernel_attr_map_ = {
    {kBatchNormOpName,
     {{BATCH_NORM_GPU_REG(kNumberTypeFloat32, float)}, {BATCH_NORM_GPU_REG(kNumberTypeFloat16, half)}}},
    {kBatchNormWithActivationOpName,
     {{BATCH_NORM_GPU_REG(kNumberTypeFloat32, float)}, {BATCH_NORM_GPU_REG(kNumberTypeFloat16, half)}}},
    {kBatchNormWithAddAndActivationOpName,
     {{BATCH_NORM_WITH_ADD_AND_ACTIVATION_GPU_REG(kNumberTypeFloat32, float)},
      {BATCH_NORM_WITH_ADD_AND_ACTIVATION_GPU_REG(kNumberTypeFloat16, half)}}}};

std::vector<KernelAttr> BatchNormGpuKernelMod::GetOpSupport() {
  auto iter = kernel_attr_map_.find(kernel_name_);
  if (iter == kernel_attr_map_.end()) {
    MS_LOG(ERROR)
      << "For 'BatchNorm', the kernel name must be in "
      << kernel::Map2Str<std::map, std::vector<std::pair<KernelAttr, BatchNormGpuKernelMod::BatchNormFunc>>>(
           kernel_attr_map_)
      << ", but got " << kernel_name_;
    return std::vector<KernelAttr>{};
  }
  std::vector<KernelAttr> support_list;
  (void)std::transform(iter->second.begin(), iter->second.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, BatchNormFunc> &item) { return item.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeGpuKernelMod, BatchNorm,
                                 []() { return std::make_shared<BatchNormGpuKernelMod>(kBatchNormOpName); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeGpuKernelMod, BatchNormWithActivation, []() {
  return std::make_shared<BatchNormGpuKernelMod>(kBatchNormWithActivationOpName);
});
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeGpuKernelMod, BatchNormWithAddAndActivation, []() {
  return std::make_shared<BatchNormGpuKernelMod>(kBatchNormWithAddAndActivationOpName);
});
}  // namespace kernel
}  // namespace mindspore
