/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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

#include <memory>
#include <set>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "base/base.h"
#include "include/common/utils/utils.h"
#include "ir/anf.h"
#include "ir/dtype.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "ir/tensor.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/nn_ops.h"
#include "ops/embedding_lookup.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
MIND_API_OPERATOR_IMPL(EmbeddingLookup, BaseOperator);
void EmbeddingLookup::Init(const bool setattr_flag) { this->set_setattr_flag(setattr_flag); }

void EmbeddingLookup::set_setattr_flag(const bool setattr_flag) {
  (void)this->AddAttr(kSetattrFlag, api::MakeValue(setattr_flag));
}

bool EmbeddingLookup::get_setattr_flag() const {
  auto value_ptr = GetAttr(kSetattrFlag);
  return GetValue<bool>(value_ptr);
}

void EmbeddingLookup::set_offset(const int64_t offset) { (void)this->AddAttr(kAxis, api::MakeValue(offset)); }

int64_t EmbeddingLookup::get_offset() {
  PrimitivePtr prim = this->GetPrim();
  MS_EXCEPTION_IF_NULL(prim);
  int64_t offset = 0;
  if (prim->HasAttr(kAttrOffset)) {
    auto value_ptr = prim->GetAttr(kAttrOffset);
    if (value_ptr->isa<tensor::Tensor>()) {
      auto off_vec = CheckAndConvertUtils::CheckTensorIntValue(kAttrOffset, value_ptr, prim->name());
      MS_LOG(EXCEPTION) << "For '" << prim->name() << "', offset must be int, bug got " << off_vec;
      offset = off_vec[0];
    } else {
      offset = GetValue<int64_t>(value_ptr);
    }
  }
  return offset;
}

class EmbeddingLookupInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    const std::string &op_name = primitive->name();
    auto params_shape_ptr = CheckAndConvertUtils::GetTensorInputShape(op_name, input_args, kInputIndex0);
    MS_EXCEPTION_IF_NULL(params_shape_ptr);
    auto params_shape = params_shape_ptr->shape();
    constexpr int64_t kEmbeddingLookupInputParamsMaxDim = 2;
    CheckAndConvertUtils::CheckInRange<int64_t>("dimension of params", SizeToLong(params_shape.size()), kIncludeBoth,
                                                {1, kEmbeddingLookupInputParamsMaxDim}, op_name);
    auto indices_shape_ptr = CheckAndConvertUtils::GetTensorInputShape(op_name, input_args, kInputIndex1);
    MS_EXCEPTION_IF_NULL(indices_shape_ptr);
    auto indices_shape = indices_shape_ptr->shape();
    (void)CheckAndConvertUtils::CheckValue<int64_t>("dimension of indices ", SizeToLong(indices_shape.size()),
                                                    kGreaterThan, 0, op_name);

    ShapeVector out_shape;
    if (!params_shape_ptr->IsDimUnknown() && !indices_shape_ptr->IsDimUnknown()) {
      out_shape = indices_shape;
      if (params_shape.size() != 1) {
        out_shape.push_back(params_shape.back());
      }
    } else {
      out_shape.push_back(abstract::Shape::kShapeRankAny);
    }
    return std::make_shared<abstract::Shape>(out_shape);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    const std::string &op_name = primitive->name();
    constexpr size_t input_num = 3;
    (void)CheckAndConvertUtils::CheckValue<size_t>("inputs number", input_args.size(), kEqual, input_num, op_name);
    std::set<TypePtr> valid_params_types = {kTensorType};
    MS_EXCEPTION_IF_NULL(input_args[kInputIndex0]);
    (void)CheckAndConvertUtils::CheckSubClass("params", input_args[kInputIndex0]->GetType(), valid_params_types,
                                              op_name);
    std::set<TypePtr> int_types = {kInt32, kInt64};
    MS_EXCEPTION_IF_NULL(input_args[kInputIndex1]);
    (void)CheckAndConvertUtils::CheckTensorTypeValid("indices", input_args[kInputIndex1]->GetType(), int_types,
                                                     op_name);
    std::set<TypePtr> int_type = {kInt32, kInt64};
    MS_EXCEPTION_IF_NULL(input_args[kInputIndex2]);
    (void)CheckAndConvertUtils::CheckTypeValid("offset", input_args[kInputIndex2]->GetType(), int_type, op_name);

    CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, 0, op_name);
    auto params = CheckAndConvertUtils::CheckArgsType(op_name, input_args, 0, kObjectTypeTensorType);
    MS_EXCEPTION_IF_NULL(params);
    return params->GetType();
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(EmbeddingLookup, prim::kPrimEmbeddingLookup, EmbeddingLookupInfer, false);
}  // namespace ops
}  // namespace mindspore
