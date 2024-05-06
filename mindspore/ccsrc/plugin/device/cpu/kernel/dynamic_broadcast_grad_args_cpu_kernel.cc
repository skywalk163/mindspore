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
#include <string>
#include <utility>

#include "mindspore/core/ops/dynamic_broadcast_gradient_args.h"
#include "plugin/device/cpu/kernel/dynamic_broadcast_grad_args_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kDynamicBroadcastGradientArgsInputsNum = 2;
constexpr size_t kDynamicBroadcastGradientArgsOutputsNum = 2;
constexpr char kKernelName[] = "DynamicBroadcastGradientArgs";
using KernelRunFunc = DynamicBroadcastGradientArgsCpuKernelMod::KernelRunFunc;
}  // namespace

template <typename T>
void AddGradReduceIdx(std::vector<std::vector<T>> *grad_reduce_idx, std::vector<bool> cur_one, bool none_one,
                      const size_t max_rank, size_t j) {
  MS_EXCEPTION_IF_NULL(grad_reduce_idx);
  for (size_t i = 0; i < kDynamicBroadcastGradientArgsInputsNum; i++) {
    if (cur_one[i] && !none_one) {
      (void)(*grad_reduce_idx)[i].emplace_back(SizeToLong(max_rank - 1 - j));
    }
  }
}

template <typename T>
std::vector<std::vector<T>> GetGradIndex(const std::vector<std::vector<T>> &revers_shapes, const size_t max_rank) {
  std::vector<std::vector<T>> grad_reduce_index(kDynamicBroadcastGradientArgsInputsNum);
  std::vector<bool> pre_one(kDynamicBroadcastGradientArgsInputsNum);
  std::vector<bool> cur_one(kDynamicBroadcastGradientArgsInputsNum);
  for (size_t i = 0; i < kDynamicBroadcastGradientArgsInputsNum; i++) {
    pre_one[i] = false;
    cur_one[i] = false;
  }
  bool set_one = false;
  for (size_t j = 0; j < max_rank; j++) {
    int out_dim = -1;
    bool out_dim_set = false;
    bool none_one = true;
    for (size_t i = 0; i < kDynamicBroadcastGradientArgsInputsNum; i++) {
      if (revers_shapes[i][j] == 1) {
        cur_one[i] = true;
        none_one = false;
      } else {
        cur_one[i] = false;
        if (!out_dim_set || revers_shapes[i][j] == static_cast<T>(out_dim)) {
          out_dim = static_cast<int>(revers_shapes[i][j]);
          out_dim_set = true;
        } else {
          MS_LOG(EXCEPTION) << "Can not broadcast inputs[0] and inputs[1].";
        }
      }
    }
    if (!out_dim_set) {
      for (size_t i = 0; i < kDynamicBroadcastGradientArgsInputsNum; i++) {
        (void)grad_reduce_index[i].emplace_back(max_rank - 1 - j);
      }
      continue;
    } else if (std::equal(cur_one.begin(), cur_one.end(), pre_one.begin()) && set_one) {
      AddGradReduceIdx(&grad_reduce_index, cur_one, none_one, max_rank, j);
    } else {
      AddGradReduceIdx(&grad_reduce_index, cur_one, none_one, max_rank, j);
    }
    set_one = true;
    for (size_t i = 0; i < kDynamicBroadcastGradientArgsInputsNum; i++) {
      pre_one[i] = cur_one[i];
    }
  }
  return grad_reduce_index;
}

template <typename T, typename S>
size_t SetOuputValue(S *addr, const std::vector<T> &grad_reduce_idx) {
  size_t index_num = grad_reduce_idx.size();
  for (size_t i = 0; i < index_num; i++) {
    addr[i] = static_cast<S>(grad_reduce_idx[index_num - 1 - i]);
  }

  return index_num;
}

