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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_ADAPTER_ACL_SRC_ACL_PASS_IMPL_H_
#define MINDSPORE_LITE_TOOLS_CONVERTER_ADAPTER_ACL_SRC_ACL_PASS_IMPL_H_

#include <string>
#include <utility>
#include <vector>
#include <memory>
#include "include/backend/optimizer/pass.h"
#include "include/errorcode.h"
#include "include/api/types.h"
#include "include/registry/converter_context.h"
#include "cxx_api/model/acl/acl_model_options.h"
#include "tools/converter/adapter/acl/common/acl_types.h"
#include "ops/custom.h"
#include "tools/converter/cxx_api/converter_para.h"

namespace mindspore {
namespace opt {
using mindspore::converter::FmkType;
using mindspore::lite::STATUS;

class AclPassImpl {
 public:
  explicit AclPassImpl(const std::shared_ptr<ConverterPara> &param);
  virtual ~AclPassImpl() = default;

  virtual bool Run(const FuncGraphPtr &func_graph);

 protected:
  /* build func graph */
  virtual STATUS BuildGraph(const FuncGraphPtr &func_graph);
  /* pre or post pass */
  STATUS PreProcGraph(const FuncGraphPtr &func_graph);
  STATUS PostProcGraph(const FuncGraphPtr &func_graph);
  STATUS PostProcCustomOp(const FuncGraphPtr &func_graph);
  /* map func graph */
  STATUS DeparseGraph(const FuncGraphPtr &func_graph, const FuncGraphManagerPtr &manager);
  STATUS ConvertGraphToOm(const FuncGraphPtr &func_graph, Buffer *om_data);

 private:
  /* pre or post pass */
  bool IsDynamicInput();
  STATUS CommonPass(const FuncGraphPtr &func_graph);
  STATUS RemoveSingleInputConcatNode(const FuncGraphPtr &func_graph);
  STATUS MakeListToMakeTuple(const FuncGraphPtr &func_graph);

 private:
  /* map func graph */
  STATUS RunPrimitiveMapper(const FuncGraphPtr &func_graph);
  std::string AdjustCnodeName(const PrimitivePtr &prim);

 private:
  /* build func graph */
  STATUS SetGraphInputShape(const FuncGraphPtr &func_graph);
  STATUS SetAclModelOptions(const FuncGraphPtr &func_graph);
  STATUS MapperForOrgMindIR(const FuncGraphPtr &func_graph);

 private: /* Quantization */
  STATUS Quantization(const FuncGraphPtr &func_graph);
  STATUS PreQuantization(const FuncGraphPtr &func_graph);
  STATUS PostQuantization(const FuncGraphPtr &func_graph);
  STATUS RemoveQuantDtypeCast(const FuncGraphPtr &func_graph);

 protected:
  std::shared_ptr<ConverterPara> param_;
  FmkType fmk_type_;
  ModelType export_mindir_;
  lite::acl::AclModelOptionCfg user_options_cfg_;
  ParameterPtr om_parameter_;
  CNodePtr custom_node_;
  std::shared_ptr<AclModelOptions> options_;
  AnfNodePtrList graph_outputs_;
  std::vector<size_t> tuple_idx_;
  std::vector<std::vector<int64_t>> graph_output_dims_;
  bool is_ptq_quant_ = false;
};
}  // namespace opt
}  // namespace mindspore
#endif  // MINDSPORE_LITE_TOOLS_CONVERTER_ADAPTER_ACL_SRC_ACL_PASS_IMPL_H_
