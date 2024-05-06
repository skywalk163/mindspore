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
#ifndef MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_SLICE_TENSORRT_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_SLICE_TENSORRT_H_
#include <string>
#include <vector>
#include <tuple>
#include <memory>
#include "src/extendrt/delegate/tensorrt/op/tensorrt_op.h"

namespace mindspore::lite {
class SliceTensorRTUtil {
 public:
  SliceTensorRTUtil() = default;
  virtual ~SliceTensorRTUtil() = default;
  virtual bool IsSupport(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &in_tensors,
                         const std::vector<TensorInfo> &out_tensors) = 0;
  virtual std::tuple<nvinfer1::Dims, nvinfer1::Dims, nvinfer1::Dims> GetSliceParams(
    const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &in_tensors,
    const std::vector<TensorInfo> &out_tensors, const ITensorHelper &helper) = 0;
  virtual nvinfer1::ITensor *PostProcess(TensorRTContext *ctx, nvinfer1::ITensor *input,
                                         const std::vector<TensorInfo> &in_tensors,
                                         const std::vector<TensorInfo> &out_tensors) {
    return input;
  }
  std::string op_name_;
};

constexpr int BEGINS_INDEX = 1;
constexpr int ENDS_INDEX = 2;
constexpr int SIZE_INDEX = 2;
constexpr int HAS_AXIS = 5;
constexpr int AXIS_INDEX = 3;
constexpr int CROP_INPUT_SIZE = 2;
constexpr int SLICE_INPUT_SIZE = 3;
class SliceTensorRT : public TensorRTOp {
 public:
  SliceTensorRT(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &inputs,
                const std::vector<TensorInfo> &outputs, std::string name);

  ~SliceTensorRT() override = default;

  int AddInnerOp(TensorRTContext *ctx) override;

  int IsSupport(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &in_tensors,
                const std::vector<TensorInfo> &out_tensors) override;

 private:
  std::unique_ptr<SliceTensorRTUtil> util_;
};
}  // namespace mindspore::lite
#endif  // MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_SLICE_TENSORRT_H_
