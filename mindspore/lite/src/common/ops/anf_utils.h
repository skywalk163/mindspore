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
#ifndef MINDSPORE_LITE_SRC_COMMON_OPS_ANF_UTILS_H_
#define MINDSPORE_LITE_SRC_COMMON_OPS_ANF_UTILS_H_

#include <memory>
#include "src/common/ops/ops_utils.h"
#ifdef PRIMITIVE_WRITEABLE
#include "abstract/ops/primitive_infer_map.h"
namespace mindspore {
namespace lite {
std::unique_ptr<schema::PrimitiveT> GetPrimitiveT(const mindspore::AnfNodePtr &node);
}
}  // namespace mindspore
#endif
#endif  // MINDSPORE_LITE_SRC_COMMON_OPS_ANF_UTILS_H_
