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
#include "tools/graph_kernel/converter/akg/ascend_kernel_builder.h"
#include "utils/anf_utils.h"
#include "tools/graph_kernel/converter/akg/utils.h"
#include "backend/common/graph_kernel/core/graph_kernel_utils.h"

namespace mindspore::graphkernel {
bool AscendKernelBuilder::CompileJsonsInAnfnodes(const AnfNodePtrList &node_list) {
  static std::string rank_id = common::GetEnv("RANK_ID");
  std::string dir;
  if (rank_id.empty()) {
    dir = "./akg_kernel_meta";
  } else {
    dir = "./rank_" + rank_id + "/akg_kernel_meta";
  }
  dir_path_ = SaveNodesInfo(node_list, dir, AkgKernelBuilder::json_option(), &node_info_map_, nullptr);
  return !dir_path_.empty();
}

AnfNodePtr AscendKernelBuilder::CreateCustomOp(const FuncGraphPtr &func_graph, const CNodePtr &cnode) {
  auto op = std::make_shared<ops::Custom>();
  op->set_type("GraphKernel");
  auto custom_prim = op->GetPrim();
  auto inputs = cnode->inputs();
  inputs[0] = NewValueNode(custom_prim);
  auto custom_cnode = func_graph->NewCNode(inputs);
  custom_prim->EraseAttr("IsFeatureMapInputList");
  custom_prim->EraseAttr("IsFeatureMapOutput");

  auto json_kernel_name = node_info_map_[cnode->cast<AnfNodePtr>()];
  auto input_num = AnfUtils::GetInputTensorNum(cnode);
  auto output_num = AnfUtils::GetOutputTensorNum(cnode);
  std::vector<std::string> input_names;
  std::vector<std::string> output_names;
  for (size_t i = 0; i < input_num; ++i) {
    input_names.push_back("x" + std::to_string(i));
  }
  for (size_t i = 0; i < output_num; ++i) {
    output_names.push_back("y" + std::to_string(i));
  }

  std::ostringstream oss;
  oss << "Fused_x" << input_num << "_y" << output_num;
  std::string op_tye = oss.str();
  custom_prim->set_attr("reg_op_name", MakeValue(op_tye));
  custom_prim->set_attr("info_path", MakeValue(dir_path_ + "/" + json_kernel_name + ".info"));
  custom_prim->set_attr("input_names", MakeValue(input_names));
  custom_prim->set_attr("output_names", MakeValue(output_names));
  custom_cnode->set_fullname_with_scope(cnode->fullname_with_scope());
  custom_cnode->set_abstract(cnode->abstract()->Clone());
  if (GkUtils::UseAkgCceLib(cnode)) {
    custom_cnode->AddAttr("use_akg_cce", MakeValue(true));
  }
  return custom_cnode;
}
}  // namespace mindspore::graphkernel
