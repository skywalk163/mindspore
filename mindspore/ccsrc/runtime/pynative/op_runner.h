/**
 * Copyright 2022-2024 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_MINDSPORE_CCSRC_RUNTIME_RUN_OP_RUN_OP_HELPER_H_
#define MINDSPORE_MINDSPORE_CCSRC_RUNTIME_RUN_OP_RUN_OP_HELPER_H_

#include <vector>
#include <string>
#include "include/backend/kernel_graph.h"
#include "runtime/pynative/op_compiler.h"
#include "runtime/hardware/device_context.h"

namespace mindspore::runtime {
struct OpRunnerInfo {
  const PrimitivePtr &prim;
  const std::string &device_target;
  const vector<ValuePtr> &inputs;
  const abstract::AbstractBasePtrList &inputs_abs;
  const std::vector<InputType> &inputs_mask;
  abstract::AbstractBasePtr output_abs;
};

class OpRunner {
 public:
  // Update Tensor or input node DeviceAddress before PyNative async running.
  static void UpdateDeviceAddress(const KernelGraphPtr &graph,
                                  const std::vector<tensor::TensorPtr> &tensors_without_value_mask,
                                  const device::DeviceContext *device_context);

  static void RunSingleOpGraph(const session::BackendOpRunInfoPtr &op_run_info,
                               const OpCompilerInfoPtr &op_compiler_info,
                               const std::vector<tensor::TensorPtr> &input_tensors);

  static std::vector<tensor::TensorPtr> GetTensorWithoutValueMask(const session::BackendOpRunInfoPtr &op_run_info);
  static void LaunchKernelTask(const runtime::KernelTaskType &task_type, DeviceContext *device_context,
                               const device::DeviceAddressPtrList &input_addr_list,
                               const device::DeviceAddressPtrList &output_addr_list, size_t stream_id);
  BACKEND_EXPORT static DeviceContext *GetDeviceContext(const std::string &device_type);
  BACKEND_EXPORT static void ChildAfterFork();
};

class DynamicOpRunner {
 public:
  static void UpdateInputDeviceAddress(const OpCompilerInfoPtr &op_compiler_info,
                                       const std::vector<tensor::TensorPtr> &input_tensors, bool is_sync);
  static void RunSingleOpGraph(const session::BackendOpRunInfoPtr &op_run_info,
                               const OpCompilerInfoPtr &op_compiler_info,
                               const std::vector<tensor::TensorPtr> &input_tensors);
  static void CopyHostToDevice(const OpCompilerInfoPtr &op_compiler_info,
                               const std::vector<tensor::TensorPtr> &input_tensors);
};
}  // namespace mindspore::runtime
#endif  // MINDSPORE_MINDSPORE_CCSRC_RUNTIME_RUN_OP_RUN_OP_HELPER_H_
