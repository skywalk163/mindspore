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

#ifndef MINDSPORE_CCSRC_FRONTEND_PARALLEL_OPS_INFO_PROMPT_K_V_CACHE_INFO_H_
#define MINDSPORE_CCSRC_FRONTEND_PARALLEL_OPS_INFO_PROMPT_K_V_CACHE_INFO_H_

#include <memory>
#include <string>
#include <vector>
#include "utils/hash_map.h"
#include "ir/value.h"
#include "frontend/parallel/ops_info/operator_info.h"
#include "frontend/parallel/strategy.h"

namespace mindspore {
namespace parallel {
/*
 * parallel class for PromptKVCache Primitive
 */
class PromptKVCacheInfo : public OperatorInfo {
 public:
  PromptKVCacheInfo(const std::string &name, const Shapes &inputs_shape, const Shapes &outputs_shape,
                    const PrimitiveAttrs &attrs)
      : OperatorInfo(name, inputs_shape, outputs_shape, attrs, std::make_shared<ActivationInfoCost>()) {}
  ~PromptKVCacheInfo() override = default;
  Status CheckStrategy(const StrategyPtr &strategy) override;
  std::vector<StrategyPtr> GenerateOpStrategies(int64_t stage_id) override { return {}; }
  Status SetCostUnderStrategy(const StrategyPtr &strategy) override { return SetCostUnderStrategyBase(strategy); }

 protected:
  Status GetAttrs() override { return SUCCESS; }
  Status InferForwardCommunication() { return SUCCESS; }
  Status InferTensorMap() override;
  Status InferDevMatrixShape() override;
  Status SetDims(const StrategyPtr &strategy);
  Status CheckStrategy3Dims(const Dimensions &strategy_cache, const Dimensions &strategy_update);
  Status CheckStrategy4Dims(const Dimensions &strategy_cache, const Dimensions &strategy_update);

 private:
  bool is_input_dims_4_ = true;
};
using PromptKVCacheInfoPtr = std::shared_ptr<PromptKVCacheInfo>;
}  // namespace parallel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_FRONTEND_PARALLEL_OPS_INFO_PRELU_INFO_H_
