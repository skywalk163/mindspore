/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/ascend/kernel/bisheng/bisheng_kernel_build.h"
#include <dlfcn.h>
#include <libgen.h>
#include <memory>
#include <string>
#include <map>
#include "plugin/device/ascend/kernel/bisheng/custom_bisheng_kernel.h"
#include "plugin/device/ascend/kernel/bisheng/bisheng_kernel_mod.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "plugin/factory/ms_factory.h"
#include "include/common/utils/anfalgo.h"
#include "utils/dlopen_macro.h"
#include "kernel/framework_utils.h"

namespace mindspore {
namespace kernel {
namespace {
bool LoadBishengKernelsLibrary() noexcept {
  Dl_info dl_info;
  if (dladdr(reinterpret_cast<void *>(LoadBishengKernelsLibrary), &dl_info) == 0) {
    MS_LOG(INFO) << "Get dladdr error.";
    return false;
  }
  std::string cur_so_path = dl_info.dli_fname;
  std::string bisheng_kernels_path = std::string(dirname(cur_so_path.data())) + "/ascend/libbisheng_kernels.so";

  auto handle = dlopen(bisheng_kernels_path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (handle == nullptr) {
    MS_LOG(INFO) << "Cannot dlopen " << bisheng_kernels_path << ", result = " << GetDlErrorMsg()
                 << ", so bisheng kernels are unavailable.";
    return false;
  }
  return true;
}

bool kBishengStatus = LoadBishengKernelsLibrary();
}  // namespace
KernelModPtr BiShengOpBuild(const AnfNodePtr &anf_node) {
  MS_EXCEPTION_IF_NULL(anf_node);
  auto cnode = anf_node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  const auto &kernel_name = common::AnfAlgo::GetCNodeName(cnode);
  if (!Factory<BiShengKernelMod>::Instance().IsRegistered(kernel_name)) {
    MS_LOG(INFO) << "Bisheng custom op " << kernel_name;
    auto kernel_mod_ptr = std::make_shared<CustomBiShengKernel>(cnode);
    MS_EXCEPTION_IF_NULL(kernel_mod_ptr);
    if (!kernel_mod_ptr->InitKernel(cnode)) {
      MS_LOG(ERROR) << "BiSheng Kernel initialize failed!";
      return nullptr;
    }
    return kernel_mod_ptr;
  }

  MS_LOG(INFO) << "Bisheng internal op " << kernel_name;
  auto kernel_mod = Factory<BiShengKernelMod>::Instance().Create(kernel_name);
  MS_EXCEPTION_IF_NULL(kernel_mod);
  std::vector<KernelTensor *> input_kernel_tensors = AnfAlgo::GetOrCreateAllInputKernelTensors(cnode);
  std::vector<KernelTensor *> output_kernel_tensors = AnfAlgo::GetOrCreateAllOutputKernelTensors(cnode);
  if (!kernel_mod->Init(common::AnfAlgo::GetCNodePrimitive(cnode), input_kernel_tensors, output_kernel_tensors)) {
    MS_LOG(EXCEPTION) << "Initialize bisheng kernel op[" << cnode->fullname_with_scope() << "] failed.";
  }
  if (CheckResizeCondition(cnode)) {
    if (kernel_mod->Resize(input_kernel_tensors, output_kernel_tensors) == KRET_RESIZE_FAILED) {
      MS_LOG(EXCEPTION) << "Bisheng kernel op[" << cnode->fullname_with_scope() << "] Resize failed.";
    }
  }
  if (!kernel_mod->GetWorkspaceFunc().empty()) {
    BiShengKernelArgs bisheng_args;
    for (size_t i = 0; i < common::AnfAlgo::GetInputTensorNum(cnode); ++i) {
      (void)bisheng_args.input_shapes.emplace_back(AnfAlgo::GetInputDeviceShape(cnode, i));
    }
    for (size_t i = 0; i < AnfAlgo::GetOutputTensorNum(cnode); ++i) {
      (void)bisheng_args.output_shapes.emplace_back(AnfAlgo::GetOutputDeviceShape(cnode, i));
    }
    std::vector<size_t> workspace_size_list;
    for (const auto &func : kernel_mod->GetWorkspaceFunc()) {
      MS_EXCEPTION_IF_NULL(func);
      auto workspace_size = func(bisheng_args);
      if (workspace_size != 0) {
        (void)workspace_size_list.emplace_back(workspace_size);
      }
    }
    kernel_mod->SetWorkspaceSizeList(workspace_size_list);
  }
  kernel_mod->SetNode(anf_node);
  return kernel_mod;
}
}  // namespace kernel
}  // namespace mindspore
