/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_FRONTEND_EXPANDER_BPROP_BPROP_IRBUILDER_H_
#define MINDSPORE_CCSRC_FRONTEND_EXPANDER_BPROP_BPROP_IRBUILDER_H_

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <unordered_set>
#include <functional>

#include "include/common/expander/core/node.h"
#include "include/common/expander/core/emitter.h"
#include "utils/hash_map.h"

namespace mindspore {
namespace expander {
namespace bprop {
class BpropBuilder;

using BpropBuilderFunc = std::function<NodePtrList(BpropBuilder *)>;
struct COMMON_EXPORT BpropHandle {
  BpropBuilderFunc func;
  mindspore::HashSet<size_t> unused_inputs;
};

class COMMON_EXPORT BpropBuilder : public Emitter {
 public:
  BpropBuilder(const std::string &name, const ExpanderInferPtr &infer)
      : Emitter(infer, std::make_shared<Scope>(std::string("Bprop/grad") + name)), name_(name) {}
  BpropBuilder();

  /// \brief Run irbuilder to generate a graph
  NodePtrList Run(const NodePtrList &inputs, const mindspore::HashMap<std::string, ValuePtr> &attrs,
                  const BpropHandle &handle, const std::string &instance_name);
  ValuePtr GetAttr(const std::string &attr) const;
  template <typename S>
  S GetAttr(const std::string &attr) const {
    return GetValue<S>(GetAttr(attr));
  }
  const mindspore::HashMap<std::string, ValuePtr> &GetAttrs() const { return *attrs_ptr_; }
  NodePtr GetInput(size_t i) const;
  const NodePtrList &GetInputs() const { return *inputs_ptr_; }

  NodePtrList BroadcastGradientArgs(const NodePtr &s0, const NodePtr &s1, size_t shift = 0);

  // For node that has single output
  ShapeVector GetShape(const NodePtr &node) const { return node->shape(); }
  size_t GetRank(const NodePtr &node) const { return GetShape(node).size(); }
  // For node that has multiple outputs
  std::vector<ShapeVector> GetShapes(const NodePtr &node) const { return node->shapes(); }
  TypePtr GetDtype(const NodePtr &node) const { return node->dtype(); }
  TypeId GetDtypeId(const NodePtr &node) const { return GetDtype(node)->type_id(); }
  ValuePtr GetAttr(const NodePtr &node, const std::string &attr) const;
  int64_t GetSize(const NodePtr &node) const;
  NodePtr DynSize(const NodePtr &node, const TypePtr &type);
  NodePtr DynSize(const NodePtr &node, TypeId type_id);
  NodePtr DynSize(const NodePtr &node);
  NodePtr Range(const NodePtr &limit) { return Range(Value<int64_t>(0), limit, Value<int64_t>(1)); }
  NodePtr Range(const NodePtr &start, const NodePtr &limit, const NodePtr &delta, int64_t max_len = 1000000) {
    return Emit("Range", {start, limit, delta, Value(max_len)});
  }

  NodePtr SequenceToTensor(const NodePtr &node, const TypePtr &dtype = kInt64);
  NodePtr TensorToSequence(const NodePtr &node, const AbstractBasePtr &abs, const TypePtr &dtype = kInt64);
  NodePtr SequenceSetItem(const NodePtr &node, const NodePtr &index, const NodePtr &value);
  NodePtr SequenceSlice(const NodePtr &node, const NodePtr &start, const NodePtr &stop, const NodePtr &step);
  NodePtr TensorToScalar(const NodePtr &node);

  std::string name() const { return name_; }
  std::string GetTargetFromContext() const;
  bool IsGraphMode() const;

  // Tensor getitem by a single integer number on the outermost axis.
  NodePtr TensorGetItem(const NodePtr &node, int64_t idx);

  // get a tensor slice like python.
  // case 1: x[...,2]   => StridedSlice(x, {{-1,{2}}})
  // case 2: x[2, ..., 1:3]   => StridedSlice(x, {{0,{2}}, {-1,{1,3}}})
  // case 3: x[..., 0:3:2, 0::2, :]   => StridedSlice(x, {{-3,{0,3,2}}, {-2,{0,LLONG_MAX,2}}})
  NodePtr StridedSlice(const NodePtr &x, const std::map<int64_t, std::vector<int64_t>> &slices);

  NodePtr StridedSlice(const NodePtr &dout, const NodePtr &begin, const NodePtr &end, const NodePtr &strides,
                       const NodePtr &begin_mask, const NodePtr &end_mask, const NodePtr &ellipsis_mask,
                       const NodePtr &new_axis_mask, const NodePtr &shrink_axis_mask) {
    return Emit("StridedSlice",
                {dout, begin, end, strides, begin_mask, end_mask, ellipsis_mask, new_axis_mask, shrink_axis_mask});
  }

