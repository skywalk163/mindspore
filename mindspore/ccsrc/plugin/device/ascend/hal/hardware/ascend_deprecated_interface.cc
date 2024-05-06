/**
 * Copyright 2022-2024 Huawei Technologies Co., Ltd
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

#include "plugin/device/ascend/hal/hardware/ascend_deprecated_interface.h"
#include <algorithm>
#include <tuple>
#include <utility>
#include "plugin/device/ascend/hal/hardware/ge_utils.h"
#include "mindspore/ccsrc/include/common/utils/convert_utils_py.h"
#include "plugin/device/ascend/hal/hardware/ge_device_context.h"
#include "include/transform/graph_ir/types.h"
#include "include/transform/graph_ir/utils.h"
#include "include/common/utils/scoped_long_running.h"
#include "transform/graph_ir/op_adapter_map.h"
#include "plugin/device/ascend/hal/device/tensorprint_utils.h"
#include "acl/acl_base.h"
#include "graph/graph_buffer.h"
#include "graph/graph.h"
#include "plugin/device/ascend/hal/common/ascend_utils.h"
#include "plugin/device/ascend/hal/profiler/parallel_strategy_profiling.h"
#include "cxx_api/graph/acl/acl_env_guard.h"
#include "mindspore/core/utils/singleton.h"
#include "utils/ms_context.h"
#include "plugin/device/ascend/hal/device/tensorsummary_utils.h"
#include "plugin/device/ascend/hal/device/tensordump_utils.h"
#include "plugin/device/ascend/hal/device/mbuf_receive_manager.h"
#include "transform/symbol/acl_base_symbol.h"
#include "transform/symbol/acl_rt_symbol.h"
#include "transform/symbol/symbol_utils.h"

using mindspore::abstract::AbstractScalar;
using mindspore::abstract::AbstractTensor;
using mindspore::abstract::AbstractTuple;
using mindspore::abstract::AbstractTuplePtr;
using mindspore::transform::GeTensorPtr;
using mindspore::transform::MeTensorPtr;
using mindspore::transform::Status;

namespace py = pybind11;

namespace mindspore {
namespace device {
namespace ascend {
namespace {
std::mutex g_tsd_mutex;
void ConvertObjectToTensors(const py::dict &dict, transform::TensorOrderMap *const tensors,
                            const FuncGraphPtr &anf_graph) {
  const auto &infer_need_update_parameter_names =
    Singleton<InferNeedUpdateParaNames>::Instance().GetInferParameterNames();
  for (auto item : dict) {
    if ((!py::isinstance<py::str>(item.first))) {
      MS_LOG(WARNING) << "Type of key of py_dict is not string, ignore it.";
      continue;
    }
    std::shared_ptr<tensor::Tensor> tensor;
    std::string name = py::cast<std::string>(item.first);
    bool infer = false;
    auto context_ptr = MsContext::GetInstance();
    MS_EXCEPTION_IF_NULL(context_ptr);
    bool enable_ge = context_ptr->backend_policy() == "ge";
    bool is_train = false;
    if (anf_graph->has_attr("phase")) {
      std::string phase = anf_graph->get_attr("phase")->ToString();
      is_train = phase == "train";
    }
    if (enable_ge && !is_train) {
      infer = true;
    }
    if (infer && infer_need_update_parameter_names.find(name) == infer_need_update_parameter_names.end() &&
        !IsEnableRefMode()) {
      continue;
    }
    if (py::isinstance<py::float_>(item.second.attr("data"))) {
      // convert float to tensor with shape([1])
      tensor = std::make_shared<tensor::Tensor>(kNumberTypeFloat32, std::vector<int64_t>({1}));
      *(static_cast<float *>(tensor->data_c())) = py::cast<float>(item.second.attr("data"));
    } else if (py::isinstance<py::int_>(item.second.attr("data"))) {
      // convert int64_t to tensor with shape([1])
      tensor = std::make_shared<tensor::Tensor>(kNumberTypeInt32, std::vector<int64_t>({1}));
      *(static_cast<float *>(tensor->data_c())) = py::cast<float>(item.second.attr("data"));
    } else if (py::isinstance<tensor::Tensor>(item.second.attr("data"))) {
      // cast tensor
      tensor = py::cast<std::shared_ptr<tensor::Tensor>>(item.second.attr("data"));
    } else if (IsStubTensor(item.second.attr("data"))) {
      // cast stub_tensor
      tensor = ConvertStubTensor(item.second.attr("data"));
    }

    if (tensor == nullptr) {
      MS_LOG(EXCEPTION) << "Get default value for " << name << " failed";
    }
    (void)tensors->emplace(name, tensor);
  }
}

void GetInputTensor(const FuncGraphPtr &anf_graph, const pybind11::dict &init_params,
                    std::vector<transform::GeTensorPtr> *ge_tensors) {
  MS_EXCEPTION_IF_NULL(anf_graph);
  transform::TensorOrderMap init_input_map;
  ConvertObjectToTensors(init_params, &init_input_map, anf_graph);
  std::vector<tensor::TensorPtr> init_input;
  (void)std::transform(init_input_map.begin(), init_input_map.end(), std::back_inserter(init_input),
                       [](const std::pair<std::string, tensor::TensorPtr> &item) { return item.second; });
  *ge_tensors = transform::ConvertInputTensors(init_input, kOpFormat_NCHW);
}
}  // namespace

void AscendDeprecatedInterface::RunInitGraph(const FuncGraphPtr &anf_graph, const pybind11::dict &init_params) {
  MS_EXCEPTION_IF_NULL(anf_graph);
  transform::RunOptions run_options;
  run_options.name = "init_subgraph." + anf_graph->ToString();

  auto graph_runner = transform::CheckAndGetGraphRunner(run_options);
  if (graph_runner == nullptr) {
    return;
  }

  std::vector<transform::GeTensorPtr> ge_outputs;
  std::vector<transform::GeTensorPtr> ge_tensors;
  GetInputTensor(anf_graph, init_params, &ge_tensors);
  {
    // Release GIL before calling into (potentially long-running) C++ code
    mindspore::ScopedLongRunning long_running;
    transform::Status ret = transform::RunGraph(graph_runner, run_options, ge_tensors, &ge_outputs);
    if (ret != transform::Status::SUCCESS) {
      MS_LOG(EXCEPTION) << "Exec " << run_options.name << " graph failed.";
    }
    MS_LOG(INFO) << "Exec " << run_options.name << " graph success.";

    if ((ConfigManager::GetInstance().parallel_strategy() == ParallelStrategy::DISTRIBUTION) &&
        (transform::GetGraphByName(BROADCAST_GRAPH_NAME) != nullptr)) {
      run_options.name = BROADCAST_GRAPH_NAME;
      ret = transform::RunGraph(graph_runner, run_options, ge_tensors, &ge_outputs);
      if (ret != transform::Status::SUCCESS) {
        MS_LOG(EXCEPTION) << "Exec BROADCAST_GRAPH_NAME failed.";
      }
      MS_LOG(INFO) << "Exec broadcast graph success.";
    }
  }
  auto &infer_need_update_parameter_names = Singleton<InferNeedUpdateParaNames>::Instance().GetInferParameterNames();
  infer_need_update_parameter_names.clear();
}

void AscendDeprecatedInterface::DoExecNonInputGraph(const std::string &phase) {
  std::vector<GeTensorPtr> ge_tensors;
  std::vector<GeTensorPtr> ge_outputs;
  transform::RunOptions run_options;
  run_options.name = phase;
  auto graph_runner = transform::GetGraphRunner();
  if (graph_runner == nullptr) {
    MS_LOG(ERROR) << "Can not found GraphRunner";
    return;
  }

  {
    // Release GIL before calling into (potentially long-running) C++ code
    ScopedLongRunning release;
    Status ret = transform::RunGraph(graph_runner, run_options, ge_tensors, &ge_outputs);
    if (ret == Status::NOT_FOUND) {
      MS_LOG(INFO) << "Exec graph:" << run_options.name << "not found, skip.";
      return;
    } else if (ret != Status::SUCCESS) {
      MS_LOG(WARNING) << "Exec graph:" << run_options.name << " failed";
      return;
    }
  }
}

void AscendDeprecatedInterface::ExportDFGraph(const std::string &file_name, const std::string &phase,
                                              const py::object &encrypt, char *key) {
  MS_LOG(DEBUG) << "Export graph begin.";
  transform::DfGraphWrapperPtr wrap_ptr = transform::GetGraphByName(phase);
  if (wrap_ptr == nullptr) {
    MS_LOG(ERROR) << "Get graph form DfGraphManager failed, phase = " << phase;
    return;
  }

  transform::DfGraphPtr ge_graph = wrap_ptr->graph_ptr_;
  if (ge_graph == nullptr) {
    MS_LOG(ERROR) << "Graph is null!";
    return;
  }
  if (key != nullptr) {
    if (py::isinstance<py::none()>(encrypt)) {
      MS_LOG(ERROR) << "ERROR: encrypt is not a function";
      return;
    }
    // get model stream
    ::ge::GraphBuffer model_data;
    auto ge_ret = ge_graph->SaveToMem(model_data);
    if (ge_ret != ::ge::SUCCESS) {
      MS_LOG(ERROR) << "ERROR: GE model save fail";
      return;
    }
    // convert model and key into py::bytes
    const std::string str(reinterpret_cast<const char *>(model_data.GetData()), model_data.GetSize());
    py::bytes model_bytes(str);
    py::bytes key_bytes(key);

    // call python encrypt func
    py::bytes encrypted_model_stream = encrypt(model_bytes, key_bytes);
    if (encrypted_model_stream == py::none()) {
      MS_LOG(ERROR) << "ERROR: Model encrypt fail";
      return;
    }
    // save to file
    std::ofstream ofs(file_name);
    if (!ofs.is_open()) {
      MS_LOG(ERROR) << "ERROR: Open File '" << file_name << "' failed!";
      return;
    }
    ofs << std::string(encrypted_model_stream);
    ofs.close();
  } else {
    if (ge_graph->SaveToFile(file_name) != 0) {
      MS_LOG(EXCEPTION) << "Export air model failed.";
    }
  }
  MS_LOG(INFO) << "Export air model finish.";
}

FuncGraphPtr AscendDeprecatedInterface::BuildDFGraph(const FuncGraphPtr &anf_graph, const pybind11::dict &init_params) {
  MS_EXCEPTION_IF_NULL(anf_graph);
  transform::TensorOrderMap init_tensors{};
  ConvertObjectToTensors(init_params, &init_tensors, anf_graph);
  return GeGraphExecutor::BuildDFGraph(anf_graph, init_tensors, true);
}

void AscendDeprecatedInterface::ClearGraphWrapper() { transform::DfGraphManager::GetInstance().ClearGraph(); }

void AscendDeprecatedInterface::ClearOpAdapterMap() { transform::OpAdapterMap::get().clear(); }

void AscendDeprecatedInterface::DumpProfileParallelStrategy(const FuncGraphPtr &func_graph) {
  return profiler::ascend::ParallelStrategy::GetInstance()->DumpProfileParallelStrategy(func_graph);
}

bool AscendDeprecatedInterface::OpenTsd(const std::shared_ptr<MsContext> &ms_context_ptr) {
  std::unique_lock<std::mutex> lock(g_tsd_mutex);
  MS_EXCEPTION_IF_NULL(ms_context_ptr);
  if (ms_context_ptr->get_param<bool>(MS_CTX_IS_PYNATIVE_GE_INIT)) {
    return true;
  }

  if (ms_context_ptr->get_param<uint32_t>(MS_CTX_TSD_REF) != 0) {
    MS_LOG(DEBUG) << "ACLTDT Dataset client is already opened.";
    ms_context_ptr->increase_param<uint32_t>(MS_CTX_TSD_REF);
    return true;
  }

  auto role = common::GetEnv("MS_ROLE");
  if (strcmp(role.c_str(), "MS_SCHED") == 0 || strcmp(role.c_str(), "MS_PSERVER") == 0) {
    return true;
  }

  uint32_t device_id = ms_context_ptr->get_param<uint32_t>(MS_CTX_DEVICE_ID);

  uint32_t rank_size;
  auto rank_size_env = common::GetEnv("RANK_SIZE");
  if (rank_size_env.empty()) {
    MS_LOG(INFO) << "Should config rank size.";
    rank_size = 1;
  } else {
    int rank_env = std::stoi(rank_size_env);
    if (rank_env <= 0) {
      MS_LOG(EXCEPTION) << "Error rank size " << rank_env << ".";
    }
    rank_size = IntToUint(rank_env);
  }
  (void)ErrorManagerAdapter::Init();
  MS_LOG(INFO) << "Device id = " << device_id << ", rank size = " << rank_size << ".";
  auto ret = CALL_ASCEND_API(aclrtSetDevice, static_cast<int32_t>(device_id));
  if (ret != ACL_ERROR_NONE) {
    MS_LOG(EXCEPTION) << "Device " << device_id << " call aclrtSetDevice failed, ret[" << static_cast<int>(ret)
                      << "]. The details refer to 'Ascend Error Message'.";
  }
  ms_context_ptr->increase_param<uint32_t>(MS_CTX_TSD_REF);

  if (!ms_context_ptr->get_param<bool>(MS_CTX_ENABLE_GE_HETEROGENOUS)) {
    MbufDataHandlerManager::GetInstance().AddHandler(std::make_unique<MbufDataHandler>(
      std::bind(&TensorPrintUtils::PrintReceiveData, &TensorPrintUtils::GetInstance(), std::placeholders::_1),
      device_id, kChannelNameNpuLog, kPrintOpName));
  }

  if (ms_context_ptr->backend_policy() == "ge") {
    MbufDataHandlerManager::GetInstance().AddHandler(std::make_unique<MbufDataHandler>(
      std::bind(&TensorDumpUtils::AsyncSaveDatasetToNpyFile, &TensorDumpUtils::GetInstance(), std::placeholders::_1),
      device_id, tensordump_mapping.first, tensordump_mapping.second));
    for (const std::pair<string, string> &summary_mapping : summary_mappings) {
      MbufDataHandlerManager::GetInstance().AddHandler(
        std::make_unique<MbufDataHandler>(std::bind(SummaryReceiveData, std::placeholders::_1, summary_mapping.first),
                                          device_id, summary_mapping.first, summary_mapping.second));
    }
  }
  return true;
}

bool AscendDeprecatedInterface::CloseTsd(const std::shared_ptr<MsContext> &ms_context_ptr, bool force) {
  std::unique_lock<std::mutex> lock(g_tsd_mutex);
  MS_EXCEPTION_IF_NULL(ms_context_ptr);
  MS_LOG(INFO) << "Start to close tsd, ref = " << ms_context_ptr->get_param<uint32_t>(MS_CTX_TSD_REF);
  if (ms_context_ptr->get_param<uint32_t>(MS_CTX_TSD_REF) == 0) {
    return true;
  }
  ms_context_ptr->decrease_param<uint32_t>(MS_CTX_TSD_REF);
  if (force || ms_context_ptr->get_param<uint32_t>(MS_CTX_TSD_REF) == 0) {
    ms_context_ptr->set_param<uint32_t>(MS_CTX_TSD_REF, 0);
    pybind11::gil_scoped_release gil_release;
    MbufDataHandlerManager::GetInstance().DestoryPrintHandler();
    if (ms_context_ptr->backend_policy() == "ge") {
      MbufDataHandlerManager::GetInstance().DestoryHandler();
    }
    (void)ErrorManagerAdapter::Init();
    uint32_t device_id = ms_context_ptr->get_param<uint32_t>(MS_CTX_DEVICE_ID);
    auto ret = CALL_ASCEND_API(aclrtResetDevice, static_cast<int32_t>(device_id));
    if (ret != ACL_ERROR_NONE) {
      MS_LOG(EXCEPTION) << "Device " << device_id << " call aclrtResetDevice failed, ret[" << static_cast<int>(ret)
                        << "]. The details refer to 'Ascend Error Message'.";
    }
    ms_context_ptr->set_param<bool>(MS_CTX_IS_PYNATIVE_GE_INIT, false);
    MS_LOG(INFO) << "Call aclrtResetDevice, destroy and close tsd successful, ret[" << static_cast<int>(ret) << "]";
  } else {
    MS_LOG(DEBUG) << "Acltdt Dataset client is used, no need to close, tsd reference = "
                  << ms_context_ptr->get_param<uint32_t>(MS_CTX_TSD_REF) << ".";
  }
  return true;
}

bool AscendDeprecatedInterface::IsTsdOpened(const std::shared_ptr<MsContext> &ms_context_ptr) {
  std::unique_lock<std::mutex> lock(g_tsd_mutex);
  if (ms_context_ptr == nullptr) {
    MS_LOG(EXCEPTION) << "nullptr";
  }
  return ms_context_ptr->get_param<uint32_t>(MS_CTX_TSD_REF) > 0;
}

bool AscendDeprecatedInterface::CheckIsAscend910Soc() {
  const char *soc_name_c = CALL_ASCEND_API(aclrtGetSocName);
  if (soc_name_c == nullptr) {
    return false;
  }
  std::string soc_name(soc_name_c);
  if (soc_name.find("910") == std::string::npos) {
    return false;
  }
  return true;
}

void AscendDeprecatedInterface::UnregisterExternalAllocator() {
  auto graph_runner = transform::GetGraphRunner();
  if (graph_runner == nullptr) {
    MS_LOG(INFO) << "The graph_runner is not exist";
    return;
  }
  if (!graph_runner->IsAllocatorRegistered()) {
    return;
  }
  MS_EXCEPTION_IF_NULL(ge_device_context_);
  MS_EXCEPTION_IF_NULL(ge_device_context_->device_res_manager_);
  auto ret = transform::UnregisterExternalAllocator(
    graph_runner, dynamic_cast<GeDeviceResManager *>(ge_device_context_->device_res_manager_.get())->GetStream());
  if (ret != transform::Status::SUCCESS) {
    MS_LOG(EXCEPTION) << "UnregisterExternalAllocator failed";
  }
}
}  // namespace ascend
}  // namespace device
}  // namespace mindspore
