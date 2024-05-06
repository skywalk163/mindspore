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

#include "plugin/device/ascend/optimizer/mindir/clip_by_norm_fission.h"
#include <algorithm>
#include "ops/math_op_name.h"
#include "ops/comparison_op_name.h"
#include "ops/framework_op_name.h"
#include "ops/nn_ops.h"
#include "ops/array_ops.h"
#include "ir/anf.h"
#include "ir/tensor.h"
#include "include/common/utils/anfalgo.h"
#include "include/backend/anf_runtime_algorithm.h"

namespace mindspore {
namespace opt {
namespace {
const size_t kOne = 1;
ShapeVector GetOutputInferShape(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto shape = common::AnfAlgo::GetOutputInferShape(node, 0);
  return shape;
}

ValueNodePtr CreateValueNode(const FuncGraphPtr &graph, const ValuePtr &value_ptr) {
  MS_EXCEPTION_IF_NULL(value_ptr);
  auto kernel_graph = graph->cast<KernelGraphPtr>();
  if (kernel_graph == nullptr) {
    auto new_node = std::make_shared<ValueNode>(value_ptr);
    MS_EXCEPTION_IF_NULL(new_node);
    auto value_abstract = value_ptr->ToAbstract();
    new_node->set_abstract(value_abstract);
    return new_node;
  } else {
    ValueNodePtr value_node = kernel_graph->NewValueNode(value_ptr->ToAbstract(), value_ptr);
    kernel_graph->AddValueNodeToGraph(value_node);
    return value_node;
  }
}

std::vector<int64_t> InferBroadcastShape(const std::vector<int64_t> &x_shape, const std::vector<int64_t> &y_shape,
                                         const std::string &op_name, const std::string &op_x_name,
                                         const std::string &op_y_name) {
  if (x_shape == y_shape) {
    return x_shape;
  }
  auto x_length = x_shape.size();
  auto y_length = y_shape.size();
  auto length = x_length < y_length ? x_length : y_length;
  std::vector<int64_t> broadcast_shape;
  if (x_length == length) {
    (void)std::copy(y_shape.begin(), y_shape.end() - SizeToInt(length), std::back_inserter(broadcast_shape));
  } else {
    (void)std::copy(x_shape.begin(), x_shape.end() - SizeToInt(length), std::back_inserter(broadcast_shape));
  }
  for (size_t i = length; i > 0; --i) {
    if (x_shape[x_length - i] == kOne) {
      broadcast_shape.push_back(y_shape[y_length - i]);
    } else if (y_shape[y_length - i] == kOne) {
      broadcast_shape.push_back(x_shape[x_length - i]);
    } else if (x_shape[x_length - i] == y_shape[y_length - i]) {
      broadcast_shape.push_back(x_shape[x_length - i]);
    } else if ((x_shape[x_length - i] == abstract::Shape::kShapeDimAny) ||
               (y_shape[y_length - i] == abstract::Shape::kShapeDimAny)) {
      MS_EXCEPTION(ValueError) << "For '" << op_name << "', input dynamic shape args is not supported.";
    } else {
      MS_EXCEPTION(ValueError) << "For '" << op_name << "', the two input '" << op_x_name << "' and '" << op_y_name
                               << "' can not broadcast";
    }
  }
  return broadcast_shape;
}

std::vector<int64_t> GetAxis(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto output_shape = common::AnfAlgo::GetOutputInferShape(node, 0);
  if (output_shape.empty()) {
    MS_LOG(INFO) << node->fullname_with_scope() << "'s output shape is empty";
  }
  auto clip_by_norm_prim = common::AnfAlgo::GetCNodePrimitive(node);
  MS_EXCEPTION_IF_NULL(clip_by_norm_prim);
  auto axis_value = clip_by_norm_prim->GetAttr(kAttrAxis);
  MS_EXCEPTION_IF_NULL(axis_value);
  if (axis_value->isa<ValueSequence>()) {
    auto axis = GetValue<std::vector<int64_t>>(axis_value);
    if (!axis.empty()) {
      return axis;
    }
  } else if (axis_value->isa<Int64Imm>()) {
    return {GetValue<int64_t>(axis_value)};
  }
  std::vector<int64_t> range;
  for (size_t i = 0; i < output_shape.size(); i++) {
    (void)range.emplace_back(i);
  }
  return range;
}

ValueNodePtr GetAxisNode(const FuncGraphPtr &graph, const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(graph);
  auto range = GetAxis(node);
  auto axis_node = CreateValueNode(graph, MakeValue(std::make_shared<tensor::Tensor>(range)));
  MS_EXCEPTION_IF_NULL(axis_node);
  return axis_node;
}
}  // namespace

AnfNodePtr ClipByNormFission::CreateCNodeBase(const FuncGraphPtr &func_graph, const std::vector<AnfNodePtr> &inps,
                                              const std::string &op_name, const AnfNodePtr &node) const {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(func_graph);
  std::vector<AnfNodePtr> new_node_inputs = {NewValueNode(std::make_shared<Primitive>(op_name))};
  for (const auto &inp : inps) {
    (void)new_node_inputs.emplace_back(inp);
  }
  auto new_node = NewCNode(new_node_inputs, func_graph);
  MS_EXCEPTION_IF_NULL(new_node);

  new_node->set_scope(node->scope());
  return new_node;
}

AnfNodePtr ClipByNormFission::CreateSquareNode(const FuncGraphPtr &func_graph, const AnfNodePtr &inp,
                                               const ShapeVector &shape_vec, const TypeId &type_id) const {
  auto square = CreateCNodeBase(func_graph, {inp}, kSquareOpName, inp);
  MS_EXCEPTION_IF_NULL(square);
  auto abs = std::make_shared<abstract::AbstractTensor>(TypeIdToType(type_id), shape_vec);
  square->set_abstract(abs);
  return square;
}

AnfNodePtr ClipByNormFission::CreateReduceSumNode(const FuncGraphPtr &func_graph, const AnfNodePtr &square,
                                                  const AnfNodePtr &clip_by_norm, const ShapeVector &shape_vec,
                                                  const TypeId &type_id) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  auto kernel_graph = func_graph->cast<std::shared_ptr<session::KernelGraph>>();
  MS_EXCEPTION_IF_NULL(kernel_graph);
  auto axis_node = GetAxisNode(func_graph, clip_by_norm);