template <typename T, typename S>
bool DynamicBroadcastGradientArgsCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                                            const std::vector<kernel::KernelTensor *> &,
                                                            const std::vector<kernel::KernelTensor *> &outputs) {
  std::vector<size_t> ranks = {inputs[0]->size() / sizeof(T), inputs[1]->size() / sizeof(T)};
  std::vector<std::vector<T>> reverse_shapes(kDynamicBroadcastGradientArgsInputsNum);
  if (!is_null_input0_) {
    const T *s0_addr = static_cast<T *>(inputs[0]->device_ptr());
    for (size_t j = 0; j < ranks[0]; j++) {
      reverse_shapes[0].push_back(s0_addr[ranks[0] - j - 1]);
    }
  } else {
    ranks[0] = 0;
  }
  if (!is_null_input1_) {
    const T *s1_addr = static_cast<T *>(inputs[1]->device_ptr());
    for (size_t j = 0; j < ranks[1]; j++) {
      reverse_shapes[1].push_back(s1_addr[ranks[1] - j - 1]);
    }
  } else {
    ranks[1] = 0;
  }
  S *r0_addr = static_cast<S *>(outputs[0]->device_ptr());
  S *r1_addr = static_cast<S *>(outputs[1]->device_ptr());

  std::vector<std::vector<T>> grad_reduce_idx(kDynamicBroadcastGradientArgsInputsNum);
  size_t max_rank = ranks[0] > ranks[1] ? ranks[0] : ranks[1];
  if (reverse_shapes[0].size() < max_rank) {
    reverse_shapes[0].resize(max_rank, 1);
  }
  if (reverse_shapes[1].size() < max_rank) {
    reverse_shapes[1].resize(max_rank, 1);
  }

  if (reverse_shapes[0] != reverse_shapes[1]) {
    grad_reduce_idx = GetGradIndex(reverse_shapes, max_rank);
  }

  r0_size_ = SetOuputValue(r0_addr, grad_reduce_idx[0]);
  r1_size_ = SetOuputValue(r1_addr, grad_reduce_idx[1]);

  return true;
}

bool DynamicBroadcastGradientArgsCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                                    const std::vector<KernelTensor *> &outputs) {
  if (inputs.size() != kDynamicBroadcastGradientArgsInputsNum ||
      outputs.size() != kDynamicBroadcastGradientArgsOutputsNum) {
    MS_LOG(ERROR) << kernel_name_ << ": input and output size should be " << kDynamicBroadcastGradientArgsInputsNum
                  << " and " << kDynamicBroadcastGradientArgsOutputsNum << ", but get " << inputs.size() << " and "
                  << outputs.size();
    return false;
  }

  if (!MatchKernelFunc(kernel_name_, inputs, outputs)) {
    return false;
  }
  return true;
}

int DynamicBroadcastGradientArgsCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                                     const std::vector<KernelTensor *> &outputs) {
  if (KernelMod::Resize(inputs, outputs) == static_cast<int>(KRET_RESIZE_FAILED)) {
    MS_LOG(WARNING) << kernel_name_ << " reinit failed.";
    return static_cast<int>(KRET_RESIZE_FAILED);
  }
  auto input_0_shape = inputs[0]->GetShapeVector();
  auto input_1_shape = inputs[1]->GetShapeVector();
  is_null_input0_ = CHECK_NULL_INPUT(input_0_shape);
  is_null_input1_ = CHECK_NULL_INPUT(input_1_shape);
  return static_cast<int>(KRET_OK);
}

const std::vector<std::pair<KernelAttr, KernelRunFunc>> &DynamicBroadcastGradientArgsCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, KernelRunFunc>> func_list = {
    {KernelAttr()
       .AddInputAttr(kNumberTypeInt32)
       .AddInputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeInt32),
     &DynamicBroadcastGradientArgsCpuKernelMod::LaunchKernel<int32_t, int32_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeInt32)
       .AddInputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64),
     &DynamicBroadcastGradientArgsCpuKernelMod::LaunchKernel<int32_t, int64_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeInt32),
     &DynamicBroadcastGradientArgsCpuKernelMod::LaunchKernel<int64_t, int32_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64),
     &DynamicBroadcastGradientArgsCpuKernelMod::LaunchKernel<int64_t, int64_t>},
  };
  return func_list;
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, DynamicBroadcastGradientArgs, DynamicBroadcastGradientArgsCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
