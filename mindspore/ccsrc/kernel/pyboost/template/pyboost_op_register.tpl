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

#include "kernel/pyboost/op_register.h"
${op_includes}

namespace mindspore {
namespace kernel {
namespace pyboost {
template <typename T>
OpFactory<T> &OpFactory<T>::Get() {
  static OpFactory<T> instance;
  return instance;
}

template <typename T>
std::shared_ptr<T> OpFactory<T>::Create(const string &device) {
  auto iter = op_creator_.find(device);
  if (iter == op_creator_.end()) {
    MS_LOG(EXCEPTION) << "Not found op " << typeid(T).name() << " on device " << device;
  }
  return iter->second();
}

${op_factory_templates}
}  // namespace pyboost
}  // namespace kernel
}  // namespace mindspore
