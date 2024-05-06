/**
 * Copyright 2021-2023 Huawei Technologies Co., Ltd
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

#include "frontend/parallel/ops_info/random_distribute_info.h"

#include <memory>
#include <utility>
#include <vector>

#include "frontend/parallel/device_matrix.h"
#include "frontend/parallel/dynamic_creator.h"
#include "frontend/parallel/strategy.h"
#include "frontend/parallel/tensor_layout/tensor_redistribution.h"

namespace mindspore {
namespace parallel {
int64_t RandomDistributeInfo::SEED_NUM = 1;

Status RandomDistributeInfo::GetAttrs() {
  seed_ = GetIntAttr(SEED);
  if (seed_ < 0) {
    MS_LOG(ERROR) << name_ << ": Seed must be greater or equal to zero, bug got " << seed_;
    return FAILED;
  }
  seed2_ = GetIntAttr(SEED2);
  if (seed2_ < 0) {
    MS_LOG(ERROR) << name_ << ": Seed2 must be greater or equal to zero, bug got " << seed2_;
    return FAILED;
  }
  ResetInputsShape();
  return SUCCESS;
}

Status RandomDistributeInfo::CheckStrategy(const StrategyPtr &strategy) {
  MS_EXCEPTION_IF_NULL(strategy);
  if (CheckStrategyValue(strategy, inputs_shape_) != SUCCESS) {
    MS_LOG(ERROR) << name_ << ": Invalid strategy";
    return FAILED;
  }

  std::vector<Dimensions> stra = strategy->GetInputDim();
  if (stra.size() != 1) {
    MS_LOG(ERROR) << name_ << ": The size of strategy must be 1, but got " << stra.size();
    return FAILED;
  }
  return SUCCESS;
}

Status RandomDistributeInfo::CheckStrategyForDynamicShape(const StrategyPtr &) {
  MS_LOG(ERROR) << name_
                << ": it does not support dynamic shape, the output shape: " << ShapeToString(outputs_shape_[0]);
  return FAILED;
}

Status RandomDistributeInfo::InferDevMatrixShape() {
  MS_EXCEPTION_IF_NULL(strategy_);
  std::vector<Dimensions> stra = strategy_->GetInputDim();
  if (stra.empty()) {
    MS_LOG(ERROR) << name_ << ": The strategy is empty";
    return FAILED;
  }

  dev_matrix_shape_ = stra[0];
  return SUCCESS;
}

Status RandomDistributeInfo::InferTensorMap() {
  TensorMap input_tensor_map;
  TensorMap output_tensor_map;
  std::vector<Dimensions> stra = strategy_->GetInputDim();
  size_t size = stra[0].size();

  for (size_t i = 0; i < size; i++) {
    input_tensor_map.push_back(SizeToLong(size - i - 1));
    output_tensor_map.push_back(SizeToLong(size - i - 1));
  }

  (void)inputs_tensor_map_.emplace_back(std::move(input_tensor_map));
  (void)outputs_tensor_map_.emplace_back(std::move(output_tensor_map));
  return SUCCESS;
}

Status RandomDistributeInfo::SetCostUnderStrategy(const StrategyPtr &strategy) {
  return SetCostUnderStrategyBase(strategy);
}

std::vector<StrategyPtr> RandomDistributeInfo::GenerateOpStrategies(int64_t stage_id) {
  Shape input0_split(inputs_shape_[0].size(), 1);
  Shapes splittable_inputs = {input0_split};

  std::vector<StrategyPtr> sp_vector;
  if (GenerateStrategiesForIndependentInputs(stage_id, inputs_shape_, splittable_inputs, &sp_vector) != SUCCESS) {
    MS_LOG(EXCEPTION) << name_ << ": Generate strategies for independent inputs() failed.";
  }
  if (sp_vector.empty()) {
    MS_LOG(EXCEPTION) << name_ << ": No available strategy.";
  }
  return sp_vector;
}

void RandomDistributeInfo::UpdateShape(const CNodePtr &cnode) {
  MS_EXCEPTION_IF_NULL(cnode);
  auto input_node = cnode->input(1)->cast<ValueNodePtr>();
  std::vector<int64_t> input_shape = GetValue<std::vector<int64_t>>(input_node->value());
  std::vector<Dimensions> stra = strategy_->GetInputDim();
  for (size_t i = 0; i < stra[0].size(); i++) {
    input_shape[i] /= stra[0][i];
  }
  auto func_graph = cnode->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  auto manager = func_graph->manager();
  MS_EXCEPTION_IF_NULL(manager);
  ValuePtr new_shape = MakeValue(input_shape);
  AnfNodePtr val = NewValueNode(new_shape);
  cnode->set_input(kIndex1, val);
}

void RandomDistributeInfo::ReplaceNodeInputOrAttrs() {
  for (auto &cnode : cnodes_) {
    // Replace input 'shape' to slice shape
    UpdateShape(cnode);

    // Update seed according rank_id
    int64_t rank_id = g_device_manager->rank_index_in_stage();
    int64_t seed_bias = 0;
    // When seed and seed2 are both 0, ensure that the 0th card in each group has the same result
    if (seed_ == 0 && seed2_ == 0) {
      seed_bias += SEED_NUM;
      ++SEED_NUM;
    }
    MS_EXCEPTION_IF_ZERO("repeated_calc_num_", repeated_calc_num_);
    if (repeated_num_in_dev_matrix_right_) {
      seed_bias += rank_id / repeated_calc_num_;
    } else {
      int64_t device_num = stage_device_size_;
      MS_EXCEPTION_IF_ZERO("device_num", device_num);
      seed_bias += rank_id % (device_num / repeated_calc_num_);
    }

    auto prim = GetValueNode<PrimitivePtr>(cnode->input(0));
    prim->set_attr(SEED, MakeValue(seed_ + seed_bias));
    prim->set_attr(SEED2, MakeValue(seed2_ + seed_bias));
  }
}

void RandomDistributeInfo::ResetInputsShape() {
  ValueTuplePtr shape_value = input_value_[0]->cast<ValueTuplePtr>();
  MS_EXCEPTION_IF_NULL(shape_value);
  inputs_shape_.push_back(GetValue<Shape>(shape_value));
  is_parameter_.push_back(false);
}

Status RandomDistributeInfo::InferMirrorOps() { return SUCCESS; }

REGISTER2(UniformRealInfo, RandomDistributeInfo);
REGISTER2(StandardNormalInfo, RandomDistributeInfo);
}  // namespace parallel
}  // namespace mindspore
