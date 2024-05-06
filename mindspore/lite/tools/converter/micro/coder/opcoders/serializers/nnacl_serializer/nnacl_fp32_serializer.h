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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_OPCODERS_SERIALIZERS_NNACL_SERIALIZER_NNACL_FP32_SERIALIZER_H_
#define MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_OPCODERS_SERIALIZERS_NNACL_SERIALIZER_NNACL_FP32_SERIALIZER_H_
#include <string>
#include <sstream>
#include <vector>
#include "coder/opcoders/serializers/serializer.h"
#include "nnacl/batchnorm_parameter.h"
#include "nnacl/fp32/arithmetic_fp32.h"
#include "nnacl/conv_parameter.h"
#include "nnacl/matmul_parameter.h"
#include "wrapper/base/micro_parameter.h"
#include "nnacl/scale_parameter.h"
#include "nnacl/slice_parameter.h"
#include "nnacl/split_parameter.h"
#include "nnacl/transpose_parameter.h"
#include "nnacl/base/tile_base.h"
#include "nnacl/fp32/transpose_fp32.h"
#include "nnacl/pooling_parameter.h"
#include "nnacl/softmax_parameter.h"
#include "nnacl/splice_parameter.h"
#include "nnacl/lstm_parameter.h"
#include "nnacl/group_norm_parameter.h"
#include "nnacl/activation_parameter.h"
#include "wrapper/fp32/dequant_int8_to_fp32_wrapper.h"
#include "nnacl/fp32/exp_fp32.h"
#include "nnacl/fp32/strided_slice_fp32.h"
#include "nnacl/tensor_c.h"
#include "wrapper/fp32/arithmetic_fp32_wrapper.h"
#include "wrapper/base/affine_wrapper.h"
#include "wrapper/fp32/conv_winograd_fp32_wrapper.h"
#include "nnacl/instance_norm_parameter.h"
#include "nnacl/layer_norm_parameter.h"
#include "nnacl/broadcast_to_parameter.h"
#include "nnacl/custom_gru_parameter.h"
#include "nnacl/unstack_parameter.h"
#include "nnacl/kernel/scale.h"
#include "nnacl/kernel/pooling.h"
#include "nnacl/kernel/layer_norm.h"
#include "nnacl/kernel/fill.h"
#include "nnacl/kernel/batch_norm.h"
#include "nnacl/kernel/tile.h"
#include "nnacl/kernel/slice.h"
#include "nnacl/kernel/strided_slice.h"
#include "coder/opcoders/nnacl/dynamic_parameter/transpose_dynamic_parameter.h"
#include "coder/opcoders/nnacl/dynamic_parameter/dynamic_lstm_parameter.h"
#include "coder/opcoders/nnacl/dynamic_parameter/slice_dynamic_parameter.h"
#include "coder/opcoders/nnacl/dynamic_parameter/split_dynamic_parameter.h"
#include "coder/opcoders/nnacl/dynamic_parameter/strided_slice_dynamic_parameter.h"
#include "coder/opcoders/nnacl/dynamic_parameter/scale_dynamic_parameter.h"
#include "coder/opcoders/nnacl/dynamic_parameter/conv_dynamic_parameter.h"
#include "coder/opcoders/nnacl/dynamic_parameter/arithmetic_dynamic_parameter.h"
#include "coder/opcoders/nnacl/dynamic_parameter/pooling_dynamic_parameter.h"

