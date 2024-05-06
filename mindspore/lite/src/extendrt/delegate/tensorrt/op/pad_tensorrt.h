/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_PAD_TENSORRT_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_PAD_TENSORRT_H_
#include <string>
#include <vector>
#include "src/extendrt/delegate/tensorrt/op/tensorrt_op.h"

namespace mindspore::lite {
class PadTensorRT : public TensorRTOp {
 public:
  PadTensorRT(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &in_tensors,
              const std::vector<TensorInfo> &out_tensors, std::string name)
      : TensorRTOp(base_operator, in_tensors, out_tensors, name) {}

  ~PadTensorRT() override = default;

  int AddInnerOp(TensorRTContext *ctx) override;

  bool IsWeightInputHanledInner() const override { return true; }

  int IsSupport(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &inputs,
                const std::vector<TensorInfo> &outputs) override;

 private:
  int AddInnerOpFix(TensorRTContext *ctx, const std::vector<int64_t> &input_shape, nvinfer1::ITensor *pad_input,
                    const std::vector<int> &pad_vec);
  int AddInnerOpDynamic(TensorRTContext *ctx, const std::vector<int64_t> &input_shape, nvinfer1::ITensor *pad_input,
                        const std::vector<int> &pad_vec);
  float constant_value_ = 0.0f;
  PaddingMode padding_mode_ = PaddingMode::CONSTANT;
  int AddInnerOpOld(TensorRTContext *ctx);
};
}  // namespace mindspore::lite
#endif  // MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_PAD_TENSORRT_H_
