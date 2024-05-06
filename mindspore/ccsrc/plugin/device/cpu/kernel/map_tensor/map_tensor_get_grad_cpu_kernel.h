/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MAP_TENSOR_MAP_TENSOR_GET_GRAD_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MAP_TENSOR_MAP_TENSOR_GET_GRAD_CPU_KERNEL_H_

#include <vector>
#include <string>
#include <utility>
#include <map>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/device/cpu/kernel/map_tensor/map_tensor_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_hash_table.h"

namespace mindspore {
namespace kernel {
using device::cpu::CPUHashTable;
constexpr size_t kMapTensorGetGradInputNum = 3;
constexpr size_t kMapTensorGetGradOutputNum = 1;

class MapTensorGetGradCpuKernelMod : public MapTensorCpuKernelMod {
 public:
  MapTensorGetGradCpuKernelMod() = default;
  ~MapTensorGetGradCpuKernelMod() override = default;

  std::vector<KernelAttr> GetOpSupport() override;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), kMapTensorGetGradInputNum, kernel_name_);
    CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kMapTensorGetGradOutputNum, kernel_name_);
    return kernel_launch_func_(this, inputs, workspace, outputs);
  }

 protected:
  bool IsNeedUpdateOutputShapeAndSize() override { return true; }
  void UpdateOutputShapeAndSize(const std::vector<KernelTensor *> &inputs,
                                const std::vector<KernelTensor *> &outputs) override;

 private:
  template <typename KeyType>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs);

  void InitSizeLists(const ShapeVector &keys_shape, const ShapeVector &dout_shape);

  size_t input_keys_type_size_{0};
  size_t input_dout_type_size_{0};

  using MapTensorGetGradLaunchFunc =
    std::function<bool(MapTensorGetGradCpuKernelMod *, const std::vector<KernelTensor *> &,
                       const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &)>;
  static std::vector<std::pair<KernelAttr, MapTensorGetGradLaunchFunc>> map_tensor_get_grad_func_list_;
  MapTensorGetGradLaunchFunc kernel_launch_func_;
  int64_t keys_size_{1};
  ShapeVector value_dims_ = {};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MAP_TENSOR_MAP_TENSOR_GET_GRAD_CPU_KERNEL_H_
