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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_OPCODERS_NNACL_FP32_SCALE_FP32_CODER_H_
#define MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_OPCODERS_NNACL_FP32_SCALE_FP32_CODER_H_

#include <vector>
#include "coder/opcoders/op_coder.h"
#include "nnacl/scale_parameter.h"
#include "nnacl/kernel/scale.h"

namespace mindspore::lite::micro::nnacl {
class ScaleFP32Coder : public OperatorCoder {
 public:
  ScaleFP32Coder(const std::vector<Tensor *> &in_tensors, const std::vector<Tensor *> &out_tensors,
                 const LiteGraph::Node *node, size_t node_index, Target target)
      : OperatorCoder(in_tensors, out_tensors, node, node_index, target) {}
  ~ScaleFP32Coder() override = default;
  int Prepare(CoderContext *const context) override;

  int DoCode(CoderContext *const context) override;

 protected:
  virtual int ReSize();
  int CalculateParameter();
  int InitScaleOffset();

 protected:
  int thread_num_ = 1;
  bool const_scale_ = false;
  bool const_offset_ = false;
  ScaleStruct scale_struct_;
  ScaleParameter *scale_param_{nullptr};
};
}  // namespace mindspore::lite::micro::nnacl
#endif  // MICRO_CODER_OPCODERS_FP32__CODER_H_
