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

#ifndef MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_NN_CALCULATION_OPS_DECLARE_H_
#define MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_NN_CALCULATION_OPS_DECLARE_H_

#include "transform/graph_ir/op_declare/op_declare_macro.h"
#include "transform/graph_ir/custom_op_proto/wkv_ops.h"
#include "utils/hash_map.h"

DECLARE_OP_ADAPTER(BiasAddGrad)
DECLARE_OP_USE_OUTPUT(BiasAddGrad)

DECLARE_OP_ADAPTER(Conv2D)
DECLARE_OP_USE_OUTPUT(Conv2D)

DECLARE_OP_ADAPTER(Conv2DBackpropInput)
DECLARE_OP_USE_OUTPUT(Conv2DBackpropInput)

DECLARE_OP_ADAPTER(Conv2DBackpropInputD)
DECLARE_OP_USE_INPUT_ATTR(Conv2DBackpropInputD)
DECLARE_OP_USE_OUTPUT(Conv2DBackpropInputD)

DECLARE_OP_ADAPTER(Conv2DBackpropFilter)
DECLARE_OP_USE_OUTPUT(Conv2DBackpropFilter)

DECLARE_OP_ADAPTER(Conv2DBackpropFilterD)
DECLARE_OP_USE_INPUT_ATTR(Conv2DBackpropFilterD)
DECLARE_OP_USE_OUTPUT(Conv2DBackpropFilterD)

DECLARE_OP_ADAPTER(Conv3DTransposeD)
DECLARE_OP_USE_OUTPUT(Conv3DTransposeD)

DECLARE_OP_ADAPTER(Conv3D)
DECLARE_OP_USE_OUTPUT(Conv3D)

DECLARE_OP_ADAPTER(Conv3DBackpropInput)
DECLARE_OP_USE_OUTPUT(Conv3DBackpropInput)

DECLARE_OP_ADAPTER(DepthwiseConv2D)
DECLARE_OP_USE_OUTPUT(DepthwiseConv2D)

DECLARE_OP_ADAPTER(DepthwiseConv2DBackpropFilterD)
DECLARE_OP_USE_INPUT_ATTR(DepthwiseConv2DBackpropFilterD)
DECLARE_OP_USE_OUTPUT(DepthwiseConv2DBackpropFilterD)

DECLARE_OP_ADAPTER(DepthwiseConv2DBackpropInputD)
DECLARE_OP_USE_INPUT_ATTR(DepthwiseConv2DBackpropInputD)
DECLARE_OP_USE_OUTPUT(DepthwiseConv2DBackpropInputD)

DECLARE_OP_ADAPTER(Deconvolution)
DECLARE_OP_USE_OUTPUT(Deconvolution)

DECLARE_OP_ADAPTER(Conv2DTransposeD)
DECLARE_OP_USE_OUTPUT(Conv2DTransposeD)

DECLARE_OP_ADAPTER(DeformableOffsets)
DECLARE_OP_USE_OUTPUT(DeformableOffsets)

DECLARE_OP_ADAPTER(DeformableOffsetsGrad)
DECLARE_OP_USE_OUTPUT(DeformableOffsetsGrad)

DECLARE_OP_ADAPTER(Conv3DBackpropFilter)
DECLARE_OP_USE_OUTPUT(Conv3DBackpropFilter)

DECLARE_OP_ADAPTER(Conv3DTranspose)
DECLARE_OP_USE_OUTPUT(Conv3DTranspose)

DECLARE_OP_ADAPTER(DeformableConv2D)
DECLARE_OP_USE_OUTPUT(DeformableConv2D)

DECLARE_OP_ADAPTER(Wkv)
DECLARE_OP_USE_OUTPUT(Wkv)

DECLARE_OP_ADAPTER(WkvGrad)
DECLARE_OP_USE_OUTPUT(WkvGrad)

DECLARE_OP_ADAPTER(Conv2DTranspose)
DECLARE_OP_USE_OUTPUT(Conv2DTranspose)
#endif  // MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_OP_DECLARE_NN_CALCULATION_OPS_DECLARE_H_
