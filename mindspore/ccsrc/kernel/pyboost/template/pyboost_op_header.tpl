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

#ifndef MINDSPORE_MINDSPORE_CCSRC_PIPELINE_PYNATIVE_FORWARD_PYBOOST_OP_${op_name_upper}_H_
#define MINDSPORE_MINDSPORE_CCSRC_PIPELINE_PYNATIVE_FORWARD_PYBOOST_OP_${op_name_upper}_H_

#include "kernel/pyboost/op_runner.h"
#include "kernel/pyboost/op_register.h"

namespace mindspore {
namespace kernel {
namespace pyboost {
class BACKEND_EXPORT ${op_name} : public pyboost::OpRunner {
 public:
  ${op_name}(PrimitivePtr primitive, const DeviceContext *device_context)
      : OpRunner(std::move(primitive), device_context) {}
  ~${op_name}() override = default;

  virtual ${return_type} Call(${call_args}) = 0;

 protected:
  static const std::string &op_name() {return op_name_;}

  inline static std::string op_name_ = "${op_name}";
};

}  // namespace pyboost
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_MINDSPORE_CCSRC_PIPELINE_PYNATIVE_FORWARD_PYBOOST_OP_${op_name_upper}_H_