  MS_EXCEPTION_IF_NULL(axis_node);
  auto keepdims_node = AnfAlgo::ConvertValueToNode(kernel_graph, MakeValue(true));
  auto skipmode_node = AnfAlgo::ConvertValueToNode(kernel_graph, MakeValue(false));
  auto reduce_sum =
    CreateCNodeBase(func_graph, {square, axis_node, keepdims_node, skipmode_node}, kReduceSumOpName, square);
  MS_EXCEPTION_IF_NULL(reduce_sum);
  // Sync the attribute of `ClipByNorm` to `ReduceSum`
  auto clip_by_norm_prim = common::AnfAlgo::GetCNodePrimitive(clip_by_norm);
  MS_EXCEPTION_IF_NULL(clip_by_norm_prim);
  auto axis_value = clip_by_norm_prim->GetAttr(kAttrAxis);
  MS_EXCEPTION_IF_NULL(axis_value);
  common::AnfAlgo::SetNodeAttr(kAttrKeepDims, MakeValue(true), reduce_sum);
  auto input_names = std::vector<std::string>{"input_x", "axis"};
  auto output_names = std::vector<std::string>{"y"};
  common::AnfAlgo::SetNodeAttr(kAttrInputNames, MakeValue(input_names), reduce_sum);
  common::AnfAlgo::SetNodeAttr(kAttrOutputNames, MakeValue(output_names), reduce_sum);
  // Get `axis` vector
  const auto dim = shape_vec.size();
  std::vector<int64_t> axis;
  if (axis_value->isa<ValueSequence>()) {
    axis = GetValue<std::vector<int64_t>>(axis_value);
    if (axis.empty()) {  // reduce_sum for all dimensions
      for (size_t i = 0; i < dim; ++i) {
        (void)axis.emplace_back(i);
      }
    }
  } else if (axis_value->isa<Int64Imm>()) {
    (void)axis.emplace_back(GetValue<int64_t>(axis_value));
  } else {
    MS_EXCEPTION(TypeError) << "For `" << prim::kPrimClipByNorm->name()
                            << "`, the type of attribute `axis` is invalid.";
  }
  // Set abstract to `reduce_sum` op
  int64_t ddim = SizeToLong(dim);
  ShapeVector reduce_sum_output_shape = shape_vec;
  for (const auto &idx : axis) {
    if (idx < -ddim || idx >= ddim) {
      MS_EXCEPTION(ValueError) << "The range of axis value should in [" << -ddim << ", " << ddim
                               << "), but got: " << idx;
    }
    auto positive_idx = idx < 0 ? idx + ddim : idx;
    reduce_sum_output_shape[LongToUlong(positive_idx)] = 1;
  }

