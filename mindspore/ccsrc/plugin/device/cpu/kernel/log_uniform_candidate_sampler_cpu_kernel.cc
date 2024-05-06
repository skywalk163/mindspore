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

#include "plugin/device/cpu/kernel/log_uniform_candidate_sampler_cpu_kernel.h"
#include <cmath>
#include <map>
#include <utility>
#include <algorithm>
#include <unordered_set>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "mindspore/core/ops/log_uniform_candidate_sampler.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace kernel {
bool LogUniformCandidateSamplerCpuKernel::Init(const std::vector<KernelTensor *> &inputs,
                                               const std::vector<KernelTensor *> &outputs) {
  num_true_ = GetValue<int64_t>(primitive_->GetAttr(ops::kNumTrue));
  num_sampled_ = GetValue<int64_t>(primitive_->GetAttr(ops::kNumSampled));
  unique_ = GetValue<bool>(primitive_->GetAttr(ops::kUnique));
  seed_ = GetValue<int64_t>(primitive_->GetAttr(ops::kSeed));
  range_max_ = GetValue<int64_t>(primitive_->GetAttr(ops::kRangeMax));
  constexpr int32_t ZERO = 0;
  if (range_max_ == ZERO) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the 'range_max' must be greater than 0, but got range_max=" << range_max_
                  << ". 'Range_max' integer overflow causes this error.";
    return false;
  }
  this->log_range_ = log1p(range_max_);
  if (unique_ && range_max_ < num_sampled_) {
    MS_LOG(ERROR) << "When unique is True, range_max must be greater than or equal to num_sampled";
    return false;
  }
  int64_t seed = 87654321;
  int64_t seed2 = seed_;
  generator_.Init(seed, seed2);
  reserveSamplesNr_ = 2048 * num_sampled_;
  return true;
}

static float CalcExpectedCount(float p, int num_sampled, int num_tries) {
  if (num_tries == num_sampled) {
    return p * num_sampled;
  }
  return -std::expm1(num_tries * std::log1p(-p));
}

float LogUniformCandidateSamplerCpuKernel::Probability(int64_t value) const {
  return (log((value + 2.0) / (value + 1.0))) / log_range_;
}

int64_t LogUniformCandidateSamplerCpuKernel::Sample(random::SinglePhiloxRandom *single) const {
  double d = single->GenDouble();
  int64_t val = static_cast<int64_t>(exp(d * log_range_)) - 1;
  return val % range_max_;
}

int LogUniformCandidateSamplerCpuKernel::Resize(const std::vector<KernelTensor *> &inputs,
                                                const std::vector<KernelTensor *> &outputs) {
  int ret = KRET_OK;
  if ((ret = NativeCpuKernelMod::Resize(inputs, outputs)) != 0) {
    return ret;
  }
  auto true_classes_shape = inputs[0]->GetShapeVector();
  if (true_classes_shape[1] != num_true_) {
    MS_LOG(ERROR) << "input true_classes dim[1] should equal to num_true, true_classes.dim[1] = "
                  << true_classes_shape[1] << ", num_true = " << num_true_;
    return KRET_RESIZE_FAILED;
  }

  auto sampled_candidates_shape = outputs[0]->GetShapeVector();
  if (sampled_candidates_shape.size() != 1 || sampled_candidates_shape[0] != static_cast<int64_t>(num_sampled_)) {
    MS_LOG(ERROR) << "output sampled_candidates shape should equal to (num_sampled, ), sampled_candidates shape = "
                  << VectorToString(sampled_candidates_shape) << ", num_sampled_ = " << num_sampled_;
    return KRET_RESIZE_FAILED;
  }

  auto true_expected_count_shape = outputs[1]->GetShapeVector();
  if (true_expected_count_shape != true_classes_shape) {
    MS_LOG(ERROR)
      << "output true_expected_count shape should be same with true_classes shape, true_expected_count shape = "
      << VectorToString(true_expected_count_shape) << ", true_classes shape = " << VectorToString(true_classes_shape);
    return KRET_RESIZE_FAILED;
  }

  auto sampled_expected_count_shape = outputs[2]->GetShapeVector();
  if (sampled_expected_count_shape.size() != 1 ||
      sampled_expected_count_shape[0] != static_cast<int64_t>(num_sampled_)) {
    MS_LOG(ERROR)
      << "output sampled_expected_count shape shape should equal to (num_sampled, ), sampled_expected_count shape = "
      << VectorToString(sampled_expected_count_shape) << ", num_sampled_ = " << num_sampled_;
    return KRET_RESIZE_FAILED;
  }
  return ret;
}
bool LogUniformCandidateSamplerCpuKernel::Launch(const std::vector<KernelTensor *> &inputs,
                                                 const std::vector<KernelTensor *> &workspace,
                                                 const std::vector<KernelTensor *> &outputs) {
  int64_t *true_classes = static_cast<int64_t *>(inputs.at(0)->device_ptr());
  auto true_classes_size = inputs.at(0)->size();
  size_t true_classes_len = static_cast<size_t>(true_classes_size / sizeof(int64_t));
  int64_t *sampled_candidates = static_cast<int64_t *>(outputs.at(0)->device_ptr());
  float *true_expected_count = static_cast<float *>(outputs.at(1)->device_ptr());
  float *sampled_expected_count = static_cast<float *>(outputs.at(2)->device_ptr());

  auto gen = generator_.ReserveSamples32(reserveSamplesNr_);

  random::SinglePhiloxRandom single(&gen);

  int num_tries = 0;
  if (unique_) {
    std::unordered_set<int64_t> used(num_sampled_);
    int32_t idx = 0;
    while (idx < num_sampled_) {
      num_tries++;
      int64_t value = Sample(&single);
      if (used.find(value) == used.end()) {
        sampled_candidates[idx++] = value;
        used.emplace(value);
      }
    }
  } else {
    for (int32_t idx = 0; idx < num_sampled_; idx++) {
      sampled_candidates[idx] = Sample(&single);
    }
    num_tries = num_sampled_;
  }

  for (int32_t i = 0; i < num_sampled_; i++) {
    sampled_expected_count[i] = CalcExpectedCount(Probability(sampled_candidates[i]), num_sampled_, num_tries);
  }

  for (size_t i = 0; i < true_classes_len; i++) {
    true_expected_count[i] = CalcExpectedCount(Probability(true_classes[i]), num_sampled_, num_tries);
  }
  return true;
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, LogUniformCandidateSampler, LogUniformCandidateSamplerCpuKernel);
}  // namespace kernel
}  // namespace mindspore
