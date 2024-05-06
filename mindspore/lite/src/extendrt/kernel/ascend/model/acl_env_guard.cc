/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "extendrt/kernel/ascend/model/acl_env_guard.h"
#include "extendrt/kernel/ascend/model/model_infer.h"
#include "common/log_adapter.h"
#include "transform/symbol/acl_symbol.h"
#include "transform/symbol/symbol_utils.h"

namespace mindspore::kernel {
namespace acl {
std::shared_ptr<AclEnvGuard> AclEnvGuard::global_acl_env_ = nullptr;
std::vector<std::shared_ptr<ModelInfer>> AclEnvGuard::model_infers_ = {};
std::mutex AclEnvGuard::global_acl_env_mutex_;

AclInitAdapter &AclInitAdapter::GetInstance() {
  static AclInitAdapter instance = {};
  return instance;
}

aclError AclInitAdapter::AclInit(const char *config_file) {
  std::lock_guard<std::mutex> lock(flag_mutex_);
  if (init_flag_) {
    return ACL_ERROR_NONE;
  }

  init_flag_ = true;
  transform::LoadAscendApiSymbols();
  auto ret = CALL_ASCEND_API(aclInit, config_file);
  if (ret == ACL_ERROR_REPEAT_INITIALIZE) {
    MS_LOG(WARNING) << "acl is repeat init";
    is_repeat_init_ = true;
  }
  return ret;
}

aclError AclInitAdapter::AclFinalize() {
  std::lock_guard<std::mutex> lock(flag_mutex_);
  if (!init_flag_) {
    MS_LOG(INFO) << "Acl had been finalized.";
    return ACL_ERROR_NONE;
  }

  MS_LOG(INFO) << "Begin to aclFinalize.";
  init_flag_ = false;
  if (!is_repeat_init_) {
    MS_LOG(INFO) << "AclInitAdapter::aclFinalize begin.";
    auto rt_ret = CALL_ASCEND_API(aclFinalize);
    if (rt_ret != ACL_ERROR_NONE) {
      MS_LOG(ERROR) << "aclFinalize failed.";
    }
    MS_LOG(INFO) << "AclInitAdapter::aclFinalize end.";
    return rt_ret;
  } else {
    MS_LOG(WARNING) << "has repeat init, not aclFinalize";
  }
  return ACL_ERROR_NONE;
}

aclError AclInitAdapter::ForceFinalize() {
  std::lock_guard<std::mutex> lock(flag_mutex_);
  MS_LOG(INFO) << "Begin to force aclFinalize.";
  init_flag_ = false;
  if (!is_repeat_init_) {
    auto rt_ret = CALL_ASCEND_API(aclFinalize);
    if (rt_ret != ACL_ERROR_NONE) {
      MS_LOG(ERROR) << "aclFinalize failed.";
    }
    return rt_ret;
  } else {
    MS_LOG(WARNING) << "has repeat init, not aclFinalize";
  }
  return ACL_ERROR_NONE;
}

AclEnvGuard::AclEnvGuard() : errno_(AclInitAdapter::GetInstance().AclInit(nullptr)) {
  if (errno_ != ACL_ERROR_NONE && errno_ != ACL_ERROR_REPEAT_INITIALIZE) {
    MS_LOG(ERROR) << "Execute aclInit failed.";
    return;
  }
  MS_LOG(INFO) << "Execute aclInit success.";
}

AclEnvGuard::AclEnvGuard(std::string_view cfg_file) : errno_(AclInitAdapter::GetInstance().AclInit(cfg_file.data())) {
  if (errno_ != ACL_ERROR_NONE && errno_ != ACL_ERROR_REPEAT_INITIALIZE) {
    MS_LOG(ERROR) << "Execute aclInit failed.";
    return;
  }
  MS_LOG(INFO) << "Execute aclInit success.";
}

AclEnvGuard::~AclEnvGuard() {
  errno_ = AclInitAdapter::GetInstance().AclFinalize();
  if (errno_ != ACL_ERROR_NONE && errno_ != ACL_ERROR_REPEAT_FINALIZE) {
    MS_LOG(ERROR) << "Execute AclFinalize failed.";
  }
  MS_LOG(INFO) << "Execute AclFinalize success.";
}

std::shared_ptr<AclEnvGuard> AclEnvGuard::GetAclEnv() {
  std::shared_ptr<AclEnvGuard> acl_env;

  std::lock_guard<std::mutex> lock(global_acl_env_mutex_);
  acl_env = global_acl_env_;
  if (acl_env != nullptr) {
    MS_LOG(INFO) << "Acl has been initialized, skip.";
  } else {
    acl_env = std::make_shared<AclEnvGuard>();
    aclError ret = acl_env->GetErrno();
    if (ret != ACL_ERROR_NONE && ret != ACL_ERROR_REPEAT_INITIALIZE) {
      MS_LOG(ERROR) << "Execute aclInit failed.";
      return nullptr;
    }
    global_acl_env_ = acl_env;
    MS_LOG(INFO) << "Execute aclInit success.";
  }
  return acl_env;
}

std::shared_ptr<AclEnvGuard> AclEnvGuard::GetAclEnv(std::string_view cfg_file) {
  std::shared_ptr<AclEnvGuard> acl_env;

  std::lock_guard<std::mutex> lock(global_acl_env_mutex_);
  acl_env = global_acl_env_;
  if (acl_env != nullptr) {
    MS_LOG(INFO) << "Acl has been initialized, skip.";
    if (!cfg_file.empty()) {
      MS_LOG(WARNING) << "Dump config file option " << cfg_file << " is ignored.";
    }
  } else {
    acl_env = std::make_shared<AclEnvGuard>(cfg_file);
    aclError ret = acl_env->GetErrno();
    if (ret != ACL_ERROR_NONE && ret != ACL_ERROR_REPEAT_INITIALIZE) {
      MS_LOG(ERROR) << "Execute aclInit failed.";
      return nullptr;
    }
    global_acl_env_ = acl_env;
    MS_LOG(INFO) << "Execute aclInit success.";
  }
  return acl_env;
}

void AclEnvGuard::AddModel(const std::shared_ptr<ModelInfer> &model_infer) {
  std::lock_guard<std::mutex> lock(global_acl_env_mutex_);
  model_infers_.push_back(model_infer);
}

bool AclEnvGuard::Finalize() {
  std::lock_guard<std::mutex> lock(global_acl_env_mutex_);
  bool model_finalized =
    std::all_of(model_infers_.begin(), model_infers_.end(),
                [](const std::shared_ptr<ModelInfer> &model_infer) { return model_infer->Finalize(); });
  if (!model_finalized || global_acl_env_.use_count() > 1) {
    MS_LOG(ERROR) << "There is model has not been unloaded, and will not finalize acl.";
    return false;
  }
  auto ret = AclInitAdapter::GetInstance().AclFinalize();
  if (ret != ACL_ERROR_NONE && ret != ACL_ERROR_REPEAT_FINALIZE) {
    MS_LOG(ERROR) << "Execute acl env finalize failed.";
    return false;
  }
  MS_LOG(INFO) << "Execute acl env finalize success.";
  return true;
}
}  // namespace acl
}  // namespace mindspore::kernel