  auto abs = std::make_shared<abstract::AbstractTensor>(TypeIdToType(type_id), reduce_sum_output_shape);
  reduce_sum->set_abstract(abs);
  return reduce_sum;
}

AnfNodePtr ClipByNormFission::CreateConstantNode(const FuncGraphPtr &func_graph, const AnfNodePtr &inp,
                                                 const ShapeVector &shape_vec, const TypeId &type_id,
                                                 const std::string &op_name) const {
  auto tensor = std::make_shared<tensor::Tensor>(type_id, shape_vec);
  MS_EXCEPTION_IF_NULL(func_graph);
  ValueNodePtr value_node = CreateValueNode(func_graph, tensor);
  MS_EXCEPTION_IF_NULL(value_node);
  auto constant_node = CreateCNodeBase(func_graph, {value_node}, op_name, inp);
  MS_EXCEPTION_IF_NULL(constant_node);

  auto abs = std::make_shared<abstract::AbstractTensor>(TypeIdToType(type_id), shape_vec);
  constant_node->set_abstract(abs);
  return constant_node;
}

AnfNodePtr ClipByNormFission::CreateGreaterNode(const FuncGraphPtr &func_graph, const AnfNodePtr &inp_a,
                                                const AnfNodePtr &inp_b, const ShapeVector &shape_vec) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  auto greater = CreateCNodeBase(func_graph, {inp_a, inp_b}, kGreaterOpName, inp_a);
  MS_EXCEPTION_IF_NULL(greater);
  auto abs = std::make_shared<abstract::AbstractTensor>(kBool, shape_vec);
  greater->set_abstract(abs);
  return greater;
}

AnfNodePtr ClipByNormFission::CreateSelectNode(const FuncGraphPtr &func_graph, const AnfNodePtr &cond,
                                               const AnfNodePtr &inp_a, const AnfNodePtr &inp_b,
                                               const ShapeVector &shape_vec, const TypeId &type_id) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(inp_a);
  MS_EXCEPTION_IF_NULL(inp_b);
  auto select = CreateCNodeBase(func_graph, {cond, inp_a, inp_b}, kSelectOpName, inp_a);
  MS_EXCEPTION_IF_NULL(select);
  auto abs = std::make_shared<abstract::AbstractTensor>(TypeIdToType(type_id), shape_vec);
  select->set_abstract(abs);
  return select;
}

AnfNodePtr ClipByNormFission::CreateSqrtNode(const FuncGraphPtr &func_graph, const AnfNodePtr &reduce_sum,
                                             const TypeId &type_id) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  auto sqrt = CreateCNodeBase(func_graph, {reduce_sum}, kSqrtOpName, reduce_sum);
  MS_EXCEPTION_IF_NULL(sqrt);
  auto abs = std::make_shared<abstract::AbstractTensor>(TypeIdToType(type_id), GetOutputInferShape(reduce_sum));
  sqrt->set_abstract(abs);
  return sqrt;
}

