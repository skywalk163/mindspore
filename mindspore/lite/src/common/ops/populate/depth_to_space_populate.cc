/**
 * Copyright 2019-2021 Huawei Technologies Co., Ltd
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
#include "src/common/ops/populate/populate_register.h"
#include "nnacl/depth_to_space_parameter.h"
using mindspore::schema::PrimitiveType_DepthToSpace;

namespace mindspore {
namespace lite {
namespace {
OpParameter *PopulateDepthToSpaceParameter(const void *prim) {
  auto primitive = static_cast<const schema::Primitive *>(prim);
  MS_ASSERT(primitive != nullptr);
  auto value = primitive->value_as_DepthToSpace();
  if (value == nullptr) {
    MS_LOG(ERROR) << "value is nullptr";
    return nullptr;
  }

  auto *param = reinterpret_cast<DepthToSpaceParameter *>(malloc(sizeof(DepthToSpaceParameter)));
  if (param == nullptr) {
    MS_LOG(ERROR) << "malloc DepthToSpaceParameter failed.";
    return nullptr;
  }
  memset(param, 0, sizeof(DepthToSpaceParameter));

  param->mode_ = 0;
  if (value->mode() != nullptr) {
    std::string mode = value->mode()->c_str();
    if (mode == "CRD") {
      param->mode_ = 1;
    }
  }

  param->op_parameter_.type_ = primitive->value_type();
  param->block_size_ = value->block_size();

  if (param->block_size_ < C2NUM) {
    MS_LOG(ERROR) << "invalid block_size value: " << param->block_size_;
    free(param);
    return nullptr;
  }
  return reinterpret_cast<OpParameter *>(param);
}
}  // namespace

REG_POPULATE(PrimitiveType_DepthToSpace, PopulateDepthToSpaceParameter, SCHEMA_CUR)
}  // namespace lite
}  // namespace mindspore
