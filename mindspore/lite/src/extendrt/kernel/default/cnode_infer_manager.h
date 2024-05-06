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

#ifndef MINDSPORE_LITE_EXTENDRT_KERNEL_DEFAULT_CNODE_INFER_MANAGER_H_
#define MINDSPORE_LITE_EXTENDRT_KERNEL_DEFAULT_CNODE_INFER_MANAGER_H_
#include <vector>
#include "mindspore/core/ir/anf.h"
#include "src/litert/inner_context.h"

namespace mindspore {
namespace kernel {
int CNodeInferShape(const CNodePtr &cnode, const std::vector<lite::Tensor *> &outputs);
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_LITE_EXTENDRT_KERNEL_DEFAULT_CNODE_INFER_MANAGER_H_
