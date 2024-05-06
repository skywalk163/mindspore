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

#include "ops/fill.h"

#include <complex>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/ops/primitive_infer_map.h"
#include "include/common/utils/utils.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "mindspore/core/ops/math_ops.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "utils/tensor_construct_utils.h"

namespace mindspore {
namespace ops {
MIND_API_OPERATOR_IMPL(Fill, BaseOperator);
template <typename T>
static tensor::TensorPtr CreateValuedTensor(const TypePtr &type, const std::vector<int64_t> &shape, T num) {
  MS_EXCEPTION_IF_NULL(type);
  auto type_id = type->type_id();
  tensor::TensorPtr tensor = std::make_shared<tensor::Tensor>(type_id, shape);
  const size_t &mem_size = LongToSize(tensor->ElementsNum());
  auto tensor_data = tensor->data_c();
  std::map<TypeId, std::function<void()>> type_dict{
    {kNumberTypeBool,
     [&tensor_data, mem_size, &num]() { SetTensorData<bool>(tensor_data, static_cast<bool>(num), mem_size); }},
    {kNumberTypeInt8,
     [&tensor_data, mem_size, &num]() { SetTensorData<int8_t>(tensor_data, static_cast<int8_t>(num), mem_size); }},
    {kNumberTypeInt16,
     [&tensor_data, mem_size, &num]() { SetTensorData<int16_t>(tensor_data, static_cast<int16_t>(num), mem_size); }},
    {kNumberTypeInt32,
     [&tensor_data, mem_size, &num]() { SetTensorData<int32_t>(tensor_data, static_cast<int32_t>(num), mem_size); }},
    {kNumberTypeInt64,
     [&tensor_data, mem_size, &num]() { SetTensorData<int64_t>(tensor_data, static_cast<int64_t>(num), mem_size); }},
    {kNumberTypeUInt8,
     [&tensor_data, mem_size, &num]() { SetTensorData<uint8_t>(tensor_data, static_cast<uint8_t>(num), mem_size); }},
    {kNumberTypeUInt16,
     [&tensor_data, mem_size, &num]() { SetTensorData<uint16_t>(tensor_data, static_cast<uint16_t>(num), mem_size); }},
    {kNumberTypeUInt32,
     [&tensor_data, mem_size, &num]() { SetTensorData<uint32_t>(tensor_data, static_cast<uint32_t>(num), mem_size); }},
    {kNumberTypeUInt64,
     [&tensor_data, mem_size, &num]() { SetTensorData<uint64_t>(tensor_data, static_cast<uint64_t>(num), mem_size); }},
    {kNumberTypeFloat16,
     [&tensor_data, mem_size, &num]() { SetTensorData<float16>(tensor_data, static_cast<float16>(num), mem_size); }},
    {kNumberTypeFloat32,
     [&tensor_data, mem_size, &num]() { SetTensorData<float>(tensor_data, static_cast<float>(num), mem_size); }},
    {kNumberTypeFloat64,
     [&tensor_data, mem_size, &num]() { SetTensorData<double>(tensor_data, static_cast<double>(num), mem_size); }},
  };
  const auto &tensor_type = tensor->data_type();
  auto iter = type_dict.find(tensor_type);
  if (iter == type_dict.end()) {
    MS_LOG(EXCEPTION) << "unsupported data type: " << tensor_type;
  }
  iter->second();
  return tensor;
}

template <typename T>
static tensor::TensorPtr CreateComplexTensor(const TypePtr &type, const std::vector<int64_t> &shape, T num) {
  MS_EXCEPTION_IF_NULL(type);
  auto type_id = type->type_id();
  tensor::TensorPtr tensor = std::make_shared<tensor::Tensor>(type_id, shape);
  const size_t &mem_size = LongToSize(tensor->ElementsNum());
  auto tensor_data = tensor->data_c();
  std::map<TypeId, std::function<void()>> type_dict{
    {kNumberTypeComplex64,
     [&tensor_data, mem_size, &num]() {
       SetTensorData<std::complex<float>>(tensor_data, static_cast<std::complex<float>>(num), mem_size);
     }},
    {kNumberTypeComplex128,
     [&tensor_data, mem_size, &num]() {
       SetTensorData<std::complex<double>>(tensor_data, static_cast<std::complex<double>>(num), mem_size);
     }},
  };
  const auto &tensor_type = tensor->data_type();
  auto iter = type_dict.find(tensor_type);
  if (iter == type_dict.end()) {
    MS_LOG(EXCEPTION) << "unsupported data type: " << tensor_type;
  }
  iter->second();
  return tensor;
}

class FillInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    std::vector<size_t> inputsIndex{kIndex0, kIndex1, kIndex2};
    if (input_args.size() == kIndex2) {
      inputsIndex[kIndex1] = kIndex0;
      inputsIndex[kIndex2] = kIndex1;
    }
    auto prim_name = primitive->name();
    if (input_args[inputsIndex[kIndex1]]->GetType()->object_type() == kObjectTypeTuple) {
      auto out_shape = GetShapeValue(primitive, input_args[inputsIndex[kIndex1]]);
      return std::make_shared<abstract::Shape>(out_shape);
    }

