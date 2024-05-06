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
#include "plugin/device/ascend/kernel/opapi/aclnn/reduce_aclnn_kernel.h"
#include <vector>
#include "ir/tensor.h"
#include "transform/acl_ir/acl_helper.h"
#include "abstract/ops/primitive_infer_map.h"
#include "transform/symbol/acl_rt_symbol.h"
#include "transform/symbol/symbol_utils.h"

namespace mindspore {
namespace kernel {
void ReduceAclnnKernelMod::GetWorkSpaceInfo(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &outputs) {}

bool ReduceAclnnKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &workspace,
                                  const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  return true;
}

void ReduceMathAclnnKernelMod::GetWorkSpaceInfo(const std::vector<KernelTensor *> &inputs,
                                                const std::vector<KernelTensor *> &outputs) {}

bool ReduceMathAclnnKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &workspace,
                                      const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  return true;
}

void ReduceSumAclnnKernelMod::GetWorkSpaceInfo(const std::vector<KernelTensor *> &inputs,
                                               const std::vector<KernelTensor *> &outputs) {
  dims_ = transform::ConvertKernelTensor<std::vector<int64_t>>(inputs[kIndex1]);
  keep_dim_ = transform::ConvertKernelTensor<bool>(inputs[kIndex2]);
  dtype_ = transform::ConvertKernelTensor<TypeId>(inputs[kIndex0]);
  auto skip_mode = transform::ConvertKernelTensor<bool>(inputs[kIndex3]);
  if (AnfAlgo::IsDynamicShapeSkipExecute(skip_mode, inputs[kIndex1]->GetShapeVector())) {
    need_skip_execute_ = true;
    return;
  } else {
    need_skip_execute_ = false;
  }
  auto return_value =
    GEN_EXECUTOR_BOOST(op_type_, hash_id_, inputs[kIndex0], dims_, keep_dim_, dtype_, outputs[kIndex0]);
  UpdateWorkspace(return_value);
}

bool ReduceSumAclnnKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &workspace,
                                     const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  MS_EXCEPTION_IF_NULL(stream_ptr);
  if (need_skip_execute_) {
    aclError status =
      CALL_ASCEND_API(aclrtMemcpyAsync, outputs[0]->device_ptr(), outputs[0]->size(), inputs[0]->device_ptr(),
                      inputs[0]->size(), ACL_MEMCPY_DEVICE_TO_DEVICE, stream_ptr);
    if (status != ACL_ERROR_NONE) {
      MS_LOG(ERROR) << "MemCpyAsync op aclrtMemcpyAsync failed, ret:" << status << " destMax:" << outputs[0]->size()
                    << " count:" << inputs[0]->size();
      return false;
    }
    return true;
  }
  ParseGenExecutor(GEN_EXECUTOR_BOOST(op_type_, hash_id_, inputs[kIndex0], dims_, keep_dim_, dtype_, outputs[kIndex0]));
  RunOp(stream_ptr, workspace);
  return true;
}
}  // namespace kernel
}  // namespace mindspore
