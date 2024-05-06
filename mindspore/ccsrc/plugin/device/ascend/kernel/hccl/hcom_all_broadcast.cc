/**
 * Copyright 2019 Huawei Technologies Co., Ltd
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

#include "plugin/device/ascend/kernel/hccl/hcom_all_broadcast.h"
#include "utils/ms_context.h"
#include "plugin/device/ascend/hal/hccl_adapter/hccl_adapter.h"

namespace mindspore {
namespace kernel {
bool HcomAllBroadCastKernel::Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                                    const std::vector<KernelTensor *> &, void *stream_ptr) {
  MS_LOG(DEBUG) << "HcomAllBroadCast launch";
  if (inputs.empty() || hccl_data_type_list_.empty()) {
    MS_LOG(ERROR) << "BroadCast param is empty";
    return false;
  }
  MS_EXCEPTION_IF_NULL(inputs[0]);
  MS_EXCEPTION_IF_NULL(stream_ptr);

#ifdef ENABLE_INTERNAL_KERNELS
  if (!common::GetEnv("MS_ENABLE_LCCL").empty()) {
    auto lccl_result =
      lccl_comm_->Broadcast(inputs[0]->device_ptr(), hccl_count_, hccl_data_type_list_[0], root_id_, stream_ptr);
    if (lccl_result != Lcal::LCAL_SUCCESS) {
      MS_LOG(EXCEPTION) << "LCCL Broadcast failed.";
    }
  } else {
    auto hccl_result = hccl::HcclAdapter::GetInstance().HcclBroadcast(
      inputs[0]->device_ptr(), hccl_count_, hccl_data_type_list_[0], root_id_, stream_ptr, comm_);
    if (hccl_result != HCCL_SUCCESS) {
      MS_LOG(ERROR) << "HcomBroadcastOp : hcom_broadcast failed, return: " << hccl_result;
      return false;
    }
  }
#else
  auto hccl_result = hccl::HcclAdapter::GetInstance().HcclBroadcast(
    inputs[0]->device_ptr(), hccl_count_, hccl_data_type_list_[0], root_id_, stream_ptr, comm_);
  if (hccl_result != HCCL_SUCCESS) {
    MS_LOG(ERROR) << "HcomBroadcastOp : hcom_broadcast failed, return: " << hccl_result;
    return false;
  }
#endif
  return true;
}
}  // namespace kernel
}  // namespace mindspore
