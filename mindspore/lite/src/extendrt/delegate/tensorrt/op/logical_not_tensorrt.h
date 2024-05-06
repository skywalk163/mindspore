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

#ifndef MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_LOGICAL_NOT_TENSORRT_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_LOGICAL_NOT_TENSORRT_H_

#include <string>
#include <vector>
#include "src/extendrt/delegate/tensorrt/op/tensorrt_plugin.h"
#include "src/extendrt/delegate/tensorrt/op/tensorrt_op.h"

namespace mindspore::lite {
class LogicalNotTensorRT : public TensorRTOp {
 public:
  LogicalNotTensorRT(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &in_tensors,
                     const std::vector<TensorInfo> &out_tensors, std::string name)
      : TensorRTOp(base_operator, in_tensors, out_tensors, name) {}

  ~LogicalNotTensorRT() override = default;

  int AddInnerOp(TensorRTContext *ctx) override;

  int IsSupport(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &in_tensors,
                const std::vector<TensorInfo> &out_tensors) override;
};

constexpr auto LOGICAL_NOT_PLUGIN_NAME{"LogicalNotPlugin"};
class LogicalNotPlugin : public TensorRTPlugin {
 public:
  LogicalNotPlugin(const std::string name, schema::PrimitiveType primitive_type)
      : TensorRTPlugin(name, std::string(LOGICAL_NOT_PLUGIN_NAME)), primitive_type_(primitive_type) {}

  LogicalNotPlugin(const char *name, const nvinfer1::PluginFieldCollection *fc)
      : TensorRTPlugin(std::string(name), std::string(LOGICAL_NOT_PLUGIN_NAME)) {
    const nvinfer1::PluginField *fields = fc->fields;
    primitive_type_ = static_cast<const schema::PrimitiveType *>(fields[0].data)[0];
  }

  LogicalNotPlugin(const char *name, const void *serialData, size_t serialLength)
      : TensorRTPlugin(std::string(name), std::string(LOGICAL_NOT_PLUGIN_NAME)) {
    DeserializeValue(&serialData, &serialLength, &primitive_type_, sizeof(schema::PrimitiveType));
  }

  LogicalNotPlugin() = delete;

  nvinfer1::IPluginV2DynamicExt *clone() const noexcept override;
  int enqueue(const nvinfer1::PluginTensorDesc *inputDesc, const nvinfer1::PluginTensorDesc *outputDesc,
              const void *const *inputs, void *const *outputs, void *workspace, cudaStream_t stream) noexcept override;
  size_t getSerializationSize() const noexcept override;
  void serialize(void *buffer) const noexcept override;
  bool supportsFormatCombination(int pos, const nvinfer1::PluginTensorDesc *tensorsDesc, int nbInputs,
                                 int nbOutputs) noexcept override;

 private:
  int RunCudaLogical(const nvinfer1::PluginTensorDesc *inputDesc, const void *const *inputs, void *const *outputs,
                     cudaStream_t stream);
  const std::string layer_name_;
  std::string name_space_;
  schema::PrimitiveType primitive_type_;
};
class LogicalNotPluginCreater : public TensorRTPluginCreater<LogicalNotPlugin> {
 public:
  LogicalNotPluginCreater() : TensorRTPluginCreater(std::string(LOGICAL_NOT_PLUGIN_NAME)) {}
};
}  // namespace mindspore::lite
#endif  // MINDSPORE_LITE_SRC_EXTENDRT_DELEGATE_TENSORRT_OP_LOGICAL_NOT_TENSORRT_H_
