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
#include "pipeline/pynative/op_function/converter.h"
#include <unordered_map>
#include "include/common/utils/convert_utils_py.h"
#include "pipeline/jit/ps/parse/data_converter.h"
#include "pipeline/pynative/pynative_utils.h"

namespace mindspore {
namespace pynative {

namespace {
static constexpr size_t N = 10;
using OP_DTYPE = mindspore::ops::OP_DTYPE;
template <typename T, typename U>
std::shared_ptr<U> PyCast(const py::object &obj) {
  return std::make_shared<U>(py::cast<T>(obj));
}

BoolImmPtr ConvertBool(const py::object &obj) {
  if (!py::isinstance<py::bool_>(obj)) {
    return nullptr;
  }
  return PyCast<bool, BoolImm>(obj);
}

Int64ImmPtr ConvertInt(const py::object &obj) {
  if (py::isinstance<py::bool_>(obj) || !py::isinstance<py::int_>(obj)) {
    return nullptr;
  }
  return PyCast<int64_t, Int64Imm>(obj);
}

FP32ImmPtr ConvertFloat(const py::object &obj) {
  if (!py::isinstance<py::float_>(obj)) {
    return nullptr;
  }
  return PyCast<double, FP32Imm>(obj);
}

ScalarPtr ConvertNumber(const py::object &obj) {
  if (py::isinstance<py::int_>(obj)) {
    return std::make_shared<Int64Imm>(py::cast<int64_t>(obj));
  } else if (py::isinstance<py::float_>(obj)) {
    return std::make_shared<FP32Imm>(py::cast<double>(obj));
  } else if (py::isinstance<py::bool_>(obj)) {
    return std::make_shared<BoolImm>(py::cast<bool>(obj));
  }
  return nullptr;
}

StringImmPtr ConvertStr(const py::object &obj) {
  if (!py::isinstance<py::str>(obj)) {
    return nullptr;
  }
  return PyCast<string, StringImm>(obj);
}

template <typename T, typename U, typename N>
ValueTuplePtr ConvertList(const py::object &obj) {
  if (!py::isinstance<T>(obj)) {
    return nullptr;
  }
  auto seq = py::cast<T>(obj);
  size_t size = seq.size();
  std::vector<ValuePtr> convert(size);
  for (size_t i = 0; i < size; ++i) {
    if (!py::isinstance<U>(seq[i])) {
      return {};
    }
    auto out = PyCast<U, N>(seq[i]);
    if (out == nullptr) {
      return nullptr;
    }
    convert[i] = out;
  }
  return std::make_shared<ValueTuple>(std::move(convert));
}
}  // namespace

Converter::Converter(ops::OpDef *op_def) : op_def_(op_def), source_type_(std::vector<ops::OP_DTYPE>(N)) {}

void Converter::Parse(const py::list &python_args) {
  if (op_def_->args_.size() != python_args.size()) {
    MS_LOG(EXCEPTION) << "For operator " << op_def_->name_ << ", it requires " << op_def_->args_.size()
                      << "parameters, bug got " << python_args.size() << "parameters!";
  }
}

ValuePtr Converter::ToTensor(const py::list &python_args, size_t i) {
  const auto &op_arg = op_def_->args_[i];
  const py::object &obj = (python_args)[i];
  source_type_[i] = OP_DTYPE::DT_BEGIN;
  auto tensor = parse::ConvertTensor(obj);
  if (tensor != nullptr) {
    if (tensor->isa<tensor::Tensor>()) {
      tensor->cast<tensor::TensorPtr>()->set_need_pipeline_sync(true);
    }
    return tensor;
  }
  if (!op_arg.cast_dtype_.empty()) {
    auto convert = ConvertByCastDtype(obj, op_arg, i);
    if (convert != nullptr && convert->isa<tensor::Tensor>()) {
      return convert->cast<tensor::TensorPtr>();
    }
  }

  PyNativeAlgo::PyParser::PrintTypeCastError(op_def_, python_args, i);
  return nullptr;
}

std::optional<ValuePtr> Converter::ToTensorOptional(const py::list &python_args, size_t i) {
  const py::object &obj = (python_args)[i];
  if (py::isinstance<py::none>(obj)) {
    return std::nullopt;
  }
  return std::make_optional(ToTensor(python_args, i));
}

template <typename T>
ValueTuplePtr Converter::ToTensorList(const py::list &python_args, size_t i) {
  const auto &op_arg = op_def_->args_[i];
  const py::object &obj = python_args[i];
  source_type_[i] = OP_DTYPE::DT_BEGIN;
  auto val_seq = parse::ConvertSequence<py::tuple, ValueTuple, parse::ConvertTensor>(obj);
  if (val_seq != nullptr && val_seq->isa<ValueTuple>()) {
    return val_seq->cast<ValueTuplePtr>();
  }
  return ConvertValueTupleByCastDtype(python_args, op_arg, i);
}

Int64ImmPtr Converter::ToInt(const py::list &python_args, size_t i) {
  const auto &op_arg = op_def_->args_[i];
  const py::object &obj = python_args[i];
  source_type_[i] = OP_DTYPE::DT_BEGIN;
  auto convert = ConvertInt(obj);
  if (convert != nullptr) {
    return convert;
  }
  if (!op_arg.cast_dtype_.empty()) {
    auto convert_value = ConvertByCastDtype(obj, op_arg, i);
    if (convert_value != nullptr && convert_value->isa<Int64Imm>()) {
      return convert_value->cast<Int64ImmPtr>();
    }
  }
  PyNativeAlgo::PyParser::PrintTypeCastError(op_def_, python_args, i);
  return nullptr;
}

std::optional<Int64ImmPtr> Converter::ToIntOptional(const py::list &python_args, size_t i) {
  const py::object &obj = python_args[i];
  if (py::isinstance<py::none>(obj)) {
    return std::nullopt;
  }
  return std::make_optional(ToInt(python_args, i));
}

template <typename T>
ValueTuplePtr Converter::ToIntList(const py::list &python_args, size_t i) {
  const auto &op_arg = op_def_->args_[i];
  const py::object &obj = python_args[i];
  ValueTuplePtr convert = ConvertList<T, py::int_, Int64Imm>(obj);
  if (convert != nullptr) {
    return convert;
  }
  return ConvertValueTupleByCastDtype(python_args, op_arg, i);
}

template <typename T>
std::optional<ValueTuplePtr> Converter::ToIntListOptional(const py::list &python_args, size_t i) {
  const py::object &obj = python_args[i];
  if (py::isinstance<py::none>(obj)) {
    return std::nullopt;
  }
  return std::make_optional(ToIntList<T>(python_args, i));
}

BoolImmPtr Converter::ToBool(const py::list &python_args, size_t i) {
  const auto &op_arg = op_def_->args_[i];
  const py::object &obj = python_args[i];
  source_type_[i] = OP_DTYPE::DT_BEGIN;
  auto convert = ConvertBool(obj);
  if (convert != nullptr) {
    return convert;
  }
  if (!op_arg.cast_dtype_.empty()) {
    auto convert_value = ConvertByCastDtype(obj, op_arg, i);
    if (convert_value != nullptr && convert_value->isa<BoolImm>()) {
      return convert_value->cast<BoolImmPtr>();
    }
  }
  PyNativeAlgo::PyParser::PrintTypeCastError(op_def_, python_args, i);
  return nullptr;
}

std::optional<BoolImmPtr> Converter::ToBoolOptional(const py::list &python_args, size_t i) {
  const py::object &obj = python_args[i];
  if (py::isinstance<py::none>(obj)) {
    return std::nullopt;
  }
  return std::make_optional(ToBool(python_args, i));
}

template <typename T>
ValueTuplePtr Converter::ToBoolList(const py::list &python_args, size_t i) {
  const auto &op_arg = op_def_->args_[i];
  const py::object &obj = python_args[i];
  source_type_[i] = OP_DTYPE::DT_BEGIN;
  ValueTuplePtr convert = ConvertList<T, py::bool_, BoolImm>(obj);
  if (convert != nullptr) {
    return convert;
  }
  return ConvertValueTupleByCastDtype(python_args, op_arg, i);
}

template <typename T>
std::optional<ValueTuplePtr> Converter::ToBoolListOptional(const py::list &python_args, size_t i) {
  const py::object &obj = python_args[i];
  source_type_[i] = OP_DTYPE::DT_BEGIN;
  if (py::isinstance<py::none>(obj)) {
    return std::nullopt;
  }
  return std::make_optional(ToBoolList<T>(python_args, i));
}

FP32ImmPtr Converter::ToFloat(const py::list &python_args, size_t i) {
  const auto &op_arg = op_def_->args_[i];
  const py::object &obj = python_args[i];
  source_type_[i] = OP_DTYPE::DT_BEGIN;
  auto convert = ConvertFloat(obj);
  if (convert != nullptr) {
    return convert;
  }
  if (!op_arg.cast_dtype_.empty()) {
    auto convert_value = ConvertByCastDtype(obj, op_arg, i);
    if (convert_value != nullptr && convert_value->isa<FP32Imm>()) {
      return convert_value->cast<FP32ImmPtr>();
    }
  }
  PyNativeAlgo::PyParser::PrintTypeCastError(op_def_, python_args, i);
  return nullptr;
}

template <typename T>
ValueTuplePtr Converter::ToFloatList(const py::list &python_args, size_t i) {
  const auto &op_arg = op_def_->args_[i];
  const py::object &obj = python_args[i];
  source_type_[i] = OP_DTYPE::DT_BEGIN;
  ValueTuplePtr convert = ConvertList<T, py::float_, FP32Imm>(obj);
  if (convert != nullptr) {
    return convert;
  }
  return ConvertValueTupleByCastDtype(python_args, op_arg, i);
}

template <typename T>
std::optional<ValueTuplePtr> Converter::ToFloatListOptional(const py::list &python_args, size_t i) {
  const py::object &obj = python_args[i];
  if (py::isinstance<py::none>(obj)) {
    return std::nullopt;
  }
  return std::make_optional(ToFloatList<T>(python_args, i));
}

ScalarPtr Converter::ToScalar(const py::list &python_args, size_t i) {
  const auto &op_arg = op_def_->args_[i];
  const py::object &obj = python_args[i];
  source_type_[i] = OP_DTYPE::DT_BEGIN;
  auto convert = ConvertNumber(obj);
  if (convert != nullptr) {
    return convert;
  }
  if (!op_arg.cast_dtype_.empty()) {
    auto convert_value = ConvertByCastDtype(obj, op_arg, i)->cast<ScalarPtr>();
    if (convert_value != nullptr && convert_value->isa<Scalar>()) {
      return convert_value->cast<ScalarPtr>();
    }
  }
  PyNativeAlgo::PyParser::PrintTypeCastError(op_def_, python_args, i);
  return nullptr;
}

std::optional<ScalarPtr> Converter::ToScalarOptional(const py::list &python_args, size_t i) {
  const py::object &obj = python_args[i];
  if (py::isinstance<py::none>(obj)) {
    return std::nullopt;
  }
  return std::make_optional(ToScalar(python_args, i));
}

StringImmPtr Converter::ToString(const py::list &python_args, size_t i) {
  const auto &op_arg = op_def_->args_[i];
  const py::object &obj = python_args[i];
  source_type_[i] = OP_DTYPE::DT_BEGIN;
  auto convert = ConvertStr(obj);
  if (convert != nullptr) {
    return convert;
  }
  if (!op_arg.cast_dtype_.empty()) {
    auto convert_value = ConvertByCastDtype(obj, op_arg, i);
    if (convert_value != nullptr && convert_value->isa<StringImm>()) {
      return convert_value->cast<StringImmPtr>();
    }
  }
  PyNativeAlgo::PyParser::PrintTypeCastError(op_def_, python_args, i);
  return nullptr;
}

std::optional<StringImmPtr> Converter::ToStringOptional(const py::list &python_args, size_t i) {
  const py::object &obj = python_args[i];
  if (py::isinstance<py::none>(obj)) {
    return std::nullopt;
  }
  return std::make_optional(ToString(python_args, i));
}

Int64ImmPtr Converter::ToDtype(const py::list &python_args, size_t i) {
  const py::object &obj = python_args[i];
  source_type_[i] = OP_DTYPE::DT_BEGIN;
  auto convert = ConvertInt(obj);
  if (convert != nullptr) {
    return convert;
  }
  if (py::isinstance<mindspore::Type>(obj)) {
    TypePtr type = py::cast<mindspore::TypePtr>(obj);
    return std::make_shared<Int64Imm>(static_cast<int>(type->type_id()));
  }
  PyNativeAlgo::PyParser::PrintTypeCastError(op_def_, python_args, i);
  return nullptr;
}

std::optional<Int64ImmPtr> Converter::ToDtypeOptional(const py::list &python_args, size_t i) {
  const py::object &obj = python_args[i];
  if (py::isinstance<py::none>(obj)) {
    return std::nullopt;
  }
  return std::make_optional(ToDtype(python_args, i));
}

ValuePtr Converter::ConvertByCastDtype(const py::object &input, const ops::OpInputArg &op_arg, size_t index) {
  for (auto &cast_dtype : op_arg.cast_dtype_) {
    auto convert_func = parse::GetConverterByType(parse::CombineTypesForTypeCast(cast_dtype, op_arg.arg_dtype_));
    if (convert_func == nullptr) {
      MS_LOG(EXCEPTION) << "Can't find convert function for src_dtype[" << cast_dtype << "] and dst_type"
                        << op_arg.arg_dtype_ << "].";
    }
    auto value = convert_func(input);
    if (value != nullptr) {
      source_type_[index] = cast_dtype;
      return value;
    }
  }
  return nullptr;
}

ValueTuplePtr Converter::ConvertValueTupleByCastDtype(const py::list &python_args, const ops::OpInputArg &op_arg,
                                                      size_t index) {
  const auto &input = python_args[index];
  if (!op_arg.cast_dtype_.empty()) {
    auto convert_value = ConvertByCastDtype(input, op_arg, index);
    if (convert_value != nullptr && convert_value->isa<ValueTuple>()) {
      return convert_value->cast<ValueTuplePtr>();
    }
  }
  PyNativeAlgo::PyParser::PrintTypeCastError(op_def_, python_args, index);
  return nullptr;
}

// Declare template to compile corresponding method.
template ValueTuplePtr Converter::ToTensorList<py::tuple>(const py::list &python_args, size_t i);
template ValueTuplePtr Converter::ToTensorList<py::list>(const py::list &python_args, size_t i);
template std::optional<ValueTuplePtr> Converter::ToIntListOptional<py::tuple>(const py::list &python_args, size_t i);
template std::optional<ValueTuplePtr> Converter::ToIntListOptional<py::list>(const py::list &python_args, size_t i);
template std::optional<ValueTuplePtr> Converter::ToBoolListOptional<py::tuple>(const py::list &python_args, size_t i);
template std::optional<ValueTuplePtr> Converter::ToBoolListOptional<py::list>(const py::list &python_args, size_t i);
template std::optional<ValueTuplePtr> Converter::ToFloatListOptional<py::tuple>(const py::list &python_args, size_t i);
template std::optional<ValueTuplePtr> Converter::ToFloatListOptional<py::list>(const py::list &python_args, size_t i);

}  // namespace pynative
}  // namespace mindspore
