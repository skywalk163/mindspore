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
#include "ops/euclidean_norm.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/named.h"
#include "ir/primitive.h"
#include "ir/value.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/math_ops.h"
#include "ops/op_name.h"
#include "ops/op_utils.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
void ReduceAxes(std::vector<int64_t> *output_shape, std::vector<int64_t> *axes, int64_t input_rank, bool keep_dims,
                const PrimitivePtr &primitive) {
  MS_EXCEPTION_IF_NULL(primitive);
  MS_EXCEPTION_IF_NULL(axes);
  MS_EXCEPTION_IF_NULL(output_shape);
  auto prim_name = primitive->name();
  auto validateAndConvertAxis = [&](int64_t &axis) {
    CheckAndConvertUtils::CheckInRange("axes value", axis, kIncludeLeft, {-input_rank, input_rank}, prim_name);
    if (axis < 0) {
      axis += input_rank;
    }
  };

  if (axes->size() > 1) {
    for (size_t i = 0; i < axes->size(); ++i) {
      validateAndConvertAxis((*axes)[i]);
    }
    constexpr int64_t place_holder = INT64_MAX;
    for (size_t i = 0; i < axes->size(); ++i) {
      auto temp = *axes;
      auto idx = std::find(temp.begin(), temp.end(), axes->at(i));
      (void)temp.erase(idx);
      auto re_idx = std::find(temp.begin(), temp.end(), axes->at(i));
      if (re_idx != temp.end()) {
        MS_EXCEPTION(ValueError) << "For '" << prim_name
                                 << "', the element of the axes must be different, but got axes: " << *axes << ".";
      }
      if (!keep_dims) {
        output_shape->at(LongToSize(axes->at(i))) = place_holder;
      } else {
        output_shape->at(LongToSize(axes->at(i))) = 1;
      }
    }
    if (!keep_dims) {
      for (auto iter = output_shape->begin(); iter != output_shape->end(); ++iter) {
        if (*iter == place_holder) {
          iter = output_shape->erase(iter);
          iter -= 1;
        }
      }
    }
  } else if (axes->size() == 1) {
    validateAndConvertAxis((*axes)[0]);
    if (!keep_dims) {
      (void)output_shape->erase(output_shape->begin() + LongToSize(axes->at(0)));
    } else {
      output_shape->at(LongToSize(axes->at(0))) = 1;
    }
  }
}

abstract::ShapePtr EuclideanNormInferShape(const PrimitivePtr &primitive,
                                           const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  auto input_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  auto axes_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  if (IsDynamicRank(input_shape)) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>{-2});
  }
  auto keep_dims_ptr = primitive->GetAttr("keep_dims");
  MS_EXCEPTION_IF_NULL(keep_dims_ptr);
  auto keep_dims = GetValue<bool>(keep_dims_ptr);
  if (IsDynamicRank(axes_shape) || IsDynamicShape(axes_shape)) {
    if (keep_dims) {
      return std::make_shared<abstract::Shape>(ShapeVector(input_shape.size(), -1));
    } else {
      return std::make_shared<abstract::Shape>(std::vector<int64_t>{-2});
    }
  }
  const int64_t min_dim = 0;
  const int64_t axes_dim = 1;
  (void)CheckAndConvertUtils::CheckInteger("the rank of input", SizeToLong(input_shape.size()), kGreaterEqual, min_dim,
                                           prim_name);
  auto output_shape = input_shape;
  auto input_rank = static_cast<int64_t>(input_shape.size());
  (void)CheckAndConvertUtils::CheckInteger("the rank of axes", SizeToLong(axes_shape.size()), kEqual, axes_dim,
                                           prim_name);
  MS_EXCEPTION_IF_NULL(input_args[kInputIndex1]);
  if (!input_args[kInputIndex1]->GetValue()->isa<ValueAny>() && !input_args[kInputIndex1]->GetValue()->isa<None>()) {
    auto axes_value = input_args[kInputIndex1]->GetValue();
    auto axes_type = input_args[kInputIndex1]->GetType();
    auto axes = CheckAndConvertUtils::CheckTensorIntValue("axes", axes_value, prim_name, axes_type);
    CheckAndConvertUtils::CheckInRange("axes size", axes.size(), kIncludeLeft, {0, input_rank + 1}, prim_name);
    ReduceAxes(&output_shape, &axes, input_rank, keep_dims, primitive);
  } else {
    if (keep_dims) {
      return std::make_shared<abstract::Shape>(ShapeVector(input_shape.size(), -1));
    } else {
      return std::make_shared<abstract::Shape>(std::vector<int64_t>{-2});
    }
  }

  return std::make_shared<abstract::Shape>(output_shape);
}

TypePtr EuclideanNormInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  auto x_type = input_args[kInputIndex0]->GetType();
  MS_EXCEPTION_IF_NULL(x_type);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x", x_type, common_valid_types_with_complex, prim_name);
  const std::set<TypePtr> axes_valid_types = {kInt64, kInt32};
  auto axes_type = input_args[kInputIndex1]->GetType();
  MS_EXCEPTION_IF_NULL(axes_type);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("axes", axes_type, axes_valid_types, prim_name);
  return x_type;
}
}  // namespace

MIND_API_OPERATOR_IMPL(EuclideanNorm, BaseOperator);
AbstractBasePtr EuclideanNormInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                   const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t kInputsNum = 2;
  auto prim_name = primitive->name();
  (void)CheckAndConvertUtils::CheckInteger("Input numbers", SizeToLong(input_args.size()), kEqual, kInputsNum,
                                           prim_name);
  auto type = EuclideanNormInferType(primitive, input_args);
  auto shape = EuclideanNormInferShape(primitive, input_args);
  return abstract::MakeAbstract(shape, type);
}

void EuclideanNorm::Init(const bool keep_dims) { this->set_keep_dims(keep_dims); }

void EuclideanNorm::set_keep_dims(const bool keep_dims) { (void)this->AddAttr(kKeepDims, api::MakeValue(keep_dims)); }

bool EuclideanNorm::get_keep_dims() const {
  auto value_ptr = this->GetAttr(kKeepDims);
  MS_EXCEPTION_IF_NULL(value_ptr);
  return GetValue<bool>(value_ptr);
}

// AG means auto generated
class MIND_API AGEuclideanNormInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return EuclideanNormInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return EuclideanNormInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return EuclideanNormInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {1}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(EuclideanNorm, prim::kPrimEuclideanNorm, AGEuclideanNormInfer, false);
}  // namespace ops
}  // namespace mindspore
