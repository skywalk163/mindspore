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

#include "plugin/device/cpu/kernel/lp_norm_cpu_kernel.h"
#include <map>
#include <utility>
#include <string>
#include <algorithm>
#include <memory>
#include <functional>
#include <set>
#ifdef PLATFORM_86
#include <pmmintrin.h>
#include <xmmintrin.h>
#endif
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "mindspore/core/ops/lp_norm.h"
#include "plugin/device/cpu/kernel/nnacl/op_base.h"

namespace mindspore {
namespace kernel {
namespace {
// An empiric parameters for parallel
constexpr size_t kGrainSize = 32768;
std::vector<size_t> CalPhysicalIndexes(const std::vector<size_t> &input_shape,
                                       const std::vector<size_t> &logical_stride, size_t input_elements) {
  size_t shape_size = input_shape.size();
  std::vector<size_t> physical_indexes(input_elements, 0);
  for (size_t position = 0; position < input_elements; ++position) {
    size_t logical_index = position;
    size_t physical_index = 0;
    for (int i = static_cast<int>(shape_size) - 1; i >= 0; --i) {
      auto size_i = static_cast<size_t>(i);
      size_t coordinate = logical_index % input_shape[size_i];
      physical_index += coordinate * logical_stride[size_i];
      logical_index = logical_index / input_shape[size_i];
    }
    physical_indexes[position] = physical_index;
  }
  return physical_indexes;
}
}  // namespace

bool LpNormCpuKernelMod::GetReductionAttr() {
  if (kernel_name_ != ops::kNameLpNorm) {
    MS_LOG(ERROR) << "For 'LpNorm', it's kernel name get failed, but got " << kernel_name_;
    return false;
  }
  int64_t p = GetValue<int64_t>(primitive_->GetAttr(ops::kP));
  is_p_zero_ = (p == 0);
  p_ = LongToFloat(p);
  epsilon_ = GetValue<float>(primitive_->GetAttr(ops::kEpsilon));
  axis_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kAxis));
  return true;
}

bool LpNormCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (inputs.empty() || outputs.empty()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' it got empty inputs or outputs, which is invalid.";
    return false;
  }
  if (!GetReductionAttr()) {
    return false;
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' it does not support this kernel type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int LpNormCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  // For Scalar Tensor, input shape is empty.
  auto input_shape = LongVecToSizeVec(inputs.at(kIndex0)->GetShapeVector());
  is_null_input_ = CHECK_SHAPE_NULL(input_shape, kernel_name_, "input shape");
  if (is_null_input_) {
    return KRET_OK;
  }
  is_scalar_input_ = input_shape.empty();
  if (is_scalar_input_) {
    return KRET_OK;
  }
  is_scalar_input_ = false;
  input_elements_ = std::accumulate(input_shape.begin(), input_shape.end(), size_t(1), std::multiplies<size_t>());
  // The axis_'s validation has been check in core/ops/lp_norm.cc, just using it.
  std::vector<size_t> axis;
  int64_t input_rank = SizeToLong(input_shape.size());
  (void)std::transform(axis_.begin(), axis_.end(), std::back_inserter(axis), [&input_rank](const int64_t &dim) {
    return dim < 0 ? LongToSize(dim + input_rank) : LongToSize(dim);
  });
  std::vector<size_t> output_stride(input_shape.size(), 1);
  for (int i = static_cast<int>(output_stride.size()) - 2; i >= 0; --i) {
    auto size_i = static_cast<size_t>(i);
    output_stride[size_i] = output_stride[size_i + 1] * input_shape[size_i + 1];
  }
  std::vector<size_t> logical_shape;
  std::vector<size_t> logical_stride;
  std::set<size_t> axis_set(axis.begin(), axis.end());
  for (size_t i = 0; i < input_shape.size(); ++i) {
    if (!axis_set.count(i)) {
      logical_shape.emplace_back(input_shape.at(i));
      logical_stride.emplace_back(output_stride.at(i));
    }
  }
  reduce_size_ = 1;
  for (const auto &dim : axis) {
    logical_shape.emplace_back(input_shape.at(dim));
    logical_stride.emplace_back(output_stride.at(dim));
    reduce_size_ *= input_shape.at(dim);
  }
  physical_indexes_ = CalPhysicalIndexes(logical_shape, logical_stride, input_elements_);
  return KRET_OK;
}

template <typename T>
bool LpNormCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                      const std::vector<kernel::KernelTensor *> &outputs) {
  auto input = GetDeviceAddress<T>(inputs, kIndex0);
  auto output = GetDeviceAddress<T>(outputs, kIndex0);
  auto template_one = static_cast<T>(1);
  auto template_zero = static_cast<T>(0);
  if (is_scalar_input_) {
    MS_EXCEPTION_IF_NULL(output);
    *output = is_p_zero_ ? template_one : std::abs(input[0]);
    return true;
  }
  bool is_parallel = input_elements_ > kGrainSize;
  size_t thread_num = is_parallel ? std::min(input_elements_, pool_->GetKernelThreadNum()) : 1;
  std::vector<std::pair<size_t, T>> reduce_buffer(thread_num, {0, template_zero});
  CTask reduce_task = [this, &input, &output, &reduce_buffer, &thread_num, &template_zero, &template_one](size_t start,
                                                                                                          size_t end) {
#ifdef PLATFORM_86
    // Small value should be reserved, or else the value scaling brought by 'pow' will cause precision loss
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_OFF);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_OFF);
#endif
    auto stride_per_thread = UP_DIV(input_elements_, thread_num);
    size_t task_id = (start / stride_per_thread);
    T acc = template_zero;
    for (size_t i = start; i < end; ++i) {
      size_t physical_index = physical_indexes_[i];
      if (!is_p_zero_) {
        acc += std::pow(std::abs(input[physical_index]), p_);
      } else if (input[physical_index] != template_zero) {
        acc += template_one;
      }
      if ((i + 1) % reduce_size_ == 0) {
        output[i / reduce_size_] = acc;
        acc = template_zero;
        continue;
      }
      if (i == end - 1) {
        reduce_buffer[task_id] = {i, acc};
      }
    }
#ifdef PLATFORM_86
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
  };
  CTask combine_task = [this, &output](size_t start, size_t end) {
#ifdef PLATFORM_86
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_OFF);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_OFF);
#endif
    for (size_t i = start; i < end; ++i) {
      output[i] = std::max(std::pow(output[i], 1 / p_), epsilon_);
    }
#ifdef PLATFORM_86
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
  };
  if (is_parallel) {
    ParallelLaunch(reduce_task, input_elements_, 0, this, pool_);
    for (const auto &buffer : reduce_buffer) {
      size_t output_index = buffer.first / reduce_size_;
      output[output_index] += buffer.second;
    }
    if (!is_p_zero_) {
      ParallelLaunch(combine_task, input_elements_ / reduce_size_, 0, this, pool_);
    }
    return true;
  }
  reduce_task(0, input_elements_);
  if (!is_p_zero_) {
    combine_task(0, input_elements_ / reduce_size_);
  }
  return true;
}

std::vector<std::pair<KernelAttr, LpNormCpuKernelMod::LpNromFunc>> LpNormCpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   &LpNormCpuKernelMod::LaunchKernel<float>}};

std::vector<KernelAttr> LpNormCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, LpNromFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, LpNorm, LpNormCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
