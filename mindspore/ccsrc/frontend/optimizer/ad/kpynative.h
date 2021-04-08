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

#ifndef MINDSPORE_CCSRC_FRONTEND_OPTIMIZER_AD_KPYNATIVE_H_
#define MINDSPORE_CCSRC_FRONTEND_OPTIMIZER_AD_KPYNATIVE_H_

#include <memory>

#include "ir/anf.h"
#include "ir/func_graph.h"

namespace mindspore {
namespace ad {
class KPynativeCell {
 public:
  virtual ~KPynativeCell() = default;
  virtual void UpdateOutputNodeOfTopCell(const AnfNodePtr &output_node) = 0;
};

using KPynativeCellPtr = std::shared_ptr<KPynativeCell>;

// bprop_fg: user defined back propagate funcgraph or back propagate funcgraph of primitive, it will be passed after
//           just parsed. will have prototype:
//           (sens_input1, sens_input2, ...) bprop_fg(input1, input2, ..., out, dout)
// c_node: CNode with contains the prim (index 0) and the formal input parameters of that prim.
// op_args: the arguments list of each input parameters.
// out: the op result.
// return: the returned funcgraph should have the same prototype.
FuncGraphPtr OptimizeBPropFuncGraph(const FuncGraphPtr &bprop_fg, const CNodePtr &c_node, const ValuePtrList &op_args,
                                    const ValuePtr &out);

// Start building back propagate funcgraph for this cell.
// cell_inputs: the input parameter list of this cell except the weights;
KPynativeCellPtr GradPynativeCellBegin(const AnfNodePtrList &cell_inputs,
                                       const std::vector<ValuePtr> &input_param_values);

// Return the back propagate funcgraph for this cell.
// weights: weights parameters used in this cell.
// grad_inputs: return sensitivity for input parameters;
// grad_weights: return sensitivity for weights;
// has_sens_arg: caller will pass sens args;
// return: the returned funcgraph will have prototype:
// if has_sens_arg is true
// (sens_input1, sens_input2, ..., sens_weight0, sens_weight1, ) bprop_fg(input1, input2, ..., weight0, weight1, ...,
// sens_out)
// else:
// (sens_input1, sens_input2, ..., sens_weight0, sens_weight1, ) bprop_fg(input1, input2, ..., weight0, weight1, ...)
FuncGraphPtr GradPynativeCellEnd(const KPynativeCellPtr &k_cell, const AnfNodePtrList &weights, bool grad_inputs,
                                 bool grad_weights, bool has_sens_arg = false);

// Grad for each operation.
// c_node: CNode with contains the prim (index 0) and the formal input parameters of that prim.
// op_args: the arguments list of each input parameters.
// out: the op result.
bool GradPynativeOp(const KPynativeCellPtr &k_cell, const CNodePtr &c_node, const ValuePtrList &op_args,
                    const ValuePtr &out);

// Grad for cell which may have user defined back propagate function.
// c_node: CNode with contains the construct function graph of cell  (index 0) and the formal input parameters of that
// cell. op_args: the arguments list of each input parameters.
// out: the op result.
// bprop_fg: user defined back propagate funcgraph, it should be passed after just parsed.
//           Should have prototype: (sens_input1, sens_input2, ...) bprop_fg(input1, input2, ..., out, dout)
bool GradPynativeWithBProp(const KPynativeCellPtr &k_cell, const CNodePtr &c_node, const ValuePtrList &op_args,
                           const ValuePtr &out, const FuncGraphPtr &bprop_fg);
}  // namespace ad
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_FRONTEND_OPTIMIZER_AD_GRAD_H_
