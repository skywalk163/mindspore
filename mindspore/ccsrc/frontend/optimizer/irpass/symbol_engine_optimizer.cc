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

#include "frontend/optimizer/irpass/symbol_engine_optimizer.h"

#include <vector>
#include <memory>
#include <utility>
#include "ir/pattern_matcher.h"
#include "ir/functor.h"
#include "ops/array_ops.h"
#include "ops/math_ops.h"
#include "ops/op_def.h"
#include "include/common/utils/utils.h"
#include "mindspore/core/symbolic_shape/symbol.h"
#include "mindspore/core/symbolic_shape/utils.h"
#include "include/common/symbol_engine/symbol_engine_impl.h"

namespace mindspore {
namespace opt {
namespace irpass {
inline SymbolEnginePtr GetSymbolEngine(const AnfNodePtr &node) { return node->func_graph()->symbol_engine(); }

bool SymbolEngineBuilder::operator()(const FuncGraphPtr &func_graph, const OptimizerPtr &opt) {
  if (only_dynshape_graph_ && !HasDynamicShapeNode(opt)) {
    MS_LOG(INFO) << "There is no dynamic shape node, the SymbolEngineBuilder is disabled.";
    return false;
  }
  try {
    MS_LOG_TRY_CATCH_SCOPE;
    symshape::SymbolEngineImpl::Build(func_graph);
    MS_LOG(INFO) << "Build symbol engine successfully.";
  } catch (std::exception &e) {
    MS_LOG(WARNING) << "Build symbol engine failed. message: " << e.what();
  }
  return true;
}

bool SymbolEngineBuilder::HasDynamicShapeNode(const OptimizerPtr &opt) const {
  auto mng = opt->manager();
  if (mng == nullptr) {
    return false;
  }
  auto &nodes = mng->all_nodes();
  for (auto &node : nodes) {
    if (!node->isa<CNode>()) {
      continue;
    }
    auto abs = node->abstract();
    if (abs != nullptr && abs->GetShape()->IsDynamic()) {
      return true;
    }
  }
  return false;
}

AnfNodePtr ElimShapeCalcOnBroadcastArgsGrad::operator()(const OptimizerPtr &opt, const AnfNodePtr &node) {
  if (GetSymbolEngine(node) == nullptr) {
    return nullptr;
  }
  PatternNode<AnfNodePtr> dout;
  PatternNode<AnfNodePtr> shape_calc;
  PatternNode<AnfNodePtr> shape;
  PatternNode<AnfNodePtr> keepdims;
  PatternNode<AnfNodePtr> skipmode;
  PConstant idx0(node, false, 0, true);
  PConstant idx1(node, false, 1, true);
  MATCH_REPLACE_IF(
    node,
    PPrimitive(prim::kPrimReduceSum, dout, PPrimitive(prim::kPrimTupleGetItem, shape_calc, idx0), keepdims, skipmode),
    dout, Check(opt, shape_calc.GetNode(node), kIndex1));
  MATCH_REPLACE_IF(
    node,
    PPrimitive(prim::kPrimReduceSum, dout, PPrimitive(prim::kPrimTupleGetItem, shape_calc, idx1), keepdims, skipmode),
    dout, Check(opt, shape_calc.GetNode(node), kIndex2));
  return nullptr;
}

bool ElimShapeCalcOnBroadcastArgsGrad::Check(const OptimizerPtr &opt, const AnfNodePtr &shape_calc,
                                             size_t input_index) {
  auto mng = opt->manager();
  MS_EXCEPTION_IF_NULL(mng);
  auto &users = mng->node_users();

  auto shapecalc_node = shape_calc->cast<CNodePtr>();
  constexpr const size_t shapecalc_size = 3;
  if (shapecalc_node == nullptr || !IsPrimitiveCNode(shapecalc_node, prim::kPrimShapeCalc) ||
      shapecalc_node->size() != shapecalc_size) {
    return false;
  }
  auto input_node = shapecalc_node->input(input_index);
  auto shapecalc_functor = common::AnfAlgo::GetNodeAttr<ShapeCalcBaseFunctorPtr>(shapecalc_node, kAttrFunctor);
  MS_EXCEPTION_IF_NULL(shapecalc_functor);
  if (shapecalc_functor->name() != "ShapeCalc_BroadcastGradientArgs") {
    // only support the broadcast gradient condition
    return false;
  }
  auto fwd_unique_id = shapecalc_node->primal_attrs().find(kPrimalAttrForwardUniqueId);
  if (fwd_unique_id == shapecalc_node->primal_attrs().end()) {
    // only support bprop node
    return false;
  }
  AnfNodePtr fwd_node = nullptr;
  for (auto &user : users[input_node]) {
    auto user_cnode = user.first->cast<CNodePtr>();
    if (user_cnode == nullptr) {
      continue;
    }
    if (auto uniq_id = user_cnode->primal_attrs().find(kPrimalAttrUniqueId);
        uniq_id != user_cnode->primal_attrs().end()) {
      if (*uniq_id->second == *fwd_unique_id->second) {
        fwd_node = user.first;
        break;
      }
    }
  }
  if (fwd_node == nullptr) {
    return false;
  }

  auto input_shape = input_node->abstract()->GetSymbolicShape();
  auto output_shape = fwd_node->abstract()->GetSymbolicShape();
  auto ret = CheckSymbolEqual(input_shape, output_shape, GetValue<size_t>(shapecalc_functor->ToValue()));
  if (ret) {
    MS_LOG(INFO) << "For " << shape_calc->DebugString() << " (" << shape_calc->fullname_with_scope() << ")"
                 << " generated by BroadcastGradientArgs. The gradient for input " << input_index
                 << " is unnecessary, which can be eliminated. grad symbol: " << input_shape->ToString()
                 << ". out symbol: " << output_shape->ToString();
  }
  return ret;
}

bool ElimShapeCalcOnBroadcastArgsGrad::CheckSymbolEqual(const ListSymbolPtr &input_shape,
                                                        const ListSymbolPtr &output_shape, size_t shift) {
  if (input_shape == nullptr && output_shape == nullptr) {
    return false;
  }
  if (input_shape->size() < output_shape->size()) {
    return false;
  }
  for (size_t i = input_shape->size(); i > shift; i--) {
    auto inp = input_shape->symbols()[input_shape->size() - i];
    if (i <= output_shape->size() && !inp->EqualsTo(output_shape->symbols()[output_shape->size() - i])) {
      return false;
    }
  }
  return true;
}

AnfNodePtr ElimNotEffectiveNode::operator()(const OptimizerPtr &, const AnfNodePtr &node) {
  if (GetSymbolEngine(node) == nullptr) {
    return nullptr;
  }
  static const PrimitiveSet supports_op = {prim::kPrimReshape, prim::kPrimReduceSum, prim::kPrimReduceMax,
                                           prim::kPrimReduceMin};
  if (!IsOneOfPrimitiveCNode(node, supports_op)) {
    return nullptr;
  }
  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  auto input_node = node->cast<CNodePtr>()->input(1);
  auto input_shape = input_node->abstract()->GetSymbolicShape();
  auto output_shape = node->abstract()->GetSymbolicShape();
  if (input_shape != nullptr && input_shape->EqualsTo(output_shape)) {
    MS_LOG(INFO) << "For node " << node->DebugString() << " (" << node->fullname_with_scope()
                 << "), the input shape and output shape is same, which can be eliminated.";
    return input_node;
  }
  return nullptr;
}

AnfNodePtr OptReshape::operator()(const OptimizerPtr &, const AnfNodePtr &node) {
  if (GetSymbolEngine(node) == nullptr) {
    return nullptr;
  }
  PatternNode<AnfNodePtr> input;
  PatternNode<AnfNodePtr> shape;
  ShapeVector shape_vec;

  auto MakeReshape = [&shape_vec, &node]() -> AnfNodePtr {
    auto shape_val = MakeValue(shape_vec);
    auto shape = NewValueNode(shape_val);
    auto cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    MS_LOG(INFO) << "For node " << cnode->DebugString()
                 << ", the symbolic value of \"shape\" is static or has only one dynamic dim, "
                 << "replace the \"shape\" to a value node: " << shape_val->ToString();
    shape->set_abstract(shape_val->ToAbstract());
    auto reshape = NewCNode({cnode->input(0), cnode->input(1), shape}, node->func_graph());
    reshape->set_abstract(node->abstract());
    return reshape;
  };
  auto CheckShape = [&shape_vec](const AnfNodePtr &shape) {
    if (!shape->isa<CNode>()) {
      return false;
    }
    auto symshape = shape->abstract()->GetSymbolicValue();
    if (symshape == nullptr || !symshape->HasData()) {
      return false;
    }
    shape_vec = symshape::ToShape(symshape);
    return std::count(shape_vec.cbegin(), shape_vec.cend(), abstract::Shape::kShapeDimAny) <= 1;
  };
  MATCH_REPLACE_LAMBDA_IF(node, PPrimitive(prim::kPrimReshape, input, shape), MakeReshape,
                          CheckShape(shape.GetNode(node)));
  return nullptr;
}

AnfNodePtr FoldConstSymbol::operator()(const OptimizerPtr &, const AnfNodePtr &node) {
  auto symbol_engine = GetSymbolEngine(node);
  if (symbol_engine == nullptr) {
    return nullptr;
  }
  auto op_def = mindspore::ops::GetOpDef(AnfUtils::GetCNodeName(node));
  if (op_def == nullptr) {
    return nullptr;
  }
  if (node->abstract() != nullptr && !symshape::QueryValue(node->abstract())->isa<ValueAny>()) {
    return nullptr;
  }
  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  AnfNodePtrList new_inputs;
  bool need_replace = false;
  for (size_t i = 1; i < cnode->size(); i++) {
    auto inp = cnode->input(i);
    if (!inp->isa<CNode>() || inp->abstract() == nullptr || i - 1 >= op_def->args_.size()) {
      continue;
    }
    auto v = symshape::QueryValue(inp->abstract());
    if (v->isa<ValueAny>()) {
      continue;
    }
    if (new_inputs.empty()) {
      new_inputs = cnode->inputs();
    }
    if (v->isa<ValueSequence>() && op_def->args_[i - 1].arg_dtype_ == ops::OP_DTYPE::DT_TUPLE_INT) {
      new_inputs[i] = NewValueNode(v);
      need_replace = true;
    } else {
      MS_LOG(INFO) << "For node " << node->DebugString() << ", the input[" << i
                   << "]'s value does not match the op_def type(" << op_def->args_[i - 1].arg_dtype_
                   << "). value = :" << v->ToString();
      continue;
    }
    MS_LOG(INFO) << "For node " << node->DebugString() << ", the input[" << i
                 << "]'s symbolic value is constant, fold the input value: " << v->ToString();
    auto new_abs = v->ToAbstract();
    MS_EXCEPTION_IF_NULL(new_abs);
    new_abs->SetSymbolicValue(inp->abstract()->GetSymbolicValue());
    new_inputs[i]->set_abstract(new_abs);
  }
  if (!need_replace) {
    return nullptr;
  }
  auto new_node = NewCNode(new_inputs, node->func_graph());
  new_node->set_abstract(node->abstract());
  return new_node;
}

bool ShapeOpCse::operator()(const FuncGraphPtr &func_graph, const OptimizerPtr &optimizer) {
  if (func_graph->symbol_engine() == nullptr) {
    return false;
  }
  auto nodes = TopoSort(func_graph->get_return(), SuccDeeperSimple, AlwaysInclude);
  auto mng = optimizer->manager();
  MS_EXCEPTION_IF_NULL(mng);
  std::vector<std::pair<AnfNodePtr, SymbolPtr>> shape_values;
  bool changed = false;
  for (auto &node : nodes) {
    if (IsPrimitiveCNode(node, prim::kPrimShape)) {
      auto v = node->abstract()->GetSymbolicValue();
      if (v == nullptr) {
        continue;
      }
      bool matched = false;
      for (auto &prev : shape_values) {
        if (node->func_graph() == prev.first->func_graph() && v->EqualsTo(prev.second)) {
          MS_LOG(INFO) << "The symbolic value of " << node->DebugString() << " (" << node->fullname_with_scope()
                       << ") is same as previous node " << prev.first->DebugString() << " ("
                       << prev.first->fullname_with_scope() << "), eliminated it. Value:" << v->ToString();
          mng->Replace(node, prev.first);
          changed = true;
          matched = true;
          break;
        }
      }
      if (!matched) {
        shape_values.emplace_back(std::make_pair(node, v));
      }
    }
  }
  return changed;
}
}  // namespace irpass
}  // namespace opt
}  // namespace mindspore
