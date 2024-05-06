/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "tools/converter/adapter/acl/mapper/concatv2_mapper.h"
#include <memory>
#include "tools/converter/adapter/acl/mapper/primitive_mapper_register.h"
#include "src/common/log_util.h"
#include "mindspore/core/ops/op_name.h"

namespace mindspore {
namespace lite {
const auto kNameConcatV2 = "ConcatV2";

namespace {
constexpr int64_t kInputMinNum = 2;
}  // namespace

STATUS ConcatV2Mapper::Mapper(const CNodePtr &cnode) {
  if (AddAttrForDynInputPrimitive(cnode) != RET_OK) {
    MS_LOG(ERROR) << "ConcatV2 mapper failed.";
    return RET_ERROR;
  }
  return RET_OK;
}

REGISTER_PRIMITIVE_MAPPER(kNameConcatV2, ConcatV2Mapper)
}  // namespace lite
}  // namespace mindspore
