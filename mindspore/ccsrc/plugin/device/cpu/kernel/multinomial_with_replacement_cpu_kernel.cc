/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/multinomial_with_replacement_cpu_kernel.h"

#include <Eigen/Dense>
#include <algorithm>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <functional>
#include <random>

#include "mindspore/core/ops/multinomial_with_replacement.h"
#include "kernel/common_utils.h"
#include "utils/ms_utils.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "plugin/device/cpu/kernel/cpu_kernel.h"

namespace mindspore {
namespace kernel {
namespace {
const size_t kMultinomialWithReplacementInputsNum = 3;
const size_t kMultinomialWithReplacementOutputsNum = 1;
}  // namespace

uint64_t MultinomialWithReplacementCpuKernelMod::New64() const {
  std::random_device device("/dev/urandom");
  static std::mt19937_64 rng = std::mt19937_64(device());
  return (rng)();
}

void MultinomialWithReplacementCpuKernelMod::InitPhiloxRandom(int64_t seed, int64_t offset) {
  if (seed == 0 && offset == 0) {
    seed = static_cast<int64_t>(New64());
    offset = static_cast<int64_t>(New64());
  }
  generator_ = random::PhiloxRandom(seed, offset);
}

float MultinomialWithReplacementCpuKernelMod::RandFloat() {
  uint32_t x = GenerateSingle();
  const uint32_t man = x & 0x7fffffu;  // 23 bit mantissa
  const uint32_t exp = static_cast<uint32_t>(127);
  const uint32_t val = (exp << 23) | man;

  float result;
  int ret = memcpy_s(&result, sizeof(result), &val, sizeof(val));
  if (ret != EOK) {
    MS_LOG(EXCEPTION) << "The memcpy error, errorno (" << ret << ")";
  }
  return result - 1.0f;
}

uint32_t MultinomialWithReplacementCpuKernelMod::GenerateSingle() {
  if (used_result_index_ == random::PhiloxRandom::kResultElementCount) {
    unused_results_ = generator_();
    used_result_index_ = 0;
  }
  return unused_results_[used_result_index_++];
}

bool MultinomialWithReplacementCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                                  const std::vector<KernelTensor *> &outputs) {
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "MultinomialWithReplacement does not support this kernel data type: " << kernel_attr;
    return false;
  }
  numsamples_ = GetValue<int64_t>(primitive_->GetAttr("numsamples"));
  replacement_ = GetValue<bool>(primitive_->GetAttr("replacement"));
  kernel_func_ = func_list_[index].second;
  return true;
}

int MultinomialWithReplacementCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                                   const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  x_shape_ = inputs[0]->GetShapeVector();
  return KRET_OK;
}

template <typename T>
bool MultinomialWithReplacementCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                                          const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kMultinomialWithReplacementInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kMultinomialWithReplacementOutputsNum, kernel_name_);

  if (numsamples_ <= 0) {
    MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', 'numsamples' should be a nonnegative number, but got "
                             << numsamples_ << ".";
  }
  auto x = reinterpret_cast<T *>(inputs[0]->device_ptr());
  auto seed = *reinterpret_cast<int64_t *>(inputs[1]->device_ptr());
  auto offset = *reinterpret_cast<int64_t *>(inputs[2]->device_ptr());
  if (init_state_) {
    init_seed_ = seed;
    init_offset_ = offset;
    InitPhiloxRandom(init_seed_, init_offset_);
    init_state_ = false;
  } else if (!init_state_ && (seed != init_seed_ || offset != init_offset_)) {
    init_seed_ = seed;
    init_offset_ = offset;
    InitPhiloxRandom(init_seed_, init_offset_);
  }

  int64_t num_row_ = 1;
  size_t num_shape = 2;
  if (x_shape_.size() == num_shape) {
    num_row_ = x_shape_[0];
  }
  int64_t num_col_ = x_shape_[x_shape_.size() - 1];

  for (int i = 0; i < num_row_; i++) {
    double sum = 0;
    auto row_start = x + i * num_col_;
    for (int64_t j = 0; j < num_col_; ++j) {
      if (static_cast<double>(*(row_start + j)) < 0) {
        MS_EXCEPTION(ValueError) << "For '" << kernel_name_
                                 << "' , each element of 'x' must be equal or greater than 0. ";
      }
      sum += static_cast<double>(*(row_start + j));
    }
    if (sum <= 0) {
      MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "' , the sum of each row of 'x' must be greater than 0. ";
    }
  }

  int64_t output_size = num_row_ * numsamples_;
  std::vector<T> RandomData(output_size);
  for (int64_t i = 0; i < output_size; i++) {
    RandomData[i] = static_cast<T>(RandFloat());
  }
  auto y = reinterpret_cast<int64_t *>(outputs[0]->device_ptr());
  for (int64_t i = 0; i < num_row_; i++) {
    if (replacement_ == true) {
      auto out = y + i * numsamples_;
      auto in = x + i * num_col_;
      out = TrueCompute<T>(in, out, RandomData.data(), i, num_col_);
    } else {
      auto out = y + i * numsamples_;
      auto in = x + i * num_col_;
      out = FalseCompute<T>(in, out, RandomData.data(), i, num_col_);
    }
  }

  return true;
}

