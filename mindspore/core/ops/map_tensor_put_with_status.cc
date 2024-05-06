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
#include "ops/map_tensor_put_with_status.h"

#include <memory>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/primitive_infer_map.h"
#include "ir/anf.h"
#include "ir/dtype/tensor_type.h"
#include "ir/dtype/type.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sparse_tensor_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/ms_utils.h"

namespace mindspore {
namespace ops {
MIND_API_OPERATOR_IMPL(MapTensorPutWithStatus, BaseOperator);

AbstractBasePtr MapTensorPutWithStatusInferInner(const PrimitivePtr &primitive,
                                                 const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  // Check number of arguments.
  constexpr int64_t input_num = 4;
  CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, input_num, kNameMapTensorPutWithStatus);
  // Check argument abstracts.
  auto abs_map_tensor = CheckAndConvertUtils::CheckArgsType(kNameMapTensorPutWithStatus, input_args, kInputIndex0,
                                                            kObjectTypeMapTensorType);

  // Get key dtype, value dtype and value shape of the map tensor.
  auto map_tensor_type = abs_map_tensor->GetType()->cast<MapTensorTypePtr>();
  MS_EXCEPTION_IF_NULL(map_tensor_type);
  auto key_dtype = map_tensor_type->key_dtype();
  auto value_dtype = map_tensor_type->value_dtype();

  // Check 'key_tensor' dtype and shape.
  auto key_tensor_dtype =
    CheckAndConvertUtils::GetTensorInputType(kNameMapTensorPutWithStatus, input_args, kInputIndex1);
  if (!common::IsEqual(key_dtype, key_tensor_dtype)) {
    MS_EXCEPTION(TypeError) << kNameMapTensorPutWithStatus << " - required key_tensor dtype " << key_dtype->ToString()
                            << " but got " << key_tensor_dtype->ToString() << ".";
  }
  auto key_tensor_shape =
    CheckAndConvertUtils::GetTensorInputShape(kNameMapTensorPutWithStatus, input_args, kInputIndex1);
  if (key_tensor_shape->shape().size() != 1) {
    MS_EXCEPTION(TypeError) << kNameMapTensorPutWithStatus << " - key_tensor shape should be 1 rank"
                            << " but got " << key_tensor_shape->ToString() << ".";
  }

  // Check 'value_tensor' dtype.
  auto value_tensor_dtype =
    CheckAndConvertUtils::GetTensorInputType(kNameMapTensorPutWithStatus, input_args, kInputIndex2);
  if (!common::IsEqual(value_dtype, value_tensor_dtype)) {
    MS_EXCEPTION(ValueError) << kNameMapTensorPutWithStatus << " - required value tensor dtype "
                             << value_dtype->ToString() << " but got " << value_tensor_dtype->ToString() << ".";
  }

  // Check 'status_tensor' dtype and shape.
  auto status_tensor_dtype =
    CheckAndConvertUtils::GetTensorInputType(kNameMapTensorPutWithStatus, input_args, kInputIndex3);
  if (!common::IsEqual(kInt32, status_tensor_dtype)) {
    MS_EXCEPTION(ValueError) << kNameMapTensorPutWithStatus << " - required status tensor dtype " << kInt32->ToString()
                             << " but got " << status_tensor_dtype->ToString() << ".";
  }

  auto status_tensor_shape =
    CheckAndConvertUtils::GetTensorInputShape(kNameMapTensorPutWithStatus, input_args, kInputIndex3);
  if (!common::IsEqual(key_tensor_shape, status_tensor_shape)) {
    MS_EXCEPTION(ValueError) << kNameMapTensorPutWithStatus << " - required status tensor shape "
                             << key_tensor_shape->ToString() << " but got " << status_tensor_shape->ToString() << ".";
  }

  // Need not check shape of value tensor on dynamic shape case.
  auto value_tensor_shape =
    CheckAndConvertUtils::GetTensorInputShape(kNameMapTensorPutWithStatus, input_args, kInputIndex2);
  if (key_tensor_shape->IsDynamic() || value_tensor_shape->IsDynamic() || status_tensor_shape->IsDynamic()) {
    // Return the input AbstractMapTensor.
    return abs_map_tensor;
  }

  // Check 'value_tensor' shape.
  // Concate key shape and value shape as the required value shape.
  ShapeVector shape_vec = key_tensor_shape->shape();
  const ShapeVector &key_value_shape = abs_map_tensor->GetShape()->GetShapeVector();
  (void)shape_vec.insert(shape_vec.end(), key_value_shape.begin() + 1, key_value_shape.end());
  auto required_value_shape = std::make_shared<abstract::Shape>(shape_vec);
  if (!common::IsEqual(required_value_shape, value_tensor_shape)) {
    MS_EXCEPTION(ValueError) << kNameMapTensorPutWithStatus << " - required value tensor shape "
                             << required_value_shape->ToString() << " but got " << value_tensor_shape->ToString()
                             << ".";
  }

  // Return the input AbstractMapTensor.
  return abs_map_tensor;
}

BaseShapePtr MapTensorPutWithStatusInferShape(const PrimitivePtr &prim,
                                              const std::vector<AbstractBasePtr> &input_args) {
  auto abs = MapTensorPutWithStatusInferInner(prim, input_args);
  return abs->GetShape();
}

TypePtr MapTensorPutWithStatusInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  auto abs = MapTensorPutWithStatusInferInner(prim, input_args);
  return abs->GetType();
}

AbstractBasePtr MapTensorPutWithStatusInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                            const std::vector<AbstractBasePtr> &input_args) {
  return MapTensorPutWithStatusInferInner(primitive, input_args);
}

// AG means auto generated
class MIND_API AGMapTensorPutWithStatusInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return MapTensorPutWithStatusInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return MapTensorPutWithStatusInferType(primitive, input_args);
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return MapTensorPutWithStatusInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(MapTensorPutWithStatus, prim::kPrimMapTensorPutWithStatus,
                                 AGMapTensorPutWithStatusInfer, false);
}  // namespace ops
}  // namespace mindspore
