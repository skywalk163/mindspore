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

#include "plugin/device/cpu/kernel/rpc/rpc_recv_kernel.h"
#include <map>
#include <utility>
#include <algorithm>
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
bool RpcRecvKernelMod::Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                              const std::vector<KernelTensor *> &) {
  if (recv_monad_) {
    MS_LOG(DEBUG) << "RpcRecv has a monad as input, no need to launch it.";
    return true;
  }

  MS_EXCEPTION_IF_NULL(remote_input_);
  // If the string body is not empty, it means we need to copy data from 'body' instead of raw pointer 'data'.
  bool use_string_msg = !remote_input_->Body().empty();
  auto data_ptr = use_string_msg ? (remote_input_->Body().data()) : (static_cast<char *>(remote_input_->data));
  size_t data_size = use_string_msg ? (remote_input_->Body().size()) : (remote_input_->size);

  if (is_dynamic_shape_) {
    if (real_data_offset_.empty()) {
      MS_LOG(EXCEPTION) << "Dynamic shape data must have data offsets to copy from source message.";
    }
    for (size_t i = 0; i < inputs.size(); i++) {
      MS_EXCEPTION_IF_NULL(inputs[i]->device_ptr());
      int ret =
        memcpy_s(inputs[i]->device_ptr(), inputs[i]->size(), data_ptr + real_data_offset_[i], inputs[i]->size());
      if (ret != EOK) {
        MS_LOG(EXCEPTION) << "memcpy_s for recv output " << i << " failed, ret code: " << ret;
      }
    }
  } else {
    size_t offset = 0;
    for (size_t i = 0; i < inputs.size(); i++) {
      MS_EXCEPTION_IF_NULL(inputs[i]->device_ptr());
      int ret = memcpy_s(inputs[i]->device_ptr(), inputs[i]->size(), data_ptr + offset, inputs[i]->size());
      if (ret != EOK) {
        MS_LOG(EXCEPTION) << "memcpy_s for recv output failed, ret code: " << ret;
      }

      offset += inputs[i]->size();
      // Maybe the size of data from remote is smaller than inputs size, need to break in advance to avoid illegal
      // memory access. For example, the 'umonad' inputs of RpcRecvKernel is not sent from remote.
      // This should be fixed in graph optimizing step.
      if (offset == data_size) {
        break;
      }
    }
  }

  // Pay attention that the remote_input_ is a pointer of MessageBase which is allocated as heap memory by rpc module.
  // We need to delete it after launching kernel.
  delete remote_input_;
  return true;
}

bool RpcRecvKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  // If the inputs size is 0, this means the input is a monad value.
  if (inputs.empty()) {
    recv_monad_ = true;
  }

  // The dynamic shape flag affects the 'Launch' method.
  is_dynamic_shape_ = std::any_of(inputs.begin(), inputs.end(),
                                  [](const auto &kernel_tensor) { return kernel_tensor->IsDynamicShape(); });
  return true;
}

std::vector<KernelAttr> RpcRecvKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list = {KernelAttr().AddSkipCheckAttr(true).AddAllOutInRef(true)};
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, RpcRecv, RpcRecvKernelMod);
}  // namespace kernel
}  // namespace mindspore
