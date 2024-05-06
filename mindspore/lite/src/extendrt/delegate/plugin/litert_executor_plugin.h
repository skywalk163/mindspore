/**
 * Copyright 2019-2021 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_LITE_SRC_EXTENDRT_LITERT_EXECUTOR_PLUGIN_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_LITERT_EXECUTOR_PLUGIN_H_
#include "include/api/status.h"
#include "utils/log_adapter.h"
#include "mindapi/base/macros.h"

namespace mindspore::infer {
class MS_API LiteRTExecutorPlugin {
 public:
  static LiteRTExecutorPlugin &GetInstance();
  bool Register();

 private:
  LiteRTExecutorPlugin();
  ~LiteRTExecutorPlugin();

  void *handle_ = nullptr;
  bool is_registered_ = false;
};

class LiteRTExecutorPluginImplBase {
 public:
  LiteRTExecutorPluginImplBase() = default;
  virtual ~LiteRTExecutorPluginImplBase() = default;
};
}  // namespace mindspore::infer
#endif  // MINDSPORE_LITE_SRC_EXTENDRT_LITERT_EXECUTOR_PLUGIN_H_
