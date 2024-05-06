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

#ifndef MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_SPLIT_COMBINATION_OPS_DECLARE_H_
#define MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_SPLIT_COMBINATION_OPS_DECLARE_H_

#include "transform/graph_ir/custom_op_proto/cust_other_ops.h"
#include "transform/graph_ir/op_declare/op_declare_macro.h"
#include "utils/hash_map.h"

DECLARE_OP_ADAPTER(SplitD)
DECLARE_OP_USE_DYN_OUTPUT(SplitD)

DECLARE_OP_ADAPTER(Split)
DECLARE_OP_USE_DYN_OUTPUT(Split)

DECLARE_OP_ADAPTER(ConcatD)
DECLARE_OP_USE_DYN_INPUT(ConcatD)
DECLARE_OP_USE_OUTPUT(ConcatD)

DECLARE_OP_ADAPTER(Concat)
DECLARE_OP_USE_OUTPUT(Concat)

DECLARE_OP_ADAPTER(ConcatV2)
DECLARE_OP_USE_DYN_INPUT(ConcatV2)
DECLARE_OP_USE_OUTPUT(ConcatV2)

DECLARE_OP_ADAPTER(ParallelConcat)
DECLARE_OP_USE_DYN_INPUT(ParallelConcat)
DECLARE_OP_USE_OUTPUT(ParallelConcat)

DECLARE_OP_ADAPTER(Pack)
DECLARE_OP_USE_DYN_INPUT(Pack)
DECLARE_OP_USE_OUTPUT(Pack)

DECLARE_OP_ADAPTER(SplitV)
DECLARE_OP_USE_DYN_OUTPUT(SplitV)

DECLARE_CUST_OP_ADAPTER(ConcatOffset)
DECLARE_CUST_OP_USE_DYN_INPUT(ConcatOffset)
DECLARE_CUST_OP_USE_DYN_OUTPUT(ConcatOffset)
DECLARE_CUST_OP_USE_OUTPUT(ConcatOffset)
#endif  // MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_SPLIT_COMBINATION_OPS_DECLARE_H_
