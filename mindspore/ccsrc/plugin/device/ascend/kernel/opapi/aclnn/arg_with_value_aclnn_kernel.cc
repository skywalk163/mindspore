/**
 * Copyright 2024 Huawei Technologies Co., Ltd
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
#include "plugin/device/ascend/kernel/opapi/aclnn/arg_with_value_aclnn_kernel.h"
#include <algorithm>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include "ir/tensor.h"
#include "runtime/device/kernel_runtime.h"
#include "transform/acl_ir/acl_helper.h"
#include "transform/acl_ir/op_api_convert.h"
#include "abstract/ops/primitive_infer_map.h"

namespace mindspore {
namespace kernel {

void ArgWithValueAscend::GetWorkSpaceInfo(const std::vector<KernelTensor *> &inputs,
                                          const std::vector<KernelTensor *> &outputs) {
  axis_ = transform::ConvertKernelTensor<int64_t>(inputs[kIndex1]);
  keep_dims_ = transform::ConvertKernelTensor<bool>(inputs[kIndex2]);
  GetWorkspaceForResize(inputs[kIndex0], axis_, keep_dims_, outputs[kIndex1], outputs[kIndex0]);
}

bool ArgWithValueAscend::Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                                const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  MS_EXCEPTION_IF_NULL(stream_ptr);
  ParseGenExecutor(
    GEN_EXECUTOR_BOOST(op_type_, hash_id_, inputs[kIndex0], axis_, keep_dims_, outputs[kIndex1], outputs[kIndex0]));
  RunOp(stream_ptr, workspace);
  return true;
}

MS_ACLNN_KERNEL_FACTORY_REG(ArgMaxWithValue, ArgMaxWithValueAscend);
MS_ACLNN_KERNEL_FACTORY_REG(ArgMinWithValue, ArgMinWithValueAscend);
}  // namespace kernel
}  // namespace mindspore
