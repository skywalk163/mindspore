/**
 * Copyright 2022-2024 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_DATA_FLOW_OPS_DECLARE_H_
#define MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_DATA_FLOW_OPS_DECLARE_H_

#include "transform/graph_ir/op_declare/op_declare_macro.h"

DECLARE_OP_ADAPTER(TensorArray)
DECLARE_OP_USE_OUTPUT(TensorArray)

DECLARE_OP_ADAPTER(TensorArrayWrite)
DECLARE_OP_USE_OUTPUT(TensorArrayWrite)

DECLARE_OP_ADAPTER(TensorArrayGather)
DECLARE_OP_USE_OUTPUT(TensorArrayGather)

DECLARE_OP_ADAPTER(DynamicStitch)
DECLARE_OP_USE_OUTPUT(DynamicStitch)

DECLARE_OP_ADAPTER(GetNextFromQueue)
DECLARE_OP_USE_DYN_OUTPUT(GetNextFromQueue)

DECLARE_OP_ADAPTER(DynamicGetNextV2)
DECLARE_OP_USE_DYN_OUTPUT(DynamicGetNextV2)
#endif  // MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_DATA_FLOW_OPS_DECLARE_H_
