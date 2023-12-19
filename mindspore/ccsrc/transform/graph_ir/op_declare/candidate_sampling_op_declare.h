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

#ifndef MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_CANDIDATE_SAMPLING_OPS_DECLARE_H_
#define MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_CANDIDATE_SAMPLING_OPS_DECLARE_H_
#include "inc/ops/candidate_sampling_ops.h"
#include "transform/graph_ir/op_declare/op_declare_macro.h"
#include "utils/hash_map.h"

DECLARE_OP_ADAPTER(ComputeAccidentalHits)
DECLARE_OP_USE_OUTPUT(ComputeAccidentalHits)
#endif  // MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_CANDIDATE_SAMPLING_OPS_DECLARE_H_
