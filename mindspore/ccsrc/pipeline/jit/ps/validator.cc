/**
 * This is the C++ adaptation and derivative work of Myia (https://github.com/mila-iqia/myia/).
 *
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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

#include "pipeline/jit/ps/validator.h"

#include <memory>
#include <mutex>
#include <string>

#include "mindspore/core/ops/structure_ops.h"
#include "mindspore/core/ops/sequence_ops.h"
#include "mindspore/core/ops/other_ops.h"
#include "mindspore/core/ops/nn_optimizer_ops.h"
#include "mindspore/core/ops/framework_ops.h"
#include "ir/manager.h"
#include "ir/dtype.h"
#include "pipeline/jit/ps/static_analysis/prim.h"
#include "pipeline/jit/ps/parse/resolve.h"
#include "pipeline/jit/ps/debug/trace.h"

namespace mindspore {
namespace validator {
using mindspore::abstract::AbstractBase;
using mindspore::abstract::AbstractFunction;
using mindspore::abstract::AbstractJTagged;
using mindspore::abstract::AbstractList;
using mindspore::abstract::AbstractMapTensor;
using mindspore::abstract::AbstractProblem;
using mindspore::abstract::AbstractRefTensor;
using mindspore::abstract::AbstractRowTensor;
using mindspore::abstract::AbstractScalar;
using mindspore::abstract::AbstractSequence;
using mindspore::abstract::AbstractTensor;
using mindspore::abstract::AbstractTuple;
using mindspore::abstract::AbstractType;

void ValidateOperation(const AnfNodePtr &node) {
  if (!IsValueNode<Primitive>(node)) {
    return;
  }

  // Primitive must in whitelist
  auto prim = GetValueNode<PrimitivePtr>(node);
  MS_EXCEPTION_IF_NULL(prim);
  if (prim->isa<prim::DoSignaturePrimitive>()) {
    MS_LOG(INTERNAL_EXCEPTION) << "Illegal DoSignaturePrimitive '" << prim->name() << "' in the graph.";
  }
  if (abstract::IsInWhiteList(prim)) {
    return;
  }
  if (prim->HasAttr("is_load")) {
    return;
  }
  if (prim->name() == "PyExecute") {
    return;
  }
  if (prim->name() == "TensorMove") {
    return;
  }

  if (prim->isa<PrimitivePy>()) {
    MS_LOG(DEBUG) << "Primitive " << prim->name() << " has python evaluator.";
    return;
  }
  if (prim->name() == "fake_bprop") {
    MS_LOG(INTERNAL_EXCEPTION) << "Illegal primitive: " << GetValue<std::string>(prim->GetAttr("info"));
  }

  MS_LOG(EXCEPTION) << "Illegal primitive: " << prim->name() << ". Please check whether to use unsupported primitive";
}

bool CheckAbstractScalar(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  AbstractBasePtr abstract = node->abstract();
  if (abstract->isa<AbstractScalar>()) {
    TypePtr type = abstract->GetTypeTrack();
    MS_EXCEPTION_IF_NULL(type);
    if (type->isa<EnvType>() || type->isa<MsClassType>()) {
      MS_LOG(EXCEPTION) << "Illegal type in the graph: " << abstract->ToString() << ", node: " << node->DebugString();
    }
    auto real_node = node;
    if (IsPrimitiveCNode(node, prim::kPrimReturn) || IsPrimitiveCNode(node, prim::kPrimDepend)) {
      real_node = real_node->cast<CNodePtr>()->input(1);
    }
    // Only allow string/number type from external.
    if (type->isa<External>() && !IsValueNode<StringImm>(real_node) && !IsValueNode<FP32Imm>(real_node) &&
        !IsValueNode<FP64Imm>(real_node)) {
      MS_LOG(EXCEPTION) << "Illegal type in the graph: " << abstract->ToString()
                        << ", node: " << real_node->DebugString();
    }
    // When a DeadNode is renormalized before, its abstract may be changed to
    // AbstractScalar(std:: make_shared<Int32Imm>(0), std:: make_shared<Problem>()).
    if (type->isa<Problem>()) {
      auto value = abstract->GetValueTrack();
      MS_EXCEPTION_IF_NULL(value);
      node->set_abstract(value->ToAbstract());
    }
    return true;
  }
  return false;
}

void ValidateAbstract(const AnfNodePtr &node) {
  if (node == nullptr) {
    MS_LOG(DEBUG) << "Node to validate is invalid";
    return;
  }
  AbstractBasePtr abstract = node->abstract();
  if (abstract == nullptr) {
    MS_LOG(DEBUG) << "Abstract is null in node: " << node->DebugString();
    return;
  }
  if (abstract->isa<AbstractJTagged>()) {
    // Validate a type.
    MS_LOG(INTERNAL_EXCEPTION) << "Illegal type in the graph: " << abstract->ToString()
                               << ", node: " << node->DebugString();
  }
  if (CheckAbstractScalar(node)) {
    return;
  }
  if (abstract->isa<AbstractProblem>()) {
    // NOTICE: validate dead code?
    MS_LOG(DEBUG) << "AbstractProblem in the graph: " << abstract->ToString();
    return;
  }
  bool is_legal_abstract = abstract->isa<AbstractType>() || abstract->isa<AbstractFunction>() ||
                           abstract->isa<AbstractTuple>() || abstract->isa<AbstractList>() ||
                           abstract->isa<AbstractTensor>() || abstract->isa<AbstractRowTensor>() ||
                           abstract->isa<AbstractRefTensor>() || abstract->isa<AbstractMapTensor>() ||
                           abstract->isa<abstract::AbstractNone>() || abstract->isa<abstract::AbstractMonad>() ||
                           abstract->isa<abstract::AbstractScript>();
  if (is_legal_abstract) {
    return;
  }

  // Other types show exception
  MS_LOG(INTERNAL_EXCEPTION) << "Illegal type in the graph: " << abstract->ToString();
}

void CheckValueTuple(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto value_node = node->cast_ptr<ValueNode>();
  MS_EXCEPTION_IF_NULL(value_node);
  const auto &value = value_node->value();
  MS_EXCEPTION_IF_NULL(value);
  auto value_tuple = value->cast_ptr<ValueTuple>();
  MS_EXCEPTION_IF_NULL(value_tuple);
  const auto &tuple_values = value_tuple->value();
  for (const auto &tuple_value : tuple_values) {
    auto input_node = NewValueNode(tuple_value);
    ValidateOperation(input_node);
  }
}

void CheckAssignReturnValue(const AnfNodePtr &node) {
  static const PrimitiveSet assign_prims = {prim::kPrimAssign, prim::kPrimAssignAdd, prim::kPrimAssignSub};
  if (IsPrimitiveCNode(node, prim::kPrimDepend)) {
    auto real_input = node->cast_ptr<CNode>()->input(1);
    while (IsPrimitiveCNode(real_input, prim::kPrimDepend)) {
      real_input = real_input->cast_ptr<CNode>()->input(1);
    }
    if (!IsOneOfPrimitiveCNode(real_input, assign_prims)) {
      return;
    }
  } else if (!IsOneOfPrimitiveCNode(node, assign_prims)) {
    return;
  }
  auto fg = node->func_graph();
  MS_EXCEPTION_IF_NULL(fg);
  auto mgr = fg->manager();
  MS_EXCEPTION_IF_NULL(mgr);
  auto &node_users = mgr->node_users();
  auto iter = node_users.find(node);
  if (iter == node_users.end()) {
    return;
  }
  static const PrimitiveSet virtual_prims = {
    prim::kPrimImageSummary, prim::kPrimScalarSummary, prim::kPrimTensorSummary, prim::kPrimHistogramSummary,
    prim::kPrimMakeTuple,    prim::kPrimStateSetItem,  prim::kPrimTupleGetItem,  prim::kPrimLoad,
    prim::kPrimPartial,      prim::kPrimDepend,        prim::kPrimUpdateState,   prim::kPrimDynamicLossScale};
  auto users = iter->second;
  for (const auto &user : users) {
    auto user_node = user.first;
    if (!IsOneOfPrimitiveCNode(user_node, virtual_prims)) {
      MS_LOG(WARNING) << "Deprecated: the return value of Assign/AssignAdd/AssignSub operator will be removed "
                      << "in subsequent releases.\n"
                      << "You can modify the code from:\na = P.Assign()(param, value)\nb = a * 2\nto: \n"
                      << "P.Assign()(param, value)\nb = param * 2\n"
                      << "Please check your code:" << trace::GetDebugInfoStr(node->debug_info());
    }
  }
}

void CheckDeadNodeInOutputRecursively(const AnfNodePtr &node, const AbstractBasePtr &abstract) {
  if (abstract == nullptr) {
    return;
  }
  TypePtr type = abstract->BuildType();
  MS_EXCEPTION_IF_NULL(type);
  if (type->isa<Problem>() || type->isa<Function>()) {
    MS_LOG(EXCEPTION) << "Function in output is not supported. Please check your code. "
                      << trace::GetDebugInfoStr(node->debug_info());
  }
  if (abstract->isa<AbstractSequence>()) {
    auto abs_seq = abstract->cast_ptr<AbstractSequence>();
    for (const auto &elem : abs_seq->elements()) {
      CheckDeadNodeInOutputRecursively(node, elem);
    }
  }
}

void ValidateTopGraphOutput(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  auto abstract = node->abstract();
  CheckDeadNodeInOutputRecursively(node, abstract);
}

void Validate(const FuncGraphPtr &func_graph) {
  FuncGraphManagerPtr mgr = Manage(func_graph, false);
  MS_EXCEPTION_IF_NULL(mgr);
  ValidateTopGraphOutput(func_graph->output());
  const AnfNodeSet &all_nodes = mgr->all_nodes();
  for (auto node : all_nodes) {
    TraceGuard guard(std::make_shared<TraceCopy>(node->debug_info()));
    CheckAssignReturnValue(node);
    while (IsPrimitiveCNode(node, prim::kPrimReturn) || IsPrimitiveCNode(node, prim::kPrimDepend)) {
      node = node->cast_ptr<CNode>()->input(1);
    }
    if (IsValueNode<ValueTuple>(node)) {
      CheckValueTuple(node);
      continue;
    }
    ValidateOperation(node);
  }
  for (const auto &node : all_nodes) {
    ValidateAbstract(node);
  }
}
}  // namespace validator
}  // namespace mindspore
