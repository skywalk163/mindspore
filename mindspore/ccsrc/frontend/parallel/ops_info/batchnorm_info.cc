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

#include "frontend/parallel/ops_info/batchnorm_info.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "utils/ms_context.h"
#include "ops/op_utils.h"
#include "frontend/parallel/device_matrix.h"
#include "frontend/parallel/dynamic_creator.h"
#include "frontend/parallel/strategy.h"
#include "frontend/parallel/tensor_layout/tensor_redistribution.h"
#include "pipeline/jit/ps/resource.h"

namespace mindspore {
namespace parallel {
Status BatchNormInfo::GetAttrs() {
  auto is_training_value = GetScalarValueFromInputsWithCheck<bool>(input_value_, name_, IS_TRAINING);

  auto epsilon_value = GetScalarValueFromInputsWithCheck<float>(input_value_, name_, EPSILON);

  auto momentum_value = GetScalarValueFromInputsWithCheck<float>(input_value_, name_, MOMENTUM);
  if (!is_training_value.has_value() || !epsilon_value.has_value() || !momentum_value.has_value()) {
    return FAILED;
  }
  is_training_ = is_training_value.value();
  epsilon_ = epsilon_value.value();
  momentum_ = momentum_value.value();
  auto attr_iter = attrs_.find(GROUP_SIZE);
  if (attr_iter != attrs_.end()) {
    group_size_ = GetIntAttr(GROUP_SIZE);
    if (group_size_ < 1 || group_size_ > stage_device_size_ ||
        ((static_cast<uint64_t>(group_size_) & static_cast<uint64_t>(group_size_ - 1)) != 0) ||
        stage_device_size_ % group_size_ != 0) {
      MS_LOG(ERROR) << name_ << ": The group size is out of range, it must be in [1, " << stage_device_size_
                    << "], it must be the power of 2, and it can divide all device num " << stage_device_size_
                    << ", but got " << group_size_;
      return FAILED;
    }
    MS_LOG(INFO) << name_ << ": The group size is " << group_size_;
  }

  auto format_int_opt = GetScalarValueFromInputsWithCheck<int64_t>(input_value_, name_, DATA_FORMAT);
  if (!format_int_opt.has_value()) {
    return FAILED;
  }
  auto format_int = format_int_opt.value();
  std::string format_string;
  if (format_int == 0) {
    format_string = "NCHW";
  } else if (format_int == 1) {
    format_string = "NHWC";
  } else {
    MS_LOG(ERROR) << name_ << ": The data format must be 0 or 1, but got " << format_int;
    return FAILED;
  }
  format_ = format_string;
  if (format_ != NCHW) {
    MS_LOG(ERROR) << name_ << ": The data format must be 'NCHW', but got " << format_;
    return FAILED;
  }

  if (inputs_shape_.empty()) {
    MS_LOG(ERROR) << name_ << ": The inputs shape is empty";
    return FAILED;
  }

  if (inputs_shape_[0].size() != 2 && inputs_shape_[0].size() != 4) {
    MS_LOG(ERROR) << name_ << ": The size of input[0]'s shape must be 2 or 4, but got " << inputs_shape_[0].size();
    return FAILED;
  }
  input_is_4d_ = (inputs_shape_[0].size() == 4);

  MS_LOG(INFO) << name_ << ": The is_traing is " << is_training_ << ", epsilon is " << epsilon_ << ", momentum is "
               << momentum_ << ", data format is " << format_;

  return SUCCESS;
}

Status BatchNormInfo::CheckStrategy(const StrategyPtr &strategy) {
  MS_EXCEPTION_IF_NULL(strategy);
  if (CheckStrategyValue(strategy, inputs_shape_) != SUCCESS) {
    MS_LOG(ERROR) << name_ << ": Invalid strategy";
    return FAILED;
  }

  std::vector<Dimensions> stra = strategy->GetInputDim();

  if (stra.size() != 5) {
    MS_LOG(ERROR) << name_ << ": The size of strategy must be 5, but got " << stra.size();
    return FAILED;
  }

  if ((stra[0].size() != 4) && (stra[0].size() != 2)) {
    MS_LOG(ERROR) << name_ << ": The size of strategy[0] must be 4 or 2, but got " << stra[0].size();
    return FAILED;
  }

  for (size_t i = 1; i < 5; ++i) {
    if (stra[i].empty()) {
      MS_LOG(ERROR) << name_ << ": The strategy can not be empty, the index is " << i;
      return FAILED;
    }
    if (stra[0][1] != stra[i][0]) {
      MS_LOG(ERROR) << name_ << ": Invalid strategy, the index is " << i << ", it must be equal to " << stra[0][1]
                    << ", but got " << stra[i][0];
      return FAILED;
    }
  }

  if (group_size_ > 0 && stra[0][0] != stage_device_size_) {
    MS_LOG(ERROR) << name_ << ": The config group size only support the N dimension is shard with device num";
    return FAILED;
  }
  return SUCCESS;
}

Status BatchNormInfo::InferDevMatrixShape() {
  MS_EXCEPTION_IF_NULL(strategy_);
  std::vector<Dimensions> stra = strategy_->GetInputDim();
  if (stra.empty()) {
    MS_LOG(ERROR) << name_ << ": The strategy can not be empty";
    return FAILED;
  }

  dev_matrix_shape_ = stra[0];
  return SUCCESS;
}

Status BatchNormInfo::InferMirrorOps() {
  if (OperatorInfo::InferMirrorOps() != SUCCESS) {
    return FAILED;
  }
  // No need to insert mirror ops
  if (mirror_ops_.empty()) {
    return SUCCESS;
  }

  // Add empty mirror ops
  for (size_t i = 0; i < kIndex4; i++) {
    (void)mirror_ops_.emplace_back(OperatorVector());
  }
  return SUCCESS;
}

Status BatchNormInfo::InferTensorMap() {
  TensorMap input_tensor_map;
  TensorMap in_other_tensor_map;

  if (input_is_4d_) {
    // if input is 4d:
    // input_strategy: ((n, c, h, w), (c), (c), (c), (c))
    // output_strategy: ((n, c, h, w), (c), (c), (c), (c))
    // dev_matrix: (n, c, h, w)
    input_tensor_map = {3, 2, 1, 0};
    in_other_tensor_map = {2};
  } else {
    // if input is 2d:
    // input_strategy: ((n, c), (c), (c), (c), (c))
    // output_strategy: ((n, c), (c), (c), (c), (c))
    // dev_matrix: (n, c)
    input_tensor_map = {1, 0};
    in_other_tensor_map = {0};
  }

  inputs_tensor_map_.push_back(input_tensor_map);     // input
  inputs_tensor_map_.push_back(in_other_tensor_map);  // scale
  inputs_tensor_map_.push_back(in_other_tensor_map);  // bias
  inputs_tensor_map_.push_back(in_other_tensor_map);  // mean
  inputs_tensor_map_.push_back(in_other_tensor_map);  // variance

  outputs_tensor_map_ = inputs_tensor_map_;
  return SUCCESS;
}

Status BatchNormInfo::InferAllReduceGroupBySize() {
  if (group_size_ == 1) {
    MS_LOG(INFO) << name_ << ": The group size is set to 1, no need forward allreduce";
    return SUCCESS;
  }

  CheckGlobalDeviceManager();
  int64_t rank = g_device_manager->global_rank();
  MS_EXCEPTION_IF_ZERO("group_size", group_size_);
  int64_t tmp = rank / group_size_;

  RankList group_rank_list;
  for (size_t i = 0; i < LongToSize(group_size_); ++i) {
    group_rank_list.push_back(tmp + SizeToLong(i));
  }
  MS_LOG(INFO) << name_ << ": The group rank list is " << group_rank_list;

  Group g;
  if (g_device_manager->CreateGroup(group_rank_list, &g) != SUCCESS) {
    MS_LOG(ERROR) << "The node " << cnode_->fullname_with_scope() << " create sync allreduce failed";
    return FAILED;
  }
  forward_allreduce_group_.push_back(g);
  return SUCCESS;
}

Status BatchNormInfo::InferForwardCommunication() {
  // if it is not training, no need forward allreduce
  if (!is_training_) {
    MS_LOG(INFO) << name_ << ": It is not training, no need forward allreduce";
    return SUCCESS;
  }

  forward_allreduce_group_.clear();
  if (group_size_ > 0) {
    return InferAllReduceGroupBySize();
  }

  TensorMap tmp_map;
  if (input_is_4d_) {
    // input is 4d:
    // if has not repeated calculation, the dev matrix is [n, c, h, w]
    // if repeated calculation and repeated num in the left of dev matrix, the dev matrix is [repeated_num, n, c, h, w]
    // if repeated calculation and repeated num in the right of dev matrix, the dev matrix is [n, c, h, w, repeated_num]
    // and the forward allreduce need to use the dimensions of n/h/w
    if (repeated_calc_num_ == 1) {
      // has not repeated calculation
      tmp_map = {-1, 2, -1, -1};
    } else if (!repeated_num_in_dev_matrix_right_) {
      // repeated calculation and repeated num in the left of dev matrix
      tmp_map = {4, -1, 2, -1, -1};
    } else {
      // repeated calculation and repeated num in the right of dev matrix
      tmp_map = {-1, 3, -1, -1, 0};
    }
  } else {
    // input is 2d:
    // if has not repeated calculation, the dev matrix is [n, c]
    // if repeated calculation and repeated num in the left of dev matrix, the dev matrix is [repeated_num, n, c]
    // if repeated calculation and repeated num in the right of dev matrix, the dev matrix is [n, c, repeated_num]
    // and the forward allreduce need to use the dimensions of n
    if (repeated_calc_num_ == 1) {
      // has not repeated calculation
      tmp_map = {-1, 0};
    } else if (!repeated_num_in_dev_matrix_right_) {
      // repeated calculation and repeated num in the left of dev matrix
      tmp_map = {2, -1, 0};
    } else {
      // repeated calculation and repeated num in the right of dev matrix
      tmp_map = {-1, 1, 0};
    }
  }

  std::vector<Group> group_list;
  if (CreateGroupByTensorMap(tmp_map, &group_list) != SUCCESS) {
    ReportError(name_ + ": Create group failed.");
    return FAILED;
  }

  if (group_list.empty()) {
    MS_LOG(INFO) << name_ << ": Forward all reduce is not required";
    return SUCCESS;
  } else {
    MS_LOG(INFO) << name_ << ": The group name of forward all reduce is " << group_list[0].name();
  }

  forward_allreduce_group_ = group_list;
  return SUCCESS;
}

void BatchNormInfo::InferReplaceOps() {
  replace_op_.clear();

  if (!is_training_) {
    MS_LOG(INFO) << name_ << ": It is not training, no need to replace op";
    return;
  }

  if (forward_allreduce_group_.empty()) {
    MS_LOG(INFO) << name_ << ": The forward allreduce group is empty, no need to replace op";
    return;
  }

  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  std::string backend = ms_context->get_param<std::string>(MS_CTX_DEVICE_TARGET);

  if (backend != kAscendDevice && backend != kDavinciDevice) {
    MS_LOG(INFO) << name_ << ": The backend is " << backend << ", it does not support SyncBatchNorm operator";
    return;
  }
  const size_t InputNumUpperBound = 5;
  auto prim_name = GetPrimNameFromInfoName(name_);
  if (ops::GetOpInputsNum(prim_name) > InputNumUpperBound) {
    MS_LOG(INFO) << name_ << ": The inputs num of " << prim_name << " is " << ops::GetOpInputsNum(prim_name)
                 << ", it does not support SyncBatchNorm operator";
    return;
  }

  ValuePtr epsilon = MakeValue(epsilon_);
  ValuePtr momentum = MakeValue(momentum_);
  ValuePtr group = MakeValue(forward_allreduce_group_[0].name());
  ValuePtr device_num = MakeValue(forward_allreduce_group_[0].GetDevNum());

  Attr attr_epsilon = std::make_pair(EPSILON, epsilon);
  Attr attr_momentum = std::make_pair(MOMENTUM, momentum);
  Attr attr_group = std::make_pair(GROUP, group);
  Attr attr_device_num = std::make_pair(DEVICE_NUM, device_num);

  OperatorAttrs attrs = {attr_epsilon, attr_momentum, attr_group, attr_device_num};
  OperatorParams params;
  OperatorArgs args = std::make_pair(attrs, params);
  replace_op_ = {std::make_pair(SYNC_BATCH_NORM, args)};
}

Status BatchNormInfo::InferAsLossDivisor() {
  if (outputs_tensor_map_.size() != 5) {
    MS_LOG(ERROR) << name_ << ": The size of outputs tensor map must be 5, but got " << outputs_tensor_map_.size();
    return FAILED;
  }
  as_loss_divisor_ = ComputeRepeatDeviceNumByTensorMap(dev_matrix_shape_, outputs_tensor_map_[0]);
  MS_LOG(INFO) << name_ << " : The dev matrix shape is " << ShapeToString(dev_matrix_shape_)
               << ", the output[0]'s tensor map is " << ShapeToString(outputs_tensor_map_[0])
               << ", as_loss_divisor_ is " << as_loss_divisor_;
  return SUCCESS;
}

Status BatchNormInfo::SetCostUnderStrategy(const StrategyPtr &strategy) { return SetCostUnderStrategyBase(strategy); }

std::vector<StrategyPtr> BatchNormInfo::GenerateOpStrategies(int64_t stage_id) {
  if (inputs_shape_.size() != BATCH_NORM_INPUTS_SIZE) {
    MS_LOG(EXCEPTION) << name_ << ": The inputs shape is invalid: " << inputs_shape_.size();
  }
  Shape input_split(inputs_shape_[0].size(), 1);

  // to generate the first input's strategy
  Shapes splittable_input = {input_split};
  Shapes tmp_inputs_shape = {inputs_shape_[0]};

  std::vector<StrategyPtr> sp_vector;
  if (GenerateStrategiesForIndependentInputs(stage_id, tmp_inputs_shape, splittable_input, &sp_vector) != SUCCESS) {
    MS_LOG(EXCEPTION) << name_ << ": Generate strategies failed";
  }

  // the others strategies are follow to the first input's strategy
  for (auto &sp : sp_vector) {
    if ((sp == nullptr) || sp->GetInputDim().empty()) {
      MS_LOG(EXCEPTION) << name_ << ": The strategy is null or empty";
    }
    Strategies tmp_strategy;
    Dimensions first_input_strategy = sp->GetInputDim()[0];
    if (first_input_strategy.size() < 2) {
      MS_LOG(EXCEPTION) << name_ << ": The size of first input strategy can not smaller than 2, but got "
                        << first_input_strategy.size();
    }
    Dimensions other_input_strategy = {first_input_strategy[1]};  // the strategy for 'C'
    tmp_strategy.push_back(first_input_strategy);
    for (size_t i = 1; i < inputs_shape_.size(); ++i) {
      tmp_strategy.push_back(other_input_strategy);
    }
    sp->ResetInputs(tmp_strategy);
  }

  return sp_vector;
}

REGISTER(BatchNormInfo);
}  // namespace parallel
}  // namespace mindspore
