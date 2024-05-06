/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/ascend/hal/hardware/ascend_communication_group.h"
#include "plugin/device/ascend/hal/common/ascend_utils.h"
#include "plugin/device/ascend/hal/hccl_adapter/hccl_adapter.h"
#include "mindspore/core/utils/ms_context.h"
#include "transform/symbol/acl_rt_symbol.h"
#include "transform/symbol/acl_symbol.h"
#include "transform/symbol/symbol_utils.h"

namespace mindspore {
namespace device {
namespace ascend {
AscendCommunicationGroup::AscendCommunicationGroup(const std::string &name, const std::vector<uint32_t> &group_ranks,
                                                   uint32_t global_rank, uint32_t local_group_rank,
                                                   uint32_t local_group_size)
    : CommunicationGroup(name, group_ranks, global_rank, local_group_rank, local_group_size),
      unique_id_({}),
      comm_(nullptr) {
  (void)memset_s(inner_comm_name_, INNER_COMM_NAME_MAX_LENGTH, 0x00, INNER_COMM_NAME_MAX_LENGTH);
}

bool AscendCommunicationGroup::Initialize(void *root_info) {
  if (initialized_) {
    return false;
  }
  if (hccl::HcclAdapter::GetInstance().UseHcclCM()) {
    // If using hccl CM envs to launch distributed job, no need to call HcclCommInitRootInfo. The group will be
    // initialized in rank table way.
    initialized_ = true;
    return true;
  }
  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  auto device_id = ms_context->get_param<uint32_t>(MS_CTX_DEVICE_ID);
  (void)CALL_ASCEND_API(aclrtSetDevice, device_id);
  unique_id_ = *(static_cast<HcclRootInfo *>(root_info));
  uint32_t group_rank;
  auto group_size = size_;
  if (!common::GetEnv(kSimulationLevel).empty()) {
    group_size = 1;
    group_rank = 0;
  } else {
    group_rank = GetGroupRank(global_rank_);
  }
  if (HcclCommInitRootInfo(static_cast<uint32_t>(group_size), &unique_id_, static_cast<uint32_t>(group_rank), &comm_) !=
      static_cast<int32_t>(HCCL_SUCCESS)) {
    const string &error_message = ErrorManagerAdapter::GetErrorMessage(true);
    MS_LOG(ERROR) << "HcclCommInitRootInfo failed. " + error_message;
    return false;
  }
  // Get HCCL comm name which is used in graph sink mode for GE.
  if (HcclGetCommName(comm_, inner_comm_name_) != static_cast<int32_t>(HCCL_SUCCESS)) {
    const string &error_message = ErrorManagerAdapter::GetErrorMessage(true);
    MS_LOG(ERROR) << "HcclGetCommName failed. " + error_message;
    return false;
  }
  initialized_ = true;
  (void)CALL_ASCEND_API(aclrtResetDevice, device_id);
  return true;
}

bool AscendCommunicationGroup::Finalize() {
  if (!initialized_) {
    return false;
  }
  if (hccl::HcclAdapter::GetInstance().UseHcclCM()) {
    // If using hccl CM envs to launch distributed job, comm_ is not initialized. So directly return.
    initialized_ = false;
    return true;
  }

  // This function will be called at a lonesome thread that has no rtContext, so HcclCommDestroy will be failed.
  // Delete these codes when these threads can be bind.
  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  auto device_id = ms_context->get_param<uint32_t>(MS_CTX_DEVICE_ID);
  (void)CALL_ASCEND_API(aclrtSetDevice, device_id);
  RETURN_IF_FALSE_WITH_LOG(HcclCommDestroy(comm_) == static_cast<int32_t>(HCCL_SUCCESS),
                           "Failed to destroy HCCL communicator.");
  (void)CALL_ASCEND_API(aclrtResetDevice, device_id);
  initialized_ = false;
  comm_ = nullptr;
  return true;
}

void *AscendCommunicationGroup::GenerateRootInfo(size_t *root_info_size) {
  *root_info_size = sizeof(unique_id_);
  if (!common::GetEnv(kSimulationLevel).empty() && !hccl::HcclAdapter::GetInstance().UseHcclCM()) {
    if (HcclGetRootInfo(&unique_id_) != static_cast<int32_t>(HCCL_SUCCESS)) {
      return nullptr;
    }
    return &unique_id_;
  }
  uint32_t group_rank = GetGroupRank(global_rank_);
  if (group_rank == 0) {
    if (hccl::HcclAdapter::GetInstance().UseHcclCM()) {
      // If using hccl CM envs to launch distributed job, no need to call HcclGetRootInfo.
      return &unique_id_;
    }
    if (HcclGetRootInfo(&unique_id_) != static_cast<int32_t>(HCCL_SUCCESS)) {
      MS_LOG(ERROR) << "Failed to get HCCL unique id: " << CALL_ASCEND_API(aclGetRecentErrMsg);
      return nullptr;
    }
  }
  return &unique_id_;
}

const HcclComm &AscendCommunicationGroup::hccl_communicator() const { return comm_; }

std::string AscendCommunicationGroup::inner_comm_name() const { return inner_comm_name_; }
}  // namespace ascend
}  // namespace device
}  // namespace mindspore
