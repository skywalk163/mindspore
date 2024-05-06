/**
 * Copyright (c) 2022-2022 Huawei Technologies Co., Ltd.  All rights reserved.
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

#include "op_proto/inc/split_combination_ops.h"
#include "register/op_impl_registry.h"
#include "utils/util.h"
#include "utils/common_shape_fns.h"
#include "utils/op_common_util.h"
#include "utils/op_const.h"

namespace ge {
static void JoinShapeRanges(std::vector<std::pair<int64_t, int64_t>> &dest_ranges,
                            const std::vector<std::pair<int64_t, int64_t>> &src_ranges) {
  auto dest_size = dest_ranges.size();
  auto src_size = src_ranges.size();
  if (dest_size != src_size) {
    return;
  }

  for (size_t i = 0; i < dest_size; i++) {
    dest_ranges[i].first = std::max(dest_ranges[i].first, src_ranges[i].first);
    dest_ranges[i].second = std::min(dest_ranges[i].second, src_ranges[i].second);
  }
}

static std::vector<std::pair<int64_t, int64_t>> GetShapeRangesWithUnKnowConcatDim(Operator &op, int64_t num_concat) {
  std::vector<std::pair<int64_t, int64_t>> input_shape_ranges;
  std::vector<std::vector<std::pair<int64_t, int64_t>>> all_input_shape_ranges;
  std::vector<std::pair<int64_t, int64_t>> output_shape_ranges;
  bool has_shape_ranges = false;
  for (int32_t i = 0; i < num_concat; i++) {
    const auto input_desc = op.GetDynamicInputDesc("x", i);
    (void)input_desc.GetShapeRange(input_shape_ranges);
    OP_LOGD(TbeGetName(op).c_str(), "input shape range:%s", to_string(input_shape_ranges).c_str());
    if (input_shape_ranges.empty()) {
      auto shape_dims = input_desc.GetShape().GetDims();
      MakeUpShapeRange(shape_dims, input_shape_ranges);
    } else {
      has_shape_ranges = true;
    }

    all_input_shape_ranges.push_back(input_shape_ranges);
  }

  if (has_shape_ranges) {
    output_shape_ranges = all_input_shape_ranges[0];
    for (size_t i = 1; i < all_input_shape_ranges.size(); i++) {
      if (output_shape_ranges.size() != all_input_shape_ranges[i].size()) {
        continue;
      }

      for (size_t j = 0; j < output_shape_ranges.size(); j++) {
        output_shape_ranges[j].first = std::max(output_shape_ranges[j].first, all_input_shape_ranges[i][j].first);
        if (output_shape_ranges[j].second == -1 || all_input_shape_ranges[i][j].second == -1) {
          output_shape_ranges[j].second = -1;
        } else {
          output_shape_ranges[j].second = output_shape_ranges[j].second + all_input_shape_ranges[i][j].second;
        }
      }
    }
  }

  return output_shape_ranges;
}

bool JoinShapes(vector<int64_t> &dst_shape, const vector<int64_t> &src_shape, int64_t axis) {
  if (dst_shape == src_shape) {
    return true;
  }

  if (dst_shape.empty() || IsUnknownRankShape(dst_shape)) {
    dst_shape = src_shape;
    return true;
  }

  if (!IsUnknownRankShape(src_shape)) {
    if (dst_shape.size() != src_shape.size()) {
      return false;
    }
    auto shape_dims = dst_shape.size();
    for (size_t i = 0; i < shape_dims; i++) {
      if (dst_shape[i] == src_shape[i]) {
        continue;
      }

      if (axis != static_cast<int64_t>(i) && dst_shape[i] != UNKNOWN_DIM && src_shape[i] != UNKNOWN_DIM) {
        return false;
      }

      if (src_shape[i] != UNKNOWN_DIM) {
        dst_shape[i] = src_shape[i];
      }
    }
  }

  return true;
}

bool ConcatInferShapeCommonStatic(Operator &op, const int64_t dynamic_input_start_idx, int64_t num_concat,
                                  int64_t axis) {
  auto input_desc = op.GetInputDesc(dynamic_input_start_idx);
  auto output_desc = op.GetOutputDesc(0);
  const Shape &input_shape = input_desc.GetShape();
  Shape output_shape = output_desc.GetShape();
  output_shape = input_shape;
  if (IsUnknownShape(output_shape) || num_concat == 1) {
    // dynamic case or the input only one will use dynamic infer func
    return false;
  }

  if (IsScalar(output_shape)) {
    // scalar to shape [1]
    output_shape = Shape({1});
  }
  size_t output_dim = output_shape.GetDimNum();
  if ((axis < -static_cast<int64_t>(output_dim)) || (axis >= static_cast<int64_t>(output_dim))) {
    // axes is valid
    return false;
  }
  if (axis < 0) {
    axis += static_cast<int64_t>(output_dim);
  }
  int64_t concat_dim_size = output_shape.GetDim(axis);

  for (int64_t input_idx = 1; input_idx < num_concat; input_idx++) {
    auto input_i_desc = op.GetInputDesc(input_idx + dynamic_input_start_idx);
    const Shape &input_i_shape = input_i_desc.GetShape();
    if (IsScalar(input_i_shape) && output_dim == 1) {
      concat_dim_size += 1;
      continue;
    }
    if (IsUnknownShape(input_i_shape)) {
      // dynamic case
      return false;
    }
    if (input_i_shape.GetDimNum() != output_dim) {
      // input shape size is not equal output
      return false;
    }
    // check whether the non concat dim is equal
    for (int64_t check_dim = 0; check_dim < static_cast<int64_t>(output_dim); check_dim++) {
      if (check_dim != axis && input_i_shape.GetDim(check_dim) != output_shape.GetDim(check_dim)) {
        return false;
      }
    }
    concat_dim_size += input_i_shape.GetDim(axis);
  }
  output_shape.SetDim(axis, concat_dim_size);

  // set data type
  output_desc.SetDataType(input_desc.GetDataType());
  op.UpdateOutputDesc("y", output_desc);
  return true;
}

static graphStatus ConcatInferShapeCommon(Operator &op, const int64_t dy_input_start_idx, int64_t num_concat,
                                          int64_t axis, bool unknown_axis) {
  if (num_concat <= 0) {
    std::string err_msg = GetAttrValueErrMsg("num_concat", std::to_string(num_concat), ConcatString("num_concat > 0"));
    VECTOR_INFER_SHAPE_INNER_ERR_REPORT(TbeGetName(op), err_msg);
    return GRAPH_FAILED;
  }

  // try static infershape directly
  if (!unknown_axis) {
    if (ConcatInferShapeCommonStatic(op, dy_input_start_idx, num_concat, axis)) {
      return GRAPH_SUCCESS;
    }
  }
  size_t dim_num = 0;
  std::vector<TensorDesc> input_x_desc;
  const string input_name = "x";
  string input_name_i = "x63";
  for (int64_t input_idx = 0; input_idx < num_concat; input_idx++) {
    input_name_i = input_name + std::to_string(input_idx);
    auto input_desc = op.GetInputDesc(input_name_i);
    input_x_desc.emplace_back(input_desc);
  }

  bool all_unknown_rank_shape = true;
  for (const auto &desc : input_x_desc) {
    dim_num = std::max(dim_num, desc.GetShape().GetDimNum());
    all_unknown_rank_shape = IsUnknownRankShape(desc.GetShape()) && all_unknown_rank_shape;
  }

  if (all_unknown_rank_shape) {
    DataType input_dtype = input_x_desc[0].GetDataType();
    auto output_desc = op.GetOutputDesc(0);
    output_desc.SetDataType(input_dtype);
    output_desc.SetShape(ge::Shape(UNKNOWN_RANK));
    OP_LOGD(TbeGetName(op).c_str(), "output shape:%s", to_string(output_desc.GetShape()).c_str());
    op.UpdateOutputDesc("y", output_desc);
    return GRAPH_SUCCESS;
  }

  if (unknown_axis) {
    DataType input_dtype = input_x_desc[0].GetDataType();
    auto output_desc = op.GetOutputDesc(0);
    output_desc.SetDataType(input_dtype);
    vector<int64_t> dimVector(dim_num, -1);
    output_desc.SetShape(ge::Shape(dimVector));
    auto output_shape_ranges = GetShapeRangesWithUnKnowConcatDim(op, num_concat);
    if (!output_shape_ranges.empty()) {
      output_desc.SetShapeRange(output_shape_ranges);
      OP_LOGD(TbeGetName(op).c_str(), "output shape range:%s", to_string(output_shape_ranges).c_str());
    }
    OP_LOGD(TbeGetName(op).c_str(), "output shape:%s", to_string(output_desc.GetShape()).c_str());
    op.UpdateOutputDesc("y", output_desc);
    return GRAPH_SUCCESS;
  }

  if ((axis < -static_cast<int64_t>(dim_num)) || (axis >= static_cast<int64_t>(dim_num))) {
    OP_LOGE(TbeGetName(op).c_str(), "the parameter [axis] should be in the range of [%ld, %ld], but actually is %ld",
            -static_cast<int64_t>(dim_num), static_cast<int64_t>(dim_num), axis);
    return GRAPH_FAILED;
  }

  int64_t non_negative_axis = axis;
  if (non_negative_axis < 0) {
    non_negative_axis += static_cast<int64_t>(dim_num);
  }

  vector<int64_t> output_shape_dims;
  for (const auto &desc : input_x_desc) {
    auto input_shape_dims = desc.GetShape().GetDims();
    if (!JoinShapes(output_shape_dims, input_shape_dims, non_negative_axis)) {
      vector<vector<int64_t>> shapes = {output_shape_dims, input_shape_dims};
      std::string err_msg =
        OtherErrMsg(ConcatString("the input shape dims should be equal except merge axis,"
                                 "shapes:",
                                 ops::to_string(shapes), "axis:", std::to_string(axis)));
      VECTOR_INFER_SHAPE_INNER_ERR_REPORT(TbeGetName(op), err_msg);
      return GRAPH_FAILED;
    }
  }

  int32_t size = 0;
  for (const auto &desc : input_x_desc) {
    if (IsUnknownRankShape(desc.GetShape())) {
      size = -1;
      break;
    }

    auto dim_value = desc.GetShape().GetDim(non_negative_axis);
    if (dim_value == -1) {
      size = -1;
      break;
    }

    if (size != -1) {
      size += dim_value;
    }
  }

  output_shape_dims[non_negative_axis] = size;
  DataType input_dtype = input_x_desc[0].GetDataType();
  auto output_desc = op.GetOutputDesc(0);
  output_desc.SetDataType(input_dtype);
  output_desc.SetShape(ge::Shape(output_shape_dims));
  OP_LOGD(TbeGetName(op).c_str(), "output shape:%s", to_string(output_desc.GetShape()).c_str());

  if (IsUnKnownShape(output_shape_dims)) {
    std::vector<std::pair<int64_t, int64_t>> input_shape_ranges;
    std::vector<std::pair<int64_t, int64_t>> output_shape_ranges;
    std::pair<int64_t, int64_t> output_concat_dim_range(0, 0);
    for (const auto &input_desc : input_x_desc) {
      if (IsUnknownRankShape(input_desc.GetShape())) {
        output_concat_dim_range = {0, -1};
        continue;
      }

      input_shape_ranges.clear();
      input_desc.GetShapeRange(input_shape_ranges);
      OP_LOGD(TbeGetName(op).c_str(), "input shape range:%s", to_string(input_shape_ranges).c_str());
      if (input_shape_ranges.empty()) {
        MakeUpShapeRange(input_desc.GetShape(), input_shape_ranges);
      }

      if (static_cast<int64_t>(input_shape_ranges.size()) > non_negative_axis) {
        output_concat_dim_range.first += input_shape_ranges[non_negative_axis].first;
        if (input_shape_ranges[non_negative_axis].second == -1 || output_concat_dim_range.second == -1) {
          output_concat_dim_range.second = -1;
        } else {
          output_concat_dim_range.second += input_shape_ranges[non_negative_axis].second;
        }
      }

      if (output_shape_ranges.empty()) {
        output_shape_ranges = input_shape_ranges;
      } else {
        JoinShapeRanges(output_shape_ranges, input_shape_ranges);
      }
    }

    if (output_concat_dim_range.second != 0 && static_cast<uint64_t>(non_negative_axis) < output_shape_ranges.size()) {
      output_shape_ranges[non_negative_axis] = output_concat_dim_range;
    }

    output_desc.SetShapeRange(output_shape_ranges);
    OP_LOGD(TbeGetName(op).c_str(), "output shape range:%s", to_string(output_shape_ranges).c_str());
  }
  op.UpdateOutputDesc("y", output_desc);

  return GRAPH_SUCCESS;
}

static graphStatus ConcatInputsVerify(const Operator &op) {
  std::vector<std::string> inputs;
  const string input_name = "x";
  string input_name_i = "x63";
  int64_t N;
  if (op.GetAttr("N", N) == GRAPH_FAILED) {
    OP_LOGE(TbeGetName(op), "get attr N failed");
    return GRAPH_FAILED;
  }
  for (int64_t input_idx = 0; input_idx < N; input_idx++) {
    input_name_i = input_name + std::to_string(input_idx);
    inputs.emplace_back(input_name_i);
  }

  if (!CheckInputDtypeSame(op, inputs)) {
    return GRAPH_FAILED;
  }
  return GRAPH_SUCCESS;
}

// ----------------Concat OP Begin-------------------
IMPLEMT_VERIFIER(Concat, ConcatVerify) { return ConcatInputsVerify(op); }

IMPLEMT_COMMON_INFERFUNC(ConcatInferShape) {
  const vector<string> depend_names = {"concat_dim"};
  PREPARE_DYNAMIC_SHAPE(depend_names);

  int64_t N;
  if (op.GetAttr("N", N) == GRAPH_FAILED) {
    AICPU_INFER_SHAPE_INNER_ERR_REPORT(TbeGetName(op), string("get attr[N] failed"));
    return GRAPH_FAILED;
  }
  Tensor data;
  bool is_unknown_axis{true};
  if (op.GetInputConstData("concat_dim", data) == GRAPH_SUCCESS) {
    is_unknown_axis = false;
  }
  OP_LOGD(TbeGetName(op), "concat_dim is unknown[%s].", is_unknown_axis ? "true" : "false");
  int64_t axis = 0;
  if (!is_unknown_axis) {
    DataType dtype = op.GetInputDesc(0).GetDataType();
    std::vector<int64_t> const_vec;
    if (!GetConstValue(op, data, dtype, const_vec)) {
      is_unknown_axis = true;
      OP_LOGW(TbeGetName(op), "Get concat_dim value failed.");
    } else {
      axis = const_vec[0];
    }
  }

  return ConcatInferShapeCommon(op, 1, N, axis, is_unknown_axis);
}

COMMON_INFER_FUNC_REG(Concat, ConcatInferShape);
VERIFY_FUNC_REG(Concat, ConcatVerify);
// ----------------Concat OP End-------------------
}  // namespace ge