/**
 * Copyright 2020-2021 Huawei Technologies Co., Ltd
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

#include "ops/quant_dtype_cast.h"

#include <memory>
#include <set>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/base/type_id.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/framework_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
MIND_API_OPERATOR_IMPL(QuantDTypeCast, BaseOperator);
void QuantDTypeCast::set_src_t(const int64_t src_t) { (void)AddAttr(kSrcT, api::MakeValue(src_t)); }
int64_t QuantDTypeCast::get_src_t() const {
  auto value_ptr = this->GetAttr(kSrcT);
  return GetValue<int64_t>(value_ptr);
}
void QuantDTypeCast::set_dst_t(const int64_t dst_t) { (void)AddAttr(kDstT, api::MakeValue(dst_t)); }
int64_t QuantDTypeCast::get_dst_t() const { return GetValue<int64_t>(GetAttr(kDstT)); }
void QuantDTypeCast::set_axis(const int64_t axis) { (void)AddAttr(kAxis, api::MakeValue(axis)); }
int64_t QuantDTypeCast::get_axis() const { return GetValue<int64_t>(GetAttr(kAxis)); }

void QuantDTypeCast::Init(const int64_t src_t, const int64_t dst_t) {
  this->set_src_t(src_t);
  this->set_dst_t(dst_t);
}

class MIND_API AGQuantDTypeCastInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    const int64_t kInputNum = 1;
    CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, kInputNum, prim_name);
    auto x = input_args[0]->GetShape();
    (void)CheckAndConvertUtils::CheckArgsType(prim_name, input_args, 0, kObjectTypeTensorType);
    auto shape_element = x->cast<abstract::ShapePtr>();
    MS_EXCEPTION_IF_NULL(shape_element);
    return shape_element;
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    const int64_t kInputNum = 1;
    CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, kInputNum, prim_name);
    MS_EXCEPTION_IF_NULL(input_args[0]);
    auto x_type = input_args[0]->GetType();
    const std::set<TypePtr> valid_types = {kInt8, kFloat32};
    (void)CheckAndConvertUtils::CheckTensorTypeValid("input_x", x_type, valid_types, prim_name);
    auto type_ptr = mindspore::TypeIdToType(static_cast<TypeId>(GetValue<int64_t>(primitive->GetAttr(kDstT))));
    return type_ptr;
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(QuantDTypeCast, prim::kPrimQuantDTypeCast, AGQuantDTypeCastInfer, false);
}  // namespace ops
}  // namespace mindspore
