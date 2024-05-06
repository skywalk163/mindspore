/**
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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
#include "include/common/debug/anf_ir_dump.h"
#if defined(_WIN32) || defined(_WIN64)
#include <stdlib.h>
#endif
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "mindspore/core/ops/structure_ops.h"
#include "utils/label.h"
#include "utils/hash_map.h"
#include "utils/symbolic.h"
#include "utils/compile_config.h"
#include "ir/primitive.h"
#include "ir/func_graph.h"
#include "ir/graph_utils.h"
#include "ir/value.h"
#include "utils/trace_base.h"
#include "utils/anf_utils.h"
#include "include/common/utils/anfalgo.h"
#include "include/common/debug/anf_dump_utils.h"
#include "mindspore/core/utils/file_utils.h"
#include "ir/meta_func_graph.h"
#include "pipeline/jit/ps/parse/resolve.h"
#include "frontend/operator/composite/composite.h"
#include "frontend/expander/bprop/bprop_meta_func_graph.h"
#include "frontend/operator/composite/vmap.h"
#include "frontend/operator/composite/map.h"

using MetaFuncGraph = mindspore::MetaFuncGraph;
using MetaFuncGraphPtr = std::shared_ptr<MetaFuncGraph>;
namespace mindspore {

enum FormatLevel : int {
  // When setting to basic level, ir will only contains operator and operands of nodes and title of subgraph with
  // its debuginfo
  kBasicLevel = 0,
  // When setting to advanced level, ir will contains all the info except scope and debug info of nodes.
  kAdvancedLevel,
  // When setting to advanced level, ir will contains all the info.
  kFullyLevel,
};

std::string GetMultitypeFuncGraphText(const prim::MultitypeFuncGraphPtr &mt_func_graph) {
  auto py_funcs = mt_func_graph->GetPyFunctions();
  if (py_funcs.empty()) {
    return "";
  }

  std::ostringstream oss;

  oss << "{";
  bool is_first = true;
  for (const auto &py_func : py_funcs) {
    if (is_first) {
      is_first = false;
    } else {
      oss << ", ";
    }
    oss << "(";
    for (size_t i = 0; i < py_func.first.size(); ++i) {
      if (i > 0) {
        oss << ", ";
      }
      oss << py_func.first[i]->DumpText();
    }
    oss << ")";
  }
  oss << "}";

  return oss.str();
}

inline bool Skip(const MetaFuncGraphPtr &meta_func_graph) {
  return meta_func_graph->isa<prim::Tail>() || meta_func_graph->isa<prim::MakeTupleGradient>() ||
         meta_func_graph->isa<prim::MakeListGradient>() || meta_func_graph->isa<prim::MakeDictGradient>() ||
         meta_func_graph->isa<prim::TupleAdd>() || meta_func_graph->isa<prim::SequenceSliceGetItem>() ||
         meta_func_graph->isa<prim::ListSliceSetItem>() || meta_func_graph->isa<prim::UnpackCall>() ||
         meta_func_graph->isa<prim::ZipOperation>() || meta_func_graph->isa<prim::ListAppend>() ||
         meta_func_graph->isa<prim::ListInsert>() || meta_func_graph->isa<prim::DoSignatureMetaFuncGraph>() ||
         meta_func_graph->isa<prim::VmapMatchOutAxis>() || meta_func_graph->isa<prim::VmapGeneralPreprocess>() ||
         meta_func_graph->isa<prim::GradAux>() || meta_func_graph->isa<prim::PyExecuteGradient>() ||
         meta_func_graph->isa<prim::MutableGradient>() || meta_func_graph->isa<prim::ZerosLike>() ||
         meta_func_graph->isa<prim::ListAdd>() || meta_func_graph->isa<prim::StarredGetItem>() ||
         meta_func_graph->isa<prim::StarredUnpack>() || meta_func_graph->isa<prim::StarredUnpackMerge>() ||
         meta_func_graph->isa<prim::IterConverter>() || meta_func_graph->isa<prim::HasNext>() ||
         meta_func_graph->isa<prim::Next>();
}

std::string GetMetaFuncGraphText(const MetaFuncGraphPtr &meta_func_graph) {
  if (meta_func_graph == nullptr) {
    return "";
  }

  std::ostringstream oss;
  oss << meta_func_graph->type_name() << "_" << meta_func_graph->name();

  if (meta_func_graph->isa<prim::MultitypeFuncGraph>()) {
    prim::MultitypeFuncGraphPtr mt_func_graph = meta_func_graph->cast<prim::MultitypeFuncGraphPtr>();
    oss << GetMultitypeFuncGraphText(mt_func_graph);
  } else if (meta_func_graph
               ->isa<prim::HyperMapPy>()) {  // This statement must before 'meta_graph->isa<prim::HyperMap>()'
    auto hyper_map = meta_func_graph->cast<prim::HyperMapPyPtr>();
    if (hyper_map->GetFnLeaf() != nullptr) {
      oss << "{fn_leaf: " << GetMetaFuncGraphText(hyper_map->GetFnLeaf()) << "}";
    }
  } else if (meta_func_graph->isa<prim::HyperMap>()) {
    auto hyper_map = meta_func_graph->cast<prim::HyperMapPtr>();
    if (hyper_map->GetFnLeaf() != nullptr) {
      oss << "{fn_leaf: " << GetMetaFuncGraphText(hyper_map->GetFnLeaf()) << "}";
    }
  } else if (meta_func_graph->isa<prim::MapPy>()) {  // This statement must before 'meta_graph->isa<prim::Map>()'
    auto map = meta_func_graph->cast<prim::MapPyPtr>();
    if (map->GetFnLeaf() != nullptr) {
      oss << "{fn_leaf: " << GetMetaFuncGraphText(map->GetFnLeaf()) << "}";
    }
  } else if (meta_func_graph->isa<prim::Map>()) {
    auto map = meta_func_graph->cast<prim::MapPtr>();
    if (map->GetFnLeaf() != nullptr) {
      oss << "{fn_leaf: " << GetMetaFuncGraphText(map->GetFnLeaf()) << "}";
    }
  } else if (meta_func_graph->isa<prim::GradOperation>()) {
    prim::GradOperationPtr grad_op = meta_func_graph->cast<prim::GradOperationPtr>();
    oss << "{get_all: " << grad_op->get_all_ << ", get_by_list: " << grad_op->get_by_list_
        << ", sens_param: " << grad_op->sens_param_ << "}";
  } else if (meta_func_graph->isa<prim::VmapGeneralRule>()) {
    prim::VmapGeneralRulePtr general_rule_fg = meta_func_graph->cast<prim::VmapGeneralRulePtr>();
    oss << "{prim: " << general_rule_fg->prim_name() << ", axis_size: " << general_rule_fg->axis_size() << "}";
  } else if (meta_func_graph->isa<expander::bprop::BpropMetaFuncGraph>()) {
    oss << "{" << meta_func_graph->name() << "}";
  } else if (Skip(meta_func_graph)) {
    // Do nothing.
  } else {
    MS_LOG(INTERNAL_EXCEPTION) << "Unknown MetaFuncGraph type " << meta_func_graph->type_name();
  }

  return oss.str();
}

std::string GetPrimitiveText(const PrimitivePtr &prim) {
  std::ostringstream oss;
  if (!prim->instance_name().empty()) {
    oss << " {";
    oss << "instance name"
        << ": ";
    oss << prim->instance_name();
    oss << "}";
  }
  auto attrs = prim->attrs();
  if (!attrs.empty()) {
    oss << " primitive_attrs: {";
    oss << prim->GetAttrsText();
    oss << "}";
  }

  if (prim->isa<prim::DoSignaturePrimitive>()) {
    auto do_signature = dyn_cast<prim::DoSignaturePrimitive>(prim);
    auto &func = do_signature->function();
    if (func->isa<Primitive>()) {
      auto sig_prim = dyn_cast<Primitive>(func);
      oss << sig_prim->GetAttrsText();
    }
  }

  return oss.str();
}

std::string GetSequenceText(const ValuePtr &value, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  std::ostringstream oss;
  // Output ValueList, ValueTuple
  ValueSequencePtr seq = dyn_cast<ValueSequence>(value);
  MS_EXCEPTION_IF_NULL(seq);
  MS_EXCEPTION_IF_NULL(value);
  bool is_tuple = value->isa<ValueTuple>();
  oss << (is_tuple ? "(" : "[");
  bool first_flag = true;
  for (auto elem : seq->value()) {
    if (first_flag) {
      first_flag = false;
    } else {
      oss << ", ";
    }
    oss << GetValueText(elem, gsub);
  }
  oss << (is_tuple ? ")" : "]");
  return oss.str();
}

std::string GetDictText(const ValuePtr &value, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  std::ostringstream oss;
  MS_EXCEPTION_IF_NULL(value);
  ValueDictionaryPtr dict = value->cast<ValueDictionaryPtr>();
  oss << "{";
  bool first_flag = true;
  for (const auto &elem : dict->value()) {
    if (first_flag) {
      first_flag = false;
    } else {
      oss << ", ";
    }
    oss << "\"" << elem.first->ToString() << "\": " << GetValueText(elem.second, gsub);
  }
  oss << "}";
  return oss.str();
}

std::string GetOtherValueText(const ValuePtr &value) {
  std::ostringstream oss;
  oss << value->type_name() << "[" << value->ToString() << "]";
  return oss.str();
}

static bool CanUseDumpText(const ValuePtr &value) {
  return (value->isa<RefKey>() || value->isa<Scalar>() || value->isa<StringImm>() || value->isa<tensor::Tensor>() ||
          value->isa<parse::Symbol>() || value->isa<None>() || value->isa<Null>() || value->isa<ValueSlice>() ||
          value->isa<Type>() || value->isa<KeywordArg>() || value->isa<SymbolicKeyInstance>());
}

std::string GetValueText(const ValuePtr &value, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  MS_EXCEPTION_IF_NULL(value);
  if (value->isa<Primitive>()) {
    return GetPrimitiveText(value->cast<PrimitivePtr>());
  }
  if (value->isa<MetaFuncGraph>()) {
    MetaFuncGraphPtr meta_func_graph = value->cast<MetaFuncGraphPtr>();
    return GetMetaFuncGraphText(meta_func_graph);
  }
  if (value->isa<ValueSequence>()) {
    return GetSequenceText(value, gsub);
  }
  if (value->isa<ValueDictionary>()) {
    return GetDictText(value, gsub);
  }
  if (CanUseDumpText(value)) {
    return value->DumpText();
  } else {
    return GetOtherValueText(value);
  }
}

void PrintTupleNodeUsedFlags(std::ostringstream &buffer, const abstract::AbstractSequencePtr &sequence_abs) {
  if (sequence_abs == nullptr || sequence_abs->sequence_nodes() == nullptr || sequence_abs->sequence_nodes()->empty()) {
    return;
  }

  buffer << ", sequence_nodes={";
  for (size_t i = 0; i < sequence_abs->sequence_nodes()->size(); ++i) {
    auto node = (*sequence_abs->sequence_nodes())[i].lock();
    if (node == nullptr) {
      MS_LOG(DEBUG) << "The node in sequence_nodes is free.";
      buffer << "node={<freed node>}";
    } else {
      buffer << "node={" << node->DebugString();
      auto flags = GetSequenceNodeElementsUseFlags(node);
      if (flags != nullptr) {
        buffer << ", elements_use_flags: {ptr: " << flags << ", value: " << (*flags) << "}";
      }
      buffer << "}";
    }
    if (i != sequence_abs->sequence_nodes()->size() - 1) {
      buffer << ", ";
    }
  }
  buffer << "}";
}

void PrintNodeOutputType(std::ostringstream &buffer, const AnfNodePtr &node) {
  if (node == nullptr) {
    return;
  }

  ValuePtr tensor_value = nullptr;
  StringImmPtr ref_key = nullptr;
  abstract::AbstractSequencePtr sequence_abs = nullptr;
  auto abstract = node->abstract();
  if (abstract != nullptr) {
    if (abstract->isa<abstract::AbstractTensor>()) {
      tensor_value = abstract->BuildValue();
    }
    if (auto ref_tensor = abstract->cast_ptr<abstract::AbstractRefTensor>(); ref_tensor != nullptr) {
      ref_key = dyn_cast<StringImm>(ref_tensor->ref_key_value());
    } else if (auto map_tensor = abstract->cast_ptr<abstract::AbstractMapTensor>(); map_tensor != nullptr) {
      ref_key = dyn_cast<StringImm>(map_tensor->ref_key_value());
    }
    sequence_abs = dyn_cast<abstract::AbstractSequence>(abstract);
  }

  abstract::BaseShapePtr shape = dyn_cast<abstract::BaseShape>(node->Shape());
  TypePtr type = dyn_cast<Type>(node->Type());
  if ((shape != nullptr) && (type != nullptr)) {
    buffer << "<" << type << ", " << shape->ToString();
    if (tensor_value != nullptr && tensor_value != kValueAny) {
      buffer << ", value=...";
    }
    if (ref_key != nullptr) {
      buffer << ", ref_key=" << ref_key->value();
    }
    PrintTupleNodeUsedFlags(buffer, sequence_abs);
    buffer << ">";
  } else if (type != nullptr) {
    buffer << "<" << type;
    if (tensor_value != nullptr && tensor_value != kValueAny) {
      buffer << ", value=...";
    }
    if (ref_key != nullptr) {
      buffer << ", ref_key=" << ref_key->value();
    }
    PrintTupleNodeUsedFlags(buffer, sequence_abs);
    buffer << ">";
  } else {
    buffer << "<null>";
  }
}

void PrintNodeInputType(std::ostringstream &buffer, const AnfNodePtr &node) {
  if (node == nullptr) {
    return;
  }

  const auto &inputs = GetInputs(node);
  size_t len = inputs.size();
  if (len > 1) {
    // Skip inputs[0] which is Primitive value node
    for (size_t i = 1; i < len; ++i) {
      AnfNodePtr in = inputs[i];
      if (i != 1) {
        buffer << ", ";
      }
      PrintNodeOutputType(buffer, in);
    }
  }
}

void PrintNodeOutputSymbolicInfo(std::ostringstream &buffer, const AnfNodePtr &node) {
  if (node == nullptr) {
    return;
  }
  auto abstract = node->abstract();
  if (abstract == nullptr) {
    buffer << "<null>";
    return;
  }
  auto shape = abstract->GetSymbolicShape();
  auto value = abstract->GetSymbolicValue();
  if (shape != nullptr || value != nullptr) {
    if (shape != nullptr) {
      buffer << "S:" << shape->ToString();
    }
    if (value != nullptr) {
      buffer << "V:" << value->ToString();
    }
  } else {
    buffer << "<null>";
  }
}

void PrintNodeInputSymbolicInfo(std::ostringstream &buffer, const AnfNodePtr &node) {
  if (node == nullptr) {
    return;
  }
  const auto &inputs = GetInputs(node);
  if (inputs.size() <= 1) {
    return;
  }
  for (size_t i = 1; i < inputs.size(); ++i) {
    if (i != 1) {
      buffer << ", ";
    }
    PrintNodeOutputSymbolicInfo(buffer, inputs[i]);
  }
}

void DumpSymbolicInfo(const AnfNodePtr &node, const FuncGraphPtr &fg, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  if (node == nullptr || fg == nullptr || gsub == nullptr || fg->symbol_engine() == nullptr) {
    return;
  }
  if (node != fg->get_return()) {
    gsub->buffer << "      : (";
    PrintNodeInputSymbolicInfo(gsub->buffer, node);
    gsub->buffer << ") -> (";
    PrintNodeOutputSymbolicInfo(gsub->buffer, node);
    gsub->buffer << ")";
  } else {
    gsub->buffer << "      : (";
    PrintNodeInputSymbolicInfo(gsub->buffer, node);
    gsub->buffer << ")";
  }
  gsub->buffer << std::endl;
}

void PrintParamSymbolicShape(std::ostringstream &buffer, const AnfNodePtr &node) {
  if (node == nullptr) {
    return;
  }
  auto abstract = node->abstract();
  if (abstract == nullptr) {
    return;
  }
  SymbolPtr shape = abstract->GetSymbolicShape();
  if (shape != nullptr) {
    buffer << " : " << shape->ToString();
  }
}

void GatherInputAndOutputInferType(std::ostringstream &buffer, const AnfNodePtr &node) {
  buffer << "      : (";
  PrintNodeInputType(buffer, node);
  buffer << ") -> (";
  PrintNodeOutputType(buffer, node);
  buffer << ")";
}

void DumpGlobalInfoEntry(const FuncGraphPtr &graph, std::ostringstream &buffer, size_t sub_graphs_size) {
  if (graph == nullptr) {
    return;
  }

  buffer << "# IR entry: @" << graph->ToString() << std::endl;
  buffer << "# Total subgraphs: " << sub_graphs_size << std::endl;
  buffer << std::endl;

  if (!graph->attrs().empty()) {
    buffer << "# Attrs:" << std::endl;
    for (const auto &attr : graph->attrs()) {
      buffer << attr.first << ": ";
      if (attr.second->isa<BoolImm>()) {
        buffer << GetValue<bool>(attr.second);
      } else if (attr.second->isa<StringImm>()) {
        buffer << GetValue<std::string>(attr.second);
      }
      buffer << std::endl;
    }
    buffer << std::endl;
  }
}

void DumpKernelObjectType(const CNodePtr &node, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  auto inputs_str = AnfDumpHandler::PrintInputKernelObjectTypes(node);
  auto outputs_str = AnfDumpHandler::PrintOutputKernelObjectTypes(node);
  if (inputs_str.empty() && outputs_str.empty()) {
    return;
  }
  gsub->buffer << "      : (" << inputs_str << ") -> (" << outputs_str << ")" << std::endl;
}

void DumpKernelInfo(const CNodePtr &node, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  if (node == nullptr || gsub == nullptr) {
    return;
  }
  auto kernel_info = node->kernel_info();
  if (kernel_info == nullptr || !kernel_info->has_build_info()) {
    return;
  }
  if (!AnfUtils::IsRealKernel(node)) {
    DumpKernelObjectType(node, gsub);
    return;
  }

  gsub->buffer << "      : (";
  gsub->buffer << AnfDumpHandler::PrintInputTypeShapeFormat(node);
  gsub->buffer << ") -> (";
  gsub->buffer << AnfDumpHandler::PrintOutputTypeShapeFormat(node);
  gsub->buffer << ")";
  gsub->buffer << std::endl;
  DumpKernelObjectType(node, gsub);
}

int32_t DumpParams(const FuncGraphPtr &graph, std::ostringstream &buffer, OrderedMap<AnfNodePtr, int32_t> *para_map) {
  if (graph == nullptr) {
    MS_LOG(INFO) << "Parameter \'graph\' should not be null.";
    return 0;
  }
  std::vector<AnfNodePtr> parameters = graph->parameters();
  buffer << "# Total params: " << parameters.size() << std::endl;

  if (parameters.empty()) {
    return 0;
  }
  buffer << "# Params:" << std::endl;
  // Dump parameters
  int32_t para_num = 1;
  for (const auto &param : parameters) {
    if (param == nullptr) {
      continue;
    }
    auto parameter_ptr = param->cast<ParameterPtr>();
    if (parameter_ptr == nullptr) {
      MS_LOG(INTERNAL_EXCEPTION) << "param cannot cast to ParameterPtr";
    }
    buffer << "%para" << para_num << "_" << parameter_ptr->name() << ": ";
    // Print parameters' type and shape
    PrintNodeOutputType(buffer, param);
    PrintParamSymbolicShape(buffer, param);
    if (parameter_ptr->has_default()) {
      buffer << "  :  has_default";
    }
    auto kernel_info = param->kernel_info();
    if (kernel_info != nullptr && kernel_info->has_build_info()) {
      buffer << "  :  ";
      buffer << AnfDumpHandler::PrintOutputTypeShapeFormat(param);
      buffer << "  :  IsWeight: " << std::boolalpha << common::AnfAlgo::IsParameterWeight(parameter_ptr);
    }
    buffer << std::endl;

    if (para_map != nullptr) {
      (*para_map)[param] = para_num++;
    }
    if (param->func_graph() == nullptr) {
      MS_LOG(EXCEPTION) << "Get func graph nullptr, node " << param->DebugString();
    }
    MS_LOG(DEBUG) << "Record param: " << param->ToString() << " graph belong : " << param->func_graph()->ToString();
  }
  return para_num;
}

void DumpParameterOperator(const AnfNodePtr &node, const std::shared_ptr<SubGraphIRInfo> &gsub, const AnfNodePtr &op) {
  if (op->func_graph() != nullptr && op->func_graph() != node->func_graph()) {
    gsub->buffer << "$(@" << op->func_graph()->ToString() << ":";
  }
  gsub->buffer << op->ToString();
  if (op->func_graph() != nullptr && op->func_graph() != node->func_graph()) {
    gsub->buffer << ")";
  }
  std::string func_str = GetNodeFuncStr(op);
  if (!func_str.empty()) {
    gsub->buffer << "[@" << func_str << "]";
  }
}

void DumpOperator(const AnfNodePtr &node, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  if (gsub == nullptr) {
    MS_LOG(INFO) << "Parameter \'gsub\' should not be null.";
    return;
  }
  auto cnode = dyn_cast<CNode>(node);
  if (cnode == nullptr) {
    MS_LOG(INTERNAL_EXCEPTION) << "Parameter \'node\' should be a CNode";
  }
  AnfNodePtr op = cnode->input(0);
  MS_EXCEPTION_IF_NULL(op);
  if (IsValueNode<FuncGraph>(op)) {
    FuncGraphPtr fg = GetValueNode<FuncGraphPtr>(op);
    if (fg != nullptr) {
      gsub->buffer << "call @" << fg->ToString();
    }
  } else if (op->isa<CNode>()) {
    std::string func_str = GetNodeFuncStr(op);
    if (gsub->local_var_map.find(op) != gsub->local_var_map.end()) {
      gsub->buffer << "%" << gsub->local_var_map[op];
    } else {
      auto input = op->cast<CNodePtr>();
      auto fg = input->func_graph();
      if (fg == nullptr) {
        MS_LOG(EXCEPTION) << "Get func graph nullptr, node " << node->DebugString();
      }
      gsub->buffer << "$(@" << fg->ToString() << ":" << input->ToString() << ")";
    }
    if (!func_str.empty()) {
      gsub->buffer << "[@" << func_str << "]";
    }
  } else if (op->isa<ValueNode>()) {
    auto value = GetValueNode(op);
    if (value != nullptr) {
      if (value->isa<Primitive>()) {
        gsub->buffer << value->ToString();
      } else {
        gsub->buffer << GetValueText(value, gsub);
      }
    }
  } else {
    // It's Parameter.
    DumpParameterOperator(node, gsub, op);
  }
}

void DumpParamterInOperand(const AnfNodePtr &node, const AnfNodePtr &in,
                           const OrderedMap<AnfNodePtr, int32_t> &para_map,
                           const std::shared_ptr<SubGraphIRInfo> &gsub) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(node->func_graph());
  MS_EXCEPTION_IF_NULL(in);
  MS_EXCEPTION_IF_NULL(gsub);
  if (in->func_graph() == nullptr) {
    MS_LOG(INFO) << "Parameter should belong to a func graph. Check func graph: " << node->func_graph();
  }
  if (in->func_graph() != nullptr && in->func_graph() != node->func_graph()) {
    gsub->buffer << "$(@" << in->func_graph()->ToString() << ":";
  } else {
    gsub->buffer << "%";
  }
  auto iter = para_map.find(in);
  if (iter == para_map.end()) {
    gsub->buffer << "para_" << in->ToString();
  } else {
    gsub->buffer << "para" << iter->second << "_" << in->ToString();
  }
  if (in->func_graph() != nullptr && in->func_graph() != node->func_graph()) {
    gsub->buffer << ")";
  }
}

void DumpOperands(const AnfNodePtr &node, const OrderedMap<AnfNodePtr, int32_t> &para_map,
                  const std::shared_ptr<SubGraphIRInfo> &gsub) {
  if (node == nullptr || gsub == nullptr) {
    return;
  }

  gsub->buffer << "(";
  const auto &inputs = GetInputs(node);
  size_t len = inputs.size();
  if (len > 1) {
    // Skip inputs[0] which is Primitive valuenode
    for (size_t i = 1; i < len; ++i) {
      AnfNodePtr in = inputs[i];
      MS_EXCEPTION_IF_NULL(in);
      if (i != 1) {
        gsub->buffer << ", ";
      }
      if (in->isa<Parameter>()) {
        DumpParamterInOperand(node, in, para_map, gsub);
      } else if (in->isa<CNode>()) {
        auto iter = gsub->local_var_map.find(in);
        if (iter != gsub->local_var_map.end()) {
          gsub->buffer << "%" << iter->second;
        } else {
          auto input = in->cast<CNodePtr>();
          auto fg = input->func_graph();
          if (fg == nullptr) {
            MS_LOG(EXCEPTION) << "Get func graph nullptr, node " << input->DebugString();
          }
          gsub->buffer << "$(@" << fg->ToString() << ":" << input->ToString() << ")";
        }
      } else if (in->isa<ValueNode>() && !IsValueNode<FuncGraph>(in)) {
        // ValueNode except FuncGraph.
        gsub->buffer << GetValueText(GetValueNode(in), gsub);
      } else if (IsValueNode<FuncGraph>(in)) {
        FuncGraphPtr fg = GetValueNode<FuncGraphPtr>(in);
        if (fg == nullptr) {
          MS_LOG(EXCEPTION) << "Get func graph nullptr, node " << in->DebugString();
        }
        gsub->buffer << "@" << fg->ToString();
      } else if (AnfUtils::IsCustomActorNode(in)) {
        gsub->buffer << "%" << AnfUtils::GetCustomActorName(in);
      } else {
        gsub->buffer << in->ToString();
      }
    }
  }
  gsub->buffer << ")";
}

void DumpParallelInfo(const CNodePtr &node, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  if ((node == nullptr) || (gsub == nullptr)) {
    return;
  }

  ValuePtr in_tmp = AnfDumpHandler::InStrategyValue(node);
  if (in_tmp == nullptr) {
    return;
  }
  gsub->buffer << " {in_strategy: ";
  gsub->buffer << in_tmp->ToString();

  ValuePtr out_tmp = AnfDumpHandler::OutStrategyValue(node);
  if (out_tmp != nullptr) {
    gsub->buffer << ", out_strategy: ";
    gsub->buffer << out_tmp->ToString();
  }

  gsub->buffer << "}";
}

void DumpAttrs(const mindspore::HashMap<std::string, ValuePtr> &attrs, const std::shared_ptr<SubGraphIRInfo> &gsub,
               bool check_strategy = false) {
  int i = 0;
  for (const auto &attr : attrs) {
    if (check_strategy && attr.first == PARALLEL_STRATEGY) {
      continue;  // Skip the strategy
    }
    if (i++ != 0) {
      gsub->buffer << ", ";
    }
    gsub->buffer << attr.first << ": ";
    if (attr.second == nullptr) {
      gsub->buffer << "null";
    } else {
      if (CanUseDumpText(attr.second)) {
        gsub->buffer << attr.second->DumpText();
      } else {
        gsub->buffer << attr.second->ToString();
      }
    }
  }
}

void DumpOperateAttrs(const AnfNodePtr &op, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  if (op == nullptr || gsub == nullptr) {
    return;
  }

  if (IsValueNode<Primitive>(op)) {
    PrimitivePtr primitive = GetValueNode<PrimitivePtr>(op);
    if (!primitive->instance_name().empty()) {
      gsub->buffer << " {";
      gsub->buffer << "instance name"
                   << ": ";
      gsub->buffer << primitive->instance_name();
      gsub->buffer << "}";
    }
    auto attrs = primitive->attrs();
    if (!attrs.empty()) {
      gsub->buffer << " primitive_attrs: {";
      DumpAttrs(attrs, gsub, true);
      gsub->buffer << "}";
    }
  }
}

void DumpCNodeAttrs(const CNodePtr &op, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  if (op == nullptr || gsub == nullptr) {
    return;
  }
  if (op->attrs().empty()) {
    return;
  }

  auto attrs = op->attrs();
  gsub->buffer << " cnode_attrs: {";
  DumpAttrs(attrs, gsub);
  gsub->buffer << "}";
}

void DumpCNodePrimalAttrs(const CNodePtr &op, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  if (op == nullptr || gsub == nullptr) {
    return;
  }
  if (op->primal_attrs().empty()) {
    return;
  }
  auto primal_attrs = op->primal_attrs();
  gsub->buffer << " cnode_primal_attrs: {";
  DumpAttrs(primal_attrs, gsub);
  gsub->buffer << "}";
}

void DumpShape(const AnfNodePtr &node, const FuncGraphPtr &sub_graph, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  if (node == nullptr || sub_graph == nullptr || gsub == nullptr) {
    return;
  }

  gsub->buffer << std::endl;
  if (node != sub_graph->get_return()) {
    gsub->buffer << "      : (";
    PrintNodeInputType(gsub->buffer, node);
    gsub->buffer << ") -> (";
    PrintNodeOutputType(gsub->buffer, node);
    gsub->buffer << ")";
  } else {
    gsub->buffer << "      : (";
    PrintNodeInputType(gsub->buffer, node);
    gsub->buffer << ")";
  }

  gsub->buffer << std::endl;
}

void DumpLocationInCurrentScope(const DebugInfoPtr &debug_info, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  auto dump_debug_info = debug_info;
  std::list<DebugInfoPtr> need_dump_debug_infos;
  while (dump_debug_info != nullptr) {
    need_dump_debug_infos.push_front(dump_debug_info);
    if (dump_debug_info->trace_info() == nullptr) {
      break;
    }
    dump_debug_info = dump_debug_info->trace_info()->debug_info();
  }
  HashSet<std::string> visited_locations;
  for (const auto &cur_debug_info : need_dump_debug_infos) {
    if (cur_debug_info->location() != nullptr) {
      auto prefix = cur_debug_info->inlined() ? "      # inlined:" : "      # ";
      auto debug_info_str = trace::GetDebugInfoStr(cur_debug_info, "", kSourceLineTipDiscard);
      if (visited_locations.find(debug_info_str) == visited_locations.cend()) {
        gsub->buffer << prefix << debug_info_str << "\n";
        (void)visited_locations.insert(debug_info_str);
      }
    }
  }
}

void DumpPrimalDebugInfos(const CNodePtr &node, const std::shared_ptr<SubGraphIRInfo> &gsub) {
  MS_EXCEPTION_IF_NULL(node);
  auto primal_debug_infos = node->primal_debug_infos();
  if (!primal_debug_infos.empty()) {
    for (const auto &primal_debug_info : primal_debug_infos) {
      std::string lines;
      auto debug_info_str = trace::GetDebugInfoStr(primal_debug_info, "      # ", kSourceLineTipDiscard);
      if (!debug_info_str.empty()) {
        lines += debug_info_str + "\n";
      }
      gsub->buffer << "      # Corresponding forward node candidate:\n";
      if (!lines.empty()) {
        gsub->buffer << lines;
      }
    }
  }
}

void DumpDebugInfo(const CNodePtr &node, const std::shared_ptr<SubGraphIRInfo> &gsub,
                   const LocDumpMode &dump_location) {
  MS_EXCEPTION_IF_NULL(node);
  // Dump comments firstly.
  if (node->debug_info() != nullptr) {
    const auto &debug_info = trace::GetSourceCodeDebugInfo(node->debug_info());
    if (debug_info->location() != nullptr) {
      const auto &comments = debug_info->location()->comments();
      if (!comments.empty()) {
        gsub->buffer << "      # Comment:\n";
        for (auto &comment : comments) {
          gsub->buffer << "        " << comment << '\n';
        }
      }
    }
  }

  // Dump line info.
  if (dump_location == kTopStack) {
    auto fused_debug_infos = node->fused_debug_infos();
    if (!fused_debug_infos.empty()) {
      for (const auto &debug_info : fused_debug_infos) {
        std::string lines;
        gsub->buffer << "      # Corresponding code candidate:\n";
        auto debug_info_str = trace::GetDebugInfoStr(debug_info, "      # ", kSourceLineTipDiscard);
        if (!debug_info_str.empty()) {
          lines += debug_info_str + "\n";
        }
        if (!lines.empty()) {
          gsub->buffer << lines;
        }
      }
    } else {
      auto debug_info_str = trace::GetDebugInfoStr(node->debug_info(), "      # ", kSourceLineTipDiscard);
      if (!debug_info_str.empty()) {
        gsub->buffer << debug_info_str << "\n";
      }
    }
    DumpPrimalDebugInfos(node, gsub);
  } else if (dump_location == kWholeStack) {
    auto fused_debug_infos = node->fused_debug_infos();
    if (!fused_debug_infos.empty()) {
      for (const auto &debug_info : fused_debug_infos) {
        gsub->buffer << "      # Corresponding code candidate:\n";
        DumpLocationInCurrentScope(debug_info, gsub);
      }
    } else {
      DumpLocationInCurrentScope(node->debug_info(), gsub);
    }
    // Print whole stack primal infos
    auto primal_debug_infos = node->primal_debug_infos();
    if (!primal_debug_infos.empty()) {
      for (const auto &primal_debug_info : primal_debug_infos) {
        gsub->buffer << "      # Corresponding forward node candidate:\n";
        DumpLocationInCurrentScope(primal_debug_info, gsub);
      }
    }
  }

  // Dump side effect info.
  auto effect_info = node->GetEffectInfo();
  if (effect_info.HasEffect()) {
    gsub->buffer << "      # " << effect_info.ToString() << '\n';
  }
}

void DumpParameters(const FuncGraphPtr &func_graph, std::ostringstream &oss) {
  std::vector<AnfNodePtr> parameters = func_graph->parameters();
  oss << "# Parameters: " << parameters.size() << ", (";
  if (parameters.size() == 1) {
    MS_EXCEPTION_IF_NULL(parameters[0]);
    PrintNodeOutputType(oss, parameters[0]);
  } else if (parameters.size() > 1) {
    for (size_t idx = 0; idx < parameters.size() - 1; idx++) {
      MS_EXCEPTION_IF_NULL(parameters[idx]);
      PrintNodeOutputType(oss, parameters[idx]);
      oss << ", ";
    }
    MS_EXCEPTION_IF_NULL(parameters[parameters.size() - 1]);
    PrintNodeOutputType(oss, parameters[parameters.size() - 1]);
  }
  oss << ")\n";
}

void DumpCNode(const CNodePtr &node, const FuncGraphPtr &sub_graph, const OrderedMap<AnfNodePtr, int32_t> &para_map,
               const std::shared_ptr<SubGraphIRInfo> &gsub, bool dump_full_name, LocDumpMode dump_location) {
  if (node == nullptr || sub_graph == nullptr || gsub == nullptr) {
    return;
  }

  if (node != sub_graph->get_return()) {
    gsub->buffer << "  %" << gsub->local_var << "(" << node->ToString() << ")"
                 << " = ";
    gsub->local_var_map[node] = gsub->local_var++;
  } else {
    gsub->buffer << "  ";
  }

  if (node->weak_inputs().empty()) {
    MS_LOG(INTERNAL_EXCEPTION) << "Input of CNode is empty";
  }

  // Print operator
  DumpOperator(node, gsub);

  // Print operands
  DumpOperands(node, para_map, gsub);

  if (gsub->format_level > kBasicLevel) {
    // Print operator attrs
    AnfNodePtr op = node->input(0);
    DumpOperateAttrs(op, gsub);

    // Print cnode attrs
    DumpCNodeAttrs(node, gsub);

    // Print cnode primal attrs
    DumpCNodePrimalAttrs(node, gsub);

    // Print parallel info
    DumpParallelInfo(node, gsub);
  }

  if (gsub->format_level > kBasicLevel || node == sub_graph->get_return()) {
    // Print shape info
    DumpShape(node, sub_graph, gsub);

    // Print symbolic shape or symbolic value
    DumpSymbolicInfo(node, sub_graph, gsub);

    // Print kernel info
    DumpKernelInfo(node, gsub);
  } else {
    gsub->buffer << std::endl;
  }

  // Use environment variables to control extra info.
  if (gsub->format_level > kAdvancedLevel) {
    if (dump_full_name) {
      gsub->buffer << "      # Fullname with scope: (" << node->fullname_with_scope() << ")" << std::endl;
    } else {
      gsub->buffer << "      # Scope: (" << node->scope()->name() << ")" << std::endl;
    }
    // Print debug info
    DumpDebugInfo(node, gsub, dump_location);
  }
}

void OutputOrderList(const FuncGraphPtr &sub_graph, std::ostringstream &oss) {
  auto &order_list = sub_graph->order_list();
  if (order_list.empty()) {
    return;
  }
  constexpr int width = 4;
  oss << "# Order:\n";
  int i = 1;
  for (auto &weak_node : order_list) {
    const auto &node = weak_node.lock();
    if (node != nullptr) {
      oss << '#' << std::setw(width) << i << ": " << node->DebugString() << '\n';
    }
    ++i;
  }
}

void DumpSymbolEngine(const FuncGraphPtr &sub_graph, std::ostringstream &oss, int format_level) {
  if (format_level <= kAdvancedLevel) {
    return;
  }
  if (sub_graph->symbol_engine() != nullptr && sub_graph->symbol_engine()->func_graph() == sub_graph) {
    oss << "\nsymbol engine details:\n";
    oss << sub_graph->symbol_engine()->DumpText();
  }
}

int GetDumpFormatLevel() {
  static std::string format = common::GetEnv("MS_DEV_DUMP_IR_FORMAT");
  int format_level = 2;
  if (format.size() == 1) {
    try {
      format_level = std::stoi(format);
    } catch (const std::invalid_argument &ia) {
      MS_LOG(EXCEPTION) << "Invalid argument: " << ia.what() << " when parse " << format
                        << ". Please set this env variable to number 0-2.";
    }
  } else if (format.size() > 1) {
    MS_LOG(EXCEPTION) << "MS_DEV_DUMP_IR_FORMAT should be a single number with one digit.";
  }

  if (format_level < 0 || format_level > 2) {
    MS_LOG(EXCEPTION) << "Format level can only be from 0 to 2";
  }

  return format_level;
}

void DumpIRInSubgraph(const std::vector<AnfNodePtr> &nodes, OrderedMap<AnfNodePtr, int32_t> *para_map,
                      OrderedMap<FuncGraphPtr, std::shared_ptr<SubGraphIRInfo>> *const sub_graphs, int32_t total_para,
                      bool dump_full_name, LocDumpMode dump_location) {
  if (para_map == nullptr || sub_graphs == nullptr) {
    return;
  }

  for (const auto &node : nodes) {
    MS_EXCEPTION_IF_NULL(node);
    FuncGraphPtr sub_graph = node->func_graph();
    if (sub_graph == nullptr) {
      MS_LOG(DEBUG) << "Node[" << node->ToString() << "] belongs to no graph!";
      continue;
    }
    std::shared_ptr<SubGraphIRInfo> gsub = (*sub_graphs)[sub_graph];
    if (gsub == nullptr) {
      gsub = std::make_shared<SubGraphIRInfo>();
      gsub->local_var = 0;
      gsub->format_level = GetDumpFormatLevel();
      (*sub_graphs)[sub_graph] = gsub;
    }
    std::vector<AnfNodePtr> parameters = sub_graph->parameters();
    for (size_t idx = 0; idx < parameters.size(); idx++) {
      MS_EXCEPTION_IF_NULL(parameters[idx]);
      if ((*para_map).count(parameters[idx]) == 0) {
        (*para_map)[parameters[idx]] = total_para++;
      }
    }
    if (!node->isa<Parameter>()) {
      if (node->isa<CNode>()) {
        // Print and record output of operator if it is not 'Return'
        DumpCNode(node->cast<CNodePtr>(), sub_graph, *para_map, gsub, dump_full_name, dump_location);
      } else if (AnfUtils::IsCustomActorNode(node)) {
        continue;
      } else {
        gsub->buffer << "  " << node->ToString() << std::endl;
      }
    }
  }
}

void DumpSubgraph(const OrderedMap<FuncGraphPtr, std::shared_ptr<SubGraphIRInfo>> *sub_graphs,
                  const FuncGraphPtr &graph, OrderedMap<AnfNodePtr, int32_t> *para_map, std::ostringstream &oss) {
  if (sub_graphs == nullptr || graph == nullptr) {
    return;
  }
  int format_level = GetDumpFormatLevel();
  for (const auto &sg : *sub_graphs) {
    MS_EXCEPTION_IF_NULL(sg.first);
    if (*(sg.first->indirect())) {
      oss << "indirect: " << *(sg.first->indirect()) << "\n";
    }
    if (format_level > kBasicLevel) {
      oss << "subgraph attr:" << std::endl;
      for (const auto &attr : sg.first->attrs()) {
        oss << attr.first << ": ";
        if (attr.second->isa<BoolImm>()) {
          oss << GetValue<bool>(attr.second);
        } else if (attr.second->isa<StringImm>()) {
          oss << (GetValue<std::string>(attr.second));
        }
        oss << std::endl;
      }
      if (sg.first->symbol_engine() != nullptr) {
        oss << "subgraph symbol engine: " << sg.first->symbol_engine()->ToString() << " : "
            << sg.first->symbol_engine().get() << std::endl;
      }
      oss << "subgraph instance: " << sg.first->ToString() << " : " << sg.first.get() << std::endl;

      // Dump side effect info.
      auto effect_info = sg.first->GetEffectInfo();
      if (effect_info.HasEffect()) {
        oss << "# " << effect_info.ToString() << '\n';
      }
      // Dump parameters info.
      if (sg.first != graph) {
        DumpParameters(sg.first, oss);
      }
    }
    if (trace::GetGlobalTraceLabelType() == trace::TraceLabelType::kWithUniqueId) {
      oss << trace::GetDebugInfoStr(sg.first->debug_info(), "# ", kSourceLineTipDiscard) << "#"
          << trace::Label(sg.first->debug_info()) << "\n";
    } else {
      oss << trace::GetDebugInfoStr(sg.first->debug_info(), "# ", kSourceLineTipDiscard) << "\n";
    }
    oss << "subgraph @" << sg.first->ToString();
    if (sg.first->manager() != nullptr && sg.first->parent() != nullptr) {
      oss << " parent: [subgraph @" << sg.first->parent()->ToString() << "]";
    }
    oss << "(";
    if (sg.first != graph) {
      std::vector<AnfNodePtr> parameters = sg.first->parameters();
      if (parameters.size() == 1) {
        MS_EXCEPTION_IF_NULL(parameters[0]);
        oss << "%para" << (*para_map)[parameters[0]] << "_" << parameters[0]->ToString();
      } else if (parameters.size() > 1) {
        for (size_t idx = 0; idx < parameters.size() - 1; idx++) {
          MS_EXCEPTION_IF_NULL(parameters[idx]);
          oss << "%para" << (*para_map)[parameters[idx]] << "_" << parameters[idx]->ToString();
          oss << ", ";
        }
        MS_EXCEPTION_IF_NULL(parameters[parameters.size() - 1]);
        oss << "%para" << (*para_map)[parameters[parameters.size() - 1]] << "_"
            << parameters[parameters.size() - 1]->ToString();
      }
    }
    oss << ") {" << std::endl;
    MS_EXCEPTION_IF_NULL(sg.second);
    oss << sg.second->buffer.str();
    oss << "}" << std::endl;
    OutputOrderList(sg.first, oss);
    DumpSymbolEngine(sg.first, oss, format_level);
    oss << std::endl;
    oss << std::endl;
  }
}

void SetDumpConfigByString(const std::string &str, DumpConfig *dump_config) {
  MS_LOG(INFO) << "Set dump config:" << str;
  static mindspore::HashMap<std::string, enum LocDumpMode> dump_level_map = {
    {kDumpConfigLineLevel0, kOff}, {kDumpConfigLineLevel1, kTopStack}, {kDumpConfigLineLevel2, kWholeStack}};
  auto it = dump_level_map.find(str);
  if (it != dump_level_map.end()) {
    dump_config->dump_line_level = it->second;
    return;
  }
  if (str == kDumpConfigDisableBackend) {
    dump_config->disable_backend_dump = true;
    return;
  }
  if (str == kDumpConfigEnablePassIR) {
    dump_config->enable_dump_pass_ir = true;
    return;
  }
}

std::shared_ptr<OrderedSet<std::string>> GetAllConfigStrings(const std::string &config_full_string) {
  size_t start_pos = 0;
  auto config_strings = std::make_shared<OrderedSet<std::string>>();
  // if '#' is the last char of str, the str is legal, so we use '<=' but not '<'.
  while (start_pos <= config_full_string.size()) {
    auto pos = config_full_string.find('#', start_pos);
    if (pos == std::string::npos) {
      pos = config_full_string.size();
    }
    auto substr = config_full_string.substr(start_pos, pos - start_pos);
    // Skip the '#'
    start_pos = pos + 1;
    if (substr.empty()) {
      continue;
    }
    (void)config_strings->insert(substr);
  }
  return config_strings;
}

bool ConfigsAreLegal(const std::shared_ptr<OrderedSet<std::string>> &config_strings) {
  // Value 'int' is used to mark config group id
  HashMap<std::string, int> config_white_list = {{kDumpConfigLineLevel0, 0},
                                                 {kDumpConfigLineLevel1, 0},
                                                 {kDumpConfigLineLevel2, 0},
                                                 {kDumpConfigDisableBackend, 1},
                                                 {kDumpConfigEnablePassIR, 2}};
  // Key 'int' is config group id, value is the config.
  HashMap<int, std::string> config_groups;
  for (const auto &config_string : *config_strings) {
    auto config_white_list_it = config_white_list.find(config_string);
    if (config_white_list_it == config_white_list.end()) {
      std::ostringstream buffer;
      buffer << "Support configs:\n"
             << "[0]: " << kDumpConfigLineLevel0 << "\n"
             << "[1]: " << kDumpConfigLineLevel1 << "\n"
             << "[2]: " << kDumpConfigLineLevel2 << "\n"
             << "[3]: " << kDumpConfigDisableBackend << "\n"
             << "[4]: " << kDumpConfigEnablePassIR;
      MS_LOG(WARNING) << "Illegal dump config:\n" << config_string << "\n" << buffer.str();
      return false;
    }
    auto group_id = config_white_list_it->second;
    // Check conflict configs.
    auto config_groups_it = config_groups.find(group_id);
    if (config_groups_it != config_groups.end()) {
      const auto &record_config = config_groups_it->second;
      MS_LOG(WARNING) << "Dump configs are conflict. Conflict configs: [" << record_config << "] and [" << config_string
                      << "].\n"
                      << "Please keep only one of them.";
      return false;
    }
    config_groups[group_id] = config_string;
  }
  return true;
}

DumpConfig GetDumpConfig() {
  static DumpConfig dump_config = DumpConfig();
  static bool parsed = false;
  if (parsed) {
    return dump_config;
  }
  parsed = true;
  // Start parse config.
  std::string str(common::GetCompileConfig("DUMP_IR_CONFIG"));
  auto constexpr max_string_len = 100;
  if (str.size() > max_string_len) {
    MS_LOG(WARNING) << "Dump ir config length exceed max length: " << max_string_len;
    return dump_config;
  }
  if (str.empty()) {
    return dump_config;
  }
  auto config_strings = GetAllConfigStrings(str);
  if (!ConfigsAreLegal(config_strings)) {
    return dump_config;
  }
  for (const auto &config : *config_strings) {
    SetDumpConfigByString(config, &dump_config);
  }
  return dump_config;
}

void GetEnvDumpIrLineLevel(LocDumpMode *dump_location) {
  const auto &config = GetDumpConfig();
  if (config.dump_line_level != kInValid) {
    *dump_location = config.dump_line_level;
  }
}

#ifdef ENABLE_DUMP_IR
void DumpIR(const std::string &filename, const FuncGraphPtr &graph, bool dump_full_name, LocDumpMode dump_location,
            const std::string &target_file) {
  GetEnvDumpIrLineLevel(&dump_location);
  if (graph == nullptr) {
    return;
  }
  auto path = GetSaveGraphsPathName(Common::AddId(filename, ".ir"));
  bool need_dump = Common::CheckIfPrintIrPass(filename);
  if (!need_dump) {
    return;
  }
  if (!target_file.empty()) {
    path = target_file;
  }
  auto realpath = Common::CreatePrefixPath(path);
  if (!realpath.has_value()) {
    MS_LOG(ERROR) << "Get real path failed, path=" << path;
    return;
  }

  ChangeFileMode(realpath.value(), S_IWUSR);
  std::ofstream fout(realpath.value());
  std::ostringstream oss;
  std::ostringstream buffer;
  if (!fout.is_open()) {
    MS_LOG(ERROR) << "Open dump file '" << realpath.value() << "' failed!" << ErrnoToString(errno);
    return;
  }

  auto nodes = TopoSort(graph->get_return(), SuccDeeperSimple, AlwaysInclude);
  OrderedMap<AnfNodePtr, int32_t> para_map;
  // Dump global info
  int32_t total_para = DumpParams(graph, oss, &para_map);

  OrderedMap<FuncGraphPtr, std::shared_ptr<SubGraphIRInfo>> sub_graphs;
  // Dump ir in each sub graph
  DumpIRInSubgraph(nodes, &para_map, &sub_graphs, total_para, dump_full_name, dump_location);

  DumpGlobalInfoEntry(graph, buffer, sub_graphs.size());
  buffer << oss.str();
  // Output global info
  fout << buffer.str() << std::endl;
  buffer.str(std::string());
  buffer.clear();

  // Output each sub graph
  DumpSubgraph(&sub_graphs, graph, &para_map, buffer);
  fout << buffer.str();

  fout.close();
  // Set file mode to read only by user
  ChangeFileMode(realpath.value(), S_IRUSR);
}

nlohmann::ordered_json ToJson(const CNodePtr &para_node, const int64_t global_rank_id,
                              std::unordered_map<std::string, std::vector<uint32_t>> group_map) {
  nlohmann::ordered_json args;
  MS_EXCEPTION_IF_NULL(para_node);
  auto abs = para_node->abstract();
  MS_EXCEPTION_IF_NULL(abs);
  auto prim = GetCNodePrimitive(para_node);
  MS_EXCEPTION_IF_NULL(prim);
  args["op_name"] = para_node->UniqueName();
  args["op_type"] = prim->name();
  args["shape"] = abs->BuildShape()->ToString();
  args["data_type"] = abs->BuildType()->ToString();
  args["global_rank_id"] = std::to_string(global_rank_id);
  std::string group_name = "";
  std::string group = "group";
  if (prim->HasAttr(group)) {
    group_name = GetValue<std::string>(prim->GetAttr(group));
  }
  args["comm_group_name"] = group_name;
  if (prim->HasAttr(kAttrGroupRankIds)) {
    auto value_ptr = prim->GetAttr(kAttrGroupRankIds);
    args["comm_group_rank_ids"] = value_ptr->ToString();
    if (group_map.find(group_name) != group_map.end()) {
      std::ostringstream oss;
      oss << "(";
      const std::vector<uint32_t> group_ranks = group_map[group_name];
      (void)std::copy(group_ranks.begin(), group_ranks.end() - 1, std::ostream_iterator<int>(oss, ","));
      oss << group_ranks.back() << ")";
      args["comm_group_rank_ids"] = oss.str();
    }
  }
  if (prim->HasAttr(kAttrSrcRank) && prim->HasAttr(kAttrSrTag)) {
    args["src_rank"] = std::to_string(GetValue<int64_t>(prim->GetAttr(kAttrSrcRank)));
    args["sr_tag"] = std::to_string(GetValue<int64_t>(prim->GetAttr(kAttrSrTag)));
  }
  if (prim->HasAttr(kAttrDestRank) && prim->HasAttr(kAttrSrTag)) {
    args["dest_rank"] = std::to_string(GetValue<int64_t>(prim->GetAttr(kAttrDestRank)));
    args["sr_tag"] = std::to_string(GetValue<int64_t>(prim->GetAttr(kAttrSrTag)));
  }
  return args;
}

void DumpParallelInfo(const FuncGraphPtr &graph, size_t *op_id, nlohmann::ordered_json *args,
                      const int64_t global_rank_id,
                      const std::unordered_map<std::string, std::vector<uint32_t>> &group_map) {
  MS_EXCEPTION_IF_NULL(graph);
  std::list<CNodePtr> graph_orders = graph->GetOrderedCnodes();
  for (auto &node : graph_orders) {
    MS_EXCEPTION_IF_NULL(node);
    if (IsValueNode<FuncGraph>(node->input(0))) {
      FuncGraphPtr sub_graph = node->input(0)->cast<ValueNodePtr>()->value()->cast<FuncGraphPtr>();
      DumpParallelInfo(sub_graph, op_id, args, global_rank_id, group_map);
    } else if (common::AnfAlgo::IsCommunicationOp(node)) {
      (*args)[std::to_string(*op_id)] = ToJson(node, global_rank_id, group_map);
      *op_id = *op_id + 1;
    } else if (node->input(0)->isa<CNode>() && node->input(0)->abstract() != nullptr) {
      auto abs = node->input(0)->abstract();
      if (abs->isa<abstract::FuncGraphAbstractClosure>()) {
        const auto &abstract_func_graph = abs->cast<abstract::FuncGraphAbstractClosurePtr>();
        MS_EXCEPTION_IF_NULL(abstract_func_graph->func_graph());
        DumpParallelInfo(abstract_func_graph->func_graph(), op_id, args, global_rank_id, group_map);
      } else if (abs->isa<abstract::PartialAbstractClosure>()) {
        const auto &abstract_partial_func = abs->cast<abstract::PartialAbstractClosurePtr>();
        const auto &abstract_fn = abstract_partial_func->fn();
        if (abstract_fn->isa<abstract::FuncGraphAbstractClosure>()) {
          const auto &abstract_func_graph = abstract_fn->cast<abstract::FuncGraphAbstractClosurePtr>();
          MS_EXCEPTION_IF_NULL(abstract_func_graph->func_graph());
          DumpParallelInfo(abstract_func_graph->func_graph(), op_id, args, global_rank_id, group_map);
        }
      }
    }
  }
}

void DumpParallelJson(const std::string &filename, const FuncGraphPtr &graph, const int64_t global_rank_id,
                      const std::unordered_map<std::string, std::vector<uint32_t>> &group_map) {
  if (graph == nullptr) {
    return;
  }
  std::string save_path = "";
  if (!common::GetEnv("MA_LOG_DIR").empty()) {
    save_path = common::GetEnv("MA_LOG_DIR");
  }
  auto path = GetSaveGraphsPathName(filename, save_path);
  auto realpath = Common::CreatePrefixPath(path);
  if (!realpath.has_value()) {
    MS_LOG(ERROR) << "Get real path failed, path=" << path;
    return;
  }

  ChangeFileMode(realpath.value(), S_IWUSR);
  std::ofstream fout(realpath.value());
  if (!fout.is_open()) {
    MS_LOG(ERROR) << "Open dump file '" << realpath.value() << "' failed!" << ErrnoToString(errno);
    return;
  }
  size_t op_id = 0;
  nlohmann::ordered_json args;
  args["hccl_algo"] = common::GetEnv("HCCL_ALGO");
  DumpParallelInfo(graph, &op_id, &args, global_rank_id, group_map);
  constexpr size_t json_dump_mode = 2;
  fout << args.dump(json_dump_mode);

  fout.close();
  // Set file mode to read only by user
  ChangeFileMode(realpath.value(), S_IRUSR);
}

void DumpIRHead(const FuncGraphPtr &top_func, std::ostringstream &ofs) {
  MS_EXCEPTION_IF_NULL(top_func);
  auto sub_graphs = top_func->func_graphs_used_total();
  DumpGlobalInfoEntry(top_func, ofs, sub_graphs.size());
  OrderedMap<AnfNodePtr, int32_t> para_map;
  (void)DumpParams(top_func, ofs, &para_map);
  ofs << std::endl;
}

void DumpIR(std::ostringstream &graph_buffer, const FuncGraphPtr &graph, bool dump_full_name,
            LocDumpMode dump_location) {
  GetEnvDumpIrLineLevel(&dump_location);
  if (graph == nullptr) {
    return;
  }
  std::ostringstream oss;
  auto nodes = TopoSort(graph->get_return(), SuccDeeperSimple, AlwaysInclude);
  OrderedMap<AnfNodePtr, int32_t> para_map;
  int32_t total_para = DumpParams(graph, oss, &para_map);

  graph_buffer << "\n";

  OrderedMap<FuncGraphPtr, std::shared_ptr<SubGraphIRInfo>> sub_graphs;
  // Dump ir in each sub graph
  DumpIRInSubgraph(nodes, &para_map, &sub_graphs, total_para, dump_full_name, dump_location);

  // Dump global info
  DumpGlobalInfoEntry(graph, graph_buffer, sub_graphs.size());
  graph_buffer << oss.str();
  // Output each sub graph
  DumpSubgraph(&sub_graphs, graph, &para_map, graph_buffer);
}

void DumpIRForRDR(const std::string &filename, const FuncGraphPtr &graph, bool dump_full_name,
                  LocDumpMode dump_location) {
  GetEnvDumpIrLineLevel(&dump_location);
  if (graph == nullptr) {
    return;
  }
  auto path = Common::AddId(filename, ".ir");
  bool need_dump = Common::CheckIfPrintIrPass(filename);
  if (!need_dump) {
    return;
  }
  auto realpath = Common::CreatePrefixPath(path);
  if (!realpath.has_value()) {
    MS_LOG(ERROR) << "Get real path failed. path=" << path;
    return;
  }

  ChangeFileMode(realpath.value(), S_IWUSR);
  std::ofstream fout(realpath.value());
  std::ostringstream buffer;
  if (!fout.is_open()) {
    MS_LOG(ERROR) << "Open dump file '" << realpath.value() << "' failed!" << ErrnoToString(errno);
    return;
  }

  auto nodes = TopoSort(graph->get_return(), SuccDeeperSimple, AlwaysInclude);
  OrderedMap<AnfNodePtr, int32_t> para_map;
  int32_t total_para = DumpParams(graph, buffer, &para_map);
  OrderedMap<FuncGraphPtr, std::shared_ptr<SubGraphIRInfo>> sub_graphs;
  // Dump ir in each sub graph
  DumpIRInSubgraph(nodes, &para_map, &sub_graphs, total_para, dump_full_name, dump_location);
  // Dump global info
  DumpGlobalInfoEntry(graph, buffer, sub_graphs.size());
  // Output global info
  fout << buffer.str() << std::endl;
  buffer.str(std::string());
  buffer.clear();

  // Output each sub graph
  DumpSubgraph(&sub_graphs, graph, &para_map, buffer);
  fout << buffer.str();

  fout.close();
  // Set file mode to read only by user
  ChangeFileMode(realpath.value(), S_IRUSR);
}

#else
void DumpIR(const std::string &, const FuncGraphPtr &, bool, LocDumpMode, const std::string &) {
  static bool already_printed = false;
  if (already_printed) {
    return;
  }
  already_printed = true;
  MS_LOG(WARNING) << "The functionality of dumping function graph IR is disabled, "
                  << "please recompile source to enable it. See help of building script.";
}

void DumpIR(std::ostringstream &, const FuncGraphPtr &, bool, LocDumpMode) {
  static bool already_printed = false;
  if (already_printed) {
    return;
  }
  already_printed = true;
  MS_LOG(WARNING) << "The functionality of dumping function graph IR is disabled, "
                  << "please recompile source to enable it. See help of building script.";
}

void DumpIRForRDR(const std::string &, const FuncGraphPtr &, bool, LocDumpMode) {
  static bool already_printed = false;
  if (already_printed) {
    return;
  }
  already_printed = true;
  MS_LOG(WARNING) << "The functionality of dumping function graph IR is disabled, "
                  << "please recompile source to enable it. See help of building script.";
}
#endif

void AnfExporter::OuputIrStyleCNodes(const FuncGraphPtr &func_graph, const std::vector<AnfNodePtr> &nodes,
                                     int32_t total_para, std::ostringstream &oss,
                                     OrderedMap<AnfNodePtr, int32_t> *para_map) {
  MS_EXCEPTION_IF_NULL(func_graph);
  auto &parameters = func_graph->parameters();
  std::shared_ptr<SubGraphIRInfo> gsub = std::make_shared<SubGraphIRInfo>();
  ParamIndexMap param_map;
  exported_[func_graph] = param_map;
  gsub->local_var = 0;
  gsub->format_level = GetDumpFormatLevel();
  for (size_t idx = 0; idx < parameters.size(); idx++) {
    MS_EXCEPTION_IF_NULL(parameters[idx]);
    if ((*para_map).count(parameters[idx]) == 0) {
      (*para_map)[parameters[idx]] = total_para++;
    }
  }
  for (const AnfNodePtr &node : nodes) {
    MS_EXCEPTION_IF_NULL(node);
    if (!node->isa<CNode>()) {
      continue;
    }
    auto cnode = node->cast<CNodePtr>();
    auto &inputs = cnode->inputs();
    for (size_t i = 0; i < inputs.size(); ++i) {
      if (IsValueNode<FuncGraph>(inputs[i])) {
        FuncGraphPtr fg = GetValueNode<FuncGraphPtr>(inputs[i]);
        if (!func_graph_set_.contains(fg) && exported_.find(fg) == exported_.end() && export_used_) {
          func_graph_set_.add(fg);
        }
      }
    }
    DumpCNode(cnode, func_graph, *para_map, gsub);
    if (trace::GetGlobalTraceLabelType() == trace::TraceLabelType::kWithUniqueId) {
      gsub->buffer << trace::GetDebugInfoStr(cnode->debug_info(), "      # ", kSourceLineTipDiscard) << "#"
                   << trace::Label(cnode->debug_info()) << "\n";
    } else {
      std::string dgi = trace::GetDebugInfoStr(cnode->debug_info(), "      # ", kSourceLineTipDiscard);
      if (dgi != "") {
        gsub->buffer << trace::GetDebugInfoStr(cnode->debug_info(), "      # ", kSourceLineTipDiscard) << "\n";
      }
    }
  }
  if (!is_top_graph_) {
    if (parameters.size() == 1) {
      MS_EXCEPTION_IF_NULL(parameters[0]);
      oss << "%para" << (*para_map)[parameters[0]] << "_" << parameters[0]->ToString();
    } else if (parameters.size() > 1) {
      for (size_t idx = 0; idx < parameters.size() - 1; idx++) {
        MS_EXCEPTION_IF_NULL(parameters[idx]);
        oss << "%para" << (*para_map)[parameters[idx]] << "_" << parameters[idx]->ToString();
        oss << ", ";
      }
      MS_EXCEPTION_IF_NULL(parameters[parameters.size() - 1]);
      oss << "%para" << (*para_map)[parameters[parameters.size() - 1]] << "_"
          << parameters[parameters.size() - 1]->ToString();
    }
  } else {
    is_top_graph_ = false;
  }
  oss << ") {\n";
  oss << gsub->buffer.str();
}

void AnfExporter::ExportOneFuncGraph(const FuncGraphPtr &func_graph, const TaggedNodeMap &tagged_cnodes_map,
                                     std::ostringstream &oss, int32_t total_para,
                                     OrderedMap<AnfNodePtr, int32_t> *para_map) {
  if (func_graph == nullptr) {
    return;
  }

  std::vector<AnfNodePtr> nodes = TopoSort(func_graph->get_return(), SuccIncoming, AlwaysInclude);

  if (*(func_graph->indirect())) {
    oss << "indirect: " << *(func_graph->indirect()) << "\n";
  }
  oss << "subgraph attr:" << std::endl;
  for (const auto &attr : func_graph->attrs()) {
    oss << attr.first << ": ";
    MS_EXCEPTION_IF_NULL(attr.second);
    if (attr.second->isa<BoolImm>()) {
      oss << GetValue<bool>(attr.second);
    } else if (attr.second->isa<StringImm>()) {
      oss << (GetValue<std::string>(attr.second));
    }
    oss << std::endl;
  }
  oss << "subgraph instance: " << func_graph->ToString() << " : " << func_graph.get() << std::endl;
  // Dump side effect info.
  auto effect_info = func_graph->GetEffectInfo();
  if (effect_info.HasEffect()) {
    oss << "# " << effect_info.ToString() << '\n';
  }
  // Dump parameters info.
  DumpParameters(func_graph, oss);
  if (trace::GetGlobalTraceLabelType() == trace::TraceLabelType::kWithUniqueId) {
    oss << trace::GetDebugInfoStr(func_graph->debug_info(), "# ", kSourceLineTipDiscard) << "#"
        << trace::Label(func_graph->debug_info()) << "\n";
  } else {
    oss << trace::GetDebugInfoStr(func_graph->debug_info(), "# ", kSourceLineTipDiscard) << "\n";
  }
  oss << "subgraph @" << func_graph->ToString();
  if (func_graph->parent() != nullptr) {
    oss << " parent: [subgraph @" << func_graph->parent()->ToString() << "]";
  }
  oss << "(";
  OuputIrStyleCNodes(func_graph, nodes, total_para, oss, para_map);

  oss << "}\n";

  OutputOrderList(func_graph, oss);
}

void ExportGlobalInfoEntry(const FuncGraphPtr &graph, std::ostringstream &buffer, int graph_size) {
  if (graph == nullptr) {
    return;
  }

  buffer << "# IR entry: @" << graph->ToString() << std::endl;
  buffer << "# Total subgraph: " << graph_size;
  buffer << std::endl;
  buffer << std::endl;
  buffer << "# attrs: " << std::endl;
  for (const auto &attr : graph->attrs()) {
    buffer << attr.first << ": ";
    MS_EXCEPTION_IF_NULL(attr.second);
    if (attr.second->isa<BoolImm>()) {
      buffer << GetValue<bool>(attr.second);
    } else if (attr.second->isa<StringImm>()) {
      buffer << (GetValue<std::string>(attr.second));
    }
    buffer << std::endl;
  }
}

void AnfExporter::ExportFuncGraph(const std::string &filename, const FuncGraphPtr &func_graph) {
  if (func_graph == nullptr) {
    return;
  }

  std::ofstream ofs(filename);
  if (!ofs.is_open()) {
    MS_LOG(ERROR) << "Open file '" << filename << "' failed!" << ErrnoToString(errno);
    return;
  }

  param_index_ = 1;
  int graph_size = 0;
  std::ostringstream oss;
  std::ostringstream paramoss;
  TaggedNodeMap tagged_cnodes_map;
  OrderedMap<AnfNodePtr, int32_t> para_map;
  int32_t total_para = DumpParams(func_graph, paramoss, &para_map);
  func_graph_set_.add(func_graph);
  is_top_graph_ = true;
  while (!func_graph_set_.empty()) {
    FuncGraphPtr fg = *func_graph_set_.cbegin();
    ExportOneFuncGraph(fg, tagged_cnodes_map, oss, total_para, &para_map);
    oss << "\n\n";
    (void)func_graph_set_.erase(fg);
    graph_size++;
  }
  std::ostringstream buffer;
  ExportGlobalInfoEntry(func_graph, buffer, graph_size);
  ofs << buffer.str() << paramoss.str() << "\n" << oss.str();
  ofs.close();
}

#ifdef ENABLE_DUMP_IR
void ExportIR(const std::string &filename, const FuncGraphPtr &func_graph) {
  bool need_dump = Common::CheckIfPrintIrPass(filename);
  if (func_graph == nullptr) {
    return;
  }
  if (!need_dump) {
    return;
  }

  auto filepath = GetSaveGraphsPathName(Common::AddId(filename, ".ir"));
  auto real_filepath = Common::CreatePrefixPath(filepath);
  if (!real_filepath.has_value()) {
    MS_LOG(ERROR) << "The export ir path: " << filepath << " is not illegal.";
    return;
  }
  ChangeFileMode(real_filepath.value(), S_IWUSR);
  AnfExporter exporter;
  exporter.ExportFuncGraph(real_filepath.value(), func_graph);
  // Set file mode to read only by user
  ChangeFileMode(real_filepath.value(), S_IRUSR);
}
#else
void ExportIR(const std::string &, const FuncGraphPtr &) {
  static bool already_printed = false;
  if (already_printed) {
    return;
  }
  already_printed = true;
  MS_LOG(WARNING) << "The functionality of dumping function graph IR is disabled, "
                  << "please recompile to enable it. See help of building script.";
}
#endif
}  // namespace mindspore
