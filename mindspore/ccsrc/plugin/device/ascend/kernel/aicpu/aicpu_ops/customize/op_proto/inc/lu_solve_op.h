/**
 * Copyright (c) 2022-2022 Huawei Technologies Co., Ltd.  All rights reserved.
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

#ifndef CUSTOMIZE_OP_PROTO_INC_LU_SOLVE_OP_H
#define CUSTOMIZE_OP_PROTO_INC_LU_SOLVE_OP_H

#include "op_proto_macro.h"

namespace ge {
REG_CUST_OP(LuSolve)
  .INPUT(x, TensorType({DT_FLOAT, DT_FLOAT16}))
  .INPUT(lu_data, TensorType({DT_FLOAT, DT_FLOAT16}))
  .INPUT(lu_pivots, TensorType({DT_INT32}))
  .OUTPUT(y, TensorType({DT_FLOAT, DT_FLOAT16}))
  .CUST_OP_END_FACTORY_REG(LuSolve)
}
#endif