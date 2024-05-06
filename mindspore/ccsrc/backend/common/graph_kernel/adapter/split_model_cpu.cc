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
#include "backend/common/graph_kernel/adapter/split_model_cpu.h"
#include <memory>
#include "utils/ms_context.h"
#include "ops/array_op_name.h"
#include "ops/nn_optimizer_op_name.h"

namespace mindspore::graphkernel::inner {
constexpr size_t kReduceFusionDepth = 20;
constexpr size_t kBroadcastFusionDepth = 20;

void SplitModelCpu::InitFusePatterns() {
  AddPattern(std::make_shared<FuseVirtualNode>(), true);
  AddPattern(std::make_shared<FuseReshape>(), true);
  AddPattern(FuseElemwiseBroadcastFwd::CreateDepthMatcher(), true);
  AddPattern(FuseElemwiseBroadcastFwd::CreateWidthMatcher(), true);
  AddPattern(FuseReduceFwd::CreateDepthMatcher(kReduceFusionDepth), true);
  AddPattern(FuseReduceFwd::CreateWidthMatcher(kReduceFusionDepth), true);
  AddPattern(FuseElemwiseBroadcastBwd::CreateDepthMatcher(kBroadcastFusionDepth), true);
  AddPattern(FuseElemwiseBroadcastBwd::CreateWidthMatcher(kBroadcastFusionDepth), true);
  AddPattern(std::make_shared<FuseIsolateReshape>(), true);
}

AreaMode SplitModelCpu::GetDefaultAreaMode(const PrimOpPtr &node) const {
  if (node != nullptr) {
    if (node->op() == kReshapeOpName || node->op() == kAssignOpName) {
      return AreaMode::BASIC;
    }
  }
  return AreaMode::COMPOSITE;
}
}  // namespace mindspore::graphkernel::inner
