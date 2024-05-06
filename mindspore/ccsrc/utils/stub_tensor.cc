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
#include "utils/ms_exception.h"
#include "include/common/utils/convert_utils_py.h"
#include "include/common/utils/stub_tensor.h"
#include "pybind_api/gil_scoped_long_running.h"
#include "include/common/profiler.h"

namespace mindspore {
namespace stub {
namespace {
StubNodePtr MakeStubNode(const TypePtr &type) {
  if (type->isa<TensorType>()) {
    return std::make_shared<TensorNode>();
  } else if (type->isa<Tuple>() || type->isa<List>()) {
    TypePtrList elements;
    if (type->isa<Tuple>()) {
      auto tuple_type = type->cast<TuplePtr>();
      elements = tuple_type->elements();
    } else {
      auto list_type = type->cast<ListPtr>();
      elements = list_type->elements();
    }
    auto node = std::make_shared<SequenceNode>(elements.size());
    for (size_t i = 0; i < elements.size(); ++i) {
      auto elem = MakeStubNode(elements[i]);
      node->SetElement(i, elem);
    }
    return node;
  } else if (type == kTypeAny) {
    return std::make_shared<AnyTypeNode>();
  } else if (type == kTypeNone) {
    return std::make_shared<NoneTypeNode>();
  } else {
    MS_LOG(WARNING) << "stub tensor is create for type: " << type->ToString();
  }
  return nullptr;
}

py::object MakeOutput(const StubNodePtr &node) {
  if (node->isa<TensorNode>()) {
    auto tensor = node->cast<std::shared_ptr<TensorNode>>();
    return py::cast(tensor);
  } else if (node->isa<SequenceNode>()) {
    auto seq = node->cast<std::shared_ptr<SequenceNode>>();
    MS_EXCEPTION_IF_NULL(seq);
    auto &elements = seq->Elements();
    if (elements.empty()) {
      return py::cast(seq);
    }
    py::tuple out(elements.size());
    for (size_t i = 0; i < elements.size(); ++i) {
      out[i] = MakeOutput(elements[i]);
    }
    return out;
  } else if (node->isa<AnyTypeNode>()) {
    auto tensor = node->cast<std::shared_ptr<AnyTypeNode>>();
    return py::cast(tensor);
  } else {
    auto tensor = node->cast<std::shared_ptr<NoneTypeNode>>();
    return py::cast(tensor);
  }
}
}  // namespace

bool StubNode::SetAbstract(const AbstractBasePtr &abs) {
  std::unique_lock<std::mutex> lock(mutex_);
  abstract_ = abs;
  cond_var_.notify_all();
  return true;
}

void StubNode::SetValue(const ValuePtr &val) {
  std::unique_lock<std::mutex> lock(mutex_);
  value_ = val;
  cond_var_.notify_all();
}

void StubNode::SetException(const std::exception_ptr &e_ptr) {
  // cppcheck-suppress unreadVariable
  std::unique_lock<std::mutex> lock(mutex_);
  e_ptr_ = e_ptr;
  cond_var_.notify_all();
}

AbstractBasePtr StubNode::WaitAbstract() {
  runtime::ProfilerStageRecorder recorder(runtime::ProfilerStage::kWaitPipeline);
  // cppcheck-suppress unreadVariable
  GilReleaseWithCheck gil_release;
  std::unique_lock<std::mutex> lock(mutex_);
  cond_var_.wait(lock, [this] { return abstract_.get() != nullptr || e_ptr_ != nullptr; });
  if (e_ptr_ != nullptr) {
    // Need to clear exception in the instance.
    MsException::Instance().CheckException();
    std::rethrow_exception(e_ptr_);
  }
  return abstract_;
}

ValuePtr StubNode::WaitValue() {
  runtime::ProfilerStageRecorder recorder(runtime::ProfilerStage::kWaitPipeline);
  // cppcheck-suppress unreadVariable
  GilReleaseWithCheck gil_release;
  std::unique_lock<std::mutex> lock(mutex_);
  cond_var_.wait(lock, [this] { return value_.get() != nullptr || e_ptr_ != nullptr; });
  if (e_ptr_ != nullptr) {
    // Need to clear exception in the instance.
    MsException::Instance().CheckException();
    std::rethrow_exception(e_ptr_);
  }
  return value_;
}

py::object TensorNode::GetValue() {
  auto val = WaitValue();
  return ValueToPyData(val);
}

py::object TensorNode::GetShape() {
  auto abs = WaitAbstract();
  auto base = abs->BuildShape();
  auto shape = base->cast<abstract::ShapePtr>();
  ShapeVector shape_vector;
  if (shape && !shape->IsDynamic()) {
    shape_vector = shape->shape();
  } else {
    auto val = WaitValue();
    auto tensor = val->cast<tensor::TensorPtr>();
    MS_EXCEPTION_IF_NULL(tensor);
    shape_vector = tensor->shape();
  }
  auto ret = py::tuple(shape_vector.size());
  for (size_t i = 0; i < shape_vector.size(); ++i) {
    ret[i] = shape_vector[i];
  }
  return ret;
}

py::object TensorNode::GetDtype() {
  auto abs = WaitAbstract();
  auto base = abs->BuildType();
  if (base->isa<TensorType>()) {
    base = base->cast<TensorTypePtr>()->element();
  }
  return py::cast(base);
}

bool TensorNode::SetAbstract(const AbstractBasePtr &abs) {
  if (!abs->isa<abstract::AbstractTensor>() && !abs->isa<abstract::AbstractMapTensor>()) {
    if (!abs->isa<abstract::AbstractScalar>() || abs->BuildValue() != kValueAny) {
      return false;
    }
  }
  return StubNode::SetAbstract(abs);
}

py::object SequenceNode::GetElements() {
  if (!is_elements_build_.load()) {
    (void)WaitAbstract();
  }
  py::tuple out(elements_.size());
  for (size_t i = 0; i < elements_.size(); ++i) {
    out[i] = MakeOutput(elements_[i]);
  }
  return out;
}

bool SequenceNode::SetAbstract(const AbstractBasePtr &abs) {
  auto seq_abs = abs->cast<abstract::AbstractSequencePtr>();
  if (seq_abs == nullptr) {
    return false;
  }
  auto children = seq_abs->elements();
  if (!is_elements_build_.load()) {
    for (auto child : children) {
      (void)elements_.emplace_back(MakeStubNode(child->BuildType()));
    }
  }
  is_elements_build_ = true;
  if (elements_.size() != children.size()) {
    return false;
  }
  for (size_t i = 0; i < elements_.size(); ++i) {
    if (!elements_[i]->SetAbstract(children[i])) {
      return false;
    }
  }
  return StubNode::SetAbstract(abs);
}

void SequenceNode::SetValue(const ValuePtr &val) {
  auto seq_value = val->cast<ValueSequencePtr>();
  auto children = seq_value->value();
  for (size_t i = 0; i < children.size(); ++i) {
    elements_[i]->SetValue(children[i]);
  }
  StubNode::SetValue(val);
}

void SequenceNode::SetException(const std::exception_ptr &e_ptr) {
  for (auto &element : elements_) {
    element->SetException(e_ptr);
  }
  StubNode::SetException(e_ptr);
}

bool AnyTypeNode::SetAbstract(const AbstractBasePtr &abs) {
  real_node_ = MakeStubNode(abs->BuildType());
  auto flag = real_node_->SetAbstract(abs);
  (void)StubNode::SetAbstract(abs);
  return flag;
}

void AnyTypeNode::SetValue(const ValuePtr &val) {
  real_node_->SetValue(val);
  StubNode::SetValue(val);
}

py::object AnyTypeNode::GetRealNode() {
  (void)WaitAbstract();
  return py::cast(real_node_);
}

py::object NoneTypeNode::GetRealValue() {
  auto val = WaitValue();
  return ValueToPyData(val);
}

void AnyTypeNode::SetException(const std::exception_ptr &e_ptr) {
  StubNode::SetException(e_ptr);
  if (real_node_ != nullptr) {
    real_node_->SetException(e_ptr);
  }
}

std::pair<py::object, StubNodePtr> MakeTopNode(const TypePtr &type) {
  auto top = MakeStubNode(type);
  auto ret = MakeOutput(top);
  return std::make_pair(ret, top);
}

void RegStubNodes(const py::module *m) {
  (void)py::class_<StubNode, std::shared_ptr<StubNode>>(*m, "StubNode");
  (void)py::class_<TensorNode, StubNode, std::shared_ptr<TensorNode>>(*m, "TensorNode")
    .def("get_value", &TensorNode::GetValue, "get output value of async stub.")
    .def("get_shape", &TensorNode::GetShape, "get output shape of async stub.")
    .def("get_dtype", &TensorNode::GetDtype, "get output dtype of async stub.");
  (void)py::class_<SequenceNode, StubNode, std::shared_ptr<SequenceNode>>(*m, "SequenceNode")
    .def("get_elements", &SequenceNode::GetElements, "get the elements of async stub_seq.");
  (void)py::class_<AnyTypeNode, StubNode, std::shared_ptr<AnyTypeNode>>(*m, "AnyTypeNode")
    .def("get_real_node", &AnyTypeNode::GetRealNode, "get the real StubNode");
  (void)py::class_<NoneTypeNode, StubNode, std::shared_ptr<NoneTypeNode>>(*m, "NoneTypeNode")
    .def("get_real_value", &NoneTypeNode::GetRealValue, "get the real value");
}
}  // namespace stub
}  // namespace mindspore