namespace mindspore::lite::micro::nnacl {
class NNaclFp32Serializer : public Serializer {
 public:
  NNaclFp32Serializer() = default;
  ~NNaclFp32Serializer() override = default;
  void CodeStruct(const std::string &name, const PoolingParameter &pooling_parameter);
  void CodeStruct(const std::string &name, const PoolingComputeParam &pooling_args);
  void CodeStruct(const std::string &name, const SoftmaxParameter &softmax_parameter);
  void CodeStruct(const std::string &name, const BatchNormStruct &bn_struct);
  void CodeStruct(const std::string &name, const InstanceNormParameter &param);
  void CodeStruct(const std::string &name, const ArithmeticParameter &arithmetic_parameter);
  void CodeStruct(const std::string &name, const ConvParameter &conv_parameter);
  void CodeStruct(const std::string &name, const MatMulParameter &mat_mul_parameter);
  void CodeStruct(const std::string &name, const MicroMatmulParameter &micro_matmul_parameter);
  void CodeStruct(const std::string &name, const LstmParameter &lstm_parameter);
  void CodeStruct(const std::string &name, const ScaleStruct &scale_struct);
  void CodeStruct(const std::string &name, const TileStruct &tile_parameter);
  void CodeStruct(const std::string &name, const TransposeParameter &transpose_parameter);
  void CodeStruct(const std::string &name, const DeQuantArg &de_quant_arg);
  void CodeStruct(const std::string &name, const SpliceParameter &splice_parameter);
  void CodeStruct(const std::string &name, const ExpStruct &exp_struct);
  void CodeStruct(const std::string &name, const StridedSliceParameter &strided_slice_parameter);
  void CodeStruct(const std::string &name, const ArithmeticWrapperInfo &arithmetic_wrapper_info);
  void CodeStruct(const std::string &name, const SpliceWrapperParam &splice_param);
  void CodeStruct(const std::string &name, const TransFuncStr trans_func_str);
  void CodeStruct(const std::string &name, const GroupNormParameter &gn_param);
  void CodeStruct(const std::string &name, const ActivationParameter &activation_parameter);
  void CodeStruct(const std::string &name, const OpParameter &op_param);
  void CodeStruct(const std::string &name, const SplitParameter &split_parameter);
  void CodeStruct(const std::string &name, const LayerNormComputeParam &param);
  void CodeStruct(const std::string &name, const BroadcastShapeInfo &param);
  void CodeStruct(const std::string &name, const CustomGruParameter &param);
  void CodeStruct(const std::string &name, const SlidingWindowParam &param);
  void CodeStruct(const std::string &name, const UnstackParameter &param);
  void CodeStruct(const std::string &name, const FillStruct &param);
  void CodeStruct(const std::string &name, const SliceStruct &param);
  void CodeStruct(const std::string &name, const TransposeParameter &transpose_param,
                  const TransposeDynamicParameter &dynamic_transpose_param);
  void CodeStruct(const std::string &name, const SplitParameter &split_parameter,
                  const SplitDynamicParameter &dynamic_split_param);
  void CodeStruct(const std::string &name, const BroadcastShapeInfo &param,
                  const BroadcastDynamicShapeInfo &dynamic_param);
  void CodeStruct(const std::string &name, const LstmParameter &lstm_param,
                  const DynamicLstmParameter &dynamic_lstm_param);
  void CodeStruct(const std::string &name, const SliceStruct &slice_parameter,
                  const SliceDynamicParameter &dynamic_slice_param);
  void CodeStruct(const std::string &name, const StridedSliceParameter &strided_slice_parameter,
                  const StridedSliceDynamicParameter &dynamic_strided_slice_param);
  void CodeStruct(const std::string &name, const StridedSliceStruct &param,
                  const StridedSliceDynamicParameter &dynamic_strided_slice_param);
  void CodeStruct(const std::string &name, const ScaleStruct &scale_struct,
                  const ScaleDynamicParameter &dynamic_scale_param);
  void CodeStruct(const std::string &name, const ConvParameter &conv_parameter,
                  const ConvDynamicParameter &dynamic_conv_param);
  void CodeStruct(const std::string &name, const PoolingComputeParam &pooling_compute,
                  const PoolingDynamicParameter &dynamic_pooling_param);
  void CodeStruct(const std::string &name, const int *list, int size);
  void CodeArrayStruct(const std::string &name, TensorC *tensorC, std::vector<Tensor *> tensor);

 private:
  static int count;
};
}  // namespace mindspore::lite::micro::nnacl
#endif  // MINDSPORE_LITE_MICRO_CODER_OPCODERS_SERIALIZERS_NNACL_FP32_ERIALIZER_H_