template <typename T>
int64_t *MultinomialWithReplacementCpuKernelMod::TrueCompute(T *in, int64_t *out, T *RandomData, int64_t i,
                                                             int64_t num_col_) const {
  double *cumulative_distribution_function = new double[num_col_];
  double running_total = 0;
  auto random = RandomData + i * numsamples_;
  for (int64_t j = 0; j < num_col_; ++j) {
    *(cumulative_distribution_function + j) = static_cast<double>(*(in + j));
  }
  for (int64_t j = 0; j < num_col_; ++j) {
    if (static_cast<double>(*(cumulative_distribution_function + j)) != static_cast<double>(0.0)) {
      running_total += *(cumulative_distribution_function + j);
      *(cumulative_distribution_function + j) = running_total;
    }
  }
  for (int64_t j = 0; j < numsamples_; j++) {
    double rand = static_cast<double>(*(random + j));
    double rr = rand * running_total;
    auto rt = running_total;
    double *temp = &rt;
    for (int k = 0; k < num_col_; k++) {
      if (*(cumulative_distribution_function + k) >= rr && *(cumulative_distribution_function + k) <= *temp) {
        *temp = *(cumulative_distribution_function + k);
      }
    }
    for (int k = 0; k < num_col_; k++) {
      if (static_cast<double>(*temp) == static_cast<double>(*(cumulative_distribution_function + k))) {
        *out = static_cast<int64_t>(k);
      }
    }
    out = out + 1;
  }
  return out;
}

template <typename T>
int64_t *MultinomialWithReplacementCpuKernelMod::FalseCompute(T *in, int64_t *out, T *RandomData, int64_t i,
                                                              int64_t num_col_) const {
  double *cumulative_distribution_function = new double[num_col_];
  T *weight = new T[num_col_];
  int64_t zero_num = 0;
  int64_t *zero_data = new int64_t[num_col_];
  double running_total = 0;
  auto random = RandomData + i * numsamples_;
  std::copy_n(in, num_col_, weight);
  for (int64_t j = 0; j < num_col_; ++j) {
    *(cumulative_distribution_function + j) = static_cast<double>(*(in + j));
  }
  for (int64_t j = 0; j < num_col_; ++j) {
    if (static_cast<double>(*(cumulative_distribution_function + j)) != static_cast<double>(0.0)) {
      running_total += *(cumulative_distribution_function + j);
      *(cumulative_distribution_function + j) = running_total;
    } else {
      *(zero_data + zero_num) = static_cast<int64_t>(j);
      zero_num = zero_num + 1;
    }
  }
  for (int j = 0; j < numsamples_; j++) {
    double rand = static_cast<double>(*(random + j));
    double rr = rand * running_total;
    auto rt = running_total;
    double *temp = &rt;
    if (j < num_col_ - zero_num) {
      for (int k = 0; k < num_col_; k++) {
        if (*(cumulative_distribution_function + k) >= rr && *(cumulative_distribution_function + k) <= *temp) {
          *temp = *(cumulative_distribution_function + k);
        }
      }
      for (int k = 0; k < num_col_; k++) {
        if (static_cast<double>(*temp) == static_cast<double>(*(cumulative_distribution_function + k))) {
          *out = static_cast<int64_t>(k);
        }
      }
      int co = *out;
      *(weight + co) = static_cast<T>(0.0);
      running_total = 0.0;
      for (int64_t t = 0; t < num_col_; t++) {
        *(cumulative_distribution_function + t) = static_cast<double>(*(weight + t));
      }
      for (int64_t t = 0; t < num_col_; t++) {
        if (static_cast<double>(*(cumulative_distribution_function + t)) != static_cast<double>(0.0)) {
          running_total += *(cumulative_distribution_function + t);
          *(cumulative_distribution_function + t) = running_total;
        }
      }
      out = out + 1;
    } else {
      *out = *(zero_data + j - num_col_ + zero_num);
      out = out + 1;
    }
  }
  return out;
}

std::vector<std::pair<KernelAttr, MultinomialWithReplacementCpuKernelMod::MultinomialWithReplacementFunc>>
  MultinomialWithReplacementCpuKernelMod::func_list_ = {
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat16)
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64),
     &MultinomialWithReplacementCpuKernelMod::LaunchKernel<float16>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64),
     &MultinomialWithReplacementCpuKernelMod::LaunchKernel<float>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat64)
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64),
     &MultinomialWithReplacementCpuKernelMod::LaunchKernel<double>}};

std::vector<KernelAttr> MultinomialWithReplacementCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, MultinomialWithReplacementFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, MultinomialWithReplacement, MultinomialWithReplacementCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
