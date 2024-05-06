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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_ONNX_ONNX_QUANTIZE_LINEAR_ADJUST_H
#define MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_ONNX_ONNX_QUANTIZE_LINEAR_ADJUST_H

#include <string>
#include <vector>
#include <functional>
#include "include/backend/optimizer/pass.h"
#include "include/backend/optimizer/optimizer.h"
#include "tools/converter/quantizer/quant_param_holder.h"
#include "tools/converter/quantizer/quant_params.h"

namespace mindspore::lite {
constexpr int kPrimOffset = 1;
class OnnxQuantizeLinearAdjust {
 public:
  static bool Adjust(const FuncGraphPtr &func_graph);

 private:
  static void RemoveDequantizeLinear(const FuncGraphPtr &func_graph, const CNodePtr &cnode);
  static bool SetQuantParam(const CNodePtr &cnode, const QuantParamHolderPtr &quant_param_holder, bool is_next_node,
                            size_t index);
  static int DoParameterQuantDeQuant(const CNodePtr &cnode, const ParameterPtr &input_node, size_t input_index,
                                     const QuantParamHolderPtr &quant_param_holder);
  static int QuantDeQuantFilter(const AnfNodePtr &parameter_node, const tensor::TensorPtr &weight,
                                const std::vector<schema::QuantParamT> &quant_params, int preferred_dim);
  static bool FoldQuantParms(const FuncGraphPtr &func_graph);
  static bool DoWeightQuantDeQuant(const FuncGraphPtr &func_graph);
};
}  // namespace mindspore::lite
#endif  // MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_ONNX_ONNX_QUANTIZE_LINEAR_ADJUST_H
