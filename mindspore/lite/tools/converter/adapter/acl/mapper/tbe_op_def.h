/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_ADAPTER_ACL_MAPPER_TBE_OP_DEF_H_
#define MINDSPORE_LITE_TOOLS_CONVERTER_ADAPTER_ACL_MAPPER_TBE_OP_DEF_H_

#include "ops/primitive_c.h"

namespace mindspore {
namespace lite {
namespace acl {
#define ADD_CONVERTER_TBE_OP(name)            \
  constexpr auto kName##name = #name;         \
  class name : public ops::PrimitiveC {       \
   public:                                    \
    name() : ops::PrimitiveC(kName##name) {}  \
    ~name() = default;                        \
    MS_DECLARE_PARENT(name, ops::PrimitiveC); \
  };

ADD_CONVERTER_TBE_OP(Pooling)
ADD_CONVERTER_TBE_OP(AvgPoolV2)
ADD_CONVERTER_TBE_OP(MaxPoolV3)
ADD_CONVERTER_TBE_OP(PadV3)
ADD_CONVERTER_TBE_OP(PadV2)
ADD_CONVERTER_TBE_OP(Pad)
ADD_CONVERTER_TBE_OP(MirrorPad)
ADD_CONVERTER_TBE_OP(StridedSliceV2)
ADD_CONVERTER_TBE_OP(GlobalAveragePool)
ADD_CONVERTER_TBE_OP(BNInference)
ADD_CONVERTER_TBE_OP(Deconvolution)
ADD_CONVERTER_TBE_OP(Upsample)
ADD_CONVERTER_TBE_OP(Conv2DTransposeD)
ADD_CONVERTER_TBE_OP(DepthwiseConv2dNative)
ADD_CONVERTER_TBE_OP(ArgMaxV2)
ADD_CONVERTER_TBE_OP(ArgMin)
ADD_CONVERTER_TBE_OP(ResizeNearestNeighborV2)
ADD_CONVERTER_TBE_OP(ResizeBilinearV2)
ADD_CONVERTER_TBE_OP(ResizeArea)
ADD_CONVERTER_TBE_OP(ResizeBicubic)
ADD_CONVERTER_TBE_OP(Conv2DBackpropInputV2)
ADD_CONVERTER_TBE_OP(ConcatV2)
ADD_CONVERTER_TBE_OP(FillV1)
ADD_CONVERTER_TBE_OP(Quant)
ADD_CONVERTER_TBE_OP(Dequant)
ADD_CONVERTER_TBE_OP(SpaceToBatchTF)
ADD_CONVERTER_TBE_OP(BatchToSpaceTF)
ADD_CONVERTER_TBE_OP(ClipByValue)
ADD_CONVERTER_TBE_OP(SqueezeV3)
ADD_CONVERTER_TBE_OP(DynamicReduceProd)
ADD_CONVERTER_TBE_OP(TopKV2)
ADD_CONVERTER_TBE_OP(CommonLSTM)
ADD_CONVERTER_TBE_OP(BatchMatMulV2)
ADD_CONVERTER_TBE_OP(MatMulV2)
ADD_CONVERTER_TBE_OP(Swish)
ADD_CONVERTER_TBE_OP(Where)
ADD_CONVERTER_TBE_OP(SelectV2)
ADD_CONVERTER_TBE_OP(ScatterNdUpdate)
ADD_CONVERTER_TBE_OP(Triu)
ADD_CONVERTER_TBE_OP(AscendAntiQuant)
ADD_CONVERTER_TBE_OP(Shrink)
ADD_CONVERTER_TBE_OP(ReduceLogSumExp)
ADD_CONVERTER_TBE_OP(ReduceLogSum)
ADD_CONVERTER_TBE_OP(SplitV)
ADD_CONVERTER_TBE_OP(MVN)
ADD_CONVERTER_TBE_OP(MVNV2)
ADD_CONVERTER_TBE_OP(CommonGRU)
ADD_CONVERTER_TBE_OP(Conv2DTransposeV2)
ADD_CONVERTER_TBE_OP(NonZeroV2)
ADD_CONVERTER_TBE_OP(AdaptiveAvgPool)
ADD_CONVERTER_TBE_OP(Im2col)
ADD_CONVERTER_TBE_OP(AscendQuant)
}  // namespace acl
}  // namespace lite
}  // namespace mindspore
#endif  // MINDSPORE_LITE_TOOLS_CONVERTER_ADAPTER_ACL_MAPPER_TBE_OP_DEF_H_
