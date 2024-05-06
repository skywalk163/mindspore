/**
 * Copyright 202 Huawei Technologies Co., Ltd
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

#include "ops/meshgrid.h"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
MIND_API_OPERATOR_IMPL(Meshgrid, BaseOperator);

void Meshgrid::Init(const std::string &indexing) { this->set_indexing(indexing); }

void Meshgrid::set_indexing(const std::string &indexing) { (void)this->AddAttr(kIndexing, api::MakeValue(indexing)); }

std::string Meshgrid::get_indexing() const {
  auto value_ptr = this->GetAttr(kIndexing);
  return GetValue<std::string>(value_ptr);
}

namespace {
abstract::TupleShapePtr MeshgridInferShape(const PrimitivePtr &primitive,
                                           const std::vector<AbstractBasePtr> &input_args) {
  AbstractBasePtrList elements = input_args;
  if (input_args.size() == 1 && input_args[0]->isa<abstract::AbstractSequence>()) {
    elements = input_args[0]->cast<abstract::AbstractSequencePtr>()->elements();
  }
  (void)CheckAndConvertUtils::CheckInteger("number of input tensors", SizeToLong(elements.size()), kGreaterThan, 1,
                                           primitive->name());
  ShapeVector output_shape;
  for (size_t i = 0; i < elements.size(); ++i) {
    auto shape = elements[i]->GetShape();
    ShapeVector input_shape;
    if (shape->isa<abstract::TensorShape>()) {
      input_shape = shape->GetShapeVector();
    }
    if (IsDynamicRank(input_shape)) {
      auto shape_ptr = std::make_shared<abstract::Shape>(std::vector<int64_t>{abstract::Shape::kShapeRankAny});
      return std::make_shared<abstract::TupleShape>(
        std::vector<abstract::BaseShapePtr>(SizeToLong(elements.size()), shape_ptr));
    }
    (void)CheckAndConvertUtils::CheckInteger("Each input dims", SizeToLong(input_shape.size()), kEqual, 1,
                                             primitive->name());
    output_shape.push_back(input_shape[0]);
  }

  std::string indexing = GetValue<std::string>(primitive->GetAttr("indexing"));
  if (indexing == "xy") {
    std::swap(output_shape[0], output_shape[1]);
  }
  auto shape_ptr = std::make_shared<abstract::Shape>(output_shape);
  return std::make_shared<abstract::TupleShape>(
    std::vector<abstract::BaseShapePtr>(SizeToLong(elements.size()), shape_ptr));
}

TuplePtr MeshgridInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  auto elements = input_args[0]->GetType()->cast<TuplePtr>()->elements();
  (void)CheckAndConvertUtils::CheckInteger("number of input tensors", SizeToLong(elements.size()), kGreaterThan, 1,
                                           prim->name());
  std::map<std::string, TypePtr> types;
  for (size_t i = 0; i < elements.size(); ++i) {
    std::string elementi = "element" + std::to_string(i);
    (void)types.emplace(elementi, elements[i]);
  }
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, common_valid_types_with_complex_and_bool, prim->name());
  return std::make_shared<Tuple>(std::vector<TypePtr>(SizeToLong(elements.size()), elements[0]));
}
}  // namespace

AbstractBasePtr MeshgridInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                              const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  (void)CheckAndConvertUtils::CheckInteger("input_args tuple size", SizeToLong(input_args.size()), kEqual, 1,
                                           prim_name);
  MS_EXCEPTION_IF_NULL(input_args[0]);
  if (input_args[0]->GetType()->object_type() != kObjectTypeTuple) {
    MS_EXCEPTION(TypeError) << "For '" << prim_name << "', the input must be tuple of tensors.";
  }
  auto elements = input_args[0]->GetShape()->cast<abstract::TupleShapePtr>()->shape();
  (void)CheckAndConvertUtils::CheckInteger("number of input tensors", SizeToLong(elements.size()), kGreaterThan, 1,
                                           prim_name);
  auto infer_type = MeshgridInferType(primitive, input_args);
  auto infer_shape = MeshgridInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

// AG means auto generated
class MIND_API AGMeshgridInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return MeshgridInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return MeshgridInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return MeshgridInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(Meshgrid, prim::kPrimMeshgrid, AGMeshgridInfer, false);
}  // namespace ops
}  // namespace mindspore
