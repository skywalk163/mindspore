/**
 * Copyright 2020-2021 Huawei Technologies Co., Ltd
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

#include "plugin/device/ascend/kernel/akg/akg_ascend_kernel_build.h"
#include <memory>
#include "ir/func_graph.h"
#include "kernel/framework_utils.h"
#include "plugin/device/ascend/kernel/akg/akg_utils.h"
#include "plugin/device/ascend/kernel/akg/akg_ascend_kernel_mod.h"
#include "include/backend/anf_runtime_algorithm.h"

namespace mindspore {
namespace kernel {
KernelPackPtr AkgAscendKernelBuilder::SearchKernelCache(const std::string &kernel_name) {
  std::string cce_json = GetCompilerCachePath() + kAkgKernelMeta + kernel_name + kJsonSuffix;
  KernelPackPtr ret = std::make_shared<KernelPack>();
  if (!ret->LoadKernelMeta(cce_json)) {
    MS_LOG(INFO) << "Read cache json and bin file failed[" << cce_json << "]";
    return nullptr;
  }
  return ret;
}

KernelPackPtr AkgAscendKernelBuilder::InsertKernelCache(const std::string &kernel_name) {
  return SearchKernelCache(kernel_name);
}

void AkgAscendKernelBuilder::SetKernelMod(const KernelPackPtr &kernel_pack,
                                          const GraphKernelJsonGenerator &json_generator, const AnfNodePtr &anf_node) {
  auto kernel_mod_ptr = std::make_shared<AkgKernelMod>(kernel_pack, anf_node);
  auto kernel_json_info = kernel_pack->kernel_json_info();
  kernel_mod_ptr->SetInputSizeList(json_generator.input_size_list());
  kernel_mod_ptr->SetOutputSizeList(json_generator.output_size_list());
  kernel_mod_ptr->SetWorkspaceSizeList(kernel_json_info.workspaces);
  AnfAlgo::SetKernelMod(kernel_mod_ptr, anf_node.get());
}

void AkgAscendKernelBuilder::SaveJsonInfo(const string &kernel_name, const string &kernel_json) {
  kernel::SaveJsonInfo(kernel_name, kernel_json, GetCompilerCachePath().append(kAkgKernelMeta));
}
}  // namespace kernel
}  // namespace mindspore
