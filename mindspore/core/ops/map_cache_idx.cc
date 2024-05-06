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

#include "ops/map_cache_idx.h"

#include <map>
#include <set>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/container.h"
#include "ir/dtype/number.h"
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
abstract::TupleShapePtr MapCacheIdxInferShape(const PrimitivePtr &primitive,
                                              const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  constexpr int64_t kInputNum = 5;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputNum, prim_name);
  auto hashmap_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->GetShape());
  auto hashmap_shape = hashmap_shape_map[kShape];
  auto indices_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->GetShape());
  auto indices_shape = indices_shape_map[kShape];

  auto cache_idx_output = std::make_shared<abstract::Shape>(indices_shape);
  auto other_output = std::make_shared<abstract::Shape>(std::vector<int64_t>{abstract::Shape::kShapeRankAny});
  if (IsDynamicRank(indices_shape)) {
    return std::make_shared<abstract::TupleShape>(
      std::vector<abstract::BaseShapePtr>{cache_idx_output, other_output, other_output, other_output});
  }

  const size_t hashmap_shape_size = 2;
  if (hashmap_shape.size() != hashmap_shape_size) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "',"
                             << " the dimension of hashmap must be equal to 2, but got: " << hashmap_shape.size()
                             << ".";
  }
  return std::make_shared<abstract::TupleShape>(
    std::vector<abstract::BaseShapePtr>{cache_idx_output, other_output, other_output, other_output});
}

TuplePtr MapCacheIdxInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  constexpr int64_t kInputNum = 5;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputNum, prim_name);
  auto hashmap_type = input_args[0]->GetType();
  auto indices_type = input_args[1]->GetType();

  const std::set<TypePtr> valid_types = {kInt64, kInt32, kInt16, kInt8};
  std::map<std::string, TypePtr> input_types;
  (void)input_types.emplace("hashmap", hashmap_type);
  (void)input_types.emplace("indices", indices_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(input_types, valid_types, prim_name);
  return std::make_shared<Tuple>(std::vector<TypePtr>{hashmap_type, hashmap_type, hashmap_type, hashmap_type});
}
}  // namespace

AbstractBasePtr MapCacheIdxInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                 const std::vector<AbstractBasePtr> &input_args) {
  auto infer_type = MapCacheIdxInferType(primitive, input_args);
  auto infer_shape = MapCacheIdxInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

MIND_API_OPERATOR_IMPL(MapCacheIdx, BaseOperator);

// AG means auto generated
class MIND_API AGMapCacheIdxInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return MapCacheIdxInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return MapCacheIdxInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return MapCacheIdxInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(MapCacheIdx, prim::kPrimMapCacheIdx, AGMapCacheIdxInfer, false);
}  // namespace ops
}  // namespace mindspore
