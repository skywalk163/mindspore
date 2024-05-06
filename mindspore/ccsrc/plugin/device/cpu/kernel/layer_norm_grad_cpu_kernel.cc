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

#include "plugin/device/cpu/kernel/layer_norm_grad_cpu_kernel.h"
#include <algorithm>
#include "kernel/common_utils.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "include/common/thread_pool.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kLayerNormGradInputsNum = 7;
constexpr size_t kLayerNormGradOutputsNum = 3;
constexpr size_t kLayerNormGradInputXIndex = 0;
constexpr size_t kLayerNormGradInputDyIndex = 1;
constexpr size_t kLayerNormGradInputVarIndex = 2;
constexpr size_t kLayerNormGradInputMeanIndex = 3;
constexpr size_t kLayerNormGradInputGammaIndex = 4;
constexpr size_t kLayerNormGradBeginNormAxisIndex = 5;
constexpr size_t kLayerNormGradBeginParamsAxisIndex = 6;
constexpr size_t kLayerNormGradOutputDxIndex = 0;
constexpr size_t kLayerNormGradOutputDgIndex = 1;
constexpr size_t kLayerNormGradOutputDbIndex = 2;
}  // namespace

bool LayerNormGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' does not support this kernel type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  if (!primitive()->HasAttr(ops::kEpsilon)) {
    MS_LOG(WARNING) << "LayerNormGrad should have attr 'epsilon'.";
  } else {
    eps_ = GetValue<float>(primitive()->GetAttr(ops::kEpsilon));
  }
  return true;
}

int LayerNormGradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }

  if (inputs.empty()) {
    MS_LOG(EXCEPTION) << "Invalid input size!";
  }

  auto x_shape = inputs[kLayerNormGradInputXIndex]->GetShapeVector();
  auto begin_norm_axis = inputs[kLayerNormGradBeginNormAxisIndex]->GetValueWithCheck<int64_t>();
  auto begin_params_axis = inputs[kLayerNormGradBeginParamsAxisIndex]->GetValueWithCheck<int64_t>();
  if (begin_norm_axis < 0) {
    begin_norm_axis += SizeToLong(x_shape.size());
  }
  if (begin_params_axis < 0) {
    begin_params_axis += SizeToLong(x_shape.size());
  }
  block_num_ = 1;
  block_size_ = 1;
  param_num_ = 1;
  param_size_ = 1;
  for (size_t i = 0; i < LongToSize(begin_norm_axis); i++) {
    block_num_ *= LongToUlong(x_shape[i]);
  }
  for (size_t i = LongToSize(begin_norm_axis); i < x_shape.size(); i++) {
    block_size_ *= LongToUlong(x_shape[i]);
  }
  for (size_t i = 0; i < LongToSize(begin_params_axis); i++) {
    param_size_ *= LongToUlong(x_shape[i]);
  }
  for (size_t i = LongToSize(begin_params_axis); i < x_shape.size(); i++) {
    param_num_ *= LongToUlong(x_shape[i]);
  }
  if (block_num_ == 0 || block_size_ == 0) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dimension of 'input_x' must be at least 1, but got "
                      << x_shape;
  }
  return ret;
}

bool LayerNormGradCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                       const std::vector<kernel::KernelTensor *> &,
                                       const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kLayerNormGradInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kLayerNormGradOutputsNum, kernel_name_);
  kernel_func_(this, inputs, outputs);
  return true;
}

