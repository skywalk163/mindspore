/*
 * Copyright (c) 2020.Huawei Technologies Co., Ltd. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_DVPP_UTILS_RESOURCE_MANAGER_H_
#define MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_DVPP_UTILS_RESOURCE_MANAGER_H_

#include <sys/stat.h>

#include <climits>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "acl/acl.h"

#ifndef BUILD_LITE
#include "mindspore/ccsrc/cxx_api/graph/acl/acl_env_guard.h"
#else
#include "mindspore/lite/src/extendrt/kernel/ascend/model/acl_env_guard.h"
#endif
#include "minddata/dataset/kernels/image/dvpp/utils/CommonDataType.h"
#include "minddata/dataset/kernels/image/dvpp/utils/ErrorCode.h"
#include "minddata/dataset/kernels/image/dvpp/utils/resouce_info.h"
#include "minddata/dataset/util/log_adapter.h"

#ifndef BUILD_LITE
using AclEnvGuard = mindspore::AclEnvGuard;
using AclInitAdapter = mindspore::AclInitAdapter;
#else
using AclEnvGuard = mindspore::kernel::acl::AclEnvGuard;
using AclInitAdapter = mindspore::kernel::acl::AclInitAdapter;
#endif

APP_ERROR ExistFile(const std::string &filePath);

class ResourceManager {
  friend APP_ERROR ExistFile(const std::string &filePath);

 public:
  ResourceManager() = default;

  ~ResourceManager() = default;

  // Get the Instance of resource manager
  static std::shared_ptr<ResourceManager> GetInstance();

  // Init the resource of resource manager
  APP_ERROR InitResource(ResourceInfo &resourceInfo);

  aclrtContext GetContext(int deviceId);

  void Release();

  static bool GetInitStatus() { return initFlag_; }

 private:
  static std::shared_ptr<ResourceManager> ptr_;
  static bool initFlag_;
  std::vector<int> deviceIds_;
  std::vector<aclrtContext> contexts_;
  std::unordered_map<int, int> deviceIdMap_;  // Map of device to index
  std::shared_ptr<AclEnvGuard> acl_env_;
};

#endif  // MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_DVPP_UTILS_RESOURCE_MANAGER_H_
