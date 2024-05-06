/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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
#include "tools/graph_kernel/converter/split_model_cpu.h"
#include <memory>
#include "utils/ms_context.h"

namespace mindspore::graphkernel::inner {
constexpr size_t kReduceFusionDepth = 20;
constexpr size_t kBroadcastFusionDepth = 20;

class FuseConv : public FusePattern {
 public:
  FuseConv() : FusePattern("conv") { direction_ = FuseDirection::BACKWARD; }
  ~FuseConv() = default;

 protected:
  bool Check(const AreaPtr &dom) override {
    if (dom->dom()->op() != "Conv2D") {
      return false;
    }
    return true;
  }
  bool Match(const AreaPtr &dom) override {
    for (auto d : dom->users_with_relation()) {
      auto a = d.first;
      if (HasCircle(dom, a)) {
        continue;
      }
      if (a->pattern() < NodePattern::BROADCAST ||
          (a->pattern() == NodePattern::BROADCAST && a->dom()->shape == dom->dom()->shape)) {
        (void)fused_areas_.emplace_back(a);
      }
    }
    return !fused_areas_.empty();
  }
};

void SplitModelCpu::InitFusePatterns() {
  AddPattern(std::make_shared<FuseVirtualNode>(), true);
  AddPattern(std::make_shared<FuseReshape>(), true);
  AddPattern(FuseElemwiseFwd::CreateDepthMatcher(), true);
  AddPattern(FuseElemwiseFwd::CreateWidthMatcher(), true);
  AddPattern(std::make_shared<FuseConv>(), true);
  AddPattern(FuseElemwiseBroadcastFwd::CreateDepthMatcher(), true);
  AddPattern(FuseElemwiseBroadcastFwd::CreateWidthMatcher(), true);
  AddPattern(FuseReduceFwd::CreateDepthMatcher(kReduceFusionDepth), true);
  AddPattern(FuseReduceFwd::CreateWidthMatcher(kReduceFusionDepth), true);
  AddPattern(FuseElemwiseBroadcastBwd::CreateDepthMatcher(kBroadcastFusionDepth), true);
  AddPattern(FuseElemwiseBroadcastBwd::CreateWidthMatcher(kBroadcastFusionDepth), true);
  AddPattern(std::make_shared<FuseIsolateReshape>(), true);
}

AreaMode SplitModelCpu::GetDefaultAreaMode(const PrimOpPtr &) const { return AreaMode::COMPOSITE; }
}  // namespace mindspore::graphkernel::inner
