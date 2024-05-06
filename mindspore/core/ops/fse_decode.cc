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

#include "ops/fse_decode.h"
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/lite_ops.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
MIND_API_OPERATOR_IMPL(FSEDecode, BaseOperator);
void FSEDecode::set_dst_t(const int64_t dst_t) { (void)AddAttr(kDstT, api::MakeValue(dst_t)); }
int64_t FSEDecode::get_dst_t() const { return GetValue<int64_t>(GetAttr(kDstT)); }

void FSEDecode::set_curr_chunk(const int64_t curr_chunk) { (void)AddAttr(KCurrChunk, api::MakeValue(curr_chunk)); }
int64_t FSEDecode::get_curr_chunk() const { return GetValue<int64_t>(GetAttr(KCurrChunk)); }

void FSEDecode::set_curr_chunk_index(const int64_t curr_chunk_index) {
  (void)AddAttr(KCurrChunkIndex, api::MakeValue(curr_chunk_index));
}
int64_t FSEDecode::get_curr_chunk_index() const { return GetValue<int64_t>(GetAttr(KCurrChunkIndex)); }

void FSEDecode::set_curr_bit_count(const int64_t curr_bit_count) {
  (void)AddAttr(KCurrBitCount, api::MakeValue(curr_bit_count));
}
int64_t FSEDecode::get_curr_bit_count() const { return GetValue<int64_t>(GetAttr(KCurrBitCount)); }

void FSEDecode::set_table_log(const int64_t table_log) { (void)AddAttr(KTableLog, api::MakeValue(table_log)); }
int64_t FSEDecode::get_table_log() const { return GetValue<int64_t>(GetAttr(KTableLog)); }

void FSEDecode::Init(const int64_t dst_t, const int64_t curr_chunk, const int64_t curr_chunk_index,
                     const int64_t curr_bit_count, const int64_t table_log) {
  this->set_dst_t(dst_t);
  this->set_curr_chunk(curr_chunk);
  this->set_curr_chunk_index(curr_chunk_index);
  this->set_curr_bit_count(curr_bit_count);
  this->set_table_log(table_log);
}

class MIND_API AGFSEDecodeInfer : public abstract::OpInferBase {
 public:
  // This is used for backend infer by kernel tensor.
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    (void)CheckAndConvertUtils::CheckArgsType(prim_name, input_args, 0, kObjectTypeTensorType);
    std::vector<int64_t> output_shape;
    auto input_y = input_args[kInputIndex6];
    MS_EXCEPTION_IF_NULL(input_y);
    if (CheckAndConvertUtils::IsTensor(input_y)) {
      abstract::ShapePtr y_shape = CheckAndConvertUtils::GetTensorInputShape(prim_name, input_args, 1);
      auto shape_value = y_shape->shape();
      if (shape_value.size() != 1) {
        MS_EXCEPTION(TypeError) << "For '" << prim_name
                                << "', the shape size must be 1, but got: " << shape_value.size() << ".";
      }
      if (MS_UNLIKELY(y_shape->IsDynamic())) {
        return std::make_shared<abstract::Shape>(ShapeVector{abstract::TensorShape::kShapeRankAny});
      }
      output_shape = GetShapeValue(primitive, input_y);
      return std::make_shared<abstract::TensorShape>(output_shape);
    } else {
      MS_EXCEPTION(TypeError) << "input_y must be AbstractTensor" << input_y;
    }
  }

  // This is used for backend infer by kernel tensor.
  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    auto dst_t = primitive->GetAttr(kDstT);
    return TypeIdToType(static_cast<TypeId>(GetValue<int64_t>(dst_t)));
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(FSEDecode, prim::kPrimFSEDecode, AGFSEDecodeInfer, false);
}  // namespace ops
}  // namespace mindspore
