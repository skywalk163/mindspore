/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#include "tools/converter/parser/onnx/onnx_multinomial_parser.h"
#include <memory>
#include "ops/multinomial.h"
#include "mindapi/ir/type.h"

namespace mindspore {
namespace lite {
PrimitiveCPtr OnnxMultinomialParser::Parse(const onnx::GraphProto &onnx_graph, const onnx::NodeProto &onnx_node) {
  auto prim = std::make_unique<ops::Multinomial>();
  MS_CHECK_TRUE_RET(prim != nullptr, nullptr);
  auto prim_c = prim->GetPrim();
  MS_CHECK_TRUE_RET(prim_c != nullptr, nullptr);
  for (const auto &onnx_node_attr : onnx_node.attribute()) {
    const auto &attribute_name = onnx_node_attr.name();
    if (attribute_name == "seed") {
      prim->set_seed(onnx_node_attr.f());
    } else if (attribute_name == "sample_size") {
      int64_t sample_size = static_cast<int64_t>(onnx_node_attr.i());
      (void)prim_c->AddAttr("sample_size", MakeValue<int64_t>(sample_size));
    } else if (attribute_name == "dtype") {
      auto onnx_dtype = static_cast<onnx::TensorProto_DataType>(onnx_node_attr.i());
      auto data_type = OnnxNodeParser::GetDataTypeFromOnnx(onnx_dtype);
      (void)prim_c->AddAttr("dtype", MakeValue<int64_t>(static_cast<int64_t>(data_type)));
    }
  }

  return prim->GetPrim();
}

OnnxNodeRegistrar g_onnxMultinomialParser("Multinomial", new OnnxMultinomialParser());
}  // namespace lite
}  // namespace mindspore
