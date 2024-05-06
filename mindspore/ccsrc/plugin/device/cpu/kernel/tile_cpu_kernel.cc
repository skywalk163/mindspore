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

#include "plugin/device/cpu/kernel/tile_cpu_kernel.h"
#include <algorithm>
#include <map>
#include <numeric>
#include <functional>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "ops/ops_func_impl/tile.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kTileInputsNum = 2;
constexpr size_t kTileOutputsNum = 1;
constexpr size_t kIndex0 = 0;
constexpr size_t kIndex1 = 1;
void ChangeEmptyToOne(ShapeVector *shape) {
  if (shape->empty()) {
    shape->push_back(1L);
  }
}
}  // namespace

void TileCpuKernelMod::TileMultipleCompute() {
  ops::AdaptShapeAndMultipies(&x_shape_, &multiples_);
  if (x_shape_.size() > MAX_SHAPE_SIZE || x_shape_.size() > y_shape_.size()) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', input shape can not be greater than default max size: " << MAX_SHAPE_SIZE
                      << " and output shape: " << y_shape_.size() << ", but got input shape " << x_shape_.size();
  }
  ChangeEmptyToOne(&x_shape_);
  ChangeEmptyToOne(&multiples_);
  ChangeEmptyToOne(&y_shape_);

  input_size_ = 1;
  tile_struct_.in_dim_ = x_shape_.size();
  for (int i = 0; i < tile_struct_.in_dim_; i++) {
    input_size_ *= x_shape_[i];
    tile_struct_.in_shape_[i] = x_shape_[i];
    tile_struct_.out_shape_[i] = y_shape_[i];
  }

  int stridex = 1;
  int stridey = 1;
  for (int i = tile_struct_.in_dim_ - 1; i >= 0; i--) {
    tile_struct_.in_strides_[i] = stridex;
    tile_struct_.out_strides_[i] = stridey;
    stridex *= x_shape_[i];
    stridey *= y_shape_[i];
  }

  int large_one_multiple_count_ = 0;
  int multiple = 0;
  size_t mul_index = 0;
  for (size_t i = 0; i < multiples_.size(); i++) {
    tile_struct_.multiples_[i] = static_cast<int>(multiples_[i]);
    if (tile_struct_.multiples_[i] > 1) {
      large_one_multiple_count_++;
      multiple = tile_struct_.multiples_[i];
      mul_index = i;
    }
  }

  one_dim_tile_ = large_one_multiple_count_ == 1;
  if (one_dim_tile_) {
    tile_struct_.fast_multiple_ = static_cast<size_t>(multiple);
    tile_struct_.fast_stride_ = static_cast<size_t>(x_shape_[mul_index] * tile_struct_.in_strides_[mul_index]);
    if (tile_struct_.fast_stride_ == 0) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', fast stride can not be equal to 0";
    }
    tile_struct_.fast_outer_size_ = static_cast<size_t>(input_size_ / tile_struct_.fast_stride_);
  }
}

bool TileCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  input_num_ = inputs.size();
  if (input_num_ != kTileInputsNum) {
    MS_LOG(EXCEPTION) << "Tile's inputs number should be " << kTileInputsNum << ", but got " << input_num_;
  }

  multiples_.clear();
  dtype_ = inputs[kIndex0]->dtype_id();
  launch_map_[kNumberTypeInt8] = &TileCpuKernelMod::LaunchKernel<int8_t>;
  launch_map_[kNumberTypeInt16] = &TileCpuKernelMod::LaunchKernel<int16_t>;
  launch_map_[kNumberTypeInt32] = &TileCpuKernelMod::LaunchKernel<int>;
  launch_map_[kNumberTypeInt64] = &TileCpuKernelMod::LaunchKernel<int64_t>;
  launch_map_[kNumberTypeUInt8] = &TileCpuKernelMod::LaunchKernel<uint8_t>;
  launch_map_[kNumberTypeUInt16] = &TileCpuKernelMod::LaunchKernel<uint16_t>;
  launch_map_[kNumberTypeUInt32] = &TileCpuKernelMod::LaunchKernel<uint32_t>;
  launch_map_[kNumberTypeUInt64] = &TileCpuKernelMod::LaunchKernel<uint64_t>;
  launch_map_[kNumberTypeFloat32] = &TileCpuKernelMod::LaunchKernel<float>;
  launch_map_[kNumberTypeFloat16] = &TileCpuKernelMod::LaunchKernel<float16>;
  launch_map_[kNumberTypeFloat64] = &TileCpuKernelMod::LaunchKernel<double>;
  launch_map_[kNumberTypeComplex64] = &TileCpuKernelMod::LaunchKernel<complex64>;
  launch_map_[kNumberTypeComplex128] = &TileCpuKernelMod::LaunchKernel<complex128>;
  launch_map_[kNumberTypeBool] = &TileCpuKernelMod::LaunchKernel<bool>;

  auto iter = launch_map_.find(dtype_);
  if (iter != launch_map_.end()) {
    launch_func_ = iter->second;
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the dtype of input must be bool, int, float, uint or complex, but got "
                      << TypeIdToType(dtype_)->ToString();
    return false;
  }
  return true;
}

int TileCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  x_shape_ = inputs[kIndex0]->GetShapeVector();
  y_shape_ = outputs[kIndex0]->GetShapeVector();
  auto multiple_shape = inputs[kIndex1]->GetShapeVector();
  multiple_num_ =
    LongToSize(std::accumulate(multiple_shape.begin(), multiple_shape.end(), 1, std::multiplies<int64_t>()));

  return KRET_OK;
}

bool TileCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                              const std::vector<kernel::KernelTensor *> &,
                              const std::vector<kernel::KernelTensor *> &outputs) {
  if (inputs.size() != kTileInputsNum) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of input must be " << kTileInputsNum << ", but got "
                      << inputs.size();
  }
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kTileOutputsNum, kernel_name_);
  launch_func_(this, inputs, outputs);
  return true;
}

template <typename T>
void TileCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  auto x_addr = reinterpret_cast<T *>(inputs[0]->device_ptr());
  auto y_addr = reinterpret_cast<T *>(outputs[0]->device_ptr());
  multiples_.clear();
  auto multiples_addr = GetDeviceAddress<int64_t>(inputs, 1);
  for (size_t i = 0; i < multiple_num_; ++i) {
    (void)multiples_.emplace_back(multiples_addr[i]);
  }
  TileMultipleCompute();

  tile_struct_.data_size_ = sizeof(T);
  if (one_dim_tile_) {
    auto task = [&x_addr, &y_addr, this](size_t start, size_t end) {
      TileSimple(x_addr, y_addr, start, end, &tile_struct_);
    };
    ParallelLaunchAutoSearch(task, tile_struct_.fast_outer_size_, this, &parallel_search_info_);
    return;
  }

  Tile(x_addr, y_addr, &tile_struct_);
}

static const std::vector<KernelAttr> support_list = {KernelAttr()
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
                                                       .AddInputAttr(kNumberTypeBool)
                                                       .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                       .AddOutputAttr(kNumberTypeBool),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeComplex64)
                                                       .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                       .AddOutputAttr(kNumberTypeComplex64),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeComplex128)
                                                       .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                                       .AddOutputAttr(kNumberTypeComplex128)};

std::vector<KernelAttr> TileCpuKernelMod::GetOpSupport() { return support_list; }

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Tile, TileCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
