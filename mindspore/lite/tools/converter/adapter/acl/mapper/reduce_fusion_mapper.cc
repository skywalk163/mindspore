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

#include "tools/converter/adapter/acl/mapper/reduce_fusion_mapper.h"
#include <memory>
#include <vector>
#include <algorithm>
#include "tools/converter/adapter/acl/mapper/primitive_mapper_register.h"
#include "tools/converter/adapter/acl/mapper/tbe_op_def.h"
#include "tools/converter/adapter/acl/common/utils.h"
#include "src/common/log_util.h"
#include "ops/op_utils.h"
#include "ops/auto_generate/gen_lite_ops.h"
#include "ops/lp_norm.h"

namespace mindspore {
namespace lite {
namespace {
constexpr auto kNameReduceMinInputNum = 2;
constexpr auto kNameReduceInputNum = 3;
}  // namespace

STATUS ReduceFusionMapper::Mapper(const CNodePtr &cnode) {
  ValueNodePtr value_node = nullptr;
  PrimitivePtr src_prim = nullptr;
  if (GetValueNodeAndPrimFromCnode(cnode, &value_node, &src_prim) != lite::RET_OK) {
    MS_LOG(ERROR) << "Get value node and primitive from cnode failed.";
    return lite::RET_ERROR;
  }
  auto attr_val = src_prim->GetAttr(ops::kMode);
  CHECK_NULL_RETURN(attr_val);
  int64_t mode = GetValue<int64_t>(attr_val);
  PrimitivePtr dst_prim = nullptr;
  if (mode == static_cast<int64_t>(ReduceMode::Reduce_Sum)) {
    ops::ReduceSum reduce_sum_op;
    dst_prim = reduce_sum_op.GetPrim();
  } else if (mode == static_cast<int64_t>(ReduceMode::Reduce_Mean)) {
    ops::ReduceMean reduce_mean_op;
    dst_prim = reduce_mean_op.GetPrim();
  } else if (mode == static_cast<int64_t>(ReduceMode::Reduce_Max)) {
    ops::ReduceMax reduce_max_op;
    dst_prim = reduce_max_op.GetPrim();
  } else if (mode == static_cast<int64_t>(ReduceMode::Reduce_Min)) {
    ops::ReduceMin reduce_min_op;
    dst_prim = reduce_min_op.GetPrim();
  } else if (mode == static_cast<int64_t>(ReduceMode::Reduce_All)) {
    ops::ReduceAll reduce_all;
    dst_prim = reduce_all.GetPrim();
  } else if (mode == static_cast<int64_t>(ReduceMode::Reduce_L2)) {
    ops::LpNorm lp_norm_op;
    auto axes_ptr = src_prim->GetAttr(ops::kAxes);
    if (axes_ptr != nullptr) {
      auto axes = GetValue<std::vector<int32_t>>(axes_ptr);
      std::vector<int64_t> axes_vec;
      std::transform(axes.begin(), axes.end(), std::back_inserter(axes_vec),
                     [](int32_t x) { return static_cast<int64_t>(x); });
      lp_norm_op.set_axis(axes_vec);
    }
    dst_prim = lp_norm_op.GetPrim();
  } else if (mode == static_cast<int64_t>(ReduceMode::Reduce_L1)) {
    ops::LpNorm lp_norm_op;
    auto axes_ptr = src_prim->GetAttr(ops::kAxes);
    if (axes_ptr != nullptr) {
      auto axes = GetValue<std::vector<int32_t>>(axes_ptr);
      std::vector<int64_t> axes_vec;
      std::transform(axes.begin(), axes.end(), std::back_inserter(axes_vec),
                     [](int32_t x) { return static_cast<int64_t>(x); });
      lp_norm_op.set_axis(axes_vec);
    }
    auto keep_dims_ptr = src_prim->GetAttr(ops::kKeepDims);
    if (keep_dims_ptr != nullptr) {
      auto keep_dims = GetValue<bool>(keep_dims_ptr);
      lp_norm_op.set_keep_dims(keep_dims);
    }
    lp_norm_op.set_p(1);
    dst_prim = lp_norm_op.GetPrim();
  } else if (mode == static_cast<int64_t>(ReduceMode::Reduce_Prod)) {
    dst_prim = std::make_shared<acl::DynamicReduceProd>();
  } else if (mode == static_cast<int64_t>(ReduceMode::Reduce_Log_Sum)) {
    dst_prim = std::make_shared<acl::ReduceLogSum>();
  } else if (mode == static_cast<int64_t>(ReduceMode::Reduce_Log_Sum_Exp)) {
    dst_prim = std::make_shared<acl::ReduceLogSumExp>();
  } else {
    MS_LOG(ERROR) << "Not support reduce mode " << static_cast<int64_t>(mode);
    return RET_ERROR;
  }
  CHECK_NULL_RETURN(dst_prim);
  dst_prim->SetAttrs(src_prim->attrs());
  value_node->set_value(dst_prim);
  if (mode == static_cast<int64_t>(ReduceMode::Reduce_Mean)) {
    return lite::RET_OK;
  }
  if (AdjustInput(cnode, dst_prim) != RET_OK) {
    MS_LOG(ERROR) << "Adjust reduce input failed.";
    return lite::RET_ERROR;
  }
  return RET_OK;
}

STATUS ReduceFusionMapper::AdjustInput(const CNodePtr &cnode, const PrimitivePtr &prim) {
  MS_ASSERT(cnode != nullptr && prim != nullptr);
  auto attr_val = prim->GetAttr(ops::kMode);
  CHECK_NULL_RETURN(attr_val);
  int64_t mode = GetValue<int64_t>(attr_val);
  if (cnode->size() == kNameReduceInputNum && mode == static_cast<int64_t>(ReduceMode::Reduce_Prod)) {
    auto axes_ptr = prim->GetAttr(ops::kAxes);
    if (axes_ptr != nullptr) {
      auto axes = GetValue<std::vector<int32_t>>(axes_ptr);
      if (axes.empty()) {
        auto new_inputs = {cnode->input(0), cnode->input(1)};
        cnode->set_inputs(new_inputs);
      }
    }
  }
  if (cnode->size() == kNameReduceMinInputNum) {
    auto func_graph = cnode->func_graph();
    CHECK_NULL_RETURN(func_graph);
    auto attr_name = mode != static_cast<int64_t>(ReduceMode::Reduce_Prod) ? ops::kAxes : ops::kKeepDims;
    auto ret = mode != static_cast<int64_t>(ReduceMode::Reduce_Prod)
                 ? AddIntVecAttrToInput(func_graph, cnode, prim, attr_name)
                 : AddIntAttrToInput(func_graph, cnode, prim, attr_name, true);
    if (ret != lite::RET_OK) {
      MS_LOG(ERROR) << "Add attr " << attr_name << " failed for cnode: " << cnode->fullname_with_scope();
      return lite::RET_ERROR;
    }
    return lite::RET_OK;
  }

  auto axes_input = cnode->input(kNameReduceInputNum - 1);
  CHECK_NULL_RETURN(axes_input);
  if (!utils::isa<ParameterPtr>(axes_input)) {
    MS_LOG(ERROR) << "The reduce node is not parameter.";
    return lite::RET_ERROR;
  }
  ParameterPtr axes_param = axes_input->cast<ParameterPtr>();
  CHECK_NULL_RETURN(axes_param);
  auto data = acl::GetIntParameterData(axes_param);
  std::vector<int64_t> axes;
  std::transform(data.begin(), data.end(), std::back_inserter(axes),
                 [](int32_t n) -> int64_t { return static_cast<int64_t>(n); });
  ValueNodePtr value_node = NewValueNode<std::vector<int64_t>>(axes);
  std::vector<int64_t> shape_vec_shape = {};
  auto abstract = std::make_shared<abstract::AbstractTensor>(kInt64, shape_vec_shape);
  value_node->set_abstract(abstract);
  CHECK_NULL_RETURN(value_node);
  cnode->set_input(kNameReduceInputNum - 1, value_node);
  return lite::RET_OK;
}

REGISTER_PRIMITIVE_MAPPER(kNameReduceFusion, ReduceFusionMapper)
}  // namespace lite
}  // namespace mindspore
