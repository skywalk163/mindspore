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

#include <map>
#include <memory>
#include <complex>
#include "ops/ops_frontend_func_impl.h"
#include "ops/ops_func_impl/mul.h"
#include "ops/op_utils.h"

namespace mindspore::ops {
BaseShapePtr MulFuncImpl::InferShape(const PrimitivePtr &primitive,
                                     const std::vector<AbstractBasePtr> &input_args) const {
  return BroadCastInferShape(primitive->name(), input_args);
}

TypePtr MulFuncImpl::InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const {
  MS_EXCEPTION_IF_NULL(input_args[kInputIndex0]->GetType());
  return input_args[kInputIndex0]->GetType()->Clone();
}

template <typename T>
void ImplMul(void *x1, void *x2, void *result, size_t size) {
  MS_EXCEPTION_IF_NULL(x1);
  MS_EXCEPTION_IF_NULL(x2);
  MS_EXCEPTION_IF_NULL(result);
  T *x1_data = static_cast<T *>(x1);
  T *x2_data = static_cast<T *>(x2);
  auto result_data = static_cast<T *>(result);
  for (size_t i = 0; i < size; ++i) {
    result_data[i] = x1_data[i] * x2_data[i];
  }
}

template <typename T>
void ImplMulBool(void *x1, void *x2, void *result, size_t size) {
  MS_EXCEPTION_IF_NULL(x1);
  MS_EXCEPTION_IF_NULL(x2);
  MS_EXCEPTION_IF_NULL(result);
  T *x1_data = static_cast<T *>(x1);
  T *x2_data = static_cast<T *>(x2);
  auto result_data = static_cast<T *>(result);
  for (size_t i = 0; i < size; ++i) {
    result_data[i] = x1_data[i] && x2_data[i];
  }
}

class MulFrontendFuncImpl : public OpFrontendFuncImpl {
 public:
  ValuePtr InferValue(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    auto x1 = input_args[kIndex0]->GetValue();
    auto x2 = input_args[kIndex1]->GetValue();
    if (x1 == nullptr || x2 == nullptr || x1->isa<ValueAny>() || x2->isa<ValueAny>()) {
      return nullptr;
    }
    auto x1_tensor = x1->cast<tensor::TensorPtr>();
    auto x2_tensor = x2->cast<tensor::TensorPtr>();
    MS_EXCEPTION_IF_NULL(x1_tensor);
    MS_EXCEPTION_IF_NULL(x2_tensor);

    auto x1_shape = input_args[kIndex0]->GetShape()->GetShapeVector();
    auto x2_shape = input_args[kIndex1]->GetShape()->GetShapeVector();
    if (IsDynamic(x1_shape) || IsDynamic(x2_shape) || x1_shape != x2_shape) {
      return nullptr;
    }

    auto data_size = x1_tensor->DataSize();
    auto dtype = x1_tensor->data_type();
    auto result_tensor = std::make_shared<tensor::Tensor>(dtype, x1_shape);
    MS_EXCEPTION_IF_NULL(result_tensor);
    auto result_datac = result_tensor->data_c();

    auto iter = func_map.find(dtype);
    if (iter != func_map.end()) {
      iter->second(x1_tensor->data_c(), x2_tensor->data_c(), result_datac, data_size);
    } else {
      MS_EXCEPTION(TypeError) << "For '" << primitive->name() << "', 'x' is " << x1_tensor->ToString()
                              << ", the type is not supported.";
    }
    return result_tensor;
  }

 private:
  std::map<TypeId, std::function<void(void *x1, void *x2, void *result, size_t size)>> func_map = {
    {kNumberTypeBool, ImplMulBool<bool>},
    {kNumberTypeInt, ImplMul<int>},
    {kNumberTypeInt8, ImplMul<int8_t>},
    {kNumberTypeInt16, ImplMul<int16_t>},
    {kNumberTypeInt32, ImplMul<int32_t>},
    {kNumberTypeInt64, ImplMul<int64_t>},
    {kNumberTypeUInt, ImplMul<u_int>},
    {kNumberTypeUInt8, ImplMul<uint8_t>},
    {kNumberTypeUInt16, ImplMul<uint16_t>},
    {kNumberTypeUInt32, ImplMul<uint32_t>},
    {kNumberTypeUInt64, ImplMul<uint64_t>},
    {kNumberTypeFloat16, ImplMul<float16>},
    {kNumberTypeFloat32, ImplMul<float>},
    {kNumberTypeFloat, ImplMul<float>},
    {kNumberTypeFloat64, ImplMul<double>},
    {kNumberTypeDouble, ImplMul<double>},
    {kNumberTypeComplex64, ImplMul<std::complex<float>>},
    {kNumberTypeComplex128, ImplMul<std::complex<double>>}};
};
REGISTER_PRIMITIVE_FUNCTION_FRONTEND_FUNC_IMPL("Mul", MulFrontendFuncImpl);
}  // namespace mindspore::ops
