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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_OPCODERS_NNACL_FP32_REDUCE_FP32_CODER_H_
#define MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_OPCODERS_NNACL_FP32_REDUCE_FP32_CODER_H_

#include <string>
#include <vector>
#include "coder/opcoders/base/reduce_base_coder.h"
#include "coder/opcoders/op_coder.h"

namespace mindspore::lite::micro::nnacl {
class ReduceFP32Coder : public ReduceBaseCoder {
 public:
  ReduceFP32Coder(const std::vector<Tensor *> &in_tensors, const std::vector<Tensor *> &out_tensors,
                  const LiteGraph::Node *node, size_t node_index, Target target)
      : ReduceBaseCoder(in_tensors, out_tensors, node, node_index, target) {}

  ~ReduceFP32Coder() override = default;

  int Prepare(CoderContext *const context) override;

  int DoCode(CoderContext *const context) override;

 protected:
  void GenerateCode(CoderContext *const context);
  int MallocTmpBuffer(mindspore::TypeId type_id);

  std::string reduce_;
  std::string int_reduce_;
  TypeIdC data_type_{::kNumberTypeFloat32};
  std::vector<float *> data_buffers_;

 private:
  int ReSize() override;
};
}  // namespace mindspore::lite::micro::nnacl
#endif  // MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_OPCODERS_NNACL_FP32_REDUCE_FP32_CODER_H_
