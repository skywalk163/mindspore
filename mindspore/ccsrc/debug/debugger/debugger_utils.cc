/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#include "debug/debugger/debugger_utils.h"
#include <vector>
#include <memory>
#include <string>
#include "include/common/debug/anf_dump_utils.h"
#include "include/backend/debug/debugger/debugger.h"
#include "include/backend/debug/data_dump/dump_json_parser.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "kernel/kernel.h"
#include "include/backend/debug/data_dump/e2e_dump.h"
#include "include/common/utils/config_manager.h"
#include "backend/common/session/session_basic.h"

constexpr int kFailure = 1;

using mindspore::kernel::AddressPtr;
using mindspore::kernel::KernelLaunchAddr;
using AddressPtrList = std::vector<mindspore::kernel::AddressPtr>;
using KernelGraph = mindspore::session::KernelGraph;
using AnfAlgo = mindspore::session::AnfRuntimeAlgorithm;

namespace mindspore {
/*
 * Feature group: Online debugger.
 * Target device group: GPU.
 * Runtime category: MindRT.
 * Description: Returns a vector containing real output number.
 */
std::vector<size_t> CheckRealOutput(const std::string &node_name, const size_t &output_size) {
  std::vector<size_t> real_outputs;
  // P.BatchNorm is used for training and inference
  // can add the filter list for more operators here....
  if (node_name == "BatchNorm") {
    MS_LOG(INFO) << "loading node named " << node_name;
    (void)real_outputs.insert(real_outputs.cend(), {0, 3, 4});
  } else {
    // by default, TensorLoader will load all outputs
    for (size_t j = 0; j < output_size; ++j) {
      real_outputs.push_back(j);
    }
  }
  return real_outputs;
}

/*
 * Feature group: Dump, Online debugger.
 * Target device group: GPU, Ascend.
 * Runtime category: MindRT.
 * Description: Get kernel inputs from launch_info and load the inputs from device to host.
 */
void LoadInputs(const CNodePtr &cnode, const KernelLaunchAddr *launch_info, uint32_t exec_order, uint32_t root_graph_id,
                const DeviceContext *device_context, const bool trans_flag) {
  MS_EXCEPTION_IF_NULL(cnode);
  MS_EXCEPTION_IF_NULL(launch_info);
  MS_EXCEPTION_IF_NULL(device_context);
  auto kernel_mod = AnfAlgo::GetKernelMod(cnode);
  std::vector<size_t> ignored_address;
  if (kernel_mod != nullptr) {
    ignored_address = kernel_mod->GetLaunchIgnoredInputAddressIdx();
  }

  // get inputs
  auto kernel_inputs = launch_info->inputs_;
  auto input_size = common::AnfAlgo::GetInputTensorNum(cnode);
  for (size_t j = 0; j < input_size; ++j) {
    auto addr = kernel_inputs[j];
    MS_EXCEPTION_IF_NULL(addr);
    // Ignore the input address that is not used in the kernel launch.
    if (std::find(ignored_address.begin(), ignored_address.end(), j) != ignored_address.end()) {
      MS_LOG(INFO) << "Ignore dump input data for kernel:" << cnode->fullname_with_scope() << " with input index:" << j;
      continue;
    }
    auto input_kernel = cnode->input(j + 1);
    std::string input_kernel_name = GetKernelNodeName(input_kernel);
    auto device_type = AnfAlgo::GetOutputDeviceDataType(input_kernel, kParameterOutputIndex);
    auto host_type = common::AnfAlgo::GetOutputInferDataType(input_kernel, kParameterOutputIndex);
    auto type = trans_flag ? host_type : device_type;
    // For example, this happens with the Depend op
    if (type == kMetaTypeNone) {
      continue;
    }
    auto host_format = kOpFormat_DEFAULT;
    auto device_format =
      E2eDump::IsDeviceTargetGPU() ? kOpFormat_DEFAULT : AnfAlgo::GetOutputFormat(input_kernel, kParameterOutputIndex);

    auto kernel_tensor = std::make_shared<kernel::KernelTensor>(
      addr->addr, addr->size, kernel::GetFormatFromStrToEnum(device_format), device_type, ShapeVector(),
      device_context->device_context_key().device_name_, device_context->device_context_key().device_id_);
    auto device_addr = device_context->device_res_manager_->CreateDeviceAddress(kernel_tensor);
    string input_tensor_name = input_kernel_name + ':' + "0";
    ShapeVector int_shapes;
    GetDumpIntShape(input_kernel, kParameterOutputIndex, NOT_NULL(&int_shapes), trans_flag);
    auto ret = device_addr->LoadMemToHost(input_tensor_name, UintToInt(exec_order), host_format, int_shapes, type, 0,
                                          true, root_graph_id, false, trans_flag);
    if (!ret) {
      MS_LOG(WARNING) << "LoadMemToHost failed:"
                      << ", tensor_name:" << input_tensor_name << ", host_format:" << host_format
                      << ", device_format:" << device_format << ".";
    }
  }
}

/*
 * Feature group: Dump, Online debugger.
 * Target device group: GPU, Ascend.
 * Runtime category: MindRT.
 * Description: Get kernel outputs from launch_info and load the inputs from device to host.
 */
void LoadOutputs(const CNodePtr &cnode, const KernelLaunchAddr *launch_info, uint32_t exec_order,
                 uint32_t root_graph_id, const DeviceContext *device_context, const bool trans_flag) {
  // get outputs
  auto kernel_outputs = launch_info->outputs_;
  auto output_size = AnfAlgo::GetOutputTensorNum(cnode);
  auto node_name = common::AnfAlgo::GetCNodeName(cnode);
  std::string kernel_name = GetKernelNodeName(cnode);
  std::vector<size_t> real_outputs = CheckRealOutput(node_name, output_size);

  for (size_t j : real_outputs) {
    auto addr = kernel_outputs[j];
    auto device_type = AnfAlgo::GetOutputDeviceDataType(cnode, j);
    auto host_type = common::AnfAlgo::GetOutputInferDataType(cnode, j);
    auto type = trans_flag ? host_type : device_type;
    // For example, this happens with the Depend op
    if (type == kMetaTypeNone) {
      continue;
    }

    auto host_format = kOpFormat_DEFAULT;
    auto device_format = E2eDump::IsDeviceTargetGPU() ? kOpFormat_DEFAULT : AnfAlgo::GetOutputFormat(cnode, j);

    auto kernel_tensor = std::make_shared<kernel::KernelTensor>(
      addr->addr, addr->size, kernel::GetFormatFromStrToEnum(device_format), device_type, ShapeVector(),
      device_context->device_context_key().device_name_, device_context->device_context_key().device_id_);
    auto device_addr = device_context->device_res_manager_->CreateDeviceAddress(kernel_tensor);
    string tensor_name = kernel_name + ':' + std::to_string(j);
    ShapeVector int_shapes;
    GetDumpIntShape(cnode, j, NOT_NULL(&int_shapes), trans_flag);
    auto ret = device_addr->LoadMemToHost(tensor_name, UintToInt(exec_order), host_format, int_shapes, type, j, false,
                                          root_graph_id, false, trans_flag);
    if (!ret) {
      MS_LOG(WARNING) << "LoadMemToHost failed:"
                      << ", tensor_name:" << tensor_name << ", host_format:" << host_format
                      << ", device_format:" << device_format << ".!";
    }
  }
}

/*
 * Feature group: Dump, Online debugger.
 * Target device group: Ascend, GPU.
 * Runtime category: MindRT.
 * Description: Returns true if the node needs to be read for Dump or online debugger. This function is used by GPU
 * and Ascend kernel-by-kernel mindRT.
 */
bool CheckReadData(const CNodePtr &cnode) {
  auto debugger = Debugger::GetInstance();
  if (!debugger) {
    return false;
  }
  bool read_data = false;
  auto &dump_json_parser = DumpJsonParser::GetInstance();
  bool dump_enabled = dump_json_parser.DumpEnabledForIter();
  MS_LOG(DEBUG) << "dump_enabled: " << dump_enabled;
  std::string kernel_name = GetKernelNodeName(cnode);
  if (dump_enabled) {
    if (dump_json_parser.NeedDump(kernel_name)) {
      read_data = true;
    }
  }
  if (debugger->debugger_enabled()) {
    read_data = debugger->ReadNodeDataRequired(cnode);
  }
  return read_data;
}

bool IsDeviceTargetGPU() {
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  return context->get_param<std::string>(MS_CTX_DEVICE_TARGET) == kGPUDevice;
}

bool GetTransFlag() {
  auto debugger = Debugger::GetInstance();
  MS_EXCEPTION_IF_NULL(debugger);
  if (debugger->debugger_enabled() || IsDeviceTargetGPU()) {
    return true;
  }
  return DumpJsonParser::GetInstance().trans_flag();
}

/*
 * Feature group: Dump, Online debugger.
 * Target device group: Ascend, GPU.
 * Runtime category: MindRT.
 * Description: Load inputs and outputs of the given node if needed and dump them if dump is enabled, then it performs
 * PostExecuteNode function on the given node for GPU.
 */
void ReadDataAndDump(const CNodePtr &cnode, const KernelLaunchAddr *launch_info, uint32_t exec_order,
                     const DeviceContext *device_context) {
  auto debugger = Debugger::GetInstance();
  if (!debugger) {
    return;
  }
  auto &dump_json_parser = DumpJsonParser::GetInstance();
  bool dump_enabled = dump_json_parser.DumpEnabledForIter();
  MS_LOG(DEBUG) << "dump_enabled: " << dump_enabled;
  auto kernel_graph = std::dynamic_pointer_cast<KernelGraph>(cnode->func_graph());
  MS_EXCEPTION_IF_NULL(kernel_graph);
  auto root_graph_id = kernel_graph->root_graph_id();
  bool trans_flag = GetTransFlag();
  if (debugger->debugger_enabled() || dump_json_parser.InputNeedDump()) {
    LoadInputs(cnode, launch_info, exec_order, root_graph_id, device_context, trans_flag);
  }
  if (debugger->debugger_enabled() || dump_json_parser.OutputNeedDump()) {
    LoadOutputs(cnode, launch_info, exec_order, root_graph_id, device_context, trans_flag);
  }
  // Dump kernel
  if (dump_enabled) {
    MS_EXCEPTION_IF_NULL(kernel_graph);
    auto graph_id = kernel_graph->graph_id();
    // for GPU, nodes are dumped in graph_id directory.
    if (IsDeviceTargetGPU()) {
      debugger->DumpSingleNode(cnode, graph_id);
    } else {
      // for Ascend, node are dumped in root_graph_id directory.
      debugger->DumpSingleNode(cnode, root_graph_id);
    }
    // Clear Dumped data when online debugger is not enabled
    if (!debugger->debugger_enabled()) {
      debugger->ClearCurrentData();
    }
  }
  if (IsDeviceTargetGPU()) {
    // check if the node is last kernel
    bool last_kernel = !common::AnfAlgo::IsInplaceNode(cnode, "skip");
    debugger->PostExecuteNode(cnode, last_kernel);
  }
}

/*
 * Feature group: Dump, Online Debugger.
 * Target device group: Ascend, GPU.
 * Runtime category: MindRT.
 * Description: Returns the error_info when sink_mode is true and we are in online debugger mode or dump mode for
 * GPU, if everything is normal the error_info string will be empty.
 */
std::string CheckDatasetSinkMode(const KernelGraphPtr &graph_ptr) {
  std::string error_info = "";
  bool sink_mode =
    ConfigManager::GetInstance().dataset_mode() == DatasetMode::DS_SINK_MODE || graph_ptr->IsDatasetGraph();
  auto debugger = Debugger::GetInstance();
  MS_EXCEPTION_IF_NULL(debugger);
  if (debugger->CheckDebuggerDumpEnabled() && sink_mode && IsDeviceTargetGPU()) {
    error_info = "e2e_dump is not supported on GPU with dataset_sink_mode=True. Please set dataset_sink_mode=False";
  }
  if (debugger->CheckDebuggerEnabled() && sink_mode) {
    error_info = "Debugger is not supported with dataset_sink_mode=True. Please set dataset_sink_mode=False";
  }
  return error_info;
}

/*
 * Feature group: Online Debugger.
 * Target device group: Ascend.
 * Runtime category: MindRT.
 * Description: Loads graph's outputs and parameters for Ascend super kernel mode.
 */
void LoadDataForDebugger(const KernelGraphPtr &graph_ptr) {
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  if (context->get_param<std::string>(MS_CTX_DEVICE_TARGET) != kAscendDevice) {
    return;
  }
#ifdef ENABLE_DEBUGGER
  auto debugger = Debugger::GetInstance();
  MS_EXCEPTION_IF_NULL(debugger);
  if (!debugger->CheckDebuggerEnabled()) {
    return;
  }
  MS_LOG(INFO) << "Start load step";
  debugger->SetGraphPtr(graph_ptr);
  // load output
  debugger->LoadGraphOutputs();
  // load parameters
  debugger->LoadParametersAndConst();

#endif
}

void Dump(const KernelGraphPtr &graph, uint32_t rank_id) {
  MS_LOG(DEBUG) << "Start!";
  MS_EXCEPTION_IF_NULL(graph);
  E2eDump::DumpData(graph.get(), rank_id);
  MS_LOG(DEBUG) << "Finish!";
}

uint32_t GetRankID() {
  uint32_t rank_id = 0;
  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  auto env_rank_id = common::GetEnv("RANK_ID");
  if (ms_context->get_param<bool>(MS_CTX_ENABLE_HCCL) && !env_rank_id.empty()) {
    // get actual rank id if it's distribution training case.
    rank_id = GetRankId();
  }
  return rank_id;
}

void SuperKernelE2eDump(const KernelGraphPtr &graph) {
#ifndef ENABLE_SECURITY
  Dump(graph, GetRankID());
#endif
}

DebuggerCommand GetCommand(const debugger::EventReply &reply) {
  DebuggerCommand cmd = DebuggerCommand::kUnknownCMD;
  switch (reply.cmd_case()) {
    case debugger::EventReply::CmdCase::kExit:
      cmd = DebuggerCommand::kExitCMD;
      break;
    case debugger::EventReply::CmdCase::kRunCmd:
      cmd = DebuggerCommand::kRunCMD;
      break;
    case debugger::EventReply::CmdCase::kSetCmd:
      cmd = DebuggerCommand::kSetCMD;
      break;
    case debugger::EventReply::CmdCase::kViewCmd:
      cmd = DebuggerCommand::kViewCMD;
      break;
    case debugger::EventReply::CmdCase::kVersionMatched:
      cmd = DebuggerCommand::kVersionMatchedCMD;
      break;
    default:
      MS_LOG(DEBUG) << "Debug: UnknownCMD";
      break;
  }
  return cmd;
}

ProtoVector<debugger::WatchCondition_Parameter> GetParameters(const debugger::EventReply &reply) {
  if (!reply.has_set_cmd() || !reply.set_cmd().has_watch_condition()) {
    MS_LOG(ERROR) << "Error: Can not get Parameters from command. Returning default value: ProtoVector<Parameter>().";
    return ProtoVector<debugger::WatchCondition_Parameter>();
  }
  return reply.set_cmd().watch_condition().params();
}

ProtoVector<debugger::WatchNode> GetWatchnodes(const debugger::EventReply &reply) {
  if (!reply.has_set_cmd()) {
    MS_LOG(ERROR) << "Error: Not SetCMD, can not get WatchNodes. Returning default value: ProtoVector<WatchNode>().";
    return ProtoVector<debugger::WatchNode>();
  }
  return reply.set_cmd().watch_nodes();
}

std::string GetNodeName(const debugger::EventReply &reply) {
  if (!reply.has_run_cmd()) {
    MS_LOG(ERROR) << "Error: Not RunCMD, can not get NodeName. Returning default value: "
                     "";
    return "";
  }
  return reply.run_cmd().node_name();
}

std::string GetRunLevel(const debugger::EventReply &reply) {
  if (!reply.has_run_cmd()) {
    MS_LOG(ERROR) << "Error: Not RunCMD, can not get RunLevel. Returning default value: "
                     "";
    return "";
  }
  return reply.run_cmd().run_level();
}

debugger::WatchCondition GetWatchcondition(const debugger::EventReply &reply) {
  if (!reply.has_set_cmd() || !reply.set_cmd().has_watch_condition()) {
    MS_LOG(ERROR) << "Error: Can not get WatchCondition from command. Returning default value: WatchCondition().";
    return debugger::WatchCondition();
  }
  return reply.set_cmd().watch_condition();
}

int32_t GetWatchpointID(const debugger::EventReply &reply) {
  if (!reply.has_set_cmd()) {
    MS_LOG(ERROR) << "Error: Not SetCMD, can not get Watchpoint ID. Returning default value: 0.";
    return 0;
  }
  return reply.set_cmd().id();
}

bool GetWatchpointDelete(const debugger::EventReply &reply) {
  if (!reply.has_set_cmd()) {
    MS_LOG(ERROR) << "Error: Not SetCMD, can not get Watchpoint delete flag. Returning default value: false.";
    return false;
  }
  return reply.set_cmd().delete_();
}

ProtoVector<debugger::TensorProto> GetTensors(const debugger::EventReply &reply) {
  if (!reply.has_view_cmd()) {
    MS_LOG(ERROR) << "Error: Not ViewCMD, can not get Tensors. Returning default value: ProtoVector<TensorProto>().";
    return ProtoVector<debugger::TensorProto>();
  }
  return reply.view_cmd().tensors();
}

bool GetMiVersionMatched(const debugger::EventReply &reply) { return reply.version_matched(); }

std::string GetTensorFullName(const debugger::TensorProto &tensor) {
  string node_name = tensor.node_name();
  if (tensor.truncate()) {
    // scopes in node name are separated by '/'
    // use the name without scope if truncate is true
    std::size_t found = node_name.find_last_of("/");
    node_name = node_name.substr(found + 1);
  }
  return node_name + ":" + tensor.slot() + (tensor.iter() == "" ? "" : ":" + tensor.iter());
}
}  // namespace mindspore
