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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RPC_RPC_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RPC_RPC_KERNEL_H_

#include "plugin/device/cpu/kernel/cpu_kernel.h"

namespace mindspore {
namespace kernel {
class RpcKernelMod : public NativeCpuKernelMod {
 public:
  RpcKernelMod() : remote_input_(nullptr), is_dynamic_shape_(false) {}
  ~RpcKernelMod() override = default;

  // Set and get remote data as input.
  void SetRemoteInput(MessageBase *const msg) { remote_input_ = msg; }
  MessageBase *GetRemoteInput() { return remote_input_; }

 protected:
  MessageBase *remote_input_;

  // Whether this kernel sends or receives dynamic shape data.
  bool is_dynamic_shape_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RPC_RPC_KERNEL_H_s
