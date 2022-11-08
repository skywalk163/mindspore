/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_OPTIMIZER_REG_CPU_CONST_INPUT_TO_ATTR_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_OPTIMIZER_REG_CPU_CONST_INPUT_TO_ATTR_H_

#include "backend/common/optimizer/op_adaptation_info_factory.h"

namespace mindspore::opt {
#define RER_CPU_STATIC_CONST_TO_ATTR(op_name, ...) RER_CONST_TO_ATTR_LIST(op_name, kCPUDevice, false, __VA_ARGS__)
#define RER_CPU_DYNAMIC_CONST_TO_ATTR(op_name, ...) RER_CONST_TO_ATTR_LIST(op_name, kCPUDevice, true, __VA_ARGS__)

RER_CPU_DYNAMIC_CONST_TO_ATTR(kCastOpName, 1);
RER_CPU_DYNAMIC_CONST_TO_ATTR(kConcatOpName, 0);
RER_CPU_DYNAMIC_CONST_TO_ATTR(kFillOpName, 0);
RER_CPU_DYNAMIC_CONST_TO_ATTR(kReduceAllOpName, 1);
RER_CPU_DYNAMIC_CONST_TO_ATTR(kReduceAnyOpName, 1);
RER_CPU_DYNAMIC_CONST_TO_ATTR(kReduceMaxOpName, 1);
RER_CPU_DYNAMIC_CONST_TO_ATTR(kReduceMeanOpName, 1);
RER_CPU_DYNAMIC_CONST_TO_ATTR(kReduceMinOpName, 1);
RER_CPU_DYNAMIC_CONST_TO_ATTR(kReduceSumOpName, 1);
RER_CPU_DYNAMIC_CONST_TO_ATTR(kReduceProdOpName, 1);
RER_CPU_DYNAMIC_CONST_TO_ATTR(kTransposeOpName, 1);

RER_CPU_STATIC_CONST_TO_ATTR(kCastOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kConv2DBackpropInputOpName, 2);
RER_CPU_STATIC_CONST_TO_ATTR(kConv2DTransposeOpName, 2);
RER_CPU_STATIC_CONST_TO_ATTR(kCOO2CSROpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kCSR2COOOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kCSRDivOpName, 3);
RER_CPU_STATIC_CONST_TO_ATTR(kCSRGatherOpName, 3);
RER_CPU_STATIC_CONST_TO_ATTR(kCSRMMOpName, 3);
RER_CPU_STATIC_CONST_TO_ATTR(kCSRMulOpName, 3);
RER_CPU_STATIC_CONST_TO_ATTR(kCSRMVOpName, 3);
RER_CPU_STATIC_CONST_TO_ATTR(kCSRReduceSumOpName, 3, 4);
RER_CPU_STATIC_CONST_TO_ATTR(kCumProdOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kCumprodOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kCumSumOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kErfOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kFillOpName, 0);
RER_CPU_STATIC_CONST_TO_ATTR(kMeanGradOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kParallelResizeBilinearGradOpName, 2);
RER_CPU_STATIC_CONST_TO_ATTR(kPullWeightOpName, 1, 2);
RER_CPU_STATIC_CONST_TO_ATTR(kReduceAllOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kReduceAnyOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kReduceMaxOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kReduceMeanOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kReduceMinOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kReduceProdOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kReduceSumOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kReshapeOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kROIAlignGradName, 2);
RER_CPU_STATIC_CONST_TO_ATTR(kSliceOpName, 1, 2);
RER_CPU_STATIC_CONST_TO_ATTR(kSparseApplyAdagradOpName, 2);
RER_CPU_STATIC_CONST_TO_ATTR(kSparseGatherV2OpName, 2);
RER_CPU_STATIC_CONST_TO_ATTR(kStridedSliceOpName, 1, 2, 3);
RER_CPU_STATIC_CONST_TO_ATTR(kSubscalarOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kTileOpName, 1);
RER_CPU_STATIC_CONST_TO_ATTR(kTransposeOpName, 1);
}  // namespace mindspore::opt

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_OPTIMIZER_REG_CPU_CONST_INPUT_TO_ATTR_H_
