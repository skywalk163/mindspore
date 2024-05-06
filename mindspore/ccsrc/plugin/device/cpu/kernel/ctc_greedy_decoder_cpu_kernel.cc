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

#include "plugin/device/cpu/kernel/ctc_greedy_decoder_cpu_kernel.h"

#include <algorithm>
#include <utility>
#include <map>
#include <set>
#include <numeric>
#include <iostream>

#include "mindspore/core/ops/ctc_greedy_decoder.h"
#include "utils/profile.h"
#include "runtime/graph_scheduler/actor/actor_common.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kInputNum = 2;
constexpr size_t kOutputNum = 4;
constexpr size_t kDecodedIndicesRank = 2;

template <typename T>
T RowMax(const T *m, int r, int *c, int dimension) {
  *c = 0;
  T p = *(m + r * dimension);
  for (int i = 1; i < dimension; ++i) {
    if (*(m + r * dimension + i) > p) {
      p = *(m + r * dimension + i);
      *c = i;
    }
  }
  return p;
}
}  // namespace

bool CTCGreedyDecoderCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &outputs) {
  if (inputs.empty() || outputs.empty()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it got empty inputs or outputs, which is invalid.";
    return false;
  }

  // Getting values
  merge_repeated_ = GetValue<bool>(primitive_->GetAttr(ops::kMergeRepeated));

  max_time_ = inputs[kIndex0]->GetShapeVector()[kIndex0];
  batch_size_ = inputs[kIndex0]->GetShapeVector()[kIndex1];
  num_classes_raw_ = inputs[kIndex0]->GetShapeVector()[kIndex2];

  if (!MatchKernelFunc(kernel_name_, inputs, outputs)) {
    return false;
  }

  for (size_t i = 0; i < kOutputNum - 1; i++) {
    (void)types_.emplace_back(TypeId::kNumberTypeInt64);
  }
  (void)types_.emplace_back(inputs[0]->dtype_id());

  return true;
}

int CTCGreedyDecoderCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                         const std::vector<KernelTensor *> &outputs) {
  auto ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }
  max_time_ = inputs[kIndex0]->GetShapeVector()[kIndex0];
  batch_size_ = inputs[kIndex0]->GetShapeVector()[kIndex1];
  num_classes_raw_ = inputs[kIndex0]->GetShapeVector()[kIndex2];
  return KRET_OK;
}