AnfNodePtr ClipByNormFission::CreateMaxNode(const FuncGraphPtr &func_graph, const AnfNodePtr &x, const AnfNodePtr &y,
                                            const TypeId &type_id) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  auto max = CreateCNodeBase(func_graph, {x, y}, kMaximumOpName, y);
  MS_EXCEPTION_IF_NULL(max);
  auto x_shape = GetOutputInferShape(x);
  auto y_shape = GetOutputInferShape(y);
  auto output_shape = InferBroadcastShape(x_shape, y_shape, "ClipByNorm", "clip_norm_cast", "l2_norm");
  auto abs = std::make_shared<abstract::AbstractTensor>(TypeIdToType(type_id), output_shape);
  max->set_abstract(abs);
  return max;
}

AnfNodePtr ClipByNormFission::CreateMulNode(const FuncGraphPtr &func_graph, const AnfNodePtr &x,
                                            const AnfNodePtr &clip_norm, const ShapeVector &shape_vec,
                                            const TypeId &type_id) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  auto mul = CreateCNodeBase(func_graph, {x, clip_norm}, kMulOpName, x);
  MS_EXCEPTION_IF_NULL(mul);
  auto output_shape = shape_vec;
  auto clip_norm_shape = GetOutputInferShape(clip_norm);
  if (clip_norm_shape.size() > output_shape.size()) {
    output_shape = clip_norm_shape;
  }

  auto abs = std::make_shared<abstract::AbstractTensor>(TypeIdToType(type_id), output_shape);
  mul->set_abstract(abs);
  return mul;
}

AnfNodePtr ClipByNormFission::CreateDivNode(const FuncGraphPtr &func_graph, const AnfNodePtr &dividend,
                                            const AnfNodePtr &divisor, const ShapeVector &shape_vec,
                                            const TypeId &type_id) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  auto div = CreateCNodeBase(func_graph, {dividend, divisor}, kDivOpName, divisor);
  MS_EXCEPTION_IF_NULL(div);
  auto abs = std::make_shared<abstract::AbstractTensor>(TypeIdToType(type_id), shape_vec);
  div->set_abstract(abs);
  return div;
}

AnfNodePtr ClipByNormFission::CreateCastNode(const FuncGraphPtr &func_graph, const AnfNodePtr &inp,
                                             const ShapeVector &shape_vec, const TypeId &src_type_id,
                                             const TypeId &dst_type_id) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  if (src_type_id == dst_type_id) {
    return inp;
  }

  auto cast = CreateCNodeBase(func_graph, {inp}, kCastOpName, inp);
  MS_EXCEPTION_IF_NULL(cast);
  if (dst_type_id == kNumberTypeFloat16) {
    common::AnfAlgo::SetNodeAttr(kAttrDstType, kFloat16, cast);
  } else if (dst_type_id == kNumberTypeFloat32) {
    common::AnfAlgo::SetNodeAttr(kAttrDstType, kFloat32, cast);
  } else {
    MS_EXCEPTION_IF_NULL(prim::kPrimClipByNorm);
    MS_EXCEPTION(TypeError) << "For '" << prim::kPrimClipByNorm->name()
                            << "`, the data type of input args only supports float16 or float32.";
  }
  common::AnfAlgo::SetNodeAttr(kIsBackendCast, MakeValue(true), cast);
  auto abs = std::make_shared<abstract::AbstractTensor>(TypeIdToType(dst_type_id), shape_vec);
  cast->set_abstract(abs);
  return cast;
}

std::vector<std::string> ClipByNormFission::MustExistPrimitiveName() const {
  std::vector<std::string> ret = {prim::kPrimClipByNorm->name()};
  return ret;
}

const BaseRef ClipByNormFission::DefinePattern() const {
  VarPtr seq_xs = std::make_shared<SeqVar>();
  return VectorRef({prim::kPrimClipByNorm, seq_xs});
}

