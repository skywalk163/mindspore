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

#include "plugin/device/ascend/kernel/pyboost/customize/gather_d_grad_v2.h"
#include <memory>
#include "plugin/device/ascend/hal/device/ascend_stream_manager.h"
#include "kernel/pyboost/op_register.h"
#include "kernel/pyboost/pyboost_utils.h"
#include "plugin/device/ascend/kernel/pyboost/aclnn_utils.h"

namespace mindspore {
namespace kernel {
namespace pyboost {
tensor::TensorPtr GatherDGradAscendCustomize(const std::shared_ptr<OpRunner> &op, const TensorPtr &x,
                                             const Int64ImmPtr dim, const TensorPtr &index, const TensorPtr &d_out) {
  MS_EXCEPTION_IF_NULL(dim);
  MS_EXCEPTION_IF_NULL(op);
  MS_EXCEPTION_IF_NULL(x);
  MS_EXCEPTION_IF_NULL(index);
  MS_EXCEPTION_IF_NULL(d_out);

  OpRunner::InferOpOutput(op, x, dim, index, d_out);
  auto dim_value = dim->value();
  PyBoostUtils::PrepareOpInputs(op->device_context(), op->stream_id(), d_out);
  PyBoostUtils::PrepareOpOutputs(op->device_context(), op->stream_id(), op->outputs());

  // Async
  PyBoostUtils::DispatchRun(std::make_shared<runtime::PyBoostDeviceTask>([op, dim_value, index, d_out]() {
    MS_LOG(DEBUG) << "Run device task GatherDGrad start";
    auto device_context = op->device_context();
    const auto &outputs = op->outputs();
    // Malloc for input tensors
    PyBoostUtils::MallocOpInputs(device_context, index, d_out);
    // Malloc for output tensors
    PyBoostUtils::MallocOpOutputs(device_context, outputs);
    LAUNCH_ACLNN(aclnnInplaceZero, device_context, op->stream_id(), outputs[0]);
    LAUNCH_ACLNN(aclnnScatterAdd, device_context, op->stream_id(), outputs[0], dim_value, index, d_out, outputs[0]);
    MS_LOG(DEBUG) << "Run device task GatherDGrad end";
  }));
  return op->output(0);
}
}  // namespace pyboost
}  // namespace kernel
}  // namespace mindspore
