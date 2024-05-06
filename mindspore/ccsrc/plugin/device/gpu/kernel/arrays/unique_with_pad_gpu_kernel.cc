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

#include "plugin/device/gpu/kernel/arrays/unique_with_pad_gpu_kernel.h"
#include <functional>
#include <utility>
#include <string>
#include <algorithm>
#include "runtime/device/ms_device_shape_transfer.h"
#include "kernel/common_utils.h"

namespace mindspore {
namespace kernel {
namespace {
template <typename T, typename S>
std::unique_ptr<cukernel::GpuKernelHelperBase> CreateUniqueWithPadKernelPtr(const std::string &kernel_name,
                                                                            const uint32_t &device_id) {
  return std::make_unique<cukernel::UniqueWithPadHelperGpuKernel<T, S>>(kernel_name, device_id);
}

using UniqueWithPadPtrCreatorFunc =
  std::function<std::unique_ptr<cukernel::GpuKernelHelperBase>(const std::string &, const uint32_t &)>;

const std::vector<std::pair<KernelAttr, UniqueWithPadPtrCreatorFunc>> kernel_attr = {
  {KernelAttr()
     .AddInputAttr(kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt32)
     .AddOutputAttr(kNumberTypeInt32)
     .AddOutputAttr(kNumberTypeInt32),
   CreateUniqueWithPadKernelPtr<int, int>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   CreateUniqueWithPadKernelPtr<int64_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddOutputAttr(kNumberTypeFloat32)
     .AddOutputAttr(kNumberTypeInt32),
   CreateUniqueWithPadKernelPtr<float, int32_t>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeFloat16)
     .AddOutputAttr(kNumberTypeFloat16)
     .AddOutputAttr(kNumberTypeInt32),
   CreateUniqueWithPadKernelPtr<half, int32_t>}};
}  // namespace

bool UniqueWithPadGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  if (primitive_->HasAttr(ops::kBatchRank)) {
    auto batch_rank = GetValue<int64_t>(primitive_->GetAttr(ops::kBatchRank));
    if (batch_rank < 0) {
      return false;
    }
    batch_rank_ = static_cast<size_t>(batch_rank);
  }

  auto [is_match, index] = MatchKernelAttr(GetKernelAttrFromTensors(inputs, outputs), GetOpSupport());
  if (!is_match) {
    return false;
  }
  helper_ptr_ = kernel_attr[index].second(kernel_name_, device_id_);
  return true;
}

int UniqueWithPadGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  std::vector<std::vector<int64_t>> input_shapes;
  std::vector<std::vector<int64_t>> output_shapes;
  constexpr size_t kUniqueWithPadInputNum = 2;
  if (inputs.size() != kUniqueWithPadInputNum) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of inputs must be 2, but got " << inputs.size();
  }
  std::vector<size_t> shape =
    std::vector<size_t>(inputs[0]->GetDeviceShapeVector().begin(), inputs[0]->GetDeviceShapeVector().end());
  if (batch_rank_ > 0) {
    if (shape.size() != static_cast<size_t>(batch_rank_ + 1)) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_
                        << "', the shape size of 'input' must be equal to 'batch_rank + 1', "
                           "but got the shape of 'input': "
                        << shape << " and 'batch_rank': " << batch_rank_;
    }
    std::vector<size_t> pad_shape =
      std::vector<size_t>(inputs[1]->GetDeviceShapeVector().begin(), inputs[1]->GetDeviceShapeVector().end());
    auto pad_nums = std::accumulate(pad_shape.begin(), pad_shape.end(), 1, std::multiplies<int64_t>());
    auto batch_size = std::accumulate(shape.begin(), shape.begin() + batch_rank_, 1, std::multiplies<int64_t>());
    if (pad_nums != static_cast<int64_t>(batch_size)) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_
                        << "', the elements num of input 'pad' must be equal to input 'x' batch size, "
                           "but got the elements num of input 'pad': "
                        << pad_shape << " and input 'x' batch size: " << batch_size;
    }
  }

  is_null_input_ = CHECK_SHAPE_NULL(shape, kernel_name_, "input");
  if (is_null_input_) {
    InitSizeLists();
    return KRET_OK;
  }

  input_shapes.emplace_back(inputs[0]->GetDeviceShapeVector());
  input_shapes.emplace_back(inputs[1]->GetDeviceShapeVector());
  helper_ptr_->CalMemSize(input_shapes, output_shapes);
  InitSizeLists();
  return KRET_OK;
}

std::vector<KernelAttr> UniqueWithPadGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(kernel_attr.begin(), kernel_attr.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, UniqueWithPadPtrCreatorFunc> &item) { return item.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, UniqueWithPad, UniqueWithPadGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
