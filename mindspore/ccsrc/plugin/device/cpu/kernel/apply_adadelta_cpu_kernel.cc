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

#include <algorithm>

#include "kernel/common_utils.h"
#include "plugin/device/cpu/kernel/apply_adadelta_cpu_kernel.h"
#include "plugin/device/cpu/kernel/nnacl/intrinsics/ms_simd_instructions.h"
#include "plugin/device/cpu/kernel/nnacl/op_base.h"
#include "ops/op_utils.h"

namespace mindspore {
namespace kernel {
constexpr size_t kApplyAdadeltaInputsNum = 7;
constexpr size_t kVarIndex = 0;
constexpr size_t kAccumIndex = 1;
constexpr size_t kAccumUpdateIndex = 2;
constexpr size_t kLRIndex = 3;
constexpr size_t kRhoIndex = 4;
constexpr size_t kEpsilonIndex = 5;
constexpr size_t kGradIndex = 6;

bool ApplyAdadeltaCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  batch_rank_ = ops::get_batch_rank(primitive_);

  auto input_type_id = inputs[0]->dtype_id();
  if (input_type_id != kNumberTypeFloat32) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "',  does not support " << TypeIdToString(input_type_id);
    return false;
  }
  unit_size_ = sizeof(float);

  return true;
}

int ApplyAdadeltaCpuKernelMod::CheckInputShape(const std::vector<KernelTensor *> &inputs) {
  std::vector<int64_t> var_shape = inputs[kVarIndex]->GetShapeVector();
  std::vector<int64_t> accum_shape = inputs[kAccumIndex]->GetShapeVector();
  std::vector<int64_t> accum_update_shape = inputs[kAccumUpdateIndex]->GetShapeVector();
  std::vector<int64_t> lr_shape = inputs[kLRIndex]->GetShapeVector();
  std::vector<int64_t> rho_shape = inputs[kRhoIndex]->GetShapeVector();
  std::vector<int64_t> epsilon_shape = inputs[kEpsilonIndex]->GetShapeVector();
  std::vector<int64_t> grad_shape = inputs[kGradIndex]->GetShapeVector();
  if (!(IsSameShape(var_shape, accum_shape) && IsSameShape(var_shape, accum_update_shape) &&
        IsSameShape(var_shape, grad_shape))) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the shape of 'var', 'accum', 'accum_update', 'grad' "
                     "must be the same, "
                     "but got the shapes 'var': "
                  << var_shape << ", 'accum': " << accum_shape << ", 'accum_update': " << accum_update_shape
                  << ", 'grad': " << grad_shape;
    return KRET_RESIZE_FAILED;
  }

  if (!(IsSameShape(lr_shape, rho_shape) || IsSameShape(lr_shape, epsilon_shape))) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the shape of 'lr', 'rho' and 'epsilon' must be the same, "
                     "but got the shapes 'lr': "
                  << lr_shape << ", 'rho': " << rho_shape << ", 'epsilon': " << epsilon_shape;

    return KRET_RESIZE_FAILED;
  }
  return KRET_OK;
}

int ApplyAdadeltaCpuKernelMod::CheckShapeSize(std::vector<int64_t> var_shape, std::vector<int64_t> lr_shape) {
  if (batch_rank_ > 1) {
    if (var_shape.size() < lr_shape.size()) {
      MS_LOG(ERROR) << "For '" << kernel_name_
                    << "', the shape size of 'var' must be greater than "
                       "'lr_shape', but got the shape of 'var': "
                    << var_shape << " and 'lr_shape': " << lr_shape;
      return KRET_RESIZE_FAILED;
    }
    std::vector<int64_t> var_batch_shape(var_shape.begin(), var_shape.begin() + batch_rank_);
    if (!IsSameShape(lr_shape, var_batch_shape)) {
      MS_LOG(ERROR) << "For '" << kernel_name_
                    << "', the batch shape of 'var' must be the same as the "
                       "shape of 'lr', "
                       "but got the batch shape of 'var': "
                    << var_batch_shape << " and the shape of 'lr': " << lr_shape;
      return KRET_RESIZE_FAILED;
    }
  }
  return KRET_OK;
}

int ApplyAdadeltaCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }

  ret = CheckInputShape(inputs);
  if (ret != KRET_OK) {
    return ret;
  }
  std::vector<int64_t> var_shape = inputs[kVarIndex]->GetShapeVector();
  std::vector<int64_t> lr_shape = inputs[kLRIndex]->GetShapeVector();

  batch_size_ = 1;
  if (!lr_shape.empty()) {
    batch_size_ = std::accumulate(lr_shape.begin(), lr_shape.end(), batch_size_, std::multiplies<int64_t>());
  }
  if (batch_size_ == 0) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', batch_size_ must be greater than 0, but got batch_size: " << batch_size_;
    return KRET_RESIZE_FAILED;
  }
  input_elements_ = std::accumulate(var_shape.begin(), var_shape.end(), int64_t(1), std::multiplies<int64_t>());
  input_elements_ = input_elements_ / batch_size_;

  ret = CheckShapeSize(var_shape, lr_shape);

  return ret;
}

bool ApplyAdadeltaCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                       const std::vector<kernel::KernelTensor *> &workspace,
                                       const std::vector<kernel::KernelTensor *> &) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kApplyAdadeltaInputsNum, kernel_name_);
  auto var = reinterpret_cast<float *>(inputs[kVarIndex]->device_ptr());
  auto accum = reinterpret_cast<float *>(inputs[kAccumIndex]->device_ptr());
  auto accum_update = reinterpret_cast<float *>(inputs[kAccumUpdateIndex]->device_ptr());
  auto lr = reinterpret_cast<float *>(inputs[kLRIndex]->device_ptr());
  auto rho = reinterpret_cast<float *>(inputs[kRhoIndex]->device_ptr());
  auto epsilon = reinterpret_cast<float *>(inputs[kEpsilonIndex]->device_ptr());
  auto grad = reinterpret_cast<float *>(inputs[kGradIndex]->device_ptr());

  for (int64_t b = 0; b < batch_size_; b++) {
    auto task = [&](size_t start, size_t end) {
      for (size_t i = start; i < end; ++i) {
        accum[i] = rho[b] * accum[i] + (1.0 - rho[b]) * (grad[i] * grad[i]);
        float update = sqrt(accum_update[i] + epsilon[b]) * (grad[i] / sqrt(accum[i] + epsilon[b]));
        accum_update[i] = rho[b] * accum_update[i] + (1.0 - rho[b]) * (update * update);
        var[i] -= lr[b] * update;
      }
    };
    ParallelLaunchAutoSearch(task, input_elements_, this, &parallel_search_info_);

    var = var + input_elements_;
    accum = accum + input_elements_;
    accum_update = accum_update + input_elements_;
    grad = grad + input_elements_;
  }

  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, ApplyAdadelta, ApplyAdadeltaCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
