/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_GRID_SAMPLER_2D_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_GRID_SAMPLER_2D_CPU_KERNEL_H_
#include <map>
#include <algorithm>
#include <vector>
#include <memory>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "mindspore/core/ops/ops_func_impl/grid_sampler_2d.h"

namespace mindspore {
namespace kernel {
class GridSampler2DCpuKernelMod : public NativeCpuKernelMod {
 public:
  GridSampler2DCpuKernelMod() = default;
  ~GridSampler2DCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  template <typename T>
  void LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);
  void LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);

  std::vector<KernelAttr> GetOpSupport() override {
    static const std::vector<KernelAttr> support_list = {KernelAttr()
                                                           .AddInputAttr(kNumberTypeFloat16)
                                                           .AddInputAttr(kNumberTypeFloat16)
                                                           .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                           .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                           .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
                                                           .AddOutputAttr(kNumberTypeFloat16),
                                                         KernelAttr()
                                                           .AddInputAttr(kNumberTypeFloat32)
                                                           .AddInputAttr(kNumberTypeFloat32)
                                                           .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                           .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                                                           .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
                                                           .AddOutputAttr(kNumberTypeFloat32)};
    return support_list;
  }

 private:
  ShapeVector x_shape_;
  ShapeVector grid_shape_;
  ShapeVector output_shape_;
  std::vector<size_t> x_stride_;
  std::vector<size_t> grid_stride_;
  std::vector<size_t> output_stride_;
  int64_t interpolation_mode_;
  int64_t padding_mode_;
  bool align_corners_{false};
  size_t output_number_{0};
  TypeId dtype_{kTypeUnknown};
  template <typename T>
  void ComputeTask(const T *x_data_addr, const T *grid_data_addr, T *output_data_addr, const size_t &seq);

  void ComputeTask(const float16 *x_data_addr, const float16 *grid_data_addr, float16 *output_data_addr,
                   const size_t &seq);

  template <typename T>
  T GridSamplerComputeSourceIndex(T coord, int64_t size, int64_t padding_mode, bool align_corners) const;

  template <typename T>
  T ReflectCoordinates(T coord, int64_t twice_low, int64_t twice_high) const;

  bool WithinBounds2D(int64_t h, int64_t w, int64_t H, int64_t W) const;

  void Call2Half(const float16 *x_data_addr, float16 *y_data_addr, const float16 *grid_data_addr,
                 std::vector<int64_t> x_dims, std::vector<int64_t> y_dims, int64_t *y_stride, int64_t *x_stride,
                 const int64_t *grid_stride, int64_t interpolation_mode, int64_t padding_mode, bool align_corners);

  void NearestHalf(float x, float y, const float16 *x_data_addr, float16 *y_data_addr, int64_t y_c,
                   std::vector<int64_t> x_dims, int64_t *y_stride, const int64_t *x_stride, int64_t x_ptr_NC,
                   int64_t y_ptr_NCHW) const;

  void BilinearHalf(float x, float y, const float16 *x_data_addr, float16 *y_data_addr, int64_t y_c,
                    std::vector<int64_t> x_dims, int64_t *y_stride, int64_t *x_stride, int64_t x_ptr_NC,
                    int64_t y_ptr_NCHW) const;
};
}  // namespace kernel
}  // namespace mindspore
#endif