  NodePtr StridedSlice(const NodePtr &dout, const NodePtr &begin, const NodePtr &end, const NodePtr &strides,
                       int64_t begin_mask = 0, int64_t end_mask = 0, int64_t ellipsis_mask = 0,
                       int64_t new_axis_mask = 0, int64_t shrink_axis_mask = 0) {
    return StridedSlice(dout, begin, end, strides, Value(begin_mask), Value(end_mask), Value(ellipsis_mask),
                        Value(new_axis_mask), Value(shrink_axis_mask));
  }

  std::string GetInstanceName() const { return instance_name_; }
  NodePtr TanhGrad(const NodePtr &y, const NodePtr &dy) { return Emit("TanhGrad", {y, dy}); }
  virtual NodePtr OutZeros(const NodePtr &node) { return ZerosLike(node); }

 protected:
  std::string name_;
  std::string instance_name_;
  const NodePtrList *inputs_ptr_{nullptr};
  const mindspore::HashMap<std::string, ValuePtr> *attrs_ptr_{nullptr};
};

class IrBuilder : public BpropBuilder {
 public:
  IrBuilder(const std::string &name, const FuncGraphPtr &func_graph, const ExpanderInferPtr &infer)
      : BpropBuilder(name, infer), func_graph_(func_graph) {}
  NodePtr EmitOp(const PrimitivePtr &prim, const NodePtrList &inputs) override;
  NodePtr EmitValue(const ValuePtr &value) override;
  using BlockFunc = std::function<NodePtrList(Emitter *)>;
  /// \brief Generate a conditional block.
  ///
  /// \param[in] cond condition node, it should be a tensor of Bool.
  /// \param[in] true_case  the true branch.
  /// \param[in] false_case the false branch.
  /// \return node of tuple or single value, which is depends on the output list of two branches.
  /// \note The overloaded operators (like a+b) should not be used for captured variables in the true_case/false_case
  /// functions, use the function argument `Emitter` instead, like `emitter->Add(a, b)`. The output list of two branches
  /// should match the join rules of control flow.
  NodePtr Conditional(const NodePtr &cond, const BlockFunc &true_case, const BlockFunc &false_case) override;

  /// \brief Generate a while-loop block.
  ///
  /// \param[in] cond condition node, it should be a tensor of Bool.
  /// \param[in] body  the loop body.
  /// \param[in] init_list the initial variables that would be modified in body.
  /// \return node of tuple or single value, which is depends on the init_list.
  /// \note The overloaded operators (like `a+b`) should not be used for captured variables in the body function, use
  /// the function argument `Emitter` instead, like `emitter->Add(a, b)`. The length and node order of the output list
  /// of the body function should match init_list.
  NodePtr While(const NodePtr &cond, const BlockFunc &body, const NodePtrList &init_list) override;
  const FuncGraphPtr &func_graph() { return func_graph_; }

 protected:
  FuncGraphPtr func_graph_;
};

class COMMON_EXPORT BpropIRBuilderFactory {
 public:
  static BpropIRBuilderFactory &Instance() {
    static BpropIRBuilderFactory instance{};
    return instance;
  }

  const BpropHandle *GetBuilder(const std::string &name) const {
    auto iter = registry_.find(name);
    return (iter == registry_.end()) ? nullptr : &(iter->second);
  }

  void RegBuilder(const std::string &name, const BpropBuilderFunc &func) { registry_[name].func = func; }
  void RegUnusedInputs(const std::string &name, const mindspore::HashSet<size_t> &unused) {
    registry_[name].unused_inputs = unused;
  }

 private:
  HashMap<std::string, BpropHandle> registry_;
};

class BpropIRBuilderRegHelper {
 public:
  explicit BpropIRBuilderRegHelper(const std::string &name) : name_(name) {}
  ~BpropIRBuilderRegHelper() = default;
  const BpropIRBuilderRegHelper &SetBody(const BpropBuilderFunc &func) const {
    BpropIRBuilderFactory::Instance().RegBuilder(name_, func);
    return *this;
  }
  const BpropIRBuilderRegHelper &SetUnusedInputs(const std::initializer_list<size_t> &unused_inputs) const {
    BpropIRBuilderFactory::Instance().RegUnusedInputs(name_, unused_inputs);
    return *this;
  }

 private:
  std::string name_;
};

#define BPROP_EXPANDER_JOIN(x, y) x##y
#define BPROP_EXPANDER_UNIQUE_NAME(prefix, cnt) BPROP_EXPANDER_JOIN(prefix, cnt)
#define REG_BPROP_BUILDER(name) \
  const BpropIRBuilderRegHelper BPROP_EXPANDER_UNIQUE_NAME(g_bprop, __COUNTER__) = BpropIRBuilderRegHelper(name)
#define BODYFUNC(v) [](BpropBuilder * (v)) -> NodePtrList

#ifdef _MSC_VER
#define REG_BPROP_BUILDERS_BEGIN(func_name) void Reg##func_name() {
#define REG_BPROP_BUILDERS_END }
#else
#define REG_BPROP_BUILDERS_BEGIN(func_name)
#define REG_BPROP_BUILDERS_END
#endif
}  // namespace bprop
}  // namespace expander
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_FRONTEND_EXPANDER_BPROP_BPROP_IRBUILDER_H_
