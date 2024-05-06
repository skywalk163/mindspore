/**
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/gpu/kernel/nn/memcpy_gpu_kernel.h"
#include <map>
#include <memory>
#include "mindspore/core/ops/array_ops.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cast_impl.cuh"

namespace mindspore {
namespace kernel {
namespace {
#define EXPAND_DIMS_GPU_REG(T)                                                                     \
  KernelAttr().AddInputAttr(T).AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(T), \
    KernelAttr().AddInputAttr(T).AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(T)

template <typename T>
using Complex = mindspore::utils::Complex<T>;
constexpr auto kReshape = "Reshape";
constexpr auto kFlatten = "Flatten";
constexpr auto kFlattenGrad = "FlattenGrad";
constexpr auto kExpandDims = "ExpandDims";
constexpr auto kSqueeze = "Squeeze";
}  // namespace

int MemcpyGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  auto shape = LongVecToSizeVec(inputs[kIndex0]->GetShapeVector());
  is_null_input_ = CHECK_SHAPE_NULL(shape, kernel_name_, "input");
  if (is_null_input_) {
    return KRET_OK;
  }
  size_t input_data_size = abstract::TypeIdSize(inputs[kIndex0]->dtype_id());
  input_size_ = std::accumulate(shape.begin(), shape.end(), input_data_size, std::multiplies{});

  return KRET_OK;
}

bool MemcpyGpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                                const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  if (is_null_input_) {
    return true;
  }
  void *input = GetDeviceAddress<void>(inputs, kIndex0);
  void *output = GetDeviceAddress<void>(outputs, kIndex0);

  if (auto ret = cudaMemcpyAsync(output, input, input_size_, cudaMemcpyDeviceToDevice,
                                 reinterpret_cast<cudaStream_t>(stream_ptr));
      ret) {
    MS_LOG(ERROR) << "cudaMemcpyAsync error in MemcpyGpuKernelMod::Launch, error code is " << ret;
    return false;
  }
  return true;
}

static std::vector<KernelAttr> common_valid_types_with_single_input = {
  KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8),
  KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16),
  KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
  KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
  KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeBool),
  KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
  KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
  KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
  KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8),
  KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16),
  KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32),
  KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64),
  KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeComplex64),
  KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeComplex128)};

static std::vector<KernelAttr> expand_dims_valid_types = {
  // index int64
  EXPAND_DIMS_GPU_REG(kNumberTypeFloat16),   EXPAND_DIMS_GPU_REG(kNumberTypeFloat32),
  EXPAND_DIMS_GPU_REG(kNumberTypeFloat64),

  EXPAND_DIMS_GPU_REG(kNumberTypeInt8),      EXPAND_DIMS_GPU_REG(kNumberTypeInt16),
  EXPAND_DIMS_GPU_REG(kNumberTypeInt32),     EXPAND_DIMS_GPU_REG(kNumberTypeInt64),

  EXPAND_DIMS_GPU_REG(kNumberTypeUInt8),     EXPAND_DIMS_GPU_REG(kNumberTypeUInt16),
  EXPAND_DIMS_GPU_REG(kNumberTypeUInt32),    EXPAND_DIMS_GPU_REG(kNumberTypeUInt64),

  EXPAND_DIMS_GPU_REG(kNumberTypeBool),      EXPAND_DIMS_GPU_REG(kNumberTypeComplex64),
  EXPAND_DIMS_GPU_REG(kNumberTypeComplex128)};

static std::vector<KernelAttr> reshape_valid_types = {KernelAttr()
                                                        .AddInputAttr(kNumberTypeInt8)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeInt8),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeInt16)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeInt16),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeInt32)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeInt32),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeInt64)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeInt64),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeBool)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeBool),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeFloat16)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeFloat16),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeFloat32)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeFloat32),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeFloat64)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeFloat64),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeUInt8)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeUInt8),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeUInt16)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeUInt16),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeUInt32)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeUInt32),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeUInt64)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeUInt64),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeComplex64)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeComplex64),
                                                      KernelAttr()
                                                        .AddInputAttr(kNumberTypeComplex128)
                                                        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                        .AddOutputAttr(kNumberTypeComplex128)};

std::vector<KernelAttr> MemcpyGpuKernelMod::GetOpSupport() {
  static std::map<std::string, std::vector<KernelAttr>> support_list_map = {
    {kReshape, reshape_valid_types},
    {kFlatten, common_valid_types_with_single_input},
    {kFlattenGrad, common_valid_types_with_single_input},
    {kExpandDims, expand_dims_valid_types},
    {kSqueeze, common_valid_types_with_single_input},
  };

  auto iter = support_list_map.find(kernel_type_);
  if (iter == support_list_map.end()) {
    MS_LOG(EXCEPTION) << "Does not support " << kernel_type_ << "!";
  }
  return iter->second;
}
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeGpuKernelMod, Reshape,
                                 []() { return std::make_shared<MemcpyGpuKernelMod>(kReshape); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeGpuKernelMod, Flatten,
                                 []() { return std::make_shared<MemcpyGpuKernelMod>(kFlatten); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeGpuKernelMod, FlattenGrad,
                                 []() { return std::make_shared<MemcpyGpuKernelMod>(kFlattenGrad); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeGpuKernelMod, ExpandDims,
                                 []() { return std::make_shared<MemcpyGpuKernelMod>(kExpandDims); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeGpuKernelMod, Squeeze,
                                 []() { return std::make_shared<MemcpyGpuKernelMod>(kSqueeze); });
}  // namespace kernel
}  // namespace mindspore
