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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_AKG_GPU_DYNAMIC_AKG_GPU_KERNEL_BUILD_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_AKG_GPU_DYNAMIC_AKG_GPU_KERNEL_BUILD_H_
#include <string>
#include "kernel/kernel.h"
#include "kernel/graph_kernel/dynamic_akg/dynamic_akg_kernel_build.h"
#include "kernel/graph_kernel/graph_kernel_builder_manager.h"
#include "base/base.h"

namespace mindspore {
namespace kernel {
class DynamicAkgGpuKernelBuilder : public DynamicAkgKernelBuilder {
 public:
  DynamicAkgGpuKernelBuilder() = default;
  ~DynamicAkgGpuKernelBuilder() = default;

  kernel::KernelBuildClient *GetClient() override { return &(kernel::AkgV2KernelBuildClient::Instance()); }
  void SetKernelMod(const KernelPackPtr &kernel_pack, const GraphKernelJsonGenerator &json_generator,
                    const AnfNodePtr &anf_node) override;
  void SaveJsonInfo(const string &kernel_name, const string &kernel_json) override;
};

REG_GRAPH_KERNEL_BUILDER(kGPUDevice, true, DynamicAkgGpuKernelBuilder);
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_AKG_GPU_DYNAMIC_AKG_GPU_KERNEL_BUILD_H_
