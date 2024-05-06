/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#include "abstract/param_validator.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <memory>
#include "abstract/dshape.h"
#include "ir/dtype.h"
#include "ir/dtype/tensor_type.h"
#include "ir/scalar.h"
namespace mindspore {
namespace abstract {
#define ABSTRACT_REPORT_NAME_DEC(abstract) constexpr char ReportNameTraits<Abstract##abstract>::name[];

ABSTRACT_REPORT_NAME_DEC(Tensor)
ABSTRACT_REPORT_NAME_DEC(Tuple)
ABSTRACT_REPORT_NAME_DEC(Scalar)
ABSTRACT_REPORT_NAME_DEC(List)
ABSTRACT_REPORT_NAME_DEC(Dictionary)
ABSTRACT_REPORT_NAME_DEC(Slice)
ABSTRACT_REPORT_NAME_DEC(Function)
ABSTRACT_REPORT_NAME_DEC(Type)
ABSTRACT_REPORT_NAME_DEC(KeywordArg)

TypePtr CheckType(TypePtr type, const TypePtrList &accepts, const std::string &error_message_prefix) {
  auto ori_type = type;
  if (type->isa<TensorType>()) {
    auto tensor = type->cast_ptr<TensorType>();
    type = tensor->element();
    MS_EXCEPTION_IF_NULL(type);
  }
  bool ok = std::any_of(accepts.begin(), accepts.end(),
                        [type](const TypePtr &accept) -> bool { return IsIdentidityOrSubclass(type, accept); });
  if (ok) {
    return type;
  } else {
    MS_EXCEPTION(TypeError) << error_message_prefix << " should be Tensor" << accepts << ",but got "
                            << ori_type->ToString();
  }
}

TypePtr CheckTensorDType(const AbstractBasePtr &tensor, const TypePtrList &accepts,
                         const std::string &error_message_prefix) {
  MS_EXCEPTION_IF_NULL(tensor);
  TypePtr type = tensor->GetType();
  MS_EXCEPTION_IF_NULL(type);
  if (!type->isa<TensorType>()) {
    MS_LOG(EXCEPTION) << error_message_prefix << "requires Tensor but got " << type->ToString();
  }
  return CheckType(type, accepts, error_message_prefix);
}

TypePtr CheckTensorsDTypeSame(const AbstractTensorPtrList &tensor_list, const TypePtrList &accepts,
                              const std::string &error_message_prefix) {
  if (tensor_list.empty()) {
    MS_LOG(EXCEPTION) << "Array list is empty";
  }

  auto sample_tensor = tensor_list[0];
  MS_EXCEPTION_IF_NULL(sample_tensor);
  auto sample_elem = sample_tensor->element();
  MS_EXCEPTION_IF_NULL(sample_elem);
  TypePtr sample_type = sample_elem->BuildType();
  MS_EXCEPTION_IF_NULL(sample_type);
  std::ostringstream loginfoBuffer;
  loginfoBuffer << "[" << sample_tensor->BuildType()->ToString();
  bool error_flag = false;
  // Check if other elements have the same type with the first element.
  for (size_t index = 1; index < tensor_list.size(); ++index) {
    MS_EXCEPTION_IF_NULL(tensor_list[index]);
    auto elem = tensor_list[index]->element();
    MS_EXCEPTION_IF_NULL(elem);
    auto a_type = elem->BuildType();
    MS_EXCEPTION_IF_NULL(a_type);
    loginfoBuffer << "," << tensor_list[index]->BuildType()->ToString();
    if (sample_type->type_id() != a_type->type_id()) {
      error_flag = true;
    }
  }
  if (error_flag) {
    MS_EXCEPTION(ValueError) << error_message_prefix << " must be same, but got " << loginfoBuffer.str() << "]";
  }
  MS_LOG(DEBUG) << error_message_prefix << loginfoBuffer.str();
  return CheckTensorDType(sample_tensor, accepts, error_message_prefix);
}

TypePtr CheckScalarType(const AbstractScalarPtr &scalar, const TypePtrList &accepts,
                        const std::string &error_message_prefix) {
  if (scalar == nullptr) {
    MS_LOG(INTERNAL_EXCEPTION) << "Scalar nullptr";
  }
  auto type = scalar->BuildType();
  if (type == nullptr) {
    MS_LOG(INTERNAL_EXCEPTION) << "Scalar value nullptr";
  }

  return CheckType(type, accepts, error_message_prefix);
}

// new function
void CheckShapeSame(const std::string &op, const AbstractBasePtr &tensor_base, const AbstractBasePtr &tensor) {
  MS_EXCEPTION_IF_NULL(tensor_base);
  if (tensor_base->GetType()->object_type() != kObjectTypeTensorType) {
    MS_EXCEPTION(TypeError) << "For primitive[" << op << "], the first input should be tensor type, but got "
                            << tensor_base->GetType()->ToString() << ".";
  }
  auto shape_base = tensor_base->GetShape();
  MS_EXCEPTION_IF_NULL(shape_base);
  MS_EXCEPTION_IF_NULL(tensor);
  if (tensor->GetType()->object_type() != kObjectTypeTensorType) {
    MS_EXCEPTION(TypeError) << "For primitive[" << op << "], the second input should be tensor type, but got "
                            << tensor->GetType()->ToString() << ".";
  }
  auto shape = tensor->GetShape();
  MS_EXCEPTION_IF_NULL(shape);
  if (shape_base->IsDimUnknown() || shape->IsDimUnknown()) {
    return;
  }

  const auto &shape_vector = shape->GetShapeVector();
  const auto &shape_base_vector = shape_base->GetShapeVector();
  if (shape_vector.size() != shape_base_vector.size()) {
    MS_EXCEPTION(ValueError) << "For '" << op << "', the shape of two args should be same, but the first arg shape "
                             << shape_base->ToString() << " are not consistent with second arg shape "
                             << shape->ToString();
  }

  for (size_t i = 0; i < shape_vector.size(); i++) {
    if (shape_vector[i] == Shape::kShapeDimAny || shape_base_vector[i] == Shape::kShapeDimAny) {
      continue;
    }
    if (shape_vector[i] != shape_base_vector[i]) {
      MS_EXCEPTION(ValueError) << "For '" << op << "',  the shape of two args should be same, but the first arg shape "
                               << shape_base->ToString() << " are not consistent with second arg shape "
                               << shape->ToString();
    }
  }
  return;
}

TypePtr CheckDtypeSame(const std::string &op, const AbstractBasePtr &tensor_base, const AbstractBasePtr &tensor) {
  MS_EXCEPTION_IF_NULL(tensor_base);
  TypePtr type_base = tensor_base->GetType();
  MS_EXCEPTION_IF_NULL(tensor);
  TypePtr type = tensor->GetType();
  MS_EXCEPTION_IF_NULL(type_base);
  MS_EXCEPTION_IF_NULL(type);
  CheckDtypeSame(op, type_base, type);
  return type_base;
}

// old function
void CheckShapeSame(const std::string &op, const AbstractTensorPtr &tensor_base, const AbstractTensorPtr &tensor) {
  MS_EXCEPTION_IF_NULL(tensor_base);
  ShapePtr shape_base = tensor_base->shape();
  MS_EXCEPTION_IF_NULL(shape_base);
  MS_EXCEPTION_IF_NULL(tensor);
  ShapePtr shape = tensor->shape();
  MS_EXCEPTION_IF_NULL(shape);
  if (shape_base->IsDimUnknown() || shape->IsDimUnknown()) {
    return;
  }

  auto shape_vector = shape->shape();
  auto shape_base_vector = shape_base->shape();
  if (shape_vector.size() != shape_base_vector.size()) {
    MS_EXCEPTION(ValueError) << "For '" << op << "', the shape of two args should be same, but the first arg shape "
                             << shape_base->ToString() << " are not consistent with second arg shape "
                             << shape->ToString();
  }

  for (size_t i = 0; i < shape_vector.size(); i++) {
    if (shape_vector[i] == Shape::kShapeDimAny || shape_base_vector[i] == Shape::kShapeDimAny) {
      continue;
    }
    if (shape_vector[i] != shape_base_vector[i]) {
      MS_EXCEPTION(ValueError) << "For '" << op << "',  the shape of two args should be same, but the first arg shape "
                               << shape_base->ToString() << " are not consistent with second arg shape "
                               << shape->ToString();
    }
  }
  return;
}

TypePtr CheckDtypeSame(const std::string &op, const AbstractTensorPtr &tensor_base, const AbstractTensorPtr &tensor) {
  MS_EXCEPTION_IF_NULL(tensor_base);
  auto base_elem = tensor_base->element();
  MS_EXCEPTION_IF_NULL(base_elem);
  TypePtr type_base = base_elem->BuildType();
  MS_EXCEPTION_IF_NULL(tensor);
  auto tensor_elem = tensor->element();
  MS_EXCEPTION_IF_NULL(tensor_elem);
  TypePtr type = tensor_elem->BuildType();
  MS_EXCEPTION_IF_NULL(type_base);
  MS_EXCEPTION_IF_NULL(type);
  CheckDtypeSame(op, type_base, type);
  return type_base;
}

int64_t CheckAxis(const std::string &op, const std::string &args_name, const ValuePtr &axis, int64_t minimum,
                  int64_t max, const std::string &rank_name) {
  if (axis == nullptr) {
    MS_LOG(EXCEPTION) << op << " evaluator axis is null";
  }
  if (!axis->isa<Int64Imm>()) {
    MS_LOG(EXCEPTION) << op << " evaluator axis should be int64_t, but got " << axis->type_name();
  }
  int64_t axis_value = GetValue<int64_t>(axis);
  if (axis_value >= max || axis_value < minimum) {
    MS_LOG(EXCEPTION) << "For primitive[" << op << "], " << rank_name << "'s rank is " << max << ", while the "
                      << "\'" << args_name << "\' value should be in the range [" << minimum << ", " << max
                      << "), but got " << axis_value;
  }
  if (axis_value < 0) {
    axis_value = axis_value + max;
  }
  return axis_value;
}
void CheckArgsSize(const std::string &op, const mindspore::abstract::AbstractBasePtrList &args_abs_list,
                   size_t size_expect) {
  if (args_abs_list.size() != size_expect) {
    MS_LOG(EXCEPTION) << "For '" << op << "', the number of input should be " << size_expect << ", but got "
                      << args_abs_list.size();
  }

  for (size_t i = 0; i < size_expect; i++) {
    MS_EXCEPTION_IF_NULL(args_abs_list[i]);
  }
}

void CheckShapeAllPositive(const std::string &op, const ShapeVector &shape) {
  for (size_t i = 0; i < shape.size(); ++i) {
    if (shape[i] < 0) {
      MS_LOG(EXCEPTION) << "For '" << op << "', shape element [" << i << "] must be positive integer, but got "
                        << shape[i];
    }
  }
}

void CheckShapeAnyAndPositive(const std::string &op, const ShapeVector &shape) {
  for (size_t i = 0; i < shape.size(); ++i) {
    if ((shape[i] < 0) && (shape[i] != Shape::kShapeDimAny)) {
      MS_EXCEPTION(ValueError) << op << " shape element [" << i
                               << "] must be positive integer or kShapeDimAny, but got " << shape[i];
    }
  }
}

void CheckRequiredArgsSize(const std::string &op, const mindspore::abstract::AbstractBasePtrList &args_abs_list,
                           size_t size_expect) {
  if (args_abs_list.size() < size_expect) {
    MS_LOG(EXCEPTION) << op << " required input args size " << size_expect << ", but got " << args_abs_list.size();
  }
  for (size_t i = 0; i < size_expect; i++) {
    MS_EXCEPTION_IF_NULL(args_abs_list[i]);
  }
}
}  // namespace abstract
}  // namespace mindspore
