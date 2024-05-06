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

#include "plugin/device/gpu/kernel/dynamic_akg/dynamic_akg_gpu_kernel_build.h"

#include "kernel/framework_utils.h"
#include "kernel/common_utils.h"
#include "utils/ms_utils.h"
#include "plugin/factory/ms_factory.h"
#include "include/common/utils/anfalgo.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "plugin/device/gpu/kernel/akg/akg_gpu_kernel_build.h"
#include "plugin/device/gpu/kernel/dynamic_akg/dynamic_akg_gpu_kernel_mod.h"

namespace mindspore {
namespace kernel {
void DynamicAkgGpuKernelBuilder::SetKernelMod(const KernelPackPtr &kernel_pack,
                                              const GraphKernelJsonGenerator &json_generator,
                                              const AnfNodePtr &anf_node) {
  auto kernel_mod_ptr = std::make_shared<DynamicAkgGpuKernelMod>(kernel_pack);
  auto kernel_json_info = kernel_pack->kernel_json_info();
  kernel_mod_ptr->SetInputSizeList(json_generator.input_size_list());
  kernel_mod_ptr->SetOutputSizeList(json_generator.output_size_list());
  kernel_mod_ptr->SetWorkspaceSizeList(kernel_json_info.workspaces);

  std::vector<KernelTensor *> input_kernel_tensors = AnfAlgo::GetOrCreateAllInputKernelTensors(anf_node);
  std::vector<KernelTensor *> output_kernel_tensors = AnfAlgo::GetOrCreateAllOutputKernelTensors(anf_node);
  bool is_dynamic_kernel =
    std::any_of(input_kernel_tensors.begin(), input_kernel_tensors.end(),
                [](const KernelTensor *item) {
                  MS_EXCEPTION_IF_NULL(item);
                  return item->IsDynamicShape();
                }) ||
    std::any_of(output_kernel_tensors.begin(), output_kernel_tensors.end(), [](const KernelTensor *item) {
      MS_EXCEPTION_IF_NULL(item);
      return item->IsDynamicShape();
    });
  kernel_mod_ptr->SetKernelDynamicStatus(is_dynamic_kernel);
  kernel_mod_ptr->Initialize();
  AnfAlgo::SetKernelMod(kernel_mod_ptr, anf_node.get());
}

void DynamicAkgGpuKernelBuilder::SaveJsonInfo(const string &kernel_name, const string &kernel_json) {
  kernel::SaveJsonInfo(kernel_name, kernel_json, kernel::KernelMeta::GetInstance()->kernel_meta_path());
}
}  // namespace kernel
}  // namespace mindspore
