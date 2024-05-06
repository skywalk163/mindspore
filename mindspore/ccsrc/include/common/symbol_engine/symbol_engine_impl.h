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
#ifndef MINDSPORE_CCSRC_INCLUDE_COMMON_SYMBOL_ENGINE_SYMBOL_ENGINE_IMPL_H_
#define MINDSPORE_CCSRC_INCLUDE_COMMON_SYMBOL_ENGINE_SYMBOL_ENGINE_IMPL_H_
#include <vector>
#include <utility>
#include <unordered_map>
#include <map>
#include <string>
#include <memory>
#include <set>

#include "ir/anf.h"
#include "ir/func_graph.h"
#include "mindspore/core/symbolic_shape/symbol_engine.h"
#include "mindspore/core/symbolic_shape/symbol.h"
#include "mindspore/core/symbolic_shape/operation_builder.h"
#include "mindspore/core/symbolic_shape/operation.h"
#include "include/common/visible.h"

namespace mindspore {
namespace symshape {
struct COMMON_EXPORT DependStatus {
  bool shape{false};
  bool value{false};
};

/// \brief When a CNode's input[0] is also a CNode, it's a SpecialCNode.
class COMMON_EXPORT SpecialCNodeHelper {
 public:
  explicit SpecialCNodeHelper(const CNodePtr &cnode) : cnode_(cnode) {}
  virtual ~SpecialCNodeHelper() = default;
  virtual void SetDependStatus(std::map<AnfNodePtr, DependStatus> *depend_status_map) = 0;
  virtual std::pair<PrimitivePtr, AbstractBasePtrList> ExtractInputs() = 0;

 protected:
  CNodePtr cnode_;
};

class COMMON_EXPORT SymbolEngineImpl : public SymbolEngine {
 public:
  explicit SymbolEngineImpl(const FuncGraphPtr &fg) : SymbolEngine(fg), name_(fg->ToString()) {}
  ~SymbolEngineImpl() = default;
  MS_DECLARE_PARENT(SymbolEngineImpl, SymbolEngine)

  /// \brief Build SymbolEngine, and set to the FuncGraph.
  static std::shared_ptr<symshape::SymbolEngineImpl> Build(const FuncGraphPtr &func_graph);

  bool Infer(const AbstractBasePtrList &inputs) override;
  bool IsDependValue(const AnfNodePtr &node) override;
  bool IsDependShape(const AnfNodePtr &node) override;
  bool SupportInfer() override { return support_infer_; }
  void QuerySymbolExpr(const AnfNodePtr &node, std::unordered_map<std::string, std::string> *symbol_expr_map) override;

  std::string ToString() const override { return "SymbolEngine_" + name_; }
  std::string DumpText() const override;

  virtual void BuildSubgraphImpl(const CNodePtr &cnode, const FuncGraphPtr &sub_fg, size_t begin_input_index);
  virtual void PreBuildQuerySubgraphDependStatus(const CNodePtr &cnode, const FuncGraphPtr &sub_fg,
                                                 size_t begin_input_index);

 protected:
  // prebuild of symbol engine, it should be called before BuildImpl
  void PreBuild();
  void PreBuildQueryDependStatus(const AnfNodePtrList &cnodes);
  void PreBuildSpecialNode(const CNodePtr &cnode);

  // build symbol engine
  void BuildImpl();
  SymbolPtr BuildCNodeSymbolicShape(OperationBuilder *builder, const PrimitivePtr &prim,
                                    const AbstractBasePtrList &inputs, const AbstractBasePtr &abstract,
                                    const CNodePtr &cnode);
  SymbolPtr BuildCNodeSymbolicValue(OperationBuilder *builder, const PrimitivePtr &prim,
                                    const AbstractBasePtrList &inputs, const AbstractBasePtr &abstract,
                                    const CNodePtr &cnode);
  virtual AbstractBasePtrList ExtractInputsAbstract(const CNodePtr &cnode);

  std::string QuerySymbolExprHelper(const SymbolPtr &s,
                                    const std::unordered_map<std::string, std::string> &symbol_expr_map);

  void BuildNodesSymbol(const FuncGraphPtr &fg, const AnfNodePtrList &cnodes);
  void BuildCNodeSymbol(const CNodePtr &cnode);

  std::string name_;
  AnfNodePtrList cnodes_;
  OpPtrList ops_;
  std::unique_ptr<OperationEmitter> emitter_;
  bool support_infer_{true};
  std::map<AnfNodePtr, DependStatus> depend_status_map_;
  std::set<FuncGraph *> visited_graph_;
  std::map<AnfNodePtr, std::shared_ptr<SpecialCNodeHelper>> special_cnodes_;
};

using SymbolEngineImplPtr = std::shared_ptr<symshape::SymbolEngineImpl>;
/// \brief nodes have same digital shape may use same abstract object, but their symbolic shape may not same, clone a
/// new abstract for symbolic info.
COMMON_EXPORT AbstractBasePtr CloneAbstractIfSymbolExists(const AbstractBasePtr &abs);
inline AbstractBasePtr CloneAbstractIfSymbolExists(const AnfNodePtr &node) {
  node->set_abstract(CloneAbstractIfSymbolExists(node->abstract()));
  return node->abstract();
}

COMMON_EXPORT void CleanSymbols(const FuncGraphPtr &func_graph);
}  // namespace symshape
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_INCLUDE_COMMON_SYMBOL_ENGINE_SYMBOL_ENGINE_IMPL_H_
