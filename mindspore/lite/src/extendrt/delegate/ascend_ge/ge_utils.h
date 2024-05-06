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

#ifndef MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_ASCEND_GE_GE_UTILS_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_ASCEND_GE_GE_UTILS_H_

#include <vector>
#include <string>
#include <memory>
#include <map>

#include "extendrt/infer_session.h"
#include "runtime/hardware/device_context.h"
#include "extendrt/session/lite_graph_executor.h"
#include "extendrt/delegate/ascend_ge/ge_device_context.h"
namespace mindspore {
class GeUtils {
 public:
  static Status AdaptGraph(const FuncGraphPtr &func_graph);
  static std::shared_ptr<AscendDeviceInfo> GetAscendDeviceInfo(const std::shared_ptr<mindspore::Context> &context);
};

std::string GetSocVersion();
}  // namespace mindspore
#endif  // MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_ASCEND_GE_GE_UTILS_H_