template <typename T>
void LayerNormGradCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                             const std::vector<KernelTensor *> &outputs) {
  auto *x = reinterpret_cast<T *>(inputs[kLayerNormGradInputXIndex]->device_ptr());
  auto *dy = reinterpret_cast<T *>(inputs[kLayerNormGradInputDyIndex]->device_ptr());
  auto *var = reinterpret_cast<float *>(inputs[kLayerNormGradInputVarIndex]->device_ptr());
  auto *mean = reinterpret_cast<float *>(inputs[kLayerNormGradInputMeanIndex]->device_ptr());
  auto *gamma = reinterpret_cast<T *>(inputs[kLayerNormGradInputGammaIndex]->device_ptr());
  auto *dx = reinterpret_cast<T *>(outputs[kLayerNormGradOutputDxIndex]->device_ptr());
  auto *dg = reinterpret_cast<T *>(outputs[kLayerNormGradOutputDgIndex]->device_ptr());
  auto *db = reinterpret_cast<T *>(outputs[kLayerNormGradOutputDbIndex]->device_ptr());
  size_t thread_num = common::ThreadPool::GetInstance().GetSyncRunThreadNum();
  auto thread_num1 = param_num_ < thread_num ? param_num_ : thread_num;
  std::vector<common::Task> tasks1;
  tasks1.reserve(thread_num1);
  auto thread_num2 = block_num_ < thread_num ? block_num_ : thread_num;
  std::vector<common::Task> tasks2;
  tasks2.reserve(thread_num2);
  auto task1 = [this, &x, &dy, &var, &mean, &dg, &db, thread_num1](size_t start) {
    for (size_t c = 0; c < ceil(static_cast<double>(param_num_) / thread_num1); ++c) {
      if (c * thread_num1 + start >= param_num_) {
        continue;
      }
      size_t param_index = c * thread_num1 + start;
      float dgamma = 0.0f;
      float dbeta = 0.0f;
      for (size_t j = param_index; j < param_size_ * param_num_; j += param_num_) {
        auto norm_shift = j / block_size_;
        dgamma += static_cast<float>(dy[j]) * std::pow(var[norm_shift] + eps_, -0.5f) *
                  (static_cast<float>(x[j]) - mean[norm_shift]);
        dbeta += static_cast<float>(dy[j]);
      }
      dg[param_index] = static_cast<T>(dgamma);
      db[param_index] = static_cast<T>(dbeta);
    }
  };
  auto task2 = [this, &x, &dy, &var, &mean, &dx, &gamma, thread_num2](size_t start) {
    for (size_t c = 0; c < ceil(static_cast<double>(block_num_) / thread_num2); ++c) {
      if (c * thread_num2 + start >= block_num_) {
        continue;
      }
      size_t block_index = c * thread_num2 + start;
      float sum1 = 0.0f;
      float sum2 = 0.0f;
      float sum3 = 0.0f;
      auto norm_shift = block_index;
      for (size_t j = block_index * block_size_; j < (block_index + 1) * block_size_; ++j) {
        auto param_shift = j % param_num_;
        auto dxm = static_cast<float>(x[j]) - mean[norm_shift];
        auto dyg = static_cast<float>(dy[j] * gamma[param_shift]);
        sum1 += dyg * dxm;
        sum2 += dyg;
        sum3 += dxm;
      }
      sum1 *= -0.5f * std::pow(var[norm_shift] + eps_, -1.5f);
      sum3 *= -2.0f;

      auto inv_block_size = 1.0f / block_size_;
      auto dx3 = std::pow(var[norm_shift] + eps_, -0.5f);
      auto dx4 = 2.0f * sum1 * inv_block_size;
      auto dx5 = (-1.0f * dx3 * sum2 + inv_block_size * sum1 * sum3) * inv_block_size;
      for (size_t j = block_index * block_size_; j < (block_index + 1) * block_size_; ++j) {
        auto param_shift = j % param_num_;
        auto dx1 = static_cast<float>(dy[j] * gamma[param_shift]);
        auto dx2 = static_cast<float>(x[j]) - mean[norm_shift];
        dx[j] = static_cast<T>(dx1 * dx3 + dx2 * dx4 + dx5);
      }
    }
  };
  for (size_t i = 0; i < thread_num1; ++i) {
    auto block = [&, i]() {
      task1(i);
      return common::SUCCESS;
    };
    (void)tasks1.emplace_back(block);
  }
  ParallelLaunch(tasks1);
  for (size_t i = 0; i < thread_num2; ++i) {
    auto block = [&, i]() {
      task2(i);
      return common::SUCCESS;
    };
    (void)tasks2.emplace_back(block);
  }
  ParallelLaunch(tasks2);
}

std::vector<std::pair<KernelAttr, LayerNormGradCpuKernelMod::KernelFunc>> LayerNormGradCpuKernelMod::func_list_ = {
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat16)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16)
     .AddOutputAttr(kNumberTypeFloat16)
     .AddOutputAttr(kNumberTypeFloat16),
   &LayerNormGradCpuKernelMod::LaunchKernel<float16>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32)
     .AddOutputAttr(kNumberTypeFloat32)
     .AddOutputAttr(kNumberTypeFloat32),
   &LayerNormGradCpuKernelMod::LaunchKernel<float>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64)
     .AddOutputAttr(kNumberTypeFloat64)
     .AddOutputAttr(kNumberTypeFloat64),
   &LayerNormGradCpuKernelMod::LaunchKernel<double>}};

std::vector<KernelAttr> LayerNormGradCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, KernelFunc> &pair) { return pair.first; });
  return support_list;
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, LayerNormGrad, LayerNormGradCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
