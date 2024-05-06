/**
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

#include "include/common/utils/convert_utils_py.h"

#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <list>
#include <utility>
#include <cfloat>

#include "mindspore/core/ops/framework_ops.h"
#include "Eigen/Core"
#include "abstract/abstract_value.h"
#include "abstract/utils.h"
#include "pipeline/jit/ps/parse/parse_base.h"
#include "pipeline/jit/ps/parse/resolve.h"
#include "ir/value.h"
#include "ir/anf.h"
#include "ir/tensor.h"
#include "ir/param_info.h"
#include "pybind_api/ir/base_ref_py.h"
#include "ir/dtype/tensor_type.h"
#include "utils/ms_context.h"
#include "include/common/fallback.h"
#include "include/common/utils/stub_tensor.h"
#include "include/common/utils/convert_utils.h"

namespace mindspore {
py::object BuiltinsToPyData(const Any &value);
py::object BuiltinsToPyData(const BaseRef &value);
py::object VectorToPyData(const Any &value);
py::object VectorRefToPyData(const VectorRef &value_list, const AbstractBasePtr &abs = nullptr);
py::object MakeCSRTensor(const VectorRef &value_list);
py::object MakeCSRTensor(const ValuePtr &value);
py::object MakeCOOTensor(const VectorRef &value_list);
py::object MakeCOOTensor(const ValuePtr &value);
ShapeVector ConvertShapeTupleToShapeVector(const ValueTuplePtr &shape_tuple);
ShapeVector ConvertToShapeVector(const VectorRef &value_list, size_t shape_idx);

// For AbstractSequence and AbstractDictionary.
template <typename T>
T CheckAbstractElementsSize(const AbstractBasePtr &abs_value, size_t value_size) {
  if (abs_value == nullptr) {
    return nullptr;
  }
  auto abs = abs_value->cast<T>();
  if (abs != nullptr && value_size != abs->size()) {
    MS_LOG(EXCEPTION) << "The size of elements should be equal to " << value_size << ", but got " << abs->size();
  }
  return abs;
}

py::object SetAdaptedAttrToTensor(const py::object &tensor, const AbstractBasePtr &abs) {
  if (abs == nullptr || !abs->isa<abstract::AbstractTensor>()) {
    return tensor;
  }
  auto tensor_abs = abs->cast<abstract::AbstractTensorPtr>();
  if (tensor_abs->is_adapter()) {
    py::setattr(tensor, "adapter_flag", py::bool_(true));
  } else {
    py::setattr(tensor, "adapter_flag", py::bool_(false));
  }
  return tensor;
}

py::object CheckAndConvertToScalar(const tensor::TensorPtr &tensor, const AbstractBasePtr &abs) {
  if (abs == nullptr || !abs->isa<abstract::AbstractScalar>()) {
    return py::none();
  }
  tensor->data_sync();
  auto *data = tensor->data_c();
  auto type = abs->BuildType()->type_id();
  switch (type) {
    case kNumberTypeBool:
      return py::bool_(*reinterpret_cast<const bool *>(data));
    case kNumberTypeInt16:
      return py::int_(*reinterpret_cast<const int16_t *>(data));
    case kNumberTypeUInt16:
      return py::int_(*reinterpret_cast<const uint16_t *>(data));
    case kNumberTypeInt8:
      return py::int_(*reinterpret_cast<const int8_t *>(data));
    case kNumberTypeUInt8:
      return py::int_(*reinterpret_cast<const uint8_t *>(data));
    case kNumberTypeInt32:
      return py::int_(*reinterpret_cast<const int32_t *>(data));
    case kNumberTypeUInt32:
      return py::int_(*reinterpret_cast<const uint32_t *>(data));
    case kNumberTypeInt64:
      return py::int_(*reinterpret_cast<const int64_t *>(data));
    case kNumberTypeUInt64:
      return py::int_(*reinterpret_cast<const uint64_t *>(data));
    case kNumberTypeFloat16: {
      const Eigen::half_impl::__half_raw data_half(*reinterpret_cast<const uint16_t *>(data));
      return py::float_(Eigen::half_impl::half_to_float(data_half));
    }
    case kNumberTypeFloat32:
      return py::float_(*reinterpret_cast<const float *>(data));
    case kNumberTypeFloat64:
      return py::float_(*reinterpret_cast<const double *>(data));
    case kNumberTypeBFloat16: {
      const Eigen::half_impl::__half_raw data_half(*reinterpret_cast<const uint16_t *>(data));
      return py::float_(Eigen::half_impl::half_to_float(data_half));
    }
    default:
      return py::none();
  }
}

py::object CSRTensorToPyData(const tensor::CSRTensorPtr &csr_tensor) {
  auto ref = py::tuple(1);
  ref[0] = csr_tensor;
  return ref[0];
}

py::object TensorToPyData(const tensor::TensorPtr &tensor, const AbstractBasePtr &abs) {
  MS_EXCEPTION_IF_NULL(tensor);
  if (tensor->NeedWait()) {
    py::gil_scoped_release release;
    tensor->Wait();
  }
  auto scalar_obj = CheckAndConvertToScalar(tensor, abs);
  if (!py::isinstance<py::none>(scalar_obj)) {
    return scalar_obj;
  }

  py::tuple v(1);
  v[0] = tensor;
  v[0] = SetAdaptedAttrToTensor(v[0], abs);
  return v[0];
}

py::object ScalarPtrToPyData(const ScalarPtr &value) {
  constexpr double eps = 1e-6;
  py::int_ int_v;
  py::float_ float_v;
  py::bool_ bool_v;
  TypeId scalar_type = value->type()->type_id();
  float float_value;
  double doubel_value;
  switch (scalar_type) {
    case kNumberTypeUInt8:
      MS_LOG(DEBUG) << "uint8";
      int_v = value->cast<UInt8ImmPtr>()->value();
      return int_v;
    case kNumberTypeUInt16:
      MS_LOG(DEBUG) << "uint16";
      int_v = value->cast<UInt16ImmPtr>()->value();
      return int_v;
    case kNumberTypeUInt32:
      MS_LOG(DEBUG) << "uint32";
      int_v = value->cast<UInt32ImmPtr>()->value();
      return int_v;
    case kNumberTypeUInt64:
      MS_LOG(DEBUG) << "uint64";
      int_v = value->cast<UInt64ImmPtr>()->value();
      return int_v;
    case kNumberTypeInt8:
      MS_LOG(DEBUG) << "int8";
      int_v = value->cast<Int8ImmPtr>()->value();
      return int_v;
    case kNumberTypeInt16:
      MS_LOG(DEBUG) << "int16";
      int_v = value->cast<Int16ImmPtr>()->value();
      return int_v;
    case kNumberTypeInt32:
      MS_LOG(DEBUG) << "int32";
      int_v = value->cast<Int32ImmPtr>()->value();
      return int_v;
    case kNumberTypeInt64:
      MS_LOG(DEBUG) << "int64";
      int_v = value->cast<Int64ImmPtr>()->value();
      return int_v;
    case kNumberTypeFloat32:
      MS_LOG(DEBUG) << "float";
      float_value = value->cast<FP32ImmPtr>()->value();
      doubel_value = value->cast<FP32ImmPtr>()->prim_value();
      // If double value is default value 0, don't use double value.
      if (std::abs(doubel_value) > std::numeric_limits<double>::epsilon() &&
          std::abs(float_value - doubel_value) < eps) {
        float_v = doubel_value;
      } else {
        float_v = float_value;
      }
      return float_v;
    case kNumberTypeFloat64:
      MS_LOG(DEBUG) << "double";
      float_v = value->cast<FP64ImmPtr>()->value();
      return float_v;
    case kNumberTypeBool:
      MS_LOG(DEBUG) << "bool";
      bool_v = value->cast<BoolImmPtr>()->value();
      return bool_v;
    default:
      MS_EXCEPTION(TypeError) << "Unsupported scalar converted to py data: " << value->ToString();
  }
}

py::object ValueSequenceToPyData(const ValueSequencePtr &value, const AbstractBasePtr &abs) {
  auto value_sequeue = value->value();
  auto value_size = value_sequeue.size();
  if (value_size == 0) {
    // If output size of value sequence is 0, return an empty sequence.
    py::tuple res_sequeue(value_size);
    if (value->isa<ValueTuple>()) {
      return res_sequeue;
    }
    return res_sequeue.cast<py::list>();
  }
  // Convert ValueNamedTuple whose object's type is tuple and size is not 0.
  if (value->isa<ValueNamedTuple>()) {
    auto value_named_tuple = value->cast<ValueNamedTuplePtr>();
    MS_LOG(DEBUG) << "Convert ValueNamedTuple: " << value_named_tuple->ToString();
    auto keys = value_named_tuple->key();
    auto keys_size = keys.size();
    py::tuple key_sequeue(keys_size);
    py::tuple ele_sequeue(keys_size);
    for (size_t i = 0; i < keys_size; i++) {
      key_sequeue[i] = ValueToPyData(keys[i]);
      ele_sequeue[i] = ValueToPyData(value_sequeue[i]);
    }
    py::module mod = python_adapter::GetPyModule(parse::PYTHON_MOD_PARSE_MODULE);
    py::str sub_class_name = py::str(value_named_tuple->sub_class_name());
    py::object named_tuple = python_adapter::CallPyModFn(mod, parse::PYTHON_MOD_CONVERT_TO_NAMEDTUPLE, sub_class_name,
                                                         key_sequeue, ele_sequeue);
    return named_tuple;
  }
  py::tuple res_sequeue(value_size);
  if (abs != nullptr && abs->isa<abstract::AbstractSequence>() &&
      abs->cast<abstract::AbstractSequencePtr>()->dynamic_len()) {
    // Dynamic length sequence directly use value to create python object.
    for (size_t i = 0; i < value_size; i++) {
      res_sequeue[i] = ValueToPyData(value_sequeue[i]);
    }
  } else {
    auto abs_sequeue = CheckAbstractElementsSize<abstract::AbstractSequencePtr>(abs, value_size);
    if (abs_sequeue == nullptr) {
      for (size_t i = 0; i < value_size; i++) {
        res_sequeue[i] = ValueToPyData(value_sequeue[i]);
      }
    } else {
      for (size_t i = 0; i < value_size; i++) {
        res_sequeue[i] = ValueToPyData(value_sequeue[i], abs_sequeue->elements()[i]);
      }
    }
  }
  if (value->isa<ValueTuple>()) {
    return res_sequeue;
  }
  return res_sequeue.cast<py::list>();
}

py::object ValueDictionaryToPyData(const ValueDictionaryPtr &value, const AbstractBasePtr &abs) {
  auto value_dict = value->value();
  auto value_size = value_dict.size();
  py::dict res_dict;
  auto abs_dict = CheckAbstractElementsSize<abstract::AbstractDictionaryPtr>(abs, value_size);
  if (abs_dict == nullptr) {
    for (const auto &v : value_dict) {
      res_dict[ValueToPyData(v.first)] = ValueToPyData(v.second);
    }
  } else {
    for (size_t i = 0; i < value_size; i++) {
      auto v = value_dict[i];
      auto abs_elem = abs_dict->elements()[i];
      res_dict[ValueToPyData(v.first, abs_elem.first)] = ValueToPyData(v.second, abs_elem.second);
    }
  }
  return res_dict;
}

using ConverterFunction = std::function<py::object(const ValuePtr &value, const AbstractBasePtr &abs)>;
using ValueNameToConverterVector = std::vector<std::pair<uint32_t, ConverterFunction>>;

// (Value Type Name) -> (Converter Function)
// The converter function is used to convert Value object to Python data object.
static ValueNameToConverterVector value_name_to_converter = {
  // Scalar
  {Scalar::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     return ScalarPtrToPyData(value->cast<ScalarPtr>());
   }},
  // Tensor
  {tensor::Tensor::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &abs) -> py::object {
     auto tensor_ptr = value->cast<tensor::TensorPtr>();
     return TensorToPyData(tensor_ptr, abs);
   }},
  // MetaTenser
  {tensor::MetaTensor::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     py::tuple tuple_container(1);
     tuple_container[0] = value->cast<tensor::MetaTensorPtr>();
     return tuple_container[0];
   }},
  // CSRTensor
  {tensor::CSRTensor::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     auto csr_tensor_ptr = value->cast<tensor::CSRTensorPtr>();
     return CSRTensorToPyData(csr_tensor_ptr);
   }},
  // RefKey
  {RefKey::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     py::tuple tuple_container(1);
     tuple_container[0] = value->cast<RefKeyPtr>();
     return tuple_container[0];
   }},
  // Type
  {Type::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     py::tuple tuple_container(1);
     tuple_container[0] = value->cast<TypePtr>();
     return tuple_container[0];
   }},
  // StringImm
  {StringImm::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     py::str res = value->cast<StringImmPtr>()->value();
     return res;
   }},
  // ValueSequence
  {ValueSequence::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &abs) -> py::object {
     auto value_sequeue = value->cast<ValueSequencePtr>();
     return ValueSequenceToPyData(value_sequeue, abs);
   }},
  // ValueDictionary
  {ValueDictionary::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &abs) -> py::object {
     auto value_dict = value->cast<ValueDictionaryPtr>();
     return ValueDictionaryToPyData(value_dict, abs);
   }},
  // ValueSlice
  {ValueSlice::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     auto slice = value->cast<ValueSlicePtr>();
     auto start = ValueToPyData(slice->start());
     auto end = ValueToPyData(slice->stop());
     auto step = ValueToPyData(slice->step());
     return python_adapter::CallPyFn(parse::PYTHON_MOD_PARSE_MODULE, parse::PYTHON_PARSE_CLASS_SLICE, start, end, step);
   }},
  // KeywordArg
  {KeywordArg::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     auto abs_keyword_arg = value->ToAbstract()->cast<abstract::AbstractKeywordArgPtr>();
     auto key = abs_keyword_arg->get_key();
     auto val = abs_keyword_arg->get_arg()->BuildValue();
     auto py_value = ValueToPyData(val);
     auto kwargs = py::kwargs();
     kwargs[key.c_str()] = py_value;
     return kwargs;
   }},
  // parse::NameSpace
  {parse::NameSpace::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     auto ns = value->cast<parse::NameSpacePtr>();
     return ns->module_obj();
   }},
  // parse::ClassType
  {parse::ClassType::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     auto class_type = value->cast<parse::ClassTypePtr>();
     return class_type->obj();
   }},
  // parse::MsClassObject
  {parse::MsClassObject::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     auto ms_class_object = value->cast<parse::MsClassObjectPtr>();
     return ms_class_object->obj();
   }},
  // parse::InterpretedObject
  {parse::InterpretedObject::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     auto interpreted_object = value->cast<parse::InterpretedObjectPtr>();
     return interpreted_object->obj();
   }},
  // parse::PyObjectWrapper
  {parse::PyObjectWrapper::kTypeId,
   [](const ValuePtr &value, const AbstractBasePtr &) -> py::object {
     auto py_object = value->cast<parse::PyObjectWrapperPtr>();
     return py_object->obj();
   }},
  // None
  {None::kTypeId, [](const ValuePtr &, const AbstractBasePtr &) -> py::object { return py::none(); }},
  // ValueAny
  {ValueAny::kTypeId, [](const ValuePtr &, const AbstractBasePtr &) -> py::object { return py::none(); }},
  // ValueProblem
  {ValueProblem::kTypeId, [](const ValuePtr &, const AbstractBasePtr &) -> py::object { return py::none(); }},
  // FuncGraph
  {FuncGraph::kTypeId, [](const ValuePtr &, const AbstractBasePtr &) -> py::object { return py::none(); }},
  // Primitive
  {Primitive::kTypeId, [](const ValuePtr &, const AbstractBasePtr &) -> py::object { return py::none(); }},
  // Monad
  {Monad::kTypeId, [](const ValuePtr &, const AbstractBasePtr &) -> py::object { return py::none(); }},
  // Ellipsis
  {Ellipsis::kTypeId, [](const ValuePtr &, const AbstractBasePtr &) -> py::object { return py::ellipsis(); }}};

// When converting data to tensor, ValueToPyData will only return _c_expression Tensor,
// but not python tensor. If python tensor is needed, call _convert_python_data to the output.
py::object ValueToPyData(const ValuePtr &value, const AbstractBasePtr &abs) {
  if (value == nullptr) {
    MS_LOG(EXCEPTION) << "The `value` should not be null";
  }
  py::gil_scoped_acquire gil;
  for (auto &iter : value_name_to_converter) {
    if (value->IsFromTypeId(iter.first)) {
      return iter.second(value, abs);
    }
  }
  MS_LOG(EXCEPTION) << "Unsupported to convert " << value->ToString() << "[" << value->type_name() << "] to a PyData";
}

py::object AnyToPyData(const Any &value) {
  py::object ret;
  MS_LOG(DEBUG) << "AnyToPyData " << value.GetString();
  if (value.is<int>() || value.is<float>() || value.is<double>() || value.is<bool>()) {
    ret = BuiltinsToPyData(value);
  } else if (value.is<ValuePtr>()) {
    MS_LOG(DEBUG) << "ValuePtr";
    ValuePtr v = value.cast<ValuePtr>();
    ret = ValueToPyData(v);
  } else if (value.is<py::object>()) {
    MS_LOG(DEBUG) << "py obj";
    ret = value.cast<py::object>();
  } else if (value.is<std::vector<tensor::TensorPtr>>() || value.is<std::vector<Any>>()) {
    ret = VectorToPyData(value);
  } else if (value.is<std::list<Any>>()) {
    MS_LOG(DEBUG) << "list_any";
    auto value_list = value.cast<std::list<Any>>();
    py::list rets = py::list();
    for (auto &v : value_list) {
      rets.append(AnyToPyData(v));
    }
    ret = rets;
  } else if (value.is<std::vector<Any>>()) {
    auto value_list = value.cast<std::vector<Any>>();
    py::tuple rets(value_list.size());
    for (size_t i = 0; i < value_list.size(); i++) {
      rets[i] = AnyToPyData(value_list[i]);
    }
    ret = rets;
  } else if (value.is<TypePtr>()) {
    py::tuple v(1);
    v[0] = value.cast<TypePtr>();
    ret = v[0];
  } else {
    MS_LOG(EXCEPTION) << "value is not support type";
  }
  return ret;
}

py::object BaseRefToPyData(const BaseRef &value, const AbstractBasePtr &abs) {
  py::object ret;
  MS_LOG(DEBUG) << "BaseRefToPyData " << value.ToString();
  if (utils::isa<int>(value) || utils::isa<float>(value) || utils::isa<double>(value) || utils::isa<bool>(value)) {
    ret = BuiltinsToPyData(value);
  } else if (utils::isa<ValuePtr>(value)) {
    MS_LOG(DEBUG) << "ValuePtr";
    ValuePtr v = utils::cast<ValuePtr>(value);
    ret = ValueToPyData(v, abs);
  } else if (utils::isa<PyObjectRef>(value)) {
    MS_LOG(DEBUG) << "py obj";
    PyObjectRef py_ref = utils::cast<PyObjectRef>(value);
    ret = py_ref.object_;
  } else if (utils::isa<VectorRef>(value)) {
    auto vec_ref = utils::cast<VectorRef>(value);
    ret = VectorRefToPyData(vec_ref, abs);
  } else if (utils::isa<TypePtr>(value)) {
    py::tuple v(1);
    v[0] = utils::cast<TypePtr>(value);
    ret = v[0];
  } else {
    MS_LOG(EXCEPTION) << "value is not support, value:" << value.ToString();
  }
  return ret;
}

py::object BuiltinsToPyData(const Any &value) {
  if (value.is<int>()) {
    MS_LOG(DEBUG) << "int";
    py::int_ ret = value.cast<int>();
    return ret;
  } else if (value.is<float>()) {
    MS_LOG(DEBUG) << "float";
    py::float_ ret = value.cast<float>();
    return ret;
  } else if (value.is<double>()) {
    MS_LOG(DEBUG) << "double";
    py::float_ ret = value.cast<double>();
    return ret;
  } else {
    MS_LOG(DEBUG) << "bool";
    py::bool_ ret = value.cast<bool>();
    return ret;
  }
}

py::object BuiltinsToPyData(const BaseRef &value) {
  if (utils::isa<int>(value)) {
    MS_LOG(DEBUG) << "int";
    py::int_ ret = utils::cast<int>(value);
    return ret;
  } else if (utils::isa<float>(value)) {
    MS_LOG(DEBUG) << "float";
    py::float_ ret = utils::cast<float>(value);
    return ret;
  } else if (utils::isa<double>(value)) {
    MS_LOG(DEBUG) << "double";
    py::float_ ret = utils::cast<double>(value);
    return ret;
  } else {
    MS_LOG(DEBUG) << "bool";
    py::bool_ ret = utils::cast<bool>(value);
    return ret;
  }
}

py::object VectorToPyData(const Any &value) {
  py::object ret;
  if (value.is<std::vector<tensor::TensorPtr>>()) {
    MS_LOG(DEBUG) << "vector_tensor";
    std::vector<tensor::TensorPtr> outputs;
    outputs = value.cast<std::vector<tensor::TensorPtr>>();
    py::tuple tensor_tuple(outputs.size());
    for (std::size_t i = 0; i < outputs.size(); ++i) {
      tensor_tuple[i] = *outputs[i];
    }
    ret = tensor_tuple;
  } else {
    MS_LOG(DEBUG) << "vector_any";
    auto value_list = value.cast<std::vector<Any>>();
    py::tuple any_tuple = py::tuple(value_list.size());
    size_t i = 0;
    for (auto &v : value_list) {
      any_tuple[i] = AnyToPyData(v);
      i++;
    }
    ret = any_tuple;
  }
  return ret;
}
template <typename T>
py::object AbstractSequenceToPyData(const VectorRef &value_list, const AbstractBasePtr &abs) {
  auto value_size = value_list.size();
  auto ret = T(value_size);
  auto seq_abs = abs->cast<abstract::AbstractSequencePtr>();
  MS_EXCEPTION_IF_NULL(seq_abs);
  bool dynamic_len = seq_abs->dynamic_len();
  auto dynamic_len_element_abs = seq_abs->dynamic_len_element_abs();
  if (dynamic_len || dynamic_len_element_abs != nullptr) {
    if (dynamic_len_element_abs == nullptr) {
      MS_LOG(INFO) << "Dynamic length sequence with no specified element abstract convert to empty tuple.";
      for (size_t i = 0; i < value_size; i++) {
        ret[i] = BaseRefToPyData(value_list[i]);
      }
      return ret;
    }
    if (dynamic_len_element_abs->isa<abstract::AbstractNone>()) {
      MS_LOG(INFO) << "Dynamic length sequence with element None convert to empty sequence.";
      return ret;
    }
    for (size_t i = 0; i < value_size; ++i) {
      ret[i] = BaseRefToPyData(value_list[i], dynamic_len_element_abs);
    }
    return ret;
  }
  const auto allow_fallback_runtime = (fallback::GetJitSyntaxLevel() >= kCompatible);
  // If FALLBACK_RUNTIME is not enable
  // The size of seq_abs may be larger than the size of value_list, because the backend will eliminate None.
  size_t ref_idx = 0;
  for (size_t i = 0; i < seq_abs->size(); i++) {
    auto elem_abs = seq_abs->elements()[i];
    if (elem_abs->isa<abstract::AbstractNone>() && !allow_fallback_runtime) {
      continue;
    }
    ret[ref_idx] = BaseRefToPyData(value_list[ref_idx], elem_abs);
    ref_idx++;
  }
  if (ref_idx != value_size) {
    MS_LOG(EXCEPTION) << "The size of elements (excluding None) should be equal to " << value_size << ", but got "
                      << ref_idx;
  }
  return ret;
}

py::object VectorRefToPyData(const VectorRef &value_list, const AbstractBasePtr &abs) {
  py::object ret;
  size_t value_size = value_list.size();
  auto ref_tuple = py::tuple(value_size);
  if (abs == nullptr) {
    for (size_t i = 0; i < value_size; i++) {
      ref_tuple[i] = BaseRefToPyData(value_list[i]);
    }
    ret = ref_tuple;
    return ret;
  }

  if (value_size == 0 && !abs->isa<abstract::AbstractList>()) {
    return ref_tuple;
  }

  // Current VectorRef reflects a COOTensor type
  if (abs->isa<abstract::AbstractCSRTensor>()) {
    return MakeCSRTensor(value_list);
  }
  if (abs->isa<abstract::AbstractCOOTensor>()) {
    return MakeCOOTensor(value_list);
  }
  if (abs->isa<abstract::AbstractList>()) {
    return AbstractSequenceToPyData<py::list>(value_list, abs);
  }
  return AbstractSequenceToPyData<py::tuple>(value_list, abs);
}

bool IsGraphOutputValueNodeOrParameter(const AnfNodePtr &output, const py::tuple &args,
                                       const std::shared_ptr<py::object> &ret_val) {
  if (output->isa<ValueNode>()) {
    MS_LOG(INFO) << "Graph's output is a constant. No need to execute.";
    ValuePtr value = GetValueNode(output);
    auto abs = output->abstract();
    MS_EXCEPTION_IF_NULL(abs);
    if (abs->isa<abstract::AbstractCSRTensor>()) {
      *ret_val = MakeCSRTensor(value);
    } else if (abs->isa<abstract::AbstractCOOTensor>()) {
      *ret_val = MakeCOOTensor(value);
    } else {
      *ret_val = ValueToPyData(value, abs);
    }
    return true;
  }

  // Adapter will transform values in __init__() and construct() to parameters, this could cause
  // inputs (a.k.a args in current function) size less than parameters'.
  if (output->isa<Parameter>()) {
    MS_LOG(INFO) << "Graph's output is a parameter. If all params are inputs, no need to execute.";
    // Find the right parameter as ret_val.
    auto func_graph = output->func_graph();
    MS_EXCEPTION_IF_NULL(func_graph);
    auto params = func_graph->parameters();
    if ((args.size() + func_graph->fv_param_count()) != params.size()) {
      MS_LOG(EXCEPTION) << "Input size " << args.size() << " add Parameter count " << func_graph->fv_param_count()
                        << " not equal to graph input size " << params.size() << ", let graph to be executed.";
    }

    auto it = std::find(params.begin(), params.end(), output);
    if (it == params.end()) {
      MS_EXCEPTION(UnknownError) << "When graph output is Parameter,  it should be found in graph parameters";
    }
    size_t index = it - params.cbegin();
    if (index >= args.size() + func_graph->fv_param_count()) {
      MS_EXCEPTION(UnknownError) << "Index " << index << " equal or larger than args size " << args.size()
                                 << " add Parameter count " << func_graph->fv_param_count() << ".";
    }
    if (index < args.size()) {
      *ret_val = args[index];
    } else {
      auto param = dyn_cast<Parameter>(params[index]);
      MS_EXCEPTION_IF_NULL(param);
      if (!param->has_default()) {
        MS_LOG(EXCEPTION) << "Can not determine value of Parameter " << index << " (" << param->name() << ")";
      }
      auto tensor = param->default_param();
      *ret_val = py::cast(tensor);
    }
    *ret_val = SetAdaptedAttrToTensor(*ret_val, output->abstract());
    auto abs = output->abstract();
    MS_EXCEPTION_IF_NULL(abs);
    if (abs->isa<abstract::AbstractTensor>()) {
      py::setattr(*ret_val, "__ms_parameter_output__", py::bool_(true));
    }
    return true;
  }
  return false;
}

// SparseTensor Converters
using TensorPtr = tensor::TensorPtr;
using CSRTensor = tensor::CSRTensor;
constexpr size_t kCSRTensorInputSize{4};
constexpr size_t kCOOTensorInputSize{3};

void CheckCSRValueNums(size_t size) {
  if (size < kCSRTensorInputSize) {
    MS_LOG(EXCEPTION) << "CSRTensor must have at least " << kCSRTensorInputSize << " inputs, but got " << size;
  }
}

py::object MakeCSRTensor(const ValuePtr &value) {
  py::object ret;
  if (value->isa<ValueSequence>()) {
    auto value_sequeue = value->cast<ValueSequencePtr>()->value();
    CheckCSRValueNums(value_sequeue.size());
    TensorPtr indptr = utils::cast<TensorPtr>(value_sequeue[tensor::CSRTensor::kIndptrIdx]);
    TensorPtr indices = utils::cast<TensorPtr>(value_sequeue[tensor::CSRTensor::kIndicesIdx]);
    TensorPtr values = utils::cast<TensorPtr>(value_sequeue[tensor::CSRTensor::kValuesIdx]);
    ValueTuplePtr shape_ptr = utils::cast<ValueTuplePtr>(value_sequeue[tensor::CSRTensor::kShapeIdx]);
    ShapeVector shape = ConvertShapeTupleToShapeVector(shape_ptr);
    auto csr_tensor_ptr = std::make_shared<CSRTensor>(indptr, indices, values, shape);
    return CSRTensorToPyData(csr_tensor_ptr);
  }
  MS_LOG_WARNING << "value is not ValueSequence, but got " << value->ToString();
  return ret;
}

py::object MakeCSRTensor(const VectorRef &value_list) {
  CheckCSRValueNums(value_list.size());
  TensorPtr indptr = utils::cast<TensorPtr>(value_list[tensor::CSRTensor::kIndptrIdx]);
  TensorPtr indices = utils::cast<TensorPtr>(value_list[tensor::CSRTensor::kIndicesIdx]);
  TensorPtr values = utils::cast<TensorPtr>(value_list[tensor::CSRTensor::kValuesIdx]);
  ShapeVector shape = ConvertToShapeVector(value_list, tensor::CSRTensor::kShapeIdx);
  auto csr_tensor_ptr = std::make_shared<CSRTensor>(indptr, indices, values, shape);
  return CSRTensorToPyData(csr_tensor_ptr);
}

ShapeVector ConvertShapeTupleToShapeVector(const ValueTuplePtr &shape_tuple) {
  ShapeVector shape;
  MS_EXCEPTION_IF_NULL(shape_tuple);
  for (const auto &v : shape_tuple->value()) {
    MS_EXCEPTION_IF_NULL(v);
    ScalarPtr scalar = v->cast<ScalarPtr>();
    MS_EXCEPTION_IF_NULL(scalar);
    shape.push_back(GetValue<int64_t>(scalar));
  }
  return shape;
}

ShapeVector ConvertToShapeVector(const VectorRef &value_list, size_t index) {
  ShapeVector shape;
  if (index >= value_list.size()) {
    MS_LOG(EXCEPTION) << "Index " << index << " is out of range of " << value_list.size();
  }
  BaseRef ref = value_list[index];
  MS_EXCEPTION_IF_NULL(ref);

  auto converter = [](const BaseRef &ref) {
    auto tensorptr = utils::cast<tensor::TensorPtr>(ref);
    MS_EXCEPTION_IF_NULL(tensorptr);
    if (tensorptr->DataDim() != 0) {
      MS_LOG(EXCEPTION) << "Element must be scalar!";
    }
    tensorptr->data_sync(false);
    return *(static_cast<int64_t *>(tensorptr->data_c()));
  };

  if (utils::isa<tensor::Tensor>(ref)) {
    (void)std::transform(value_list.begin() + SizeToLong(index), value_list.end(), std::back_inserter(shape),
                         converter);
  } else if (utils::isa<VectorRef>(ref)) {
    VectorRef shape_ref = utils::cast<VectorRef>(ref);
    (void)std::transform(shape_ref.begin(), shape_ref.end(), std::back_inserter(shape), converter);
  } else if (utils::isa<ValueTuple>(ref)) {
    ValueTuplePtr shape_tuple = utils::cast<ValueTuplePtr>(ref);
    shape = ConvertShapeTupleToShapeVector(shape_tuple);
  }
  if (shape.empty()) {
    MS_LOG(ERROR) << "ShapeVector is empty!";
  }
  return shape;
}

void CheckCOOValueNums(size_t size) {
  if (size < kCOOTensorInputSize) {
    MS_LOG(EXCEPTION) << "COOTensor must have at least " << kCOOTensorInputSize << " inputs, but got " << size;
  }
}

py::object MakeCOOTensor(const ValuePtr &value) {
  auto ret = py::tuple(1);
  if (value->isa<ValueSequence>()) {
    auto value_sequeue = value->cast<ValueSequencePtr>()->value();
    CheckCOOValueNums(value_sequeue.size());
    TensorPtr indices = utils::cast<TensorPtr>(value_sequeue[tensor::COOTensor::kIndicesIdx]);
    TensorPtr values = utils::cast<TensorPtr>(value_sequeue[tensor::COOTensor::kValuesIdx]);
    ValueTuplePtr shape_ptr = utils::cast<ValueTuplePtr>(value_sequeue[tensor::COOTensor::kShapeIdx]);
    ShapeVector shape = ConvertShapeTupleToShapeVector(shape_ptr);
    ret[0] = std::make_shared<tensor::COOTensor>(indices, values, shape);
  }
  MS_LOG_WARNING << "value is not ValueSequence, but got " << value->ToString();
  return ret[0];
}

py::object MakeCOOTensor(const VectorRef &value_list) {
  CheckCOOValueNums(value_list.size());
  tensor::TensorPtr indices = utils::cast<tensor::TensorPtr>(value_list[tensor::COOTensor::kIndicesIdx]);
  tensor::TensorPtr values = utils::cast<tensor::TensorPtr>(value_list[tensor::COOTensor::kValuesIdx]);
  ShapeVector shape = ConvertToShapeVector(value_list, tensor::COOTensor::kShapeIdx);
  auto ret = py::tuple(1);
  ret[0] = std::make_shared<tensor::COOTensor>(indices, values, shape);
  return ret[0];
}

bool IsStubTensor(const py::handle &obj) { return py::hasattr(obj, stub::PY_ATTR_STUB); }

tensor::TensorPtr ConvertStubTensor(const py::handle &obj) {
  auto py_stub = py::getattr(obj, stub::PY_ATTR_STUB);
  auto stub = py_stub.cast<stub::StubNodePtr>();
  if (stub == nullptr) {
    return py::getattr(obj, stub::PY_ATTR_TENSOR).cast<tensor::TensorPtr>();
  }
  auto func_sync = obj.attr(stub::PY_ATTR_SYNC);
  auto res = func_sync();
  return res.cast<tensor::TensorPtr>();
}

ValuePtr PyStubNodeCast(const py::handle &obj) {
  auto py_stub = py::getattr(obj, stub::PY_ATTR_STUB);
  auto stub = py_stub.cast<stub::StubNodePtr>();
  if (stub == nullptr) {
    return py::getattr(obj, stub::PY_ATTR_TENSOR).cast<tensor::TensorPtr>();
  }
  return stub;
}

std::pair<ShapeVector, TypePtr> GetStubTensorInfo(const py::handle &obj) {
  auto py_stub = py::getattr(obj, stub::PY_ATTR_STUB);
  ValuePtr stub = py_stub.cast<stub::StubNodePtr>();
  AbstractBasePtr stub_abs;
  if (stub == nullptr) {
    auto tensor_ptr = py::getattr(obj, stub::PY_ATTR_TENSOR).cast<tensor::TensorPtr>();
    MS_EXCEPTION_IF_NULL(tensor_ptr);
    stub_abs = tensor_ptr->ToAbstract();
  } else {
    stub_abs = stub->ToAbstract();
  }
  MS_EXCEPTION_IF_NULL(stub_abs);
  return {dyn_cast<abstract::Shape>(stub_abs->BuildShape())->shape(), stub_abs->BuildType()};
}

ValuePtr ShallowCopyTensorValue(const ValuePtr &value) {
  MS_EXCEPTION_IF_NULL(value);
  if (value->isa<tensor::Tensor>()) {
    auto tensor_value = value->cast<tensor::TensorPtr>();
    MS_EXCEPTION_IF_NULL(tensor_value);
    auto shallow_tensor = std::make_shared<tensor::Tensor>(*tensor_value);
    shallow_tensor->set_base_shape(tensor_value->base_shape_ptr());
    return shallow_tensor;
  } else if (value->isa<ValueSequence>()) {
    std::vector<ValuePtr> values;
    const auto &value_seq = value->cast<ValueSequencePtr>();
    MS_EXCEPTION_IF_NULL(value_seq);
    (void)std::transform(value_seq->value().begin(), value_seq->value().end(), std::back_inserter(values),
                         [](const ValuePtr &elem) { return ShallowCopyTensorValue(elem); });
    return std::make_shared<ValueTuple>(values);
  } else if (value->isa<stub::StubNode>()) {
    auto stub_node = value->cast<stub::StubNodePtr>();
    MS_EXCEPTION_IF_NULL(stub_node);
    return ShallowCopyTensorValue(stub_node->WaitValue());
  } else {
    return value;
  }
}

void ConvertPyObjectToTensor(const py::object &input_object, std::vector<ValuePtr> *tensors) {
  MS_EXCEPTION_IF_NULL(tensors);
  ValuePtr tensor_ptr = nullptr;
  if (py::isinstance<tensor::Tensor>(input_object)) {
    tensor_ptr = py::cast<tensor::TensorPtr>(input_object);
  } else if (IsStubTensor(input_object)) {
    tensor_ptr = ConvertStubTensor(input_object);
  } else if (py::isinstance<py::float_>(input_object)) {
    double input_value = py::cast<py::float_>(input_object);
    tensor_ptr = std::make_shared<tensor::Tensor>(input_value, kFloat32);
  } else if (py::isinstance<py::int_>(input_object)) {
    tensor_ptr = std::make_shared<tensor::Tensor>(py::cast<int64_t>(input_object), kInt64);
  } else if (py::isinstance<py::list>(input_object)) {
    auto list_inputs = py::cast<py::list>(input_object);
    for (size_t i = 0; i < list_inputs.size(); ++i) {
      ConvertPyObjectToTensor(list_inputs[i], tensors);
    }
    return;
  } else if (py::isinstance<py::tuple>(input_object)) {
    auto tuple_inputs = py::cast<py::tuple>(input_object);
    for (size_t i = 0; i < tuple_inputs.size(); ++i) {
      ConvertPyObjectToTensor(tuple_inputs[i], tensors);
    }
    return;
  } else if (py::isinstance<tensor::CSRTensor>(input_object)) {
    tensor_ptr = py::cast<tensor::CSRTensorPtr>(input_object);
  } else if (py::isinstance<tensor::COOTensor>(input_object)) {
    tensor_ptr = py::cast<tensor::COOTensorPtr>(input_object);
  } else {
    MS_EXCEPTION(TypeError) << "Unreasonable data type: " << input_object.get_type() << ".";
  }
  MS_EXCEPTION_IF_NULL(tensor_ptr);
  (void)tensors->emplace_back(tensor_ptr);
}

void ConvertCTensorToPyTensor(const py::tuple &input_args, py::tuple *convert_args) {
  MS_EXCEPTION_IF_NULL(convert_args);
  if (input_args.size() != (*convert_args).size()) {
    MS_LOG(EXCEPTION) << "The size of input_args: " << input_args.size()
                      << " should be equal to the size of convert_args: " << (*convert_args).size();
  }
  for (size_t i = 0; i < input_args.size(); ++i) {
    if (py::isinstance<tensor::Tensor>(input_args[i])) {
      (*convert_args)[i] =
        python_adapter::CallPyFn(parse::PYTHON_MOD_PARSE_MODULE, parse::PYTHON_MOD_CONVERT_TO_MS_TENSOR, input_args[i]);
    } else if (py::isinstance<tensor::CSRTensor>(input_args[i])) {
      (*convert_args)[i] = python_adapter::CallPyFn(parse::PYTHON_MOD_PARSE_MODULE,
                                                    parse::PYTHON_MOD_CONVERT_TO_MS_CSRTENSOR, input_args[i]);
    } else if (py::isinstance<tensor::COOTensor>(input_args[i])) {
      (*convert_args)[i] = python_adapter::CallPyFn(parse::PYTHON_MOD_PARSE_MODULE,
                                                    parse::PYTHON_MOD_CONVERT_TO_MS_COOTENSOR, input_args[i]);
    } else if (py::isinstance<py::tuple>(input_args[i])) {
      auto tuple_inp_arg = py::cast<py::tuple>(input_args[i]);
      py::tuple convert_tuple_arg(tuple_inp_arg.size());
      ConvertCTensorToPyTensor(tuple_inp_arg, &convert_tuple_arg);
      (*convert_args)[i] = convert_tuple_arg;
    } else {
      (*convert_args)[i] = input_args[i];
    }
  }
}
}  // namespace mindspore
