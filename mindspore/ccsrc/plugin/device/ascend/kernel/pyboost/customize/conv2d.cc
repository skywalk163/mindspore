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

#include "plugin/device/ascend/kernel/pyboost/customize/conv2d.h"
#include <memory>
#include "plugin/device/ascend/hal/device/ascend_stream_manager.h"
#include "kernel/pyboost/pyboost_utils.h"
#include "plugin/device/ascend/kernel/pyboost/aclnn_utils.h"

namespace mindspore {
namespace kernel {
namespace pyboost {
namespace {
std::vector<int64_t> ExpandVector(const std::vector<int64_t> &value, const size_t &expected_dim) {
  if (value.size() == 1) {
    std::vector<int64_t> expand_vec;
    for (size_t i = 0; i < expected_dim; i++) {
      expand_vec.emplace_back(value[0]);
    }
    return expand_vec;
  }
  return value;
}

tensor::TensorPtr Conv2DAscendCall(const std::shared_ptr<OpRunner> &op, const device::DeviceContext *device_context,
                                   const tensor::TensorPtr &input_tensor, const tensor::TensorPtr &weight_tensor,
                                   const std::optional<tensor::TensorPtr> &bias_tensor,
                                   const std::vector<int64_t> &stride, const std::vector<int64_t> &padding,
                                   const std::vector<int64_t> &dilation, const int64_t &groups,
                                   const std::vector<tensor::TensorPtr> &outputs) {
  MS_LOG(DEBUG) << "Call start";
  const size_t dim = 2;
  const auto &expand_stride = ExpandVector(stride, dim);
  const auto &expand_padding = ExpandVector(padding, dim);
  const auto &expand_dilation = ExpandVector(dilation, dim);
  auto transposed = false;
  std::vector<int64_t> output_padding = {0, 0};
  if (outputs.empty()) {
    MS_LOG(EXCEPTION) << "outputs is empty";
    return nullptr;
  }
  auto cube_math_type = GetCubeMathType();
  LAUNCH_ACLNN(aclnnConvolution, device_context, op->stream_id(), input_tensor, weight_tensor, bias_tensor,
               expand_stride, expand_padding, expand_dilation, transposed, output_padding, groups, outputs[0],
               cube_math_type);
  MS_LOG(DEBUG) << "Launch end";
  return outputs[0];
}
}  // namespace

tensor::TensorPtr Conv2DAscendCustomize(const std::shared_ptr<OpRunner> &op, const TensorPtr &input_tensor,
                                        const TensorPtr &weight_tensor, const std::optional<TensorPtr> &bias_tensor,
                                        const ValueTuplePtr &stride, const ValueTuplePtr &padding,
                                        const ValueTuplePtr &dilation, const Int64ImmPtr &groups) {
  OpRunner::InferOpOutput(op, input_tensor, weight_tensor, bias_tensor, stride, padding, dilation, groups);
  // Convert ValueTuple to std::vector
  std::vector<int64_t> stride_vector = ConvertValueTupleToVector<int64_t>(stride);
  std::vector<int64_t> padding_vector = ConvertValueTupleToVector<int64_t>(padding);
  std::vector<int64_t> dilation_vector = ConvertValueTupleToVector<int64_t>(dilation);
  // Convert ValuePtr to c++ scalar
  auto groups_imm = GetValue<int64_t>(groups);

  PyBoostUtils::PrepareOpInputs(op->device_context(), op->stream_id(), input_tensor, weight_tensor, bias_tensor);
  PyBoostUtils::PrepareOpOutputs(op->device_context(), op->stream_id(), op->outputs());

  // Async
  PyBoostUtils::DispatchRun(std::make_shared<runtime::PyBoostDeviceTask>(
    [op, input_tensor, weight_tensor, bias_tensor, stride_vector, padding_vector, dilation_vector, groups_imm]() {
      MS_LOG(DEBUG) << "Run device task Conv2d start";
      auto device_context = op->device_context();
      const auto &outputs = op->outputs();
      // Malloc for input tensors
      PyBoostUtils::MallocOpInputs(op->device_context(), input_tensor, weight_tensor, bias_tensor);
      // Malloc for output tensors
      PyBoostUtils::MallocOpOutputs(op->device_context(), op->outputs());

      Conv2DAscendCall(op, device_context, input_tensor, weight_tensor, bias_tensor, stride_vector, padding_vector,
                       dilation_vector, groups_imm, outputs);
      MS_LOG(DEBUG) << "Run device task Conv2d end";
    }));
  return op->output(0);
}
}  // namespace pyboost
}  // namespace kernel
}  // namespace mindspore
