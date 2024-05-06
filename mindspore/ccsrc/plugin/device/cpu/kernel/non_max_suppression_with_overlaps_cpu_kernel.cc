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
#include "plugin/device/cpu/kernel/non_max_suppression_with_overlaps_cpu_kernel.h"
#include <algorithm>
#include <deque>
#include <queue>
#include "Eigen/Core"
#include "kernel/common_utils.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "unsupported/Eigen/CXX11/Tensor"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kNonMaxSuppressionWithOverlapsInputsNum = 5;
constexpr size_t kNonMaxSuppressionWithOverlapsOutputsNum = 1;
constexpr size_t kOverlapsRank = 2;
}  // namespace

bool NonMaxSuppressionWithOverlapsCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                                     const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kNonMaxSuppressionWithOverlapsInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kNonMaxSuppressionWithOverlapsOutputsNum, kernel_name_);
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto is_match = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match.first) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }
  return true;
}

int NonMaxSuppressionWithOverlapsCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                                      const std::vector<KernelTensor *> &outputs) {
  auto ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_UNKNOWN_OUT_SHAPE && ret != KRET_OK) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', resize failed, ret: " << ret;
    return ret;
  }

  auto overlaps_shape = inputs[kIndex0]->GetDeviceShapeVector();
  num_boxes_ = LongToInt(overlaps_shape[0]);
  return KRET_OK;
}

bool NonMaxSuppressionWithOverlapsCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                                       const std::vector<kernel::KernelTensor *> &,
                                                       const std::vector<kernel::KernelTensor *> &outputs) {
  Eigen::TensorMap<Eigen::Tensor<float, kOverlapsRank, Eigen::RowMajor>> overlaps_map(
    reinterpret_cast<float *>(inputs[0]->device_ptr()), num_boxes_, num_boxes_);
  std::vector<float> scores_data(num_boxes_);
  (void)std::copy_n(reinterpret_cast<float *>(inputs[1]->device_ptr()), num_boxes_, scores_data.begin());
  auto max_output_size = *reinterpret_cast<int32_t *>(inputs[2]->device_ptr());
  if (max_output_size < 0) {
    MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', the input max_output_size must be non-negative.";
  }
  auto overlap_threshold = *reinterpret_cast<float *>(inputs[3]->device_ptr());
  auto score_threshold = *reinterpret_cast<float *>(inputs[4]->device_ptr());
  std::unique_ptr<int32_t[]> indices_data = std::make_unique<int32_t[]>(static_cast<size_t>(max_output_size));
  if (indices_data == nullptr) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', new indices_data failed.";
  }

  struct Candidate {
    int32_t box_index;
    float score;
    int32_t suppress_begin_index;
  };
  auto cmp = [](const Candidate boxes_i, const Candidate boxes_j) { return boxes_i.score < boxes_j.score; };
  std::priority_queue<Candidate, std::deque<Candidate>, decltype(cmp)> candidate_priority_queue(cmp);
  for (size_t i = 0; i < scores_data.size(); ++i) {
    if (scores_data[i] > score_threshold) {
      candidate_priority_queue.emplace(Candidate({static_cast<int32_t>(i), scores_data[i]}));
    }
  }
  float similarity = static_cast<float>(0.0);
  Candidate next_candidate;
  next_candidate.box_index = 0;
  next_candidate.score = static_cast<float>(0.0);
  next_candidate.suppress_begin_index = 0;
  int32_t cnt = 0;
  while (cnt < max_output_size && !candidate_priority_queue.empty()) {
    next_candidate = candidate_priority_queue.top();
    candidate_priority_queue.pop();
    bool should_suppress = false;
    for (int32_t j = cnt - 1; j >= next_candidate.suppress_begin_index; --j) {
      similarity = overlaps_map(next_candidate.box_index, indices_data[IntToSize(j)]);
      if (similarity >= overlap_threshold) {
        should_suppress = true;
        break;
      }
    }
    next_candidate.suppress_begin_index = cnt;
    if (!should_suppress) {
      indices_data[IntToSize(cnt)] = next_candidate.box_index;
      cnt += 1;
    }
  }
  auto value = reinterpret_cast<int32_t *>(outputs[0]->device_ptr());
  real_output_size_ = std::min(cnt, max_output_size);
  for (int32_t j = 0; j < real_output_size_; ++j) {
    *(value + j) = indices_data[IntToSize(j)];
  }
  return true;
}

void NonMaxSuppressionWithOverlapsCpuKernelMod::UpdateOutputShapeAndSize(const std::vector<KernelTensor *> &inputs,
                                                                         const std::vector<KernelTensor *> &outputs) {
  std::vector<int64_t> new_output_shape = {real_output_size_};
  outputs[kIndex0]->SetShapeVector(new_output_shape);
  outputs[kIndex0]->set_size(real_output_size_ * UnitSizeInBytes(outputs[kIndex0]->dtype_id()));
}

std::vector<KernelAttr> NonMaxSuppressionWithOverlapsCpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> kernel_attr_list = {{KernelAttr()
                                                        .AddInputAttr(kNumberTypeFloat32)
                                                        .AddInputAttr(kNumberTypeFloat32)
                                                        .AddInputAttr(kNumberTypeInt32)
                                                        .AddInputAttr(kNumberTypeFloat32)
                                                        .AddInputAttr(kNumberTypeFloat32)
                                                        .AddOutputAttr(kNumberTypeInt32)}};
  return kernel_attr_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, NonMaxSuppressionWithOverlaps, NonMaxSuppressionWithOverlapsCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
