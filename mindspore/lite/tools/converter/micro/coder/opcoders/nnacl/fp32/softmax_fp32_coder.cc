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

#include "coder/opcoders/nnacl/fp32/softmax_fp32_coder.h"
#include <string>
#include "coder/opcoders/serializers/nnacl_serializer/nnacl_fp32_serializer.h"
#include "schema/inner/ops_generated.h"
#include "coder/opcoders/file_collector.h"

using mindspore::schema::PrimitiveType_LogSoftmax;
using mindspore::schema::PrimitiveType_Softmax;

namespace mindspore::lite::micro::nnacl {
int SoftMaxFP32Coder::Prepare(CoderContext *const context) {
  auto ret = SoftmaxBaseCoder::Init();
  MS_CHECK_RET_CODE(ret, "SoftmaxBaseCoder::Init() failed!");
  ret = SoftmaxBaseCoder::MallocTmpBuffer();
  MS_CHECK_RET_CODE(ret, "SoftmaxBaseCoder::MallocTmpBuffer() failed!");
  sum_data_ = static_cast<float *>(allocator_->Malloc(input_tensor_->data_type(), sum_data_size_, kWorkspace));
  return RET_OK;
}

int SoftMaxFP32Coder::DoCode(CoderContext *const context) {
  Collect(context,
          {
            "nnacl/fp32/softmax_fp32.h",
            "nnacl/fp32/log_softmax_fp32.h",
          },
          {
            "softmax_fp32.c",
            "log_softmax_fp32.c",
            "exp_fp32.c",
          });
  NNaclFp32Serializer code;
  std::string param_name = "softmax_parameter";
  code.CodeStruct(param_name, *softmax_param_);
  code.CodeStruct("input_shape", input_shape_, DIMENSION_5D);
  code.CodeFunction("memset", sum_data_, "0", sum_data_size_);
  auto primitive_type = softmax_param_->op_parameter_.type_;
  if (support_parallel_) {
    code << "    " << param_name << ".op_parameter_.thread_num_ = 1;\n";
  }
  if (primitive_type == schema::PrimitiveType_Softmax) {
    code.CodeFunction("Softmax", input_tensor_, output_tensor_, sum_data_, "softmax_parameter.axis_", n_dim_,
                      "input_shape");
  } else {
    code.CodeFunction("LogSoftmax", input_tensor_, output_tensor_, sum_data_, "input_shape", n_dim_,
                      "softmax_parameter.axis_");
  }
  context->AppendCode(code.str());
  return RET_OK;
}

REG_OPERATOR_CODER(kAllTargets, kNumberTypeFloat32, PrimitiveType_Softmax, CPUOpCoderCreator<SoftMaxFP32Coder>)
}  // namespace mindspore::lite::micro::nnacl
