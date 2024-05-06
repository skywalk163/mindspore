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
#include <string>
#include <utility>
#include <vector>
#include "plugin/device/ascend/kernel/opapi/aclnn_kernel_build.h"
#include "plugin/device/ascend/kernel/opapi/aclnn_kernel_mod.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "plugin/factory/ms_factory.h"
#include "kernel/framework_utils.h"
#include "ops/op_def.h"
#include "utils/trace_base.h"

namespace mindspore {
namespace kernel {
KernelModPtr AclnnOpBuild(const AnfNodePtr &anf_node) {
  MS_EXCEPTION_IF_NULL(anf_node);

  std::string opname = common::AnfAlgo::GetCNodeName(anf_node);
  MS_LOG(DEBUG) << "aclnn op [" << opname << "]";
  auto kernel_ptr = Factory<AclnnKernelMod>::Instance().Create(opname);
  if (kernel_ptr == nullptr) {
    MS_LOG(ERROR) << "aclnn can't find Kernel[" << opname << "]";
    return nullptr;
  }
  std::vector<KernelTensor *> input_kernel_tensors = AnfAlgo::GetOrCreateAllInputKernelTensors(anf_node);
  std::vector<KernelTensor *> output_kernel_tensors = AnfAlgo::GetOrCreateAllOutputKernelTensors(anf_node);

  if (!std::static_pointer_cast<KernelMod>(kernel_ptr)
         ->Init(common::AnfAlgo::GetCNodePrimitive(anf_node), input_kernel_tensors, output_kernel_tensors)) {
    MS_LOG(EXCEPTION) << "#dmsg#Kernel build failed:#dmsg#Initialize aclnn kernel op["
                      << anf_node->fullname_with_scope() << "] failed." << trace::DumpSourceLines(anf_node);
  }

  auto cnode = anf_node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  if (CheckResizeCondition(cnode)) {
    if (kernel_ptr->Resize(input_kernel_tensors, output_kernel_tensors) == KRET_RESIZE_FAILED) {
      MS_LOG(EXCEPTION) << "#dmsg#Kernel build failed:#dmsg#hostapi kernel op[" << cnode->fullname_with_scope()
                        << "] Resize failed.";
    }
  }
  transform::AclnnInit();
  return kernel_ptr;
}

bool IsRegisteredAclnnOp(const AnfNodePtr &anf_node) {
  MS_EXCEPTION_IF_NULL(anf_node);
  std::string opname = common::AnfAlgo::GetCNodeName(anf_node);
  return Factory<AclnnKernelMod>::Instance().IsRegistered(opname);
}

bool IsEnabledAclnnDispatch(const AnfNodePtr &anf_node) {
  MS_EXCEPTION_IF_NULL(anf_node);
  std::string op_name = common::AnfAlgo::GetCNodeName(anf_node);
  mindspore::ops::OpDefPtr op_def = mindspore::ops::GetOpDef(op_name);
  if (op_def == nullptr) {
    MS_LOG(INFO) << op_name << " is not defined in opdef.";
    return false;
  }
  return op_def->enable_dispatch_;
}
}  // namespace kernel
}  // namespace mindspore
