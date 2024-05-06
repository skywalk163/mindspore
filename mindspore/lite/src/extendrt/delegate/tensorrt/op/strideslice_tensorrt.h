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
#ifndef MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_STRIDE_SLICE_TENSORRT_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_STRIDE_SLICE_TENSORRT_H_
#include <string>
#include <vector>
#include <tuple>
#include <memory>
#include "src/extendrt/delegate/tensorrt/op/tensorrt_op.h"

namespace mindspore::lite {
constexpr int BEGINS_INDEX = 1;
constexpr int ENDS_INDEX = 2;
constexpr int HAS_AXIS = 5;
constexpr int AXIS_INDEX = 3;
class StrideSliceTensorRT : public TensorRTOp {
 public:
  StrideSliceTensorRT(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &in_tensors,
                      const std::vector<TensorInfo> &out_tensors, std::string name)
      : TensorRTOp(base_operator, in_tensors, out_tensors, name) {}

  ~StrideSliceTensorRT() override = default;

  int AddInnerOp(TensorRTContext *ctx) override;

  int IsSupport(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &in_tensors,
                const std::vector<TensorInfo> &out_tensors) override;

 private:
  nvinfer1::ITensor *GetDynamicAxisSliceStart(TensorRTContext *ctx, nvinfer1::ITensor *input, int axis, int nbdims);
  nvinfer1::ITensor *GetDynamicSliceSize(TensorRTContext *ctx, nvinfer1::ITensor *input,
                                         const nvinfer1::Dims &size_dims, const nvinfer1::Dims &start_dims);
  nvinfer1::ITensor *GetDynamicSliceSize(TensorRTContext *ctx, nvinfer1::ITensor *slice_input, size_t end_mask);
  nvinfer1::ITensor *GetDynamicAxisSliceSize(TensorRTContext *ctx, nvinfer1::ITensor *input, int size_dim, int axis,
                                             nvinfer1::ITensor *size_tensor);
  int ComputeSliceDims(TensorRTContext *ctx, ITensorHelper *slice_input);
  int ComputeDims(TensorRTContext *ctx, ITensorHelper *slice_input, const TensorInfo &begin, const TensorInfo &stride,
                  const TensorInfo &end, size_t start_mask, size_t end_mask);
  int ComputeDimsSingle(TensorRTContext *ctx, ITensorHelper *slice_input, const TensorInfo &begin,
                        const TensorInfo &stride, const TensorInfo &end, size_t start_mask, size_t end_mask);
  int ComputeDimsMulti(TensorRTContext *ctx, ITensorHelper *slice_input, const TensorInfo &begin,
                       const TensorInfo &stride, const TensorInfo &end, size_t start_mask, size_t end_mask);
  bool GetConstInputValue(int *start_val, int *stride_val);
  int GetAxis(TensorRTContext *ctx);
  size_t shrink_axis_;
  size_t start_axis_;
  size_t end_axis_;
  nvinfer1::Dims start_dims_;
  nvinfer1::Dims size_dims_;
  nvinfer1::Dims stride_dims_;
  nvinfer1::ITensor *size_tensor_{nullptr};
  nvinfer1::ITensor *start_tensor_{nullptr};
};
}  // namespace mindspore::lite
#endif  // MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_STRIDE_SLICE_TENSORRT_H_
