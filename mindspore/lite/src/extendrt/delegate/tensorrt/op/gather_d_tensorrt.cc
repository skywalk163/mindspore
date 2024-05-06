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

#include "src/extendrt/delegate/tensorrt/op/gather_d_tensorrt.h"
#include <cuda_runtime.h>
#include <numeric>
#include <memory>
#include <functional>
#include "src/extendrt/delegate/tensorrt/tensorrt_utils.h"
#include "ops/auto_generate/gen_lite_ops.h"

namespace mindspore::lite {
REGISTER_TENSORRT_PLUGIN(GatherDPluginCreater);
template class TensorRTPluginCreater<GatherDPlugin>;
template <class T>
nvinfer1::PluginFieldCollection TensorRTPluginCreater<T>::field_collection_{};
template <class T>
std::vector<nvinfer1::PluginField> TensorRTPluginCreater<T>::fields_;

int GatherDTensorRT::IsSupport(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &in_tensors,
                               const std::vector<TensorInfo> &out_tensors) {
  if (!IsShapeKnown()) {
    MS_LOG(ERROR) << "Unsupported gatherd input tensor unknown shape: " << op_name_;
    return RET_ERROR;
  }
  if (in_tensors.size() != INPUT_SIZE3) {
    MS_LOG(ERROR) << "invalid gatherd input tensor size: " << in_tensors.size();
    return RET_ERROR;
  }
  if (out_tensors.size() != 1) {
    MS_LOG(ERROR) << "invalid gatherd output tensor size: " << out_tensors.size();
    return RET_ERROR;
  }
  return RET_OK;
}

int GatherDTensorRT::AddInnerOp(TensorRTContext *ctx) {
  nvinfer1::ITensor *inputTensors[] = {input(ctx, 0).trt_tensor_, input(ctx, 2).trt_tensor_};
  auto dim_vec = ConvertTensorAsIntVector(in_tensors_[1]);
  if (dim_vec.size() != 1) {
    MS_LOG(ERROR) << "Failed to get dim input, dim count " << dim_vec.size() << ", node: " << op_name_;
    return RET_ERROR;
  }
  size_t dim = dim_vec[0];

  auto plugin = std::make_shared<GatherDPlugin>(op_name_, dim, device_id_);
  nvinfer1::IPluginV2Layer *gatherd_layer = ctx->network()->addPluginV2(inputTensors, INPUT_SIZE2, *plugin);
  if (gatherd_layer == nullptr) {
    MS_LOG(ERROR) << "create gatherd failed for: " << op_name_;
    return RET_ERROR;
  }
  nvinfer1::ITensor *gatherd_out = gatherd_layer->getOutput(0);
  gatherd_layer->setName(op_name_.c_str());
  ctx->RegisterTensor(ITensorHelper{gatherd_out, input(ctx, 0).format_, input(ctx, 0).same_format_},
                      out_tensors_[0].Name());
  this->layer_ = gatherd_layer;
  return RET_OK;
}

int GatherDPlugin::enqueue(const nvinfer1::PluginTensorDesc *inputDesc, const nvinfer1::PluginTensorDesc *outputDesc,
                           const void *const *inputs, void *const *outputs, void *workspace,
                           cudaStream_t stream) noexcept {
  nvinfer1::Dims input_dims = inputDesc[0].dims;
  int dims = input_dims.nbDims;
  if (axis_ < 0) {
    axis_ += dims;
  }

  if (inputDesc->type == nvinfer1::DataType::kINT32) {
    auto input = static_cast<const int *>(inputs[0]);
    auto index = static_cast<const int *>(inputs[1]);
    auto output = static_cast<int *>(outputs[0]);
    Reshape(inputDesc, outputDesc);
    GatherD<int, int>(input, index, output, static_cast<size_t>(axis_), num_, input_dims.nbDims, input_shape_helper_,
                      index_shape_helper_, stream, device_id_);
  } else if (inputDesc->type == nvinfer1::DataType::kFLOAT) {
    auto input = static_cast<const float *>(inputs[0]);
    auto index = static_cast<const int *>(inputs[1]);
    auto output = static_cast<float *>(outputs[0]);
    Reshape(inputDesc, outputDesc);
    GatherD<float, int>(input, index, output, static_cast<size_t>(axis_), num_, input_dims.nbDims, input_shape_helper_,
                        index_shape_helper_, stream, device_id_);
  } else {
    MS_LOG(ERROR) << "unsupported data type gatherd" << layer_name_;
  }
  return RET_OK;
}

nvinfer1::IPluginV2DynamicExt *GatherDPlugin::clone() const noexcept {
  auto *plugin = new GatherDPlugin(*this);
  plugin->setPluginNamespace(name_space_.c_str());
  return plugin;
}

nvinfer1::DimsExprs GatherDPlugin::getOutputDimensions(int outputIndex, const nvinfer1::DimsExprs *inputs, int nbInputs,
                                                       nvinfer1::IExprBuilder &exprBuilder) noexcept {
  nvinfer1::DimsExprs out_dims{};
  out_dims.nbDims = inputs[1].nbDims;
  for (int i = 0; i < inputs[1].nbDims; i++) {
    out_dims.d[i] = inputs[1].d[i];
  }
  return out_dims;
}

void GatherDPlugin::Reshape(const nvinfer1::PluginTensorDesc *inputDesc, const nvinfer1::PluginTensorDesc *outputDesc) {
  nvinfer1::Dims input_dims = inputDesc[0].dims;
  nvinfer1::Dims output_dims = outputDesc[0].dims;

  if (input_dims.nbDims > static_cast<int64_t>(kMaxShapeRank)) {
    MS_LOG(EXCEPTION) << "The rank of input should be less than " << kMaxShapeRank << ", but got " << input_dims.nbDims
                      << ".";
  }
  num_ = 1;
  for (size_t i = 0; i < static_cast<size_t>(input_dims.nbDims); i++) {
    input_shape_helper_.shape[i] = static_cast<size_t>(input_dims.d[i]);
    index_shape_helper_.shape[i] = static_cast<size_t>(output_dims.d[i]);
    num_ *= static_cast<size_t>(output_dims.d[i]);
  }

  return;
}
REGISTER_TENSORRT_CREATOR(ops::kNameGatherD, GatherDTensorRT)
}  // namespace mindspore::lite
