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

#include "transform/acl_ir/acl_adapter_info.h"

namespace mindspore {
namespace transform {

std::string Set5HDFormat(TypeId, const std::vector<ShapeVector> &) { return kOpFormat_NC1HWC0; }

REGISTER_ACL_OP(BNTrainingReduce)
  .Input(0, {"NCHW"}, "NCH")
  .set_extra_supported_datatype({ge::DT_FLOAT16})
  .OutputSelector(&Set5HDFormat);

REGISTER_ACL_OP(BNTrainingUpdate)
  .Input(0, {"NCHW"}, "NCH")
  .Output(0, 0)
  .set_extra_supported_datatype({ge::DT_FLOAT16})
  .OutputSelector(&Set5HDFormat);

REGISTER_ACL_OP(BNTrainingUpdateGrad)
  .Input(0, {"NCHW"}, "NCH")
  .Input(1, {"NCHW"}, "NCH")
  .set_extra_supported_datatype({ge::DT_FLOAT16});

REGISTER_ACL_OP(BNTrainingReduceGrad)
  .Input(0, {"NCHW"}, "NCH")
  .Input(1, {"NCHW"}, "NCH")
  .Output(0, 0)
  .set_extra_supported_datatype({ge::DT_FLOAT16});

REGISTER_ACL_OP(BNInfer).Input(0, {"NCHW"}, "NCH").Output(0, 0).set_extra_supported_datatype({ge::DT_FLOAT16});

REGISTER_ACL_OP(BNInferGrad).Input(0, {"NCHW"}, "NCH").Output(0, 0).set_extra_supported_datatype({ge::DT_FLOAT16});
}  // namespace transform
}  // namespace mindspore
