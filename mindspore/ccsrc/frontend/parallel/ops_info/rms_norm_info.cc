/**
 * Copyright 2024 Huawei Technologies Co., Ltd
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

#include "frontend/parallel/ops_info/rms_norm_info.h"

#include <algorithm>
#include <vector>

#include "frontend/parallel/device_matrix.h"
#include "frontend/parallel/dynamic_creator.h"
#include "frontend/parallel/strategy.h"

namespace mindspore {
namespace parallel {
Status RmsNormInfo::GetAttrs() {
  // RmsNorm always run in last dim
  int64_t dim = SizeToLong(inputs_shape_[0].size());
  // begin_norm_axis_ will be the last axis
  begin_norm_axis_ = LongToSize(dim) - 1;
  return SUCCESS;
}

Status RmsNormInfo::CheckStrategy(const StrategyPtr &strategy) {
  MS_EXCEPTION_IF_NULL(strategy);
  Strategies stra = strategy->GetInputDim();
  if (stra.size() != RMS_NORM_INPUT_SIZE) {
    MS_LOG(ERROR) << name_ << ": Invalid strategy size " << stra.size();
    return FAILED;
  }

  if (CheckStrategyValue(strategy, inputs_shape_) != SUCCESS) {
    MS_LOG(ERROR) << name_ << ": Invalid strategy value";
    return FAILED;
  }

  Dimensions input_strategy = stra[RMS_NORM_INPUT_INDEX];
  Dimensions gamma_strategy = stra[RMS_NORM_GAMMA_INDEX];
  // check input strategy
  for (size_t i = begin_norm_axis_; i < input_strategy.size(); ++i) {
    if (input_strategy[i] != NO_SPLIT_STRATEGY) {
      MS_LOG(ERROR) << name_ << ": Invalid input strategy " << ShapeToString(input_strategy);
      return FAILED;
    }
  }
  // check gamma  strategy
  if ((gamma_strategy.size() > input_strategy.size())) {
    MS_LOG(ERROR) << name_ << " : The strategy size of gamma is lager than input strategy";
    return FAILED;
  }

  size_t gamma_diff = input_strategy.size() - gamma_strategy.size();
  for (size_t j = 0; j < gamma_strategy.size(); ++j) {
    if (gamma_strategy[j] != input_strategy[gamma_diff + j]) {
      MS_LOG(ERROR) << name_ << ": Invalid gamma strategy " << ShapeToString(gamma_strategy);
      return FAILED;
    }
  }

  return SUCCESS;
}

Status RmsNormInfo::InferDevMatrixShape() {
  if (strategy_ == nullptr) {
    MS_LOG(ERROR) << name_ << ": The strategy is null";
    return FAILED;
  }
  Strategies stra = strategy_->GetInputDim();
  if (stra.empty()) {
    MS_LOG(ERROR) << name_ << ": The strategy is empty";
    return FAILED;
  }
  dev_matrix_shape_ = stra[0];
  return SUCCESS;
}

Status RmsNormInfo::CreateInputTensorMap(size_t input_index) {
  if (inputs_shape_.size() <= input_index) {
    MS_LOG(ERROR) << name_ << ": Invalid index" << input_index;
    return FAILED;
  }
  Shape shape = inputs_shape_[input_index];
  Shape tensor_map;
  for (size_t i = 0; i < shape.size(); ++i) {
    tensor_map.push_back(SizeToLong(shape.size() - i - 1));
  }
  inputs_tensor_map_.push_back(tensor_map);
  return SUCCESS;
}

Status RmsNormInfo::InferTensorMap() {
  if ((CreateInputTensorMap(RMS_NORM_INPUT_INDEX) != SUCCESS) ||
      (CreateInputTensorMap(RMS_NORM_GAMMA_INDEX) != SUCCESS)) {
    MS_LOG(ERROR) << name_ << ": Create input tensor map failed";
    return FAILED;
  }

  Shape first_output_tensor_map = inputs_tensor_map_[0];
  Shape second_output_tensor_map = first_output_tensor_map;
  for (size_t i = begin_norm_axis_; i < second_output_tensor_map.size(); ++i) {
    second_output_tensor_map[i] = MAP_NONE;
  }

  outputs_tensor_map_.push_back(first_output_tensor_map);
  outputs_tensor_map_.push_back(second_output_tensor_map);
  return SUCCESS;
}

Status RmsNormInfo::InferAsLossDivisor() {
  if (outputs_tensor_map_.size() != RMS_NORM_INPUT_SIZE) {
    MS_LOG(ERROR) << name_ << ": The size of outputs tensor map " << outputs_tensor_map_.size() << " is error";
    return FAILED;
  }
  as_loss_divisor_ = ComputeRepeatDeviceNumByTensorMap(dev_matrix_shape_, outputs_tensor_map_[0]);
  MS_LOG(INFO) << name_ << " : The dev matrix shape is " << ShapeToString(dev_matrix_shape_)
               << ", the output[0]'s tensor map is " << ShapeToString(outputs_tensor_map_[0])
               << ", as_loss_divisor_ is " << as_loss_divisor_;
  return SUCCESS;
}

Status RmsNormInfo::SetCostUnderStrategy(const StrategyPtr &strategy) { return SetCostUnderStrategyBase(strategy); }

Status RmsNormInfo::GenerateGammaStrategies(const std::vector<StrategyPtr> &sp_vector) {
  if ((gamma_shape_.size() > input_shape_.size())) {
    MS_LOG(ERROR) << name_ << ": The dimension of gamma is lager than input";
    return FAILED;
  }

  size_t gamma_diff = input_shape_.size() - gamma_shape_.size();
  for (auto &sp : sp_vector) {
    if ((sp == nullptr) || sp->GetInputDim().empty()) {
      MS_LOG(ERROR) << name_ << ": Invalid strategy";
      return FAILED;
    }
    Strategies tmp_strategy;
    Dimensions input_strategy = sp->GetInputDim()[0];
    Dimensions gamma_strategy = input_strategy;
    (void)gamma_strategy.erase(gamma_strategy.cbegin(),
                               gamma_strategy.cbegin() + static_cast<different_type>(gamma_diff));

    // reset the strategy
    tmp_strategy.push_back(input_strategy);
    tmp_strategy.push_back(gamma_strategy);
    sp->ResetInputs(tmp_strategy);
  }
  return SUCCESS;
}

std::vector<StrategyPtr> RmsNormInfo::GenerateOpStrategies(int64_t stage_id) {
  if (InitShapes() != SUCCESS) {
    MS_LOG(EXCEPTION) << name_ << ": Init shapes failed";
  }
  Shape input_split(input_shape_.size(), SPLIT_FLAG);
  if (begin_norm_axis_ >= input_split.size()) {
    MS_LOG(EXCEPTION) << name_ << ": Invalid begin norm axis " << begin_norm_axis_;
  }

  // Can not split the dimensions from begin norm axis
  for (size_t i = begin_norm_axis_; i < input_split.size(); ++i) {
    input_split[i] = NO_SPLIT_FLAG;
  }

  // Generate strategy for input
  Shapes splittable_inputs = {input_split};
  Shapes tmp_inputs_shape = {input_shape_};
  std::vector<StrategyPtr> sp_vector;
  if (GenerateStrategiesForIndependentInputs(stage_id, tmp_inputs_shape, splittable_inputs, &sp_vector) != SUCCESS) {
    MS_LOG(EXCEPTION) << name_ << ": Generate input strategy failed";
  }

  // Generate the strategies for gamma and beta
  if (GenerateGammaStrategies(sp_vector) != SUCCESS) {
    MS_LOG(EXCEPTION) << name_ << ": Generate gamma and beta strategies failed";
  }

  return sp_vector;
}

Status RmsNormInfo::InitShapes() {
  if (inputs_shape_.size() != RMS_NORM_INPUT_SIZE) {
    MS_LOG(ERROR) << name_ << ": Invalid inputs size";
    return FAILED;
  }
  input_shape_ = inputs_shape_[RMS_NORM_INPUT_INDEX];
  gamma_shape_ = inputs_shape_[RMS_NORM_GAMMA_INDEX];
  return SUCCESS;
}

Status RmsNormInfo::CheckInputLayout() {
  // Check all device matrix should be the same
  if (inputs_tensor_info_.size() != kSizeTwo) {
    MS_LOG(ERROR) << "The size of input_tensor_layout for rmsnorm is " << inputs_tensor_info_.size()
                  << " rather than 2.";
    return FAILED;
  }
  auto in_layout = inputs_tensor_info_[kIndex0].tensor_layout();
  auto gamma_layout = inputs_tensor_info_[kIndex1].tensor_layout();

  // check input layout
  // [begin_norm_axis_, -1] should not shard after begin_norm_axis
  const std::vector<int64_t> np_split_map = {-1};
  for (size_t i = begin_norm_axis_; i < in_layout.tensor_map_before().size(); ++i) {
    if (in_layout.tensor_map_before()[i] != np_split_map) {
      MS_LOG(ERROR) << "RmsNorm Invalid input layout " << in_layout.tensor_map_before();
      return FAILED;
    }
  }

  size_t gamma_diff = in_layout.tensor_map_before().size() - gamma_layout.tensor_map_before().size();
  for (size_t j = 0; j < gamma_layout.tensor_map_before().size(); ++j) {
    if (gamma_layout.tensor_map_before()[j] != in_layout.tensor_map_before()[gamma_diff + j]) {
      MS_LOG(ERROR) << "RmsNorm Invalid gamma layout " << gamma_layout.tensor_map_before();
      return FAILED;
    }
  }

  return SUCCESS;
}

Status RmsNormInfo::CheckOutputLayout() {
  // Check all device matrix should be the same
  if (outputs_tensor_info_.size() != kSizeTwo) {
    MS_LOG(ERROR) << "The size of output_tensor_layout for rmsnorm is " << outputs_tensor_info_.size()
                  << " rather than 2.";
    return FAILED;
  }
  if (output_infer_tensor_layout_.tensor_shape_before().array().empty()) {
    MS_LOG(ERROR) << "Parameter of output tensor layout for rmsnorm is not allowed to be set by users.";
    return FAILED;
  }
  MS_LOG(INFO) << name_ << ": Using output tensor layout infer by input tensor layout.";
  return SUCCESS;
}

Status RmsNormInfo::InferOutputLayout() {
  auto input_layout = inputs_tensor_info_[kIndex0].tensor_layout();

  TensorLayout output_tensor_layout;
  TensorLayout rstd_tensor_layout;
  output_tensor_layout = input_layout;
  rstd_tensor_layout = output_tensor_layout;
  std::vector<Shape> rstd_extended_tensor_map;
  Shape rstd_tensor_shape;

  for (size_t i = 0; i < rstd_tensor_layout.tensor_shape_before().array().size(); ++i) {
    auto map_dim = input_layout.tensor_map_before()[i];
    auto shp_dim = input_layout.tensor_shape_before().array()[i];
    rstd_extended_tensor_map.push_back(map_dim);
    if (i < begin_norm_axis_) {
      rstd_tensor_shape.push_back(shp_dim);
    } else {
      rstd_tensor_shape.push_back(1);
    }
  }
  rstd_tensor_layout.InitFromExtendVector(rstd_tensor_layout.device_arrangement_origin().array(),
                                          rstd_extended_tensor_map, rstd_tensor_shape);

  output_infer_tensor_layout_ = output_tensor_layout;
  rstd_infer_tensor_layout_ = rstd_tensor_layout;

  return SUCCESS;
}

Status RmsNormInfo::InferOutputTensorInfo() {
  InferOutputLayout();
  if (output_infer_tensor_layout_.tensor_shape_before().array() != outputs_shape_[kIndex0]) {
    MS_LOG(ERROR) << "The infer output shape " << output_infer_tensor_layout_.tensor_shape_before().array()
                  << " dose not match the output shape " << outputs_shape_[kIndex0];
    return FAILED;
  }
  if (rstd_infer_tensor_layout_.tensor_shape_before().array() != outputs_shape_[kIndex1]) {
    MS_LOG(ERROR) << "The infer output rstd shape " << rstd_infer_tensor_layout_.tensor_shape_before().array()
                  << " dose not match the output shape " << outputs_shape_[kIndex1];
    return FAILED;
  }
  TensorInfo output_tensor_info(output_infer_tensor_layout_);
  TensorInfo rstd_tensor_info(rstd_infer_tensor_layout_);
  outputs_tensor_info_.push_back(output_tensor_info);
  outputs_tensor_info_.push_back(rstd_tensor_info);
  return SUCCESS;
}

REGISTER(RmsNormInfo);
}  // namespace parallel
}  // namespace mindspore
