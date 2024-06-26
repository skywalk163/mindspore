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

#include "plugin/device/gpu/kernel/nn/grid_sampler_gpu_kernel.h"

namespace mindspore {
namespace kernel {
MS_REG_GPU_KERNEL_ONE(GridSampler2D,
                      KernelAttr()
                        .AddInputAttr(kNumberTypeFloat16)
                        .AddInputAttr(kNumberTypeFloat16)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
                        .AddOutputAttr(kNumberTypeFloat16),
                      GridSampler2DGpuKernelMod, half)
MS_REG_GPU_KERNEL_ONE(GridSampler2D,
                      KernelAttr()
                        .AddInputAttr(kNumberTypeFloat32)
                        .AddInputAttr(kNumberTypeFloat32)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
                        .AddOutputAttr(kNumberTypeFloat32),
                      GridSampler2DGpuKernelMod, float)
MS_REG_GPU_KERNEL_ONE(GridSampler2D,
                      KernelAttr()
                        .AddInputAttr(kNumberTypeFloat64)
                        .AddInputAttr(kNumberTypeFloat64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
                        .AddOutputAttr(kNumberTypeFloat64),
                      GridSampler2DGpuKernelMod, double)
MS_REG_GPU_KERNEL_ONE(GridSampler3D,
                      KernelAttr()
                        .AddInputAttr(kNumberTypeFloat16)
                        .AddInputAttr(kNumberTypeFloat16)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
                        .AddOutputAttr(kNumberTypeFloat16),
                      GridSampler3DGpuKernelMod, half)
MS_REG_GPU_KERNEL_ONE(GridSampler3D,
                      KernelAttr()
                        .AddInputAttr(kNumberTypeFloat32)
                        .AddInputAttr(kNumberTypeFloat32)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
                        .AddOutputAttr(kNumberTypeFloat32),
                      GridSampler3DGpuKernelMod, float)
MS_REG_GPU_KERNEL_ONE(GridSampler3D,
                      KernelAttr()
                        .AddInputAttr(kNumberTypeFloat64)
                        .AddInputAttr(kNumberTypeFloat64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
                        .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
                        .AddOutputAttr(kNumberTypeFloat64),
                      GridSampler3DGpuKernelMod, double)
}  // namespace kernel
}  // namespace mindspore