    if (input_args[inputsIndex[kIndex1]]->GetType()->object_type() != kObjectTypeTensorType) {
      MS_EXCEPTION(TypeError) << "For '" << prim_name << "', input[1] must be tensor.";
    }
    MS_EXCEPTION_IF_NULL(primitive);
    const uint32_t kInputDims = 1;
    auto shape_arg = input_args[inputsIndex[1]];
    MS_EXCEPTION_IF_NULL(shape_arg);
    if (!IsValueKnown(shape_arg->GetValue()) && shape_arg->GetType()->object_type() == kObjectTypeTensorType) {
      auto abs_tensor_shape = shape_arg->GetShape()->GetShapeVector();
      if (abs_tensor_shape.size() != kInputDims) {
        MS_EXCEPTION(TypeError) << "For '" << prim_name
                                << "', the shape size of 'input1' must be 1, but got: " << abs_tensor_shape.size()
                                << ".";
      }
    }
    auto input2_shape =
      CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[inputsIndex[kIndex2]]->GetShape())[kShape];
    if (input2_shape.size() > 1 || (input2_shape.size() == 1 && input2_shape[0] > 1)) {
      MS_EXCEPTION(TypeError) << "For '" << prim_name
                              << "', the shape size of 'input2' must be 0, but got: " << input2_shape.size() << ".";
    }

    auto output_shape = GetShapeValue(primitive, shape_arg);
    return std::make_shared<abstract::Shape>(output_shape);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    if (input_args.size() <= kSize1) {
      MS_EXCEPTION(TypeError) << "For '" << prim_name
                              << "', the inputs take 2 or 3 arguments, but got less than 2 here!";
    }
    ShapeVector value_shape;
    TypePtr value_type;
    ValuePtr input_value;
    if (input_args.size() == kSize3) {
      value_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kIndex2]->GetShape())[kShape];
      value_type = input_args[kIndex2]->GetType();
      input_value = input_args[kIndex0]->GetValue();
    } else {
      value_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kIndex1]->GetShape())[kShape];
      value_type = input_args[kIndex1]->GetType();
      if (!primitive->HasAttr()) {
        MS_LOG(EXCEPTION) << "prim: " << prim_name << " should has attr 'type'";
      }
      input_value = primitive->GetAttr("type");
    }
    MS_EXCEPTION_IF_NULL(value_type);
    MS_EXCEPTION_IF_NULL(input_value);

    // check
    TypePtr input2_element_dtype;
    if (value_type->isa<TensorType>()) {
      auto value_tensor_type = value_type->cast<TensorTypePtr>();
      MS_EXCEPTION_IF_NULL(value_tensor_type);
      input2_element_dtype = value_tensor_type->element();
    } else {
      input2_element_dtype = value_type;
    }
    if (value_shape.size() > 1 || (value_shape.size() == 1 && value_shape[0] > 1)) {
      MS_EXCEPTION(TypeError) << "For '" << prim_name
                              << "', the value input only takes scalar or scalar within a tensor!";
    }
    MS_EXCEPTION_IF_NULL(input_value);
    if (!input_value->isa<Type>()) {
      MS_EXCEPTION(TypeError)
        << "For '" << prim_name
        << "', the supported data type is ['bool', 'int8', 'int16', 'int32', 'int64', 'uint8', 'uint16','uint32', "
           "'uint64','float16', 'float32', 'float64'], but got an invalid dtype!";
    }
    auto output_dtype = input_value->cast<TypePtr>();

    const std::set<TypePtr> valid_types = {kBool,   kInt8,   kInt16,   kInt32,   kInt64,   kUInt8,     kUInt16,
                                           kUInt32, kUInt64, kFloat16, kFloat32, kFloat64, kComplex64, kComplex128};
    CheckAndConvertUtils::CheckSubClass("dtype", input2_element_dtype, valid_types, prim_name);
    return CheckAndConvertUtils::CheckSubClass("dtype", output_dtype, valid_types, prim_name);
  }

  ValuePtr InferValue(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(prim);
    const int64_t input_num = 2;
    CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, input_num, prim->name());
    auto infered_type = InferType(prim, input_args);
    MS_EXCEPTION_IF_NULL(infered_type);
    auto input_value_ptr = input_args[2]->GetValue();
    auto input_value_type_id = input_args[2]->GetType()->type_id();
    auto tmp_shape = InferShape(prim, input_args);
    MS_EXCEPTION_IF_NULL(tmp_shape);
    auto infered_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(tmp_shape)[kShape];
    auto input_value_tensor = input_value_ptr->cast<tensor::TensorPtr>();
    tensor::TensorPtr infer_result;
    if (input_value_type_id == kNumberTypeBool) {
      infer_result = CreateValuedTensor<bool>(infered_type, infered_shape, GetValue<bool>(input_value_ptr));
    } else if (input_value_type_id == kNumberTypeFloat32) {
      infer_result = CreateValuedTensor<float>(infered_type, infered_shape, GetValue<float>(input_value_ptr));
    } else if (input_value_type_id == kNumberTypeInt32) {
      infer_result = CreateValuedTensor<int32_t>(infered_type, infered_shape, GetValue<int32_t>(input_value_ptr));
    } else if (input_value_type_id == kNumberTypeInt64) {
      infer_result = CreateValuedTensor<int64_t>(infered_type, infered_shape, GetValue<int64_t>(input_value_ptr));
    } else if (input_value_type_id == kNumberTypeComplex64) {
      infer_result = CreateComplexTensor<std::complex<float>>(
        infered_type, infered_shape, static_cast<std::complex<float> *>(input_value_tensor->data_c())[0]);
    } else if (input_value_type_id == kNumberTypeComplex128) {
      infer_result = CreateComplexTensor<std::complex<double>>(
        infered_type, infered_shape, static_cast<std::complex<double> *>(input_value_tensor->data_c())[0]);
    }
    return infer_result;
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {0, 2}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(Fill, prim::kPrimFill, FillInfer, true);
}  // namespace ops
}  // namespace mindspore
