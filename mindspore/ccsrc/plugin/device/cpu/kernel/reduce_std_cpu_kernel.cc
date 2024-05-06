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
#include "plugin/device/cpu/kernel/reduce_std_cpu_kernel.h"
#include <memory>
#include <algorithm>
#include <utility>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "nnacl/fp32/reduce_fp32.h"
#include "mindspore/core/ops/auto_generate/gen_ops_primitive.h"

namespace mindspore {
namespace kernel {
constexpr size_t kReduceStdInputsNum = 1;
constexpr size_t kReduceStdOutputsNum = 2;
constexpr size_t kReduceSmallVectorSize = 200000;
constexpr int kPowExp = 2;
bool ReduceStdCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  if (kernel_name_ != prim::kPrimReduceStd->name()) {
    MS_LOG(ERROR) << "For 'ReduceStd', the kernel name must be 'ReduceStd', but got " << kernel_name_;
    return false;
  }
  if (inputs.empty() || outputs.empty()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it got empty inputs or outputs, which is invalid.";
    return false;
  }
  dtype_ = inputs[0]->dtype_id();
  if (dtype_ != kNumberTypeFloat16 && dtype_ != kNumberTypeFloat32) {
    MS_EXCEPTION(TypeError) << "For '" << kernel_name_ << "', input dtype only support float16 and float32, but got ["
                            << dtype_ << "].";
  }
  return true;
}

int ReduceStdCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  input_shape_ = inputs.at(kIndex0)->GetShapeVector();
  int64_t dimension = SizeToLong(input_shape_.size());

  axis_ = inputs[kIndex1]->GetValueWithCheck<std::vector<int64_t>>();
  (void)std::for_each(axis_.begin(), axis_.end(), [dimension](auto &a) {
    if (a < -dimension || a >= dimension) {
      MS_LOG(EXCEPTION) << "For reduce std, the each axis element should be in [" << -dimension << ", " << dimension
                        << "), but got " << a;
    }
    a = a < 0 ? dimension + a : a;
  });
  sort(axis_.begin(), axis_.end());
  auto last = std::unique(axis_.begin(), axis_.end());
  (void)axis_.erase(last, axis_.end());
  unbiased_ = inputs[kIndex2]->GetValueWithCheck<bool>();
  return KRET_OK;
}

template <typename T>
void ReduceStdCpuKernelMod::RunReduceStd(const std::vector<kernel::KernelTensor *> &inputs,
                                         const std::vector<kernel::KernelTensor *> &outputs) {
  size_t input_size = inputs[0]->size() / sizeof(T);
  if (input_size > kReduceSmallVectorSize) {
    MS_LOG(EXCEPTION) << "For reduce std, the input size should be < " << kReduceSmallVectorSize;
  }
  T *input_addr = reinterpret_cast<T *>(inputs[0]->device_ptr());
  T *output_std_addr = reinterpret_cast<T *>(outputs[0]->device_ptr());
  T *output_mean_addr = reinterpret_cast<T *>(outputs[1]->device_ptr());
  float mean = 0.0;
  for (size_t i = 0; i < input_size; ++i) {
    mean += static_cast<float>(input_addr[i]);
  }
  mean = mean / SizeToFloat(input_size);
  *output_mean_addr = static_cast<T>(mean);
  float deviation = 0.0;
  for (size_t i = 0; i < input_size; ++i) {
    deviation += std::pow(static_cast<float>(input_addr[i]) - mean, kPowExp);
  }
  float length = unbiased_ ? (input_size - 1) : input_size;
  deviation = std::sqrt(deviation / length);
  *output_std_addr = static_cast<T>(deviation);
}

template <typename T>
void ReduceStdCpuKernelMod::RunReduceStdWithSAxis(const std::vector<kernel::KernelTensor *> &inputs,
                                                  const std::vector<kernel::KernelTensor *> &outputs) {
  T *input_addr = reinterpret_cast<T *>(inputs[0]->device_ptr());
  T *output_std_addr = reinterpret_cast<T *>(outputs[0]->device_ptr());
  T *output_mean_addr = reinterpret_cast<T *>(outputs[1]->device_ptr());
  size_t dimension = input_shape_.size();
  size_t stride = 1;
  std::vector<size_t> axes(input_shape_.size());
  size_t j = 0;
  size_t k = 0;
  for (size_t i = 0; i < dimension; ++i) {
    if (j == axis_.size() || i != LongToSize(axis_[j])) {
      axes[k] = i;
      ++k;
    } else {
      stride *= LongToSize(input_shape_[i]);
      ++j;
    }
  }
  for (auto &it : axis_) {
    axes[k] = LongToSize(it);
    ++k;
  }
  size_t output_size = outputs[0]->size() / sizeof(T);
  std::vector<int64_t> transpose_shape(input_shape_.size());
  for (size_t i = 0; i < dimension; ++i) {
    transpose_shape[i] = input_shape_[axes[i]];
  }
  TransposeIterator base_iter(std::move(transpose_shape), std::move(axes), input_shape_);
  auto task = [this, &base_iter, input_addr, output_mean_addr, output_std_addr, stride](size_t start, size_t end) {
    auto iter = base_iter;
    iter.SetPos(start * stride);
    for (size_t i = start; i < end; ++i) {
      std::vector<float> src_data(stride);
      for (size_t j = 0; j < stride; ++j) {
        src_data[j] = static_cast<float>(input_addr[iter.GetPos()]);
        iter.GenNextPos();
      }
      float mean = 0.0f;
      ReduceMeanWithAxis(src_data.data(), &mean, stride);
      output_mean_addr[i] = static_cast<T>(mean);

      float deviation = 0.0f;
      float size = unbiased_ ? static_cast<float>(stride - 1) : static_cast<float>(stride);
      ReduceDeviation(src_data.data(), stride, mean, &deviation);
      deviation = std::sqrt(deviation / SizeToFloat(size));
      output_std_addr[i] = static_cast<T>(deviation);
    }
  };
  ParallelLaunchAutoSearch(task, output_size, this, &parallel_search_info_);
}

bool ReduceStdCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                   const std::vector<kernel::KernelTensor *> &,
                                   const std::vector<kernel::KernelTensor *> &outputs) {
  if (axis_.empty() || input_shape_.empty() || input_shape_.size() == 1) {
    if (dtype_ == kNumberTypeFloat16) {
      RunReduceStd<float16>(inputs, outputs);
    } else if (dtype_ == kNumberTypeFloat32) {
      RunReduceStd<float>(inputs, outputs);
    }
  } else {
    if (dtype_ == kNumberTypeFloat16) {
      RunReduceStdWithSAxis<float16>(inputs, outputs);
    } else if (dtype_ == kNumberTypeFloat32) {
      RunReduceStdWithSAxis<float>(inputs, outputs);
    }
  }
  return true;
}

std::vector<KernelAttr> ReduceStdCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list = {KernelAttr()
                                            .AddInputAttr(kNumberTypeFloat16)
                                            .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)  // axis
                                            .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)  // unbiased
                                            .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)  // keep_dims
                                            .AddOutputAttr(kNumberTypeFloat16)
                                            .AddOutputAttr(kNumberTypeFloat16),
                                          KernelAttr()
                                            .AddInputAttr(kNumberTypeFloat32)
                                            .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                            .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
                                            .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
                                            .AddOutputAttr(kNumberTypeFloat32)
                                            .AddOutputAttr(kNumberTypeFloat32)};
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, ReduceStd, ReduceStdCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
