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

#ifndef MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_REDUCE_OPS_DECLARE_H_
#define MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_REDUCE_OPS_DECLARE_H_

#include "transform/graph_ir/op_declare/op_declare_macro.h"
#include "utils/hash_map.h"

DECLARE_OP_ADAPTER(ReduceMean)
DECLARE_OP_USE_OUTPUT(ReduceMean)

DECLARE_OP_ADAPTER(ReduceMin)
DECLARE_OP_USE_OUTPUT(ReduceMin)

DECLARE_OP_ADAPTER(ReduceMax)
DECLARE_OP_USE_OUTPUT(ReduceMax)

DECLARE_OP_ADAPTER(ReduceAll)
DECLARE_OP_USE_OUTPUT(ReduceAll)

DECLARE_OP_ADAPTER(BNTrainingReduce)
DECLARE_OP_USE_OUTPUT(BNTrainingReduce)

DECLARE_OP_ADAPTER(BNTrainingReduceGrad)
DECLARE_OP_USE_OUTPUT(BNTrainingReduceGrad)

DECLARE_OP_ADAPTER(BNTrainingUpdate)
DECLARE_OP_USE_OUTPUT(BNTrainingUpdate)

DECLARE_OP_ADAPTER(BNTrainingUpdateGrad)
DECLARE_OP_USE_OUTPUT(BNTrainingUpdateGrad)

DECLARE_OP_ADAPTER(ReduceSum)
DECLARE_OP_USE_OUTPUT(ReduceSum)

DECLARE_OP_ADAPTER(ReduceAny)
DECLARE_OP_USE_OUTPUT(ReduceAny)

DECLARE_OP_ADAPTER(ReduceStd)
DECLARE_OP_USE_OUTPUT(ReduceStd)

DECLARE_OP_ADAPTER(ReduceProd)
DECLARE_OP_USE_OUTPUT(ReduceProd)

DECLARE_OP_ADAPTER(ReduceLogSumExp)
DECLARE_OP_USE_OUTPUT(ReduceLogSumExp)

DECLARE_OP_ADAPTER(ReduceLogSum)
DECLARE_OP_USE_OUTPUT(ReduceLogSum)
#endif  // MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_REDUCE_OPS_DECLARE_H_
