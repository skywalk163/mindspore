/**
 * Copyright 2021-2023 Huawei Technologies Co., Ltd
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
#include "ops/make_tuple.h"

#include <memory>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "base/base.h"
#include "ir/anf.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sequence_ops.h"
#include "ops/primitive_c.h"
#include "ops/real_maketuple.h"
#include "ops/fusion/make_tuple_v2.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
MIND_API_OPERATOR_IMPL(MakeTuple, BaseOperator);
MIND_API_OPERATOR_IMPL(RealMakeTuple, BaseOperator);
MIND_API_OPERATOR_IMPL(MakeTupleV2, BaseOperator);
AbstractBasePtr MakeTupleInnerInfer(const std::vector<AbstractBasePtr> &input_args) {
  return std::make_shared<abstract::AbstractTuple>(input_args);
}

class MakeTupleInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &, const std::vector<AbstractBasePtr> &input_args) const override {
    return MakeTupleInnerInfer(input_args)->GetShape();
  }

  TypePtr InferType(const PrimitivePtr &, const std::vector<AbstractBasePtr> &input_args) const override {
    return MakeTupleInnerInfer(input_args)->GetType();
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &, const PrimitivePtr &,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return MakeTupleInnerInfer(input_args);
  }
};
REGISTER_PRIMITIVE_OP_INFER_IMPL(MakeTuple, prim::kPrimMakeTuple, MakeTupleInfer, false);
REGISTER_PRIMITIVE_OP_INFER_IMPL(RealMakeTuple, prim::kPrimRealMakeTuple, MakeTupleInfer, false);
REGISTER_PRIMITIVE_OP_INFER_IMPL(MakeTupleV2, prim::kPrimMakeTupleV2, MakeTupleInfer, false);
}  // namespace ops
}  // namespace mindspore
