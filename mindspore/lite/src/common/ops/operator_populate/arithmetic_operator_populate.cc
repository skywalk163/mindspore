/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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
#include "src/common/ops/operator_populate/arithmetic_operator_populate.h"
#include <memory>
#include <string>
#include "ops/auto_generate/gen_lite_ops.h"
#include "ops/squared_difference.h"
#include "ops/mod.h"
#include "ops/fusion/add_fusion.h"
#include "ops/fusion/mul_fusion.h"
#include "ops/eltwise.h"
#include "ops/fusion/div_fusion.h"
#include "ops/fusion/sub_fusion.h"

using mindspore::ops::kActivationType;
using mindspore::ops::kMode;
using mindspore::ops::kNameAdd;
using mindspore::ops::kNameAddFusion;
using mindspore::ops::kNameBiasAdd;
using mindspore::ops::kNameDiv;
using mindspore::ops::kNameDivFusion;
using mindspore::ops::kNameEltwise;
using mindspore::ops::kNameEqual;
using mindspore::ops::kNameFloorDiv;
using mindspore::ops::kNameFloorMod;
using mindspore::ops::kNameGreater;
using mindspore::ops::kNameGreaterEqual;
using mindspore::ops::kNameLess;
using mindspore::ops::kNameLessEqual;
using mindspore::ops::kNameLogicalAnd;
using mindspore::ops::kNameLogicalOr;
using mindspore::ops::kNameMaximum;
using mindspore::ops::kNameMinimum;
using mindspore::ops::kNameMod;
using mindspore::ops::kNameMul;
using mindspore::ops::kNameMulFusion;
using mindspore::ops::kNameNotEqual;
using mindspore::ops::kNameRealDiv;
using mindspore::ops::kNameSquaredDifference;
using mindspore::ops::kNameSub;
using mindspore::ops::kNameSubFusion;

using mindspore::schema::PrimitiveType_AddFusion;
using mindspore::schema::PrimitiveType_BiasAdd;
using mindspore::schema::PrimitiveType_DivFusion;
using mindspore::schema::PrimitiveType_Eltwise;
using mindspore::schema::PrimitiveType_Equal;
using mindspore::schema::PrimitiveType_FloorDiv;
using mindspore::schema::PrimitiveType_FloorMod;
using mindspore::schema::PrimitiveType_Greater;
using mindspore::schema::PrimitiveType_GreaterEqual;
using mindspore::schema::PrimitiveType_Less;
using mindspore::schema::PrimitiveType_LessEqual;
using mindspore::schema::PrimitiveType_LogicalAnd;
using mindspore::schema::PrimitiveType_LogicalOr;
using mindspore::schema::PrimitiveType_Maximum;
using mindspore::schema::PrimitiveType_Minimum;
using mindspore::schema::PrimitiveType_Mod;
using mindspore::schema::PrimitiveType_MulFusion;
using mindspore::schema::PrimitiveType_NotEqual;
using mindspore::schema::PrimitiveType_RealDiv;
using mindspore::schema::PrimitiveType_SquaredDifference;
using mindspore::schema::PrimitiveType_SubFusion;

namespace mindspore {
namespace lite {
OpParameter *PopulateArithmeticCommonOpPara(const BaseOperatorPtr &base_operator) {
  auto param = reinterpret_cast<ArithmeticParameter *>(PopulateOpParameter<ArithmeticParameter>(base_operator));
  if (param == nullptr) {
    MS_LOG(ERROR) << "new ArithmeticParameter failed.";
    return nullptr;
  }
  param->broadcasting_ = false;
  param->ndim_ = 0;
  param->activation_type_ = 0;
  return reinterpret_cast<OpParameter *>(param);
}

OpParameter *PopulateArithmeticFusionOpParameter(const BaseOperatorPtr &base_operator) {
  ArithmeticParameter *param = reinterpret_cast<ArithmeticParameter *>(PopulateArithmeticCommonOpPara(base_operator));
  if (param == nullptr) {
    MS_LOG(ERROR) << "PopulateArithmeticCommonOpPara failed.";
    return nullptr;
  }
  mindspore::ValuePtr attr = base_operator->GetPrim()->GetAttr(kActivationType);
  if (attr != nullptr) {
    param->activation_type_ = static_cast<int>(ActivationType(GetValue<int64_t>(attr)));
  }
  return reinterpret_cast<OpParameter *>(param);
}

OpParameter *PopulateEltwiseOpParameter(const BaseOperatorPtr &base_operator) {
  ArithmeticParameter *param = reinterpret_cast<ArithmeticParameter *>(PopulateArithmeticCommonOpPara(base_operator));
  if (param == nullptr) {
    MS_LOG(ERROR) << "PopulateArithmeticCommonPara failed.";
    return nullptr;
  }
  mindspore::ValuePtr attr = base_operator->GetPrim()->GetAttr(kMode);
  if (attr == nullptr) {
    MS_LOG(ERROR) << "The attr(" << kMode << ") of operator(" << base_operator->name() << ") not exist";
    free(param);
    return nullptr;
  }
  param->eltwise_mode_ = GetValue<int64_t>(attr);
  return reinterpret_cast<OpParameter *>(param);
}

REG_OPERATOR_POPULATE(kNameAdd, PrimitiveType_AddFusion, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameAddFusion, PrimitiveType_AddFusion, PopulateArithmeticFusionOpParameter)
REG_OPERATOR_POPULATE(kNameBiasAdd, PrimitiveType_BiasAdd, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameRealDiv, PrimitiveType_RealDiv, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameLogicalAnd, PrimitiveType_LogicalAnd, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameLogicalOr, PrimitiveType_LogicalOr, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameEqual, PrimitiveType_Equal, PopulateArithmeticCommonOpPara);
REG_OPERATOR_POPULATE(kNameNotEqual, PrimitiveType_NotEqual, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameLess, PrimitiveType_Less, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameLessEqual, PrimitiveType_LessEqual, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameGreater, PrimitiveType_Greater, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameGreaterEqual, PrimitiveType_GreaterEqual, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameMaximum, PrimitiveType_Maximum, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameMinimum, PrimitiveType_Minimum, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameFloorDiv, PrimitiveType_FloorDiv, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameFloorMod, PrimitiveType_FloorMod, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameMod, PrimitiveType_Mod, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameMul, PrimitiveType_MulFusion, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameMulFusion, PrimitiveType_MulFusion, PopulateArithmeticFusionOpParameter)
REG_OPERATOR_POPULATE(kNameSquaredDifference, PrimitiveType_SquaredDifference, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameEltwise, PrimitiveType_Eltwise, PopulateEltwiseOpParameter)
REG_OPERATOR_POPULATE(kNameDiv, PrimitiveType_DivFusion, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameDivFusion, PrimitiveType_DivFusion, PopulateArithmeticFusionOpParameter)
REG_OPERATOR_POPULATE(kNameSub, PrimitiveType_SubFusion, PopulateArithmeticCommonOpPara)
REG_OPERATOR_POPULATE(kNameSubFusion, PrimitiveType_SubFusion, PopulateArithmeticFusionOpParameter)
}  // namespace lite
}  // namespace mindspore
