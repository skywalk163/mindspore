/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "ops/unravel_index.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr UravelIndexInferShape(const PrimitivePtr &primitive,
                                         const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  const int64_t input_num = 2;
  (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual, input_num, op_name);
  if (!CheckAndConvertUtils::IsTensor(input_args[0]) || !CheckAndConvertUtils::IsTensor(input_args[1])) {
    MS_EXCEPTION(TypeError) << "Input must be a Tensor.";
  }

  auto indices_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->GetShape())[kShape];
  auto dims_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->GetShape())[kShape];
  // support dynamic shape
  if (IsDynamicRank(indices_shape) || IsDynamicRank(dims_shape)) {
    return std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeRankAny}));
  }

  const int64_t indices_shape_dim = SizeToLong(indices_shape.size());
  const int64_t dims_shape_dim = SizeToLong(dims_shape.size());
  (void)CheckAndConvertUtils::CheckInteger("dims shape", dims_shape_dim, kEqual, 1, op_name);
  if (indices_shape_dim == 0) {
    int64_t dims_shape_v = dims_shape[0];
    std::vector<int64_t> output_shape;
    output_shape.push_back(dims_shape_v);
    return std::make_shared<abstract::Shape>(output_shape);
  } else {
    (void)CheckAndConvertUtils::CheckInteger("indices shape", indices_shape_dim, kEqual, 1, op_name);
    int64_t indices_shape_v = indices_shape[0];
    int64_t dims_shape_v = dims_shape[0];
    std::vector<int64_t> output_shape;
    output_shape.push_back(dims_shape_v);
    output_shape.push_back(indices_shape_v);
    return std::make_shared<abstract::Shape>(output_shape);
  }
}

TypePtr UravelIndexInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  std::map<std::string, TypePtr> types;
  auto op_name = prim->name();
  const int64_t input_num = 2;
  (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual, input_num, op_name);
  (void)types.emplace("indices", input_args[0]->GetType());
  (void)types.emplace("dims", input_args[1]->GetType());
  return CheckAndConvertUtils::CheckTensorTypeSame(types, {kInt32, kInt64}, prim->name());
}
}  // namespace

MIND_API_OPERATOR_IMPL(UnravelIndex, BaseOperator);
AbstractBasePtr UnravelIndexInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                  const std::vector<AbstractBasePtr> &input_args) {
  const int64_t input_num = 2;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, primitive->name());
  auto infer_type = UravelIndexInferType(primitive, input_args);
  auto infer_shape = UravelIndexInferShape(primitive, input_args);
  return std::make_shared<abstract::AbstractTensor>(infer_type, infer_shape);
}

// AG means auto generated
class MIND_API AGUnravelIndexInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return UravelIndexInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return UravelIndexInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return UnravelIndexInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(UnravelIndex, prim::kPrimUnravelIndex, AGUnravelIndexInfer, false);
}  // namespace ops
}  // namespace mindspore
