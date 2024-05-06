/**
 * Copyright 2019-2024 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_ENVIRON_OPS_DECLARE_H_
#define MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_ENVIRON_OPS_DECLARE_H_

#include "transform/graph_ir/custom_op_proto/cust_environ_ops.h"
#include "transform/graph_ir/op_declare/op_declare_macro.h"
#include "utils/hash_map.h"

DECLARE_CUST_OP_ADAPTER(EnvironCreate)
DECLARE_CUST_OP_USE_OUTPUT(EnvironCreate)

DECLARE_CUST_OP_ADAPTER(EnvironDestroyAll)
DECLARE_CUST_OP_USE_OUTPUT(EnvironDestroyAll)

DECLARE_CUST_OP_ADAPTER(EnvironSet)
DECLARE_CUST_OP_USE_OUTPUT(EnvironSet)

DECLARE_CUST_OP_ADAPTER(EnvironGet)
DECLARE_CUST_OP_USE_OUTPUT(EnvironGet)
#endif  // MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_ENVIRON_OPS_DECLARE_H_
