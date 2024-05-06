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

#ifndef MINDSPORE_COMMON_UNIFY_MINDIR_H
#define MINDSPORE_COMMON_UNIFY_MINDIR_H
#include <memory>
#include "include/backend/kernel_graph.h"
#include "include/backend/optimizer/pass_manager.h"
namespace mindspore {
namespace opt {
void GetBackendCommonUnifyMindIRPassManager(PassManagerPtr *unify_mindir_pm);
void AscendUnfoldInputsForSpecialNodes(const KernelGraphPtr &kernel_graph);
}  // namespace opt
}  // namespace mindspore

#endif  // MINDSPORE_COMMON_UNIFY_MINDIR_H
