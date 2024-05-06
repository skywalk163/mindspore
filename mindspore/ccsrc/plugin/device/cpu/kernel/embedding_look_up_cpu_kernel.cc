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

#include "plugin/device/cpu/kernel/embedding_look_up_cpu_kernel.h"
#include "mindspore/core/ops/embedding_lookup.h"
#include "utils/check_convert_utils.h"
#include "include/backend/distributed/embedding_cache/embedding_cache_utils.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kEmbeddingLookupInputsNum = 3;
constexpr size_t kEmbeddingLookUpInputParamsMaxDim = 2;
constexpr size_t kOffsetIndex = 2;
using KernelRunFunc = EmbeddingLookUpCpuKernelMod::KernelRunFunc;

#define ADD_KERNEL(input_params_dtype, input_indices_dtype, output_dtype, input_params_type, input_indices_type) \
  {                                                                                                              \
    KernelAttr()                                                                                                 \
      .AddInputAttr(kNumberType##input_params_dtype)                                                             \
      .AddInputAttr(kNumberType##input_indices_dtype)                                                            \
      .AddInputAttr(kNumberTypeInt64)                                                                            \
      .AddOutputAttr(kNumberType##output_dtype),                                                                 \
      &EmbeddingLookUpCpuKernelMod::LaunchKernel<input_params_type, input_indices_type, int64_t>                 \
  }

#define ADD_KERNEL_INT32(input_params_dtype, input_indices_dtype, output_dtype, input_params_type, input_indices_type) \
  {                                                                                                                    \
    KernelAttr()                                                                                                       \
      .AddInputAttr(kNumberType##input_params_dtype)                                                                   \
      .AddInputAttr(kNumberType##input_indices_dtype)                                                                  \
      .AddInputAttr(kNumberTypeInt32)                                                                                  \
      .AddOutputAttr(kNumberType##output_dtype),                                                                       \
      &EmbeddingLookUpCpuKernelMod::LaunchKernel<input_params_type, input_indices_type, int32_t>                       \
  }

template <typename T, typename S>
void LookUpTableTask(const T *input_addr, const S *indices_addr, T *output_addr, size_t indices_lens,
                     size_t outer_dim_size, int64_t offset, size_t first_dim_size, std::string kernel_name_) {
  auto type_size = sizeof(T);
  size_t lens = outer_dim_size * type_size;
  for (size_t i = 0; i < indices_lens; ++i) {
    S index = indices_addr[i] - static_cast<S>(offset);
    if (index >= 0 && index < SizeToInt(first_dim_size)) {
      size_t pos = static_cast<size_t>(index) * outer_dim_size;
      auto ret = memcpy_s(output_addr, (indices_lens - i) * lens, input_addr + pos, lens);
      if (ret != EOK) {
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', memcpy failed. Error no: " << ret;
      }
    } else {
      auto ret = memset_s(output_addr, (indices_lens - i) * lens, 0, lens);
      if (ret != EOK) {
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', memset failed. Error no: " << ret;
      }
    }
    output_addr += outer_dim_size;
  }
}

// Indices should start from zero and should minus offset.
template <typename S>
void RectifyIndex(S *indices_addr, size_t indices_lens, int64_t offset) {
  for (size_t i = 0; i < indices_lens; ++i) {
    indices_addr[i] -= static_cast<S>(offset);
  }
}
}  // namespace

const std::vector<std::pair<KernelAttr, KernelRunFunc>> &EmbeddingLookUpCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, KernelRunFunc>> func_list = {
    ADD_KERNEL(Bool, Int32, Bool, bool, int32_t),
    ADD_KERNEL(Int8, Int32, Int8, int8_t, int32_t),
    ADD_KERNEL(Int16, Int32, Int16, int16_t, int32_t),
    ADD_KERNEL(Int32, Int32, Int32, int32_t, int32_t),
    ADD_KERNEL(Int64, Int32, Int64, int64_t, int32_t),
    ADD_KERNEL(UInt8, Int32, UInt8, uint8_t, int32_t),
    ADD_KERNEL(UInt16, Int32, UInt16, uint16_t, int32_t),
    ADD_KERNEL(UInt32, Int32, UInt32, uint32_t, int32_t),
    ADD_KERNEL(UInt64, Int32, UInt64, uint64_t, int32_t),
    ADD_KERNEL(Float16, Int32, Float16, float16, int32_t),
    ADD_KERNEL(Float32, Int32, Float32, float, int32_t),
    ADD_KERNEL(Float64, Int32, Float64, double, int32_t),

    ADD_KERNEL(Bool, Int64, Bool, bool, int64_t),
    ADD_KERNEL(Int8, Int64, Int8, int8_t, int64_t),
    ADD_KERNEL(Int16, Int64, Int16, int16_t, int64_t),
    ADD_KERNEL(Int32, Int64, Int32, int32_t, int64_t),
    ADD_KERNEL(Int64, Int64, Int64, int64_t, int64_t),
    ADD_KERNEL(UInt8, Int64, UInt8, uint8_t, int64_t),
    ADD_KERNEL(UInt16, Int64, UInt16, uint16_t, int64_t),
    ADD_KERNEL(UInt32, Int64, UInt32, uint32_t, int64_t),
    ADD_KERNEL(UInt64, Int64, UInt64, uint64_t, int64_t),
    ADD_KERNEL(Float16, Int64, Float16, float16, int64_t),
    ADD_KERNEL(Float32, Int64, Float32, float, int64_t),
    ADD_KERNEL(Float64, Int64, Float64, double, int64_t),

    ADD_KERNEL_INT32(Int32, Int32, Int32, int32_t, int32_t),
    ADD_KERNEL_INT32(Float32, Int32, Float32, float, int32_t)};

  return func_list;
}

bool EmbeddingLookUpCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  if (primitive_->HasAttr(kAttrEnableEmbeddingStorage)) {
    enable_embedding_storage_ = GetValue<bool>(primitive_->GetAttr(kAttrEnableEmbeddingStorage));
  }
  if (primitive_->HasAttr(kAttrParameterKey)) {
    parameter_key_ = GetValue<int32_t>(primitive_->GetAttr(kAttrParameterKey));
  }

  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int EmbeddingLookUpCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  if (inputs.size() != kEmbeddingLookupInputsNum || outputs.size() != 1) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', input and output size must be " << kEmbeddingLookupInputsNum
                  << ", but got " << inputs.size() << " and " << outputs.size();
  }

  std::vector<int64_t> input_params_shape = inputs[kIndex0]->GetShapeVector();
  if (input_params_shape.empty() || input_params_shape.size() > kEmbeddingLookUpInputParamsMaxDim) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dimension of input must be 1-"
                      << kEmbeddingLookUpInputParamsMaxDim << "D, but got " << input_params_shape.size() << "D.";
  }
  first_dim_size_ = LongToSize(input_params_shape[0]);
  outer_dim_size_ = 1;
  for (size_t i = 1; i < input_params_shape.size(); ++i) {
    outer_dim_size_ *= LongToSize(input_params_shape[i]);
  }
  input_params_dtype_ = inputs[kIndex0]->dtype_id();

  std::vector<int64_t> input_indices_shape = inputs[kIndex1]->GetShapeVector();
  input_indices_lens_ = SizeOf(input_indices_shape);
  input_indices_dtype_ = inputs[kIndex1]->dtype_id();
  return KRET_OK;
}

template <typename T, typename S, typename G>
bool EmbeddingLookUpCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                               const std::vector<KernelTensor *> &,
                                               const std::vector<KernelTensor *> &outputs) {
  T *input_params_addr = GetDeviceAddress<T>(inputs, 0);
  S *input_indices_addr = GetDeviceAddress<S>(inputs, 1);
  T *output_addr = GetDeviceAddress<T>(outputs, 0);
  G offset = static_cast<G *>(inputs[kOffsetIndex]->device_ptr())[0];
  offset_ = static_cast<int64_t>(offset);

  if (enable_embedding_storage_) {
    if (offset_ != 0) {
      // Indices should start from zero, so minus offset first.
      auto rectify_index_task = [&](size_t start, size_t end) {
        size_t task_proc_lens = end - start;
        RectifyIndex<S>(input_indices_addr + start, task_proc_lens, offset_);
      };
      ParallelLaunchAutoSearch(rectify_index_task, input_indices_lens_, this, &parallel_search_info_);
    }

    auto embedding_storage = embedding_storage_manager.Get(parameter_key_);
    MS_ERROR_IF_NULL(embedding_storage);
    if (!embedding_storage->Get({input_indices_addr, inputs[1]->size()}, {output_addr, outputs[0]->size()})) {
      MS_LOG(ERROR) << "For '" << kernel_name_
                    << "', lookup embedding from embedding storage failed, parameter key: " << parameter_key_;
      return false;
    }
    return true;
  }

  auto task = [&](size_t start, size_t end) {
    size_t task_proc_lens = end - start;
    LookUpTableTask<T, S>(input_params_addr, input_indices_addr + start, output_addr + start * outer_dim_size_,
                          task_proc_lens, outer_dim_size_, offset_, first_dim_size_, kernel_name_);
  };

  ParallelLaunchAutoSearch(task, input_indices_lens_, this, &parallel_search_info_);
  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, EmbeddingLookup, EmbeddingLookUpCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
