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

#include "tools/converter/parser/pytorch/pytorch_reshape_parser.h"
#include <memory>
#include "ops/auto_generate/gen_lite_ops.h"
#include "ops/squeeze.h"
#include "ops/unsqueeze.h"
#include "nnacl/op_base.h"

namespace mindspore {
namespace lite {
PrimitiveCPtr PytorchReshapeParser::Parse(const torch::jit::Node *torch_node, std::vector<size_t> *input_indices) {
  MS_ASSERT(torch_node != nullptr && input_indices != nullptr);
  auto prim = std::make_unique<ops::Reshape>();
  MS_CHECK_TRUE_RET(prim != nullptr, nullptr);
  auto prim_c = prim->GetPrim();
  MS_CHECK_TRUE_RET(prim_c != nullptr, nullptr);

  input_indices->push_back(0);

  if (torch_node->inputs().size() > SECOND_INPUT) {
    std::vector<int32_t> shape;
    shape = PytorchNodeParser::GetValueFromConstNode<std::vector<int32_t>>(torch_node->input(SECOND_INPUT));
    prim_c->AddAttr("shape", MakeValue(shape));
  }

  return prim->GetPrim();
}

PrimitiveCPtr PytorchSqueezeParser::Parse(const torch::jit::Node *torch_node, std::vector<size_t> *input_indices) {
  MS_ASSERT(torch_node != nullptr && input_indices != nullptr);
  auto prim = std::make_unique<ops::Squeeze>();
  MS_CHECK_TRUE_RET(prim != nullptr, nullptr);

  input_indices->push_back(0);
  if (torch_node->inputs().size() > SECOND_INPUT) {
    auto dim = PytorchNodeParser::GetValueFromConstNode<std::vector<int64_t>>(torch_node->input(SECOND_INPUT));
    prim->set_axis(dim);
  }

  return prim->GetPrim();
}

PrimitiveCPtr PytorchUnSqueezeParser::Parse(const torch::jit::Node *torch_node, std::vector<size_t> *input_indices) {
  MS_ASSERT(torch_node != nullptr && input_indices != nullptr);
  auto prim = std::make_unique<ops::Unsqueeze>();
  MS_CHECK_TRUE_RET(prim != nullptr, nullptr);

  input_indices->push_back(0);
  std::vector<int64_t> axis;
  if (torch_node->inputs().size() > SECOND_INPUT) {
    auto dim = PytorchNodeParser::GetValueFromConstNode<int64_t>(torch_node->input(SECOND_INPUT));
    axis.emplace_back(dim);
    prim->set_axis(axis);
  }

  return prim->GetPrim();
}

PytorchNodeRegistrar g_pytorchReshapeParser("reshape", new PytorchReshapeParser());
PytorchNodeRegistrar g_pytorchSqueezeParser("squeeze", new PytorchSqueezeParser());
PytorchNodeRegistrar g_pytorchUnSqueezeParser("unsqueeze", new PytorchUnSqueezeParser());
}  // namespace lite
}  // namespace mindspore
