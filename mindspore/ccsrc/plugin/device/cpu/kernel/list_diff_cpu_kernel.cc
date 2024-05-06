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

#include "plugin/device/cpu/kernel/list_diff_cpu_kernel.h"
#include <unordered_set>
#include "plugin/factory/ms_factory.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "kernel/common_utils.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kListDiffInputNum = 2;
constexpr size_t kListDiffOutputNum = 2;

#define LIST_DIFF_COMPUTE_CASE(data_type, type)              \
  case (data_type): {                                        \
    if (idx_type_ == kNumberTypeInt32) {                     \
      result = LaunchKernel<type, int32_t>(inputs, outputs); \
    } else {                                                 \
      result = LaunchKernel<type, int64_t>(inputs, outputs); \
    }                                                        \
    break;                                                   \
  }
}  // namespace

bool ListDiffCPUKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  TypeId x_type = inputs.at(kIndex0)->dtype_id();
  TypeId y_type = inputs.at(kIndex1)->dtype_id();
  out_type_ = outputs.at(kIndex0)->dtype_id();
  if (x_type != y_type || x_type != out_type_) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', input 'x', 'y' and output 'out' should be same type, but get x["
                      << TypeIdLabel(x_type) << "], y[" << TypeIdLabel(y_type) << "], out[" << TypeIdLabel(out_type_)
                      << "].";
  }
  idx_type_ = outputs.at(kIndex1)->dtype_id();
  MS_EXCEPTION_IF_CHECK_FAIL((idx_type_ == kNumberTypeInt32 || idx_type_ == kNumberTypeInt64),
                             "attr 'out_idx' should be int32 or int64");
  out_size_ = 0;
  data_size_ = abstract::TypeIdSize(x_type);
  index_size_ = abstract::TypeIdSize(idx_type_);
  return true;
}

void ListDiffCPUKernelMod::ResetResource() noexcept {
  output_size_list_.clear();
  workspace_size_list_.clear();
}

int ListDiffCPUKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  for (auto &input : inputs) {
    MS_EXCEPTION_IF_NULL(input);
    auto shape = input->GetShapeVector();
    if (!IsValidShape(shape)) {
      return KRET_UNKNOWN_SHAPE;
    }
  }
  ResetResource();
  auto x_shape = inputs[kIndex0]->GetShapeVector();
  auto y_shape = inputs[kIndex1]->GetShapeVector();
  x_size_ = x_shape[0];
  y_size_ = y_shape[0];
  (void)output_size_list_.emplace_back(x_size_ * data_size_);
  (void)output_size_list_.emplace_back(x_size_ * index_size_);
  return KRET_OK;
}

template <typename T, typename Tidx>
bool ListDiffCPUKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &outputs) {
  auto x_addr = static_cast<T *>(inputs[0]->device_ptr());
  auto y_addr = static_cast<T *>(inputs[1]->device_ptr());
  auto out_addr = static_cast<T *>(outputs[0]->device_ptr());
  auto idx_addr = static_cast<Tidx *>(outputs[1]->device_ptr());
  MS_EXCEPTION_IF_NULL(x_addr);
  MS_EXCEPTION_IF_NULL(y_addr);
  MS_EXCEPTION_IF_NULL(out_addr);
  MS_EXCEPTION_IF_NULL(idx_addr);

  std::unordered_set<T> y_set;
  y_set.reserve(y_size_);
  for (int64_t i = 0; i < y_size_; ++i) {
    (void)y_set.insert(y_addr[i]);
  }
  // Compute the size of the output.
  out_size_ = 0;
  for (int64_t i = 0; i < x_size_; ++i) {
    if (y_set.count(x_addr[i]) == 0) {
      ++out_size_;
    }
  }
  // calculate results
  for (Tidx i = 0, p = 0; i < static_cast<Tidx>(x_size_); ++i) {
    if (0 == y_set.count(x_addr[i])) {
      MS_EXCEPTION_IF_CHECK_FAIL(p < static_cast<Tidx>(out_size_),
                                 "Try to set output index failure for index out of out_size");
      out_addr[p] = x_addr[i];
      idx_addr[p] = i;
      p++;
    }
  }

  return true;
}

bool ListDiffCPUKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &workspace,
                                  const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kListDiffInputNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kListDiffOutputNum, kernel_name_);
  bool result = false;
  switch (out_type_) {
    LIST_DIFF_COMPUTE_CASE(kNumberTypeInt8, int8_t)
    LIST_DIFF_COMPUTE_CASE(kNumberTypeInt16, int16_t)
    LIST_DIFF_COMPUTE_CASE(kNumberTypeInt32, int32_t)
    LIST_DIFF_COMPUTE_CASE(kNumberTypeInt64, int64_t)
    LIST_DIFF_COMPUTE_CASE(kNumberTypeUInt8, uint8_t)
    LIST_DIFF_COMPUTE_CASE(kNumberTypeUInt16, uint16_t)
    LIST_DIFF_COMPUTE_CASE(kNumberTypeFloat16, float16)
    LIST_DIFF_COMPUTE_CASE(kNumberTypeFloat32, float)
    LIST_DIFF_COMPUTE_CASE(kNumberTypeFloat64, double)
    default: {
      MS_LOG(EXCEPTION)
        << "For [" << kernel_name_
        << "] input data type should be in [int8, int16, int32, int64, uint8, uint16, float16, float32, float64],"
        << " but get" << TypeIdLabel(out_type_) << ".";
    }
  }
  return result;
}

