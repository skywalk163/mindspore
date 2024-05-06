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

#include "ops/non_zero_with_value_shape.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <numeric>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/param_validator.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
// NonZeroWithValueShape
MIND_API_OPERATOR_IMPL(NonZeroWithValueShape, BaseOperator);
AbstractBasePtr NonZeroWithValueShapeInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                           const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const std::string &op_name = primitive->name();
  constexpr size_t input_num = 3;
  abstract::CheckArgsSize(op_name, input_args, input_num);
  const auto x = CheckAndConvertUtils::CheckArgsType(op_name, input_args, 0, kObjectTypeTensorType);

  MS_EXCEPTION_IF_NULL(x);
  auto x_shape = x->GetShape();
  MS_EXCEPTION_IF_NULL(x_shape);
  ShapeVector y_shape;

  int64_t rank_base = SizeToLong(x_shape->GetShapeVector().size());
  auto shape_vec = x_shape->GetShapeVector();
  if (shape_vec.size() == 1 && shape_vec[0] == abstract::TensorShape::kShapeRankAny) {
    rank_base = abstract::Shape::kShapeDimAny;
  }
  (void)y_shape.emplace_back(rank_base);
  // Indices of elements that are non-zero
  (void)y_shape.emplace_back(abstract::Shape::kShapeDimAny);

  auto value = std::make_shared<abstract::AbstractTensor>(x->GetType(), std::make_shared<abstract::Shape>(y_shape));
  auto index = std::make_shared<abstract::AbstractTensor>(kInt32, std::make_shared<abstract::Shape>(y_shape));
  AbstractBasePtrList result = {value, index};
  return std::make_shared<abstract::AbstractTuple>(result);
}

class MIND_API AGNonZeroWithValueShapeInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    const std::string &op_name = primitive->name();
    constexpr size_t input_num = 3;
    abstract::CheckArgsSize(op_name, input_args, input_num);
    auto x = CheckAndConvertUtils::CheckArgsType(op_name, input_args, 0, kObjectTypeTensorType);

    MS_EXCEPTION_IF_NULL(x);
    auto x_shape = x->GetShape();
    MS_EXCEPTION_IF_NULL(x_shape);
    ShapeVector y_shape;

    int64_t rank_base = SizeToLong(x_shape->GetShapeVector().size());
    int64_t max_size = std::accumulate(x_shape->GetShapeVector().begin(), x_shape->GetShapeVector().end(), 1,
                                       std::multiplies<int64_t>());
    (void)y_shape.emplace_back(rank_base);
    // Set as max shape when in running process.
    (void)y_shape.emplace_back(max_size);

    auto value = std::make_shared<abstract::Shape>(y_shape);
    auto index = std::make_shared<abstract::Shape>(y_shape);
    abstract::BaseShapePtrList result = {value, index};
    return std::make_shared<abstract::TupleShape>(result);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    auto x = CheckAndConvertUtils::CheckArgsType(primitive->name(), input_args, 0, kObjectTypeTensorType);
    return std::make_shared<Tuple>(std::vector<TypePtr>{x->GetType(), kInt32});
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return NonZeroWithValueShapeInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(NonZeroWithValueShape, prim::kPrimNonZeroWithValueShape, AGNonZeroWithValueShapeInfer,
                                 false);
}  // namespace ops
}  // namespace mindspore