template <typename T>
bool CTCGreedyDecoderCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                                const std::vector<KernelTensor *> &,
                                                const std::vector<kernel::KernelTensor *> &outputs) {
  if (inputs.size() != kInputNum) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of inputs should be " << kInputNum << ", but got "
                      << inputs.size() << " input(s).";
  }
  if (outputs.size() != kOutputNum) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of outputs should be " << kOutputNum << ", but got "
                      << outputs.size() << " output(s).";
  }
  auto inputs_x = reinterpret_cast<T *>(inputs[kIndex0]->device_ptr());
  auto sequence_length = reinterpret_cast<int32_t *>(inputs[kIndex1]->device_ptr());

  auto decoded_indices = reinterpret_cast<int64_t *>(outputs[kIndex0]->device_ptr());
  auto decoded_values = reinterpret_cast<int64_t *>(outputs[kIndex1]->device_ptr());
  auto decoded_shape = reinterpret_cast<int64_t *>(outputs[kIndex2]->device_ptr());
  auto log_probability = reinterpret_cast<T *>(outputs[kIndex3]->device_ptr());

  int ret = memset_s(log_probability, sizeof(T) * batch_size_, 0, sizeof(T) * batch_size_);
  if (ret != EOK) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', memset_s error. Error no: " << ret;
  }

  const int64_t max_time = max_time_;
  const int64_t batch_size = batch_size_;
  const int64_t num_classes_raw = num_classes_raw_;
  const bool merge_repeated = merge_repeated_;
  const int num_classes = static_cast<const int>(num_classes_raw);
  int blank_index = num_classes - 1;
  std::vector<std::vector<std::vector<int>>> sequences(batch_size);

  for (auto b = 0; b < batch_size; ++b) {
    if (!(sequence_length[b] <= max_time)) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', sequence_length[" << b << "] " << sequence_length[b]
                        << " should be less than " << max_time << ".";
    }
  }

  auto task = [this, &sequences, &log_probability, &inputs_x, &sequence_length, &batch_size, &num_classes, &blank_index,
               &merge_repeated](size_t start, size_t end) {
    for (auto b = start; b < end; ++b) {
      sequences[b].resize(1);
      auto &sequence = sequences[b][0];
      int prev_indices = -1;
      for (auto t = 0; t < sequence_length[b]; ++t) {
        int max_class_indices;
        log_probability[b] += -RowMax(inputs_x + t * batch_size * num_classes, b, &max_class_indices, num_classes);
        if (max_class_indices != blank_index && !(merge_repeated && max_class_indices == prev_indices)) {
          sequence.push_back(max_class_indices);
        }
        prev_indices = max_class_indices;
      }
    }
  };

  ParallelLaunchAutoSearch(task, batch_size, this, &parallel_search_info_);

  std::vector<int64_t> num_entries(1, 0);
  // Calculate num_entries per path
  for (const auto &batch_s : sequences) {
    if (batch_s.size() != 1) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', batch_s.size() must be 1.";
    }
    num_entries[0] += batch_s[0].size();
  }

  const int64_t p_num = num_entries[0];
  int64_t max_decoded = 0;
  int64_t offset = 0;

  for (int64_t b = 0; b < batch_size; ++b) {
    auto &p_batch = sequences[b][0];
    int64_t num_decoded = p_batch.size();
    max_decoded = std::max(max_decoded, num_decoded);
    std::copy_n(p_batch.begin(), num_decoded, decoded_values + offset);
    for (int64_t t = 0; t < num_decoded; ++t, ++offset) {
      *(decoded_indices + offset * kDecodedIndicesRank) = b;
      *(decoded_indices + offset * kDecodedIndicesRank + 1) = t;
    }
  }
  *(decoded_shape + 0) = batch_size;
  *(decoded_shape + 1) = max_decoded;

  const int64_t decoded_shape_size = 2;
  std::vector<int64_t> decoded_indices_shape = {p_num, 2};
  std::vector<int64_t> decoded_values_shape = {p_num};
  std::vector<int64_t> decoded_shape_shape = {2};
  std::vector<int64_t> log_probability_shape = {batch_size, 1};
  outputs[kIndex0]->SetShapeVector(decoded_indices_shape);
  outputs[kIndex0]->set_size(p_num * decoded_shape_size * UnitSizeInBytes(outputs[kIndex0]->dtype_id()));
  outputs[kIndex1]->SetShapeVector(decoded_values_shape);
  outputs[kIndex1]->set_size(p_num * UnitSizeInBytes(outputs[kIndex1]->dtype_id()));
  outputs[kIndex2]->SetShapeVector(decoded_shape_shape);
  outputs[kIndex2]->set_size(decoded_shape_size * UnitSizeInBytes(outputs[kIndex2]->dtype_id()));
  outputs[kIndex3]->SetShapeVector(log_probability_shape);
  outputs[kIndex3]->set_size(batch_size * UnitSizeInBytes(outputs[kIndex3]->dtype_id()));

  return true;
}

using ctcGreedyDecoderPair = std::pair<KernelAttr, CTCGreedyDecoderCpuKernelMod::KernelRunFunc>;
const std::vector<ctcGreedyDecoderPair> &CTCGreedyDecoderCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, CTCGreedyDecoderCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeFloat32),
     &CTCGreedyDecoderCpuKernelMod::LaunchKernel<float>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat64)
       .AddInputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeFloat64),
     &CTCGreedyDecoderCpuKernelMod::LaunchKernel<double>},
  };
  return func_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, CTCGreedyDecoder, CTCGreedyDecoderCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
