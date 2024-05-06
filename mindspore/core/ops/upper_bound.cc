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

#include "ops/upper_bound.h"

#include <map>
#include <set>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/dtype/type.h"
#include "ir/primitive.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr UpperBoundInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->GetShape())[kShape];
  auto values_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->GetShape())[kShape];
  if (IsDynamicRank(values_shape)) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>{-2});
  }
  size_t size_exp = 2;
  if (!IsDynamicRank(x_shape) && x_shape.size() != size_exp) {
    MS_EXCEPTION(ValueError) << "For '" << primitive->name()
                             << "', the rank of 'sorted_x' must be equal to 2, but got: " << x_shape.size() << ".";
  }
  if (values_shape.size() != size_exp) {
    MS_EXCEPTION(ValueError) << "For '" << primitive->name()
                             << "', the rank of 'values' must be equal to 2, but got: " << values_shape.size() << ".";
  }
  if (!IsDynamic(x_shape) && !IsDynamic(values_shape) && x_shape[0] != values_shape[0]) {
    MS_EXCEPTION(ValueError)
      << "For '" << primitive->name()
      << "', the number of rows of 'sorted_x' must be consistent with that of 'values', but got the shape of 'values': "
      << input_args[1]->GetShape()->ToString() << ", the shape of 'sorted_x': " << input_args[0]->GetShape()->ToString()
      << ".";
  }
  return std::make_shared<abstract::Shape>(values_shape);
}

TypePtr UpperBoundInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  std::map<std::string, TypePtr> input_types;
  std::set<TypePtr> input_valid_types = {kFloat16, kFloat32, kFloat64, kInt8, kInt16, kInt32, kInt64, kUInt8, kUInt16};
  TypePtr sorted_x_type = input_args[0]->GetType();
  TypePtr values_type = input_args[1]->GetType();
  (void)input_types.emplace("sorted_x", sorted_x_type);
  (void)input_types.emplace("values", values_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(input_types, input_valid_types, primitive->name());
  auto dtype_attr = primitive->GetAttr("out_type");
  MS_EXCEPTION_IF_NULL(dtype_attr);
  auto out_type = dtype_attr->cast<TypePtr>();
  MS_EXCEPTION_IF_NULL(out_type);
  auto out_type_id = out_type->type_id();
  if (out_type_id != kInt32->type_id() && out_type_id != kInt64->type_id()) {
    MS_EXCEPTION(TypeError) << "For '" << primitive->name() << "', 'out_type' must be int32 or int64.";
  }
  return out_type;
}
}  // namespace

MIND_API_OPERATOR_IMPL(UpperBound, BaseOperator);
AbstractBasePtr UpperBoundInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t input_num = 2;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, primitive->name());
  auto infer_type = UpperBoundInferType(primitive, input_args);
  auto infer_shape = UpperBoundInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

// AG means auto generated
class MIND_API AGUpperBoundInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return UpperBoundInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return UpperBoundInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return UpperBoundInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(UpperBound, prim::kPrimUpperBound, AGUpperBoundInfer, false);
}  // namespace ops
}  // namespace mindspore