std::vector<KernelAttr> ListDiffCPUKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> support_list = {KernelAttr()
                                                   .AddInputAttr(kNumberTypeFloat16)
                                                   .AddInputAttr(kNumberTypeFloat16)
                                                   .AddOutputAttr(kNumberTypeFloat16)
                                                   .AddOutputAttr(kNumberTypeInt32),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddOutputAttr(kNumberTypeFloat32)
                                                   .AddOutputAttr(kNumberTypeInt32),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeFloat64)
                                                   .AddInputAttr(kNumberTypeFloat64)
                                                   .AddOutputAttr(kNumberTypeFloat64)
                                                   .AddOutputAttr(kNumberTypeInt32),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeUInt8)
                                                   .AddInputAttr(kNumberTypeUInt8)
                                                   .AddOutputAttr(kNumberTypeUInt8)
                                                   .AddOutputAttr(kNumberTypeInt32),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeUInt16)
                                                   .AddInputAttr(kNumberTypeUInt16)
                                                   .AddOutputAttr(kNumberTypeUInt16)
                                                   .AddOutputAttr(kNumberTypeInt32),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeInt8)
                                                   .AddInputAttr(kNumberTypeInt8)
                                                   .AddOutputAttr(kNumberTypeInt8)
                                                   .AddOutputAttr(kNumberTypeInt32),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeInt16)
                                                   .AddInputAttr(kNumberTypeInt16)
                                                   .AddOutputAttr(kNumberTypeInt16)
                                                   .AddOutputAttr(kNumberTypeInt32),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeInt32)
                                                   .AddInputAttr(kNumberTypeInt32)
                                                   .AddOutputAttr(kNumberTypeInt32)
                                                   .AddOutputAttr(kNumberTypeInt32),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeInt64)
                                                   .AddInputAttr(kNumberTypeInt64)
                                                   .AddOutputAttr(kNumberTypeInt64)
                                                   .AddOutputAttr(kNumberTypeInt32),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeFloat16)
                                                   .AddInputAttr(kNumberTypeFloat16)
                                                   .AddOutputAttr(kNumberTypeFloat16)
                                                   .AddOutputAttr(kNumberTypeInt64),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddOutputAttr(kNumberTypeFloat32)
                                                   .AddOutputAttr(kNumberTypeInt64),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeFloat64)
                                                   .AddInputAttr(kNumberTypeFloat64)
                                                   .AddOutputAttr(kNumberTypeFloat64)
                                                   .AddOutputAttr(kNumberTypeInt64),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeUInt8)
                                                   .AddInputAttr(kNumberTypeUInt8)
                                                   .AddOutputAttr(kNumberTypeUInt8)
                                                   .AddOutputAttr(kNumberTypeInt64),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeUInt16)
                                                   .AddInputAttr(kNumberTypeUInt16)
                                                   .AddOutputAttr(kNumberTypeUInt16)
                                                   .AddOutputAttr(kNumberTypeInt64),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeInt8)
                                                   .AddInputAttr(kNumberTypeInt8)
                                                   .AddOutputAttr(kNumberTypeInt8)
                                                   .AddOutputAttr(kNumberTypeInt64),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeInt16)
                                                   .AddInputAttr(kNumberTypeInt16)
                                                   .AddOutputAttr(kNumberTypeInt16)
                                                   .AddOutputAttr(kNumberTypeInt64),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeInt32)
                                                   .AddInputAttr(kNumberTypeInt32)
                                                   .AddOutputAttr(kNumberTypeInt32)
                                                   .AddOutputAttr(kNumberTypeInt64),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeInt64)
                                                   .AddInputAttr(kNumberTypeInt64)
                                                   .AddOutputAttr(kNumberTypeInt64)
                                                   .AddOutputAttr(kNumberTypeInt64)};

  return support_list;
}

void ListDiffCPUKernelMod::UpdateOutputShapeAndSize(const std::vector<KernelTensor *> &inputs,
                                                    const std::vector<KernelTensor *> &outputs) {
  // update out
  ShapeVector out_shape_ = {out_size_};
  ShapeVector idx_shape_ = {out_size_};
  // set output shape and dtype
  outputs[0]->SetShapeVector(out_shape_);
  outputs[0]->set_size(out_size_ * UnitSizeInBytes(out_type_));
  outputs[1]->SetShapeVector(idx_shape_);
  outputs[1]->set_size(out_size_ * UnitSizeInBytes(idx_type_));
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, ListDiff, ListDiffCPUKernelMod);
}  // namespace kernel
}  // namespace mindspore
