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

#ifndef MINDSPORE_CCSRC_COMMON_EXPANDER_CORE_INFER_H_
#define MINDSPORE_CCSRC_COMMON_EXPANDER_CORE_INFER_H_
#include <memory>
#include "include/common/expander/core/node.h"
#include "abstract/ops/primitive_infer_map.h"

namespace mindspore {
namespace expander {
/// \brief ExpanderInfer is the adapter for inferring functions that is called in emitter.
class COMMON_EXPORT ExpanderInfer {
 public:
  ExpanderInfer() = default;
  virtual ~ExpanderInfer() = default;

  /// \brief Infer shape and dtype for node
  virtual void Infer(const NodePtr &node) = 0;

  virtual AbstractBasePtr GetAbstract(const NodePtr &node) = 0;
  virtual BaseShapePtr GetShape(const NodePtr &node) = 0;
  virtual TypePtr GetDtype(const NodePtr &node) = 0;
};
using ExpanderInferPtr = std::shared_ptr<ExpanderInfer>;

/// \brief CppInfer calls the InferShapeAndType interface of frontend or backend map.
class COMMON_EXPORT CppInfer : public ExpanderInfer {
 public:
  void Infer(const NodePtr &node) override { return InferAnfnode(node->get()); }
  AbstractBasePtr GetAbstract(const NodePtr &node) override { return node->get()->abstract(); }
  BaseShapePtr GetShape(const NodePtr &node) override;
  TypePtr GetDtype(const NodePtr &node) override;

 protected:
  void InferAnfnode(const AnfNodePtr &anfnode) const;
  static abstract::PrimitiveEvalImplMap &infer_impl_cache() {
    static abstract::PrimitiveEvalImplMap cache{};
    return cache;
  }
};
}  // namespace expander
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_COMMON_EXPANDER_CORE_INFER_H_
