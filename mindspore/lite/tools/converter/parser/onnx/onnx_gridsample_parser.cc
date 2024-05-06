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

#include "tools/converter/parser/onnx/onnx_gridsample_parser.h"
#include <memory>
#include "ops/auto_generate/gen_lite_ops.h"
#include "nnacl/op_base.h"
#include "ops/op_enum.h"

namespace mindspore {
namespace lite {
PrimitiveCPtr OnnxGridSampleParser::Parse(const onnx::GraphProto &onnx_graph, const onnx::NodeProto &onnx_node) {
  auto prim = std::make_unique<ops::GridSampler2D>();
  MS_CHECK_TRUE_RET(prim != nullptr, nullptr);
  for (const auto &onnx_node_attr : onnx_node.attribute()) {
    if (onnx_node_attr.name() == "mode") {
      int64_t mode = ops::StringToEnumImpl(prim->name(), "interpolation_mode", onnx_node_attr.s());
      prim->set_interpolation_mode(mode);
    } else if (onnx_node_attr.name() == "padding_mode") {
      int64_t padding_mode = ops::StringToEnumImpl(prim->name(), "padding_mode", onnx_node_attr.s());
      prim->set_padding_mode(padding_mode);
    } else if (onnx_node_attr.name() == "align_corners") {
      prim->set_align_corners(static_cast<bool>(onnx_node_attr.i()));
    }
  }

  return prim->GetPrim();
}

OnnxNodeRegistrar g_onnxGridSampleParser("GridSample", new OnnxGridSampleParser());
}  // namespace lite
}  // namespace mindspore
