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

#include "plugin/device/ascend/hal/device/ascend_device_synchronizer.h"
#include "plugin/device/ascend/hal/device/ascend_stream_manager.h"
#include "include/common/utils/utils.h"
#include "utils/log_adapter.h"
#include "transform/symbol/acl_rt_symbol.h"
#include "transform/symbol/symbol_utils.h"

namespace mindspore {
namespace device {
namespace ascend {
bool AscendDeviceSynchronizer::SyncDeviceToHost(void *host_ptr, const void *device_ptr, size_t size,
                                                const std::string &device_name, uint32_t device_id,
                                                mindspore::Format format, const ShapeVector &shape, size_t stream_id,
                                                const UserDataPtr &user_data) const {
  MS_EXCEPTION_IF_NULL(host_ptr);
  MS_EXCEPTION_IF_NULL(device_ptr);
  auto stream = AscendStreamMng::GetInstance().GetStream(stream_id);
  if (stream == nullptr) {
    stream = AscendStreamMng::GetInstance().GetStream(kDefaultStreamIndex);
  }
  MS_ERROR_IF_NULL(stream);

  auto ascend_device_context = DeviceContextManager::GetInstance().GetOrCreateDeviceContext({device_name, device_id});
  MS_EXCEPTION_IF_NULL(ascend_device_context);
  if (!ascend_device_context->device_res_manager_->BindDeviceToCurrentThread(false)) {
    MS_LOG(WARNING) << "Bind device to current thread failed.";
  }

  auto ret = CALL_ASCEND_API(aclrtMemcpyAsync, host_ptr, size, device_ptr, size, ACL_MEMCPY_DEVICE_TO_HOST, stream);
  if (ret != ACL_ERROR_NONE) {
    MS_LOG(ERROR) << "Call aclrtMemcpyAsync device to host failed, the error num[" << ret << "]";
    return false;
  }

  if (!AscendStreamMng::GetInstance().SyncStream(stream)) {
    MS_LOG(ERROR) << "Sync stream failed, stream id: " << stream_id;
    return false;
  }
  return true;
}

bool AscendDeviceSynchronizer::SyncHostToDevice(void *device_ptr, const void *host_ptr, size_t size,
                                                const std::string &device_name, uint32_t device_id,
                                                mindspore::Format format, const ShapeVector &shape, size_t stream_id,
                                                const UserDataPtr &user_data) const {
  MS_EXCEPTION_IF_NULL(device_ptr);
  MS_EXCEPTION_IF_NULL(host_ptr);
  auto stream = AscendStreamMng::GetInstance().GetStream(stream_id);
  if (stream == nullptr) {
    stream = AscendStreamMng::GetInstance().GetStream(kDefaultStreamIndex);
  }
  MS_ERROR_IF_NULL(stream);

  auto ascend_device_context = DeviceContextManager::GetInstance().GetOrCreateDeviceContext({device_name, device_id});
  MS_EXCEPTION_IF_NULL(ascend_device_context);
  if (!ascend_device_context->device_res_manager_->BindDeviceToCurrentThread(false)) {
    MS_LOG(WARNING) << "Bind device to current thread failed.";
  }

  auto ret = CALL_ASCEND_API(aclrtMemcpyAsync, device_ptr, size, host_ptr, size, ACL_MEMCPY_HOST_TO_DEVICE, stream);
  if (ret != ACL_ERROR_NONE) {
    MS_LOG(ERROR) << "Call aclrtMemcpyAsync device to host failed, the error num[" << ret << "]";
    return false;
  }

  if (!AscendStreamMng::GetInstance().SyncStream(stream)) {
    MS_LOG(ERROR) << "Sync stream failed, stream id: " << stream_id;
    return false;
  }
  return true;
}
}  // namespace ascend
}  // namespace device
}  // namespace mindspore
