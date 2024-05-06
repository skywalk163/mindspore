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

#include "ops/parallel_concat.h"
#include <map>
#include <string>
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr ParallelConcatInferShape(const PrimitivePtr &primitive,
                                            const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const auto &prim_name = primitive->name();
  abstract::BaseShapePtrList elements;
  if (input_args.size() == 1) {
    if (input_args[0]->GetType()->object_type() == kObjectTypeTuple) {
      elements = input_args[0]->GetShape()->cast<abstract::TupleShapePtr>()->shape();
    } else if (input_args[0]->GetType()->object_type() == kObjectTypeList) {
      elements = input_args[0]->GetShape()->cast<abstract::ListShapePtr>()->shape();
    } else {
      MS_EXCEPTION(TypeError) << "For '" << prim_name << "', the input data type must be list or tuple of tensors.";
    }
  } else {
    (void)std::transform(input_args.begin(), input_args.end(), std::back_inserter(elements),
                         [](const AbstractBasePtr &input_arg) {
                           MS_CHECK_VALUE(input_arg->GetType()->object_type() == kObjectTypeTensorType,
                                          "the inputs of ParallelConcat must be tuple(tensor) or list(tensor).");
                           return input_arg->GetShape();
                         });
  }
  (void)CheckAndConvertUtils::CheckInteger("concat element num", SizeToLong(elements.size()), kGreaterEqual, 1,
                                           prim_name);

  for (size_t i = 0; i < elements.size(); ++i) {
    auto shape_map_i = CheckAndConvertUtils::ConvertShapePtrToShapeMap(elements[i]);
    auto shape_i = shape_map_i[kShape];
    if (IsDynamicRank(shape_i)) {
      return std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeRankAny}));
    }
  }
  auto element0_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(elements[0])[kShape];
  auto element0_rank = element0_shape.size();
  if (element0_rank < 1) {
    MS_EXCEPTION(ValueError) << "For [" << prim_name
                             << "], the rank of input must greater than 1. But got:" << element0_rank << ".";
  }

  auto axis = 0;
  int64_t all_shp = static_cast<int64_t>(element0_shape[IntToSize(axis)]);
  for (size_t i = 0; i < elements.size(); ++i) {
    if (elements[i]->IsDynamic()) {
      auto ret_shape = element0_shape;
      ret_shape[IntToSize(axis)] = -1;
      return std::make_shared<abstract::Shape>(ret_shape);
    }
  }
  for (size_t i = 1; i < elements.size(); ++i) {
    auto elementi_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(elements[i])[kShape];
    (void)CheckAndConvertUtils::CheckInteger("x" + std::to_string(i) + ".shape[0]", elementi_shape[0], kEqual, 1,
                                             prim_name);
    if (elementi_shape.size() != element0_shape.size()) {
      MS_EXCEPTION(ValueError) << "For [" << prim_name << "], the rank of all elements should be the same, but got "
                               << "x0.rank [" << element0_shape.size() << "] and x" << std::to_string(i) << ".rank ["
                               << elementi_shape.size() << "].";
    }
    for (size_t j = 1; j < element0_rank; ++j) {
      if (elementi_shape[j] != element0_shape[j]) {
        MS_EXCEPTION(ValueError) << "For [" << prim_name << "], the shape of all elements should be the same, but got "
                                 << "x0.shape[" << std::to_string(j) << "] = [" << element0_shape[j] << "] and x"
                                 << std::to_string(i) << ".shape[" << std::to_string(j) << "] = [" << elementi_shape[j]
                                 << "].";
      }
    }

    all_shp = all_shp + elementi_shape[IntToSize(axis)];
  }
  auto ret_shape = element0_shape;
  ret_shape[IntToSize(axis)] = all_shp;
  (void)primitive->AddAttr("shape", MakeValue(ret_shape));
  return std::make_shared<abstract::Shape>(ret_shape);
}

TypePtr ParallelConcatInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const auto &prim_name = primitive->name();
  TypePtrList elements;
  if (input_args.size() == 1) {
    if (input_args[0]->GetType()->object_type() == kObjectTypeTuple) {
      elements = input_args[0]->GetType()->cast<TuplePtr>()->elements();
    } else if (input_args[0]->GetType()->object_type() == kObjectTypeList) {
      elements = input_args[0]->GetType()->cast<ListPtr>()->elements();
    } else {
      MS_EXCEPTION(TypeError) << "For '" << prim_name << "', the input data type must be list or tuple of tensors.";
    }
  }
  (void)CheckAndConvertUtils::CheckInteger("concat element num", SizeToLong(elements.size()), kGreaterEqual, 1,
                                           prim_name);
  std::map<std::string, TypePtr> types;
  for (size_t i = 0; i < elements.size(); ++i) {
    std::string elementi = "element" + std::to_string(i);
    (void)types.emplace(elementi, elements[i]);
  }
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, common_valid_types_with_complex_and_bool, prim_name);
  return elements[0];
}
}  // namespace

MIND_API_OPERATOR_IMPL(ParallelConcat, BaseOperator);
AbstractBasePtr ParallelConcatInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) {
  auto infer_type = ParallelConcatInferType(primitive, input_args);
  auto infer_shape = ParallelConcatInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

// AG means auto generated
class MIND_API AGParallelConcatInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return ParallelConcatInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return ParallelConcatInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return ParallelConcatInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(ParallelConcat, prim::kPrimParallelConcat, AGParallelConcatInfer, false);
}  // namespace ops
}  // namespace mindspore