const AnfNodePtr ClipByNormFission::Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                                            const EquivPtr &) const {
  // Get `ClipByNorm` cnode
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(node);
  auto clip_by_norm = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(clip_by_norm);
  if (common::AnfAlgo::IsNodeInputDynamicShape(clip_by_norm)) {
    return clip_by_norm;
  }
  constexpr size_t clip_by_norm_inp_num = 2;
  CheckCNodeInputSize(clip_by_norm, clip_by_norm_inp_num);
  // Get input node `x` and `clip_norm`)
  const auto &inp_x = clip_by_norm->input(1);
  constexpr size_t clip_norm_inp_idx = 2;
  const auto &inp_clip_norm = clip_by_norm->input(clip_norm_inp_idx);
  // Get abstract info
  TypeId dst_type_id = kNumberTypeFloat32;
  auto shape_vec = GetOutputInferShape(clip_by_norm);
  auto x_type_id = common::AnfAlgo::GetPrevNodeOutputInferDataType(clip_by_norm, 0);
  // Create `op1 = square(x)` op
  auto square = CreateSquareNode(func_graph, inp_x, shape_vec, x_type_id);
  square->set_scope(node->scope());
  // Create `op2 = reduce_sum(op1)` op
  auto reduce_sum = CreateReduceSumNode(func_graph, square, clip_by_norm, shape_vec, x_type_id);
  reduce_sum->set_scope(node->scope());
  ShapeVector reduce_sum_output_shape = GetOutputInferShape(reduce_sum);
  // Create `op3 = cast(op2)` to float32 data type
  auto reduce_sum_cast = CreateCastNode(func_graph, reduce_sum, reduce_sum_output_shape, x_type_id, dst_type_id);
  reduce_sum_cast->set_scope(node->scope());
  // Create `op4 = greater(op3, zeros)` op
  auto zeros_node =
    CreateConstantNode(func_graph, reduce_sum_cast, reduce_sum_output_shape, dst_type_id, ops::kNameZerosLike);
  zeros_node->set_scope(node->scope());
  auto greater = CreateGreaterNode(func_graph, reduce_sum_cast, zeros_node, reduce_sum_output_shape);
  greater->set_scope(node->scope());
  // Create `op5 = select(op4, op3, Ones)` op
  auto ones_node =
    CreateConstantNode(func_graph, reduce_sum_cast, reduce_sum_output_shape, dst_type_id, ops::kNameOnesLike);
  ones_node->set_scope(node->scope());
  auto safe_reduce_sum_cast =
    CreateSelectNode(func_graph, greater, reduce_sum_cast, ones_node, reduce_sum_output_shape, dst_type_id);
  safe_reduce_sum_cast->set_scope(node->scope());
  // Create `op6 = sqrt(op5)` op
  auto sqrt = CreateSqrtNode(func_graph, safe_reduce_sum_cast, dst_type_id);
  sqrt->set_scope(node->scope());
  // Create `op7 = select(op4, op6, op3)` op
  auto safe_sqrt = CreateSelectNode(func_graph, greater, sqrt, reduce_sum_cast, reduce_sum_output_shape, dst_type_id);
  safe_sqrt->set_scope(node->scope());
  // Create 'op8 = x * clip_norm' op
  auto inp_x_cast = CreateCastNode(func_graph, inp_x, shape_vec, x_type_id, dst_type_id);
  inp_x_cast->set_scope(node->scope());
  auto clip_norm_cast = CreateCastNode(func_graph, inp_clip_norm, GetOutputInferShape(inp_clip_norm),
                                       common::AnfAlgo::GetOutputInferDataType(inp_clip_norm, 0), dst_type_id);
  clip_norm_cast->set_scope(node->scope());
  auto mul = CreateMulNode(func_graph, inp_x_cast, clip_norm_cast, shape_vec, dst_type_id);
  mul->set_scope(node->scope());
  // Create `op9 = max(op8, op7)` op
  auto max = CreateMaxNode(func_graph, clip_norm_cast, safe_sqrt, dst_type_id);
  max->set_scope(node->scope());
  // Create 'op10 = op8 / op9' op
  auto div = CreateDivNode(func_graph, mul, max, shape_vec, dst_type_id);
  div->set_scope(node->scope());
  return div;
}
}  // namespace opt
}  // namespace mindspore
