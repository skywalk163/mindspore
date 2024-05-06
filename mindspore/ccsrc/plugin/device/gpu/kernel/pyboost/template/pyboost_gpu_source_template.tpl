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

#include "plugin/device/gpu/kernel/pyboost/auto_generate/${operator_name}.h"
#include "runtime/hardware/device_context_manager.h"
#include "plugin/device/gpu/hal/device/gpu_device_manager.h"
${customize_include}

namespace mindspore {
namespace kernel {
namespace pyboost {
${return_type} ${op_name}GPU::Call(${call_args_with_type}) {
  ${call_impl}
}
MS_REG_PYBOOST_OP(GPU, ${op_name});
}  // namespace pyboost
}  // namespace kernel
}  // namespace mindspore
