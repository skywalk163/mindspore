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
#include "plugin/device/ascend/hal/hccl_adapter/all_to_all_v_calc_param.h"
#include <map>
#include <string>
#include <memory>
#include <utility>

#include "include/backend/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "abstract/utils.h"
#include "runtime/device/memory_manager.h"
#include "include/common/utils/utils.h"
#include "ir/anf.h"
#include "ir/value.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore::hccl {
namespace {
bool IsInTheOrder(const std::vector<int64_t> &vec) {
  for (size_t i = 1; i < vec.size(); ++i) {
    if (vec[i] <= vec[i - 1]) {
      return false;
    }
  }

  return true;
}
}  // namespace
AllToAllvCalcParam::AllToAllvCalcParam(const PrimitivePtr &prim, uint32_t rank_size)
    : prim_(prim),
      rank_size_(rank_size),
      send_counts_(rank_size, 0),
      sdispls_(rank_size, 0),
      recv_counts_(rank_size, 0),
      rdispls_(rank_size, 0) {}

void AllToAllvCalcParam::CalcOpParam(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  size_t input_num = inputs.size();
  // ignore send empty input
  if (prim_->HasAttr(kAttrNeedDropInput)) {
    bool need_drop_input = GetValue<bool>(prim_->GetAttr(kAttrNeedDropInput));
    if (need_drop_input) {
      input_num = 0;
    }
  }
  size_t output_num = outputs.size();
  std::vector<size_t> input_aligned_mem_size(input_num);
  std::vector<size_t> output_aligned_mem_size(output_num);
  std::vector<size_t> input_real_mem_size(input_num);
  std::vector<size_t> output_real_mem_size(output_num);
  for (size_t i = 0; i < input_num; ++i) {
    auto ms_shape = inputs[i]->GetShapeVector();
    auto type_size = abstract::TypeIdSize(inputs[i]->dtype_id());
    if (type_size == 0) {
      MS_LOG(INTERNAL_EXCEPTION) << "Invalid type_size 0 of node: " << prim_->name();
    }
    size_t origin_mem_size = type_size * SizeOf(ms_shape);
    size_t aligned_mem_size = device::MemoryManager::GetCommonAlignSize(origin_mem_size);
    input_aligned_mem_size[i] = aligned_mem_size / type_size;
    input_real_mem_size[i] = origin_mem_size / type_size;
  }
  for (size_t i = 0; i < output_num; ++i) {
    auto ms_shape = outputs[i]->GetShapeVector();
    auto type_size = abstract::TypeIdSize(outputs[i]->dtype_id());
    if (type_size == 0) {
      MS_LOG(INTERNAL_EXCEPTION) << "Invalid type_size 0 of node: " << prim_->name();
    }
    size_t origin_mem_size = type_size * SizeOf(ms_shape);
    size_t aligned_mem_size = device::MemoryManager::GetCommonAlignSize(origin_mem_size);
    output_aligned_mem_size[i] = aligned_mem_size / type_size;
    output_real_mem_size[i] = origin_mem_size / type_size;
  }
  CalcMemOffset(input_aligned_mem_size, input_real_mem_size, kAttrSendRankIds, &send_counts_, &sdispls_);
  CalcMemOffset(output_aligned_mem_size, output_real_mem_size, kAttrRecvRankIds, &recv_counts_, &rdispls_);
}

void AllToAllvCalcParam::CalcMemOffset(const std::vector<size_t> &mem_sizes, const std::vector<size_t> &real_sizes,
                                       const std::string &rank_ids_attr, std::vector<int64_t> *counts,
                                       std::vector<int64_t> *displs) {
  auto rank_ids = GetValue<std::vector<int64_t>>(prim_->GetAttr(rank_ids_attr));
  if (mem_sizes.size() != rank_ids.size() || real_sizes.size() != rank_ids.size()) {
    MS_LOG(INTERNAL_EXCEPTION) << "Invalid addr num " << mem_sizes.size() << " and " << real_sizes.size()
                               << " must be equal to rank ids size " << rank_ids.size();
  }

  if (!IsInTheOrder(rank_ids)) {
    std::vector<size_t> mem_offset(mem_sizes.size(), 0);
    for (size_t i = 1; i < mem_sizes.size(); ++i) {
      mem_offset[i] = mem_offset[i - 1] + mem_sizes[i - 1];
    }
    for (size_t i = 0; i < rank_ids.size(); ++i) {
      if (rank_ids[i] < 0 || static_cast<size_t>(rank_ids[i]) >= rank_size_) {
        MS_LOG(EXCEPTION) << "Invalid rank id " << rank_ids[i] << " at index " << i << " as rank size " << rank_size_;
      }
      (*counts)[LongToSize(rank_ids[i])] = static_cast<int64_t>(real_sizes[i]);
      (*displs)[LongToSize(rank_ids[i])] = static_cast<int64_t>(mem_offset[i]);
    }
    return;
  }

  std::map<int64_t, size_t> rank_id_map;
  for (size_t i = 0; i < rank_ids.size(); ++i) {
    auto rank_id = rank_ids.at(i);
    if (rank_id < 0 || LongToSize(rank_id) >= rank_size_) {
      MS_LOG(EXCEPTION) << "Invalid rank id " << rank_id << " at index " << i << " as rank size " << rank_size_;
    }
    (void)rank_id_map.emplace(rank_id, i);
  }

  size_t offset = 0;
  for (uint32_t i = 0; i < rank_size_; ++i) {
    (*displs)[i] = SizeToLong(offset);
    decltype(rank_id_map)::const_iterator iter = rank_id_map.find(i);
    if (iter != rank_id_map.end()) {
      (*counts)[i] = static_cast<int64_t>(real_sizes[iter->second]);
      offset += mem_sizes[iter->second];
    } else {
      (*counts)[i] = 0;
    }
  }
}
}  // namespace mindspore::hccl
