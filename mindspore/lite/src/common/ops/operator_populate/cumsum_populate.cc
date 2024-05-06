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
#include "src/common/ops/operator_populate/operator_populate_register.h"
#include "nnacl/cumsum_parameter.h"
#include "ops/auto_generate/gen_lite_ops.h"
using mindspore::ops::kNameCumSum;
using mindspore::schema::PrimitiveType_CumSum;
namespace mindspore {
namespace lite {
OpParameter *PopulateCumSumOpParameter(const BaseOperatorPtr &base_operator) {
  auto param = reinterpret_cast<CumSumParameter *>(PopulateOpParameter<CumSumParameter>(base_operator));
  if (param == nullptr) {
    MS_LOG(ERROR) << "new CumSumParameter failed.";
    return nullptr;
  }
  auto op = dynamic_cast<ops::CumSum *>(base_operator.get());
  if (op == nullptr) {
    MS_LOG(ERROR) << "base_operator cast to CumSum failed";
    free(param);
    return nullptr;
  }
  param->exclusive_ = op->get_exclusive();
  param->reverse_ = op->get_reverse();
  return reinterpret_cast<OpParameter *>(param);
}

REG_OPERATOR_POPULATE(kNameCumSum, PrimitiveType_CumSum, PopulateCumSumOpParameter)
}  // namespace lite
}  // namespace mindspore
