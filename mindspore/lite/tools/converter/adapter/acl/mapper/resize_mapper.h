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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_ADAPTER_ACL_MAPPER_RESIZE_MAPPER_H_
#define MINDSPORE_LITE_TOOLS_CONVERTER_ADAPTER_ACL_MAPPER_RESIZE_MAPPER_H_

#include "tools/converter/adapter/acl/mapper/primitive_mapper.h"
#include "ops/resize.h"

namespace mindspore {
namespace lite {
using mindspore::ops::kNameResize;

class ResizeMapper : public PrimitiveMapper {
 public:
  ResizeMapper() : PrimitiveMapper(kNameResize) {}

  ~ResizeMapper() override = default;

  STATUS Mapper(const CNodePtr &cnode) override;

 private:
  STATUS ProcScaleInput(const CNodePtr &cnode, const PrimitivePtr &prim);
  STATUS CalResizeShape(const CNodePtr &cnode, const PrimitivePtr &prim);
};
}  // namespace lite
}  // namespace mindspore
#endif  // MINDSPORE_LITE_TOOLS_CONVERTER_ADAPTER_ACL_MAPPER_RESIZE_MAPPER_H_
