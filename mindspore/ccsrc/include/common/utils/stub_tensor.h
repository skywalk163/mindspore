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

#ifndef MINDSPORE_CCSRC_UTILS_STUB_TENSOR_PY_H_
#define MINDSPORE_CCSRC_UTILS_STUB_TENSOR_PY_H_
#include <memory>
#include <atomic>
#include <vector>
#include <utility>
#include <exception>
#include <condition_variable>
#include <mutex>

#include "pybind11/pytypes.h"
#include "pybind11/pybind11.h"
#include "base/base.h"
#include "ir/value.h"
#include "ir/tensor.h"
#include "mindapi/base/shape_vector.h"
#include "abstract/abstract_value.h"

namespace mindspore {
namespace stub {
constexpr auto PY_ATTR_STUB = "stub";
constexpr auto PY_ATTR_TENSOR = "tensor";
constexpr auto PY_ATTR_SYNC = "stub_sync";

namespace py = pybind11;
class StubNode;
using StubNodePtr = std::shared_ptr<StubNode>;
using abstract::AbstractBasePtr;

class COMMON_EXPORT StubNode : public Value {
 public:
  StubNode() = default;
  virtual ~StubNode() = default;
  MS_DECLARE_PARENT(StubNode, Value);

  virtual bool SetAbstract(const AbstractBasePtr &abs);
  virtual void SetValue(const ValuePtr &val);
  virtual void SetException(const std::exception_ptr &e_ptr);

  AbstractBasePtr WaitAbstract();
  ValuePtr WaitValue();

  AbstractBasePtr ToAbstract() override { return WaitAbstract(); }
  bool operator==(const Value &other) const override { return other.isa<StubNode>() && &other == this; }

 protected:
  AbstractBasePtr abstract_;
  ValuePtr value_;
  std::condition_variable cond_var_;
  std::mutex mutex_;
  std::exception_ptr e_ptr_{};
};

class TensorNode : public StubNode {
 public:
  TensorNode() = default;
  MS_DECLARE_PARENT(TensorNode, StubNode);
  bool SetAbstract(const AbstractBasePtr &abs) override;

  py::object GetValue();
  py::object GetShape();
  py::object GetDtype();
};

class SequenceNode : public StubNode {
 public:
  explicit SequenceNode(size_t size = 0) : elements_(size), is_elements_build_(size > 0) {}
  MS_DECLARE_PARENT(SequenceNode, StubNode);

  py::object GetElements();

  bool SetAbstract(const AbstractBasePtr &abs) override;
  void SetValue(const ValuePtr &val) override;
  void SetException(const std::exception_ptr &e_ptr) override;

  void SetElement(size_t i, const StubNodePtr &node) { elements_[i] = node; }
  const std::vector<StubNodePtr> &Elements() const { return elements_; }

 private:
  std::vector<StubNodePtr> elements_;
  std::atomic<bool> is_elements_build_{false};
};
using SequenceNodePtr = std::shared_ptr<SequenceNode>;

class AnyTypeNode : public StubNode {
 public:
  AnyTypeNode() = default;
  MS_DECLARE_PARENT(AnyTypeNode, StubNode);
  bool SetAbstract(const AbstractBasePtr &abs) override;
  void SetValue(const ValuePtr &val) override;
  void SetException(const std::exception_ptr &e_ptr) override;
  py::object GetRealNode();

 private:
  StubNodePtr real_node_;
};

class NoneTypeNode : public StubNode {
 public:
  NoneTypeNode() = default;
  MS_DECLARE_PARENT(NoneTypeNode, StubNode);
  py::object GetRealValue();
};

COMMON_EXPORT std::pair<py::object, StubNodePtr> MakeTopNode(const TypePtr &type);
COMMON_EXPORT void RegStubNodes(const py::module *m);
}  // namespace stub
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_UTILS_STUB_TENSOR_PY_H_
