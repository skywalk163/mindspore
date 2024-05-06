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

#include "plugin/device/ascend/kernel/pyboost/customize/argmin_with_value.h"

#include <memory>
#include <string>
#include <tuple>

#include "plugin/device/ascend/hal/device/ascend_stream_manager.h"
#include "kernel/pyboost/pyboost_utils.h"
#include "plugin/device/ascend/kernel/pyboost/aclnn_utils.h"

namespace mindspore {
namespace kernel {
namespace pyboost {
std::tuple<tensor::TensorPtr, tensor::TensorPtr> ArgMinWithValueAscendCustomize(const std::shared_ptr<OpRunner> &op,
                                                                                const TensorPtr &input_tensor,
                                                                                const Int64ImmPtr &axis,
                                                                                const BoolImmPtr &keep_dims) {
  OpRunner::InferOpOutput(op, input_tensor, axis, keep_dims);
  // Convert ValuePtr to c++ scalar
  auto axis_imm = GetValue<int64_t>(axis);
  auto keep_dims_imm = GetValue<bool>(keep_dims);

  PyBoostUtils::PrepareOpInputs(op->device_context(), op->stream_id(), input_tensor);

  PyBoostUtils::PrepareOpOutputs(op->device_context(), op->stream_id(), op->outputs());

  // Async
  PyBoostUtils::DispatchRun(std::make_shared<runtime::PyBoostDeviceTask>([op, input_tensor, axis_imm, keep_dims_imm]() {
    MS_LOG(DEBUG) << "Run device task ArgMinWithValue end";
    auto device_context = op->device_context();
    const auto &outputs = op->outputs();
    // Malloc for input tensors
    PyBoostUtils::MallocOpInputs(device_context, input_tensor);
    // Malloc for output tensors
    PyBoostUtils::MallocOpOutputs(device_context, outputs);

    LAUNCH_ACLNN(aclnnMinDim, device_context, op->stream_id(), input_tensor, axis_imm, keep_dims_imm, outputs[1],
                 outputs[0]);
    MS_LOG(DEBUG) << "Run device task ArgMinWithValue end";
  }));
  return std::make_tuple(op->outputs()[0], op->outputs()[1]);
}
}  // namespace pyboost
}  // namespace kernel
}  // namespace mindspore
