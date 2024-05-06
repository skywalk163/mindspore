/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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
#include "plugin/device/ascend/hal/hccl_adapter/hccl_adapter.h"
#include <dlfcn.h>
#include <map>
#include <algorithm>
#define google ascend_private
#include "common/opskernel/ops_kernel_info_store.h"
#include "common/opskernel/ops_kernel_builder.h"
#include "ge/ge_api_types.h"
#undef google
#include "hccl/hccl.h"
#include "hccl/hcom.h"
#include "utils/log_adapter.h"
#include "utils/ms_utils.h"
#include "utils/ms_context.h"
#include "include/backend/distributed/constants.h"
#include "include/common/utils/parallel_context.h"
#include "include/common/utils/anfalgo.h"
#include "ops/ascend_op_name.h"
#include "ops/framework_op_name.h"

static constexpr const auto kHcclPluginFileName = "libhccl_plugin.so";

#define CHECK_SYMBOL_NULL(symbol)                                                    \
  if ((symbol) == nullptr) {                                                         \
    MS_LOG(WARNING) << #symbol << " is null, hccl has not been inited, do nothing."; \
    return HcclResult::HCCL_E_RESERVED;                                              \
  }

static std::string GenerateCMChiefWorkDevice() {
  if (mindspore::common::GetEnv(mindspore::distributed::kEnvWorkerNum) == "1") {
    // If only one device is used, use 'DEVICE_ID' env if it's set.
    auto device_id_env = mindspore::common::GetEnv("DEVICE_ID");
    return device_id_env.empty() ? "0" : device_id_env;
  } else {
    // If multiple device number is set, we need to find the smallest device id. Set to "0" for now.
    return "0";
  }
}

static std::map<std::string, std::string> GenHcclOptions(uint32_t device_id, std::string_view rank_id,
                                                         std::string_view rank_file = "") {
  std::map<std::string, std::string> default_options_map = {
    {ge::OPTION_EXEC_IS_USEHCOM, "1"},         {ge::OPTION_EXEC_IS_USEHVD, "0"},
    {ge::OPTION_EXEC_HCCL_FLAG, "1"},          {ge::OPTION_EXEC_DEVICE_ID, std::to_string(device_id)},
    {ge::OPTION_EXEC_RANK_ID, rank_id.data()}, {ge::OPTION_EXEC_POD_NAME, rank_id.data()},
    {ge::OPTION_GRAPH_RUN_MODE, "1"},          {ge::OPTION_EXEC_HCCL_FLAG, "1"},
    {ge::OPTION_EXEC_DEPLOY_MODE, "0"}};

  if (!rank_file.empty()) {
    default_options_map.emplace(ge::OPTION_EXEC_RANK_TABLE_FILE, rank_file.data());
  }
  if (mindspore::hccl::HcclAdapter::GetInstance().UseHcclCM()) {
    mindspore::hccl::HcclAdapter::AddCMEnvToHcclOption(&default_options_map);
  }

  return default_options_map;
}

namespace mindspore::hccl {
namespace {
const char kDefaultGroup[] = "__default_group";
constexpr uint32_t kDeviceNumOfServer = 8;
}  // namespace

HcclAdapter &HcclAdapter::GetInstance() {
  static HcclAdapter instance;
  return instance;
}

void HcclAdapter::InitPlugin() {
  if (plugin_handle_ != nullptr) {
    return;
  }

  plugin_handle_ = dlopen(kHcclPluginFileName, RTLD_DEEPBIND | RTLD_NOW | RTLD_LOCAL);
  if (plugin_handle_ == nullptr) {
    MS_LOG(EXCEPTION) << "Dlopen " << kHcclPluginFileName << " failed, result = " << GetDlErrorMsg();
  }
  init_hcom_graph_adapter_ = DlsymFuncObj(InitHcomGraphAdapter, plugin_handle_);
  finalize_hcom_graph_adapter_ = DlsymFuncObj(FinalizeHcomGraphAdapter, plugin_handle_);
  get_hccl_kernel_info_store_ = DlsymFuncObj(GetHcclKernelInfoStore, plugin_handle_);
  get_all_kernel_builder_ = DlsymFuncObj(GetAllKernelBuilder, plugin_handle_);
  init_hccl_comm_ = DlsymFuncObj(HcclCommInitClusterInfo, plugin_handle_);
  finalize_hccl_comm_ = DlsymFuncObj(HcclCommDestroy, plugin_handle_);
  single_op_hccl_get_rank_id_ = DlsymFuncObj(HcclGetRankId, plugin_handle_);
  single_op_hccl_get_rank_size_ = DlsymFuncObj(HcclGetRankSize, plugin_handle_);
  launch_hccl_broadcast_ = DlsymFuncObj(HcclBroadcast, plugin_handle_);
  launch_hccl_all_reduce_ = DlsymFuncObj(HcclAllReduce, plugin_handle_);
  launch_hccl_reduce_ = DlsymFuncObj(HcclReduce, plugin_handle_);
  launch_hccl_reduce_scatter_ = DlsymFuncObj(HcclReduceScatter, plugin_handle_);
  launch_hccl_all_gather_ = DlsymFuncObj(HcclAllGather, plugin_handle_);
  launch_hccl_send_ = DlsymFuncObj(HcclSend, plugin_handle_);
  launch_hccl_recv_ = DlsymFuncObj(HcclRecv, plugin_handle_);
  launch_hccl_barrier_ = DlsymFuncObj(HcclBarrier, plugin_handle_);
  hccl_create_group_ = DlsymFuncObj(HcomCreateGroup, plugin_handle_);
  hccl_destroy_group_ = DlsymFuncObj(HcomDestroyGroup, plugin_handle_);
  hccl_get_rank_id_ = DlsymFuncObj(HcomGetRankId, plugin_handle_);
  hccl_get_rank_size_ = DlsymFuncObj(HcomGetRankSize, plugin_handle_);
  hccl_get_local_rank_id_ = DlsymFuncObj(HcomGetLocalRankId, plugin_handle_);
  hccl_get_local_rank_size_ = DlsymFuncObj(HcomGetLocalRankSize, plugin_handle_);
  hccl_get_world_rank_by_group_rank_ = DlsymFuncObj(HcomGetWorldRankFromGroupRank, plugin_handle_);
  hccl_get_group_rank_by_world_rank_ = DlsymFuncObj(HcomGetGroupRankFromWorldRank, plugin_handle_);
  hccl_exec_initialize_ = DlsymFuncObj(HcomExecInitialize, plugin_handle_);
  hccl_exec_finalize_ = DlsymFuncObj(HcomExecFinalize, plugin_handle_);
  hccl_exec_enqueue_op_ = DlsymFuncObj(HcomExecEnqueueOperation, plugin_handle_);
  hccl_exec_enqueue_all_to_all_v_ = DlsymFuncObj(HcomExecEnqueueAllToAllV, plugin_handle_);
  launch_hccl_all_to_allv_ = DlsymFuncObj(HcclAlltoAllV, plugin_handle_);
  hcom_destroy_ = DlsymFuncObj(HcomDestroy, plugin_handle_);
}

void HcclAdapter::FinalizePlugin() {
  if (plugin_handle_ == nullptr) {
    return;
  }
  init_hcom_graph_adapter_ = nullptr;
  finalize_hcom_graph_adapter_ = nullptr;
  get_hccl_kernel_info_store_ = nullptr;
  get_all_kernel_builder_ = nullptr;
  init_hccl_comm_ = nullptr;
  finalize_hccl_comm_ = nullptr;
  launch_hccl_broadcast_ = nullptr;
  launch_hccl_all_reduce_ = nullptr;
  launch_hccl_reduce_ = nullptr;
  launch_hccl_reduce_scatter_ = nullptr;
  launch_hccl_all_gather_ = nullptr;
  launch_hccl_send_ = nullptr;
  launch_hccl_recv_ = nullptr;
  launch_hccl_barrier_ = nullptr;
  hccl_create_group_ = nullptr;
  hccl_destroy_group_ = nullptr;
  hccl_get_rank_id_ = nullptr;
  hccl_get_local_rank_id_ = nullptr;
  hccl_get_local_rank_size_ = nullptr;
  hccl_get_world_rank_by_group_rank_ = nullptr;
  hccl_get_group_rank_by_world_rank_ = nullptr;
  hccl_get_rank_size_ = nullptr;
  hccl_exec_initialize_ = nullptr;
  hccl_exec_finalize_ = nullptr;
  hccl_exec_enqueue_op_ = nullptr;
  hccl_exec_enqueue_all_to_all_v_ = nullptr;
  launch_hccl_all_to_allv_ = nullptr;
  hcom_destroy_ = nullptr;
  (void)dlclose(plugin_handle_);
  plugin_handle_ = nullptr;
}

HcclMode HcclAdapter::GetCurrentHcclMode() const {
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  bool is_graph_mode = context->get_param<int>(MS_CTX_EXECUTION_MODE) == kGraphMode;
  bool is_task_sink = context->get_param<bool>(MS_CTX_ENABLE_TASK_SINK);
  bool graph_op_run = context->IsKByKExecutorMode();
  if (!is_graph_mode) {
    return HcclMode::kPynative;
  } else if (is_task_sink && !graph_op_run) {
    return HcclMode::kGraph;
  } else {
    return HcclMode::kKernelByKernel;
  }
}

void HcclAdapter::CheckExcutionMode() const {
  auto hccl_mode = GetCurrentHcclMode();
  if (hccl_mode != hccl_mode_ && (!common::UseHostCollective() || UseHcclCM()) &&
      common::GetEnv(kSimulationLevel).empty()) {
    MS_LOG(EXCEPTION) << "HCCL is initialized in " << GetHcclModeString(hccl_mode_) << " but current execution mode is "
                      << GetHcclModeString(hccl_mode)
                      << ". Please set the execution mode before HCCL init(), and then do not change it in the "
                         "subsequent script";
  }
}

std::string HcclAdapter::GetHcclModeString(HcclMode hccl_mode) {
  static std::map<HcclMode, std::string> kHcclModeString = {{HcclMode::kGraph, "GRAPH_MODE"},
                                                            {HcclMode::kPynative, "PYNATIVE_MODE"},
                                                            {HcclMode::kKernelByKernel, "KERNEL_BY_KERNEL_MODE"}};
  return kHcclModeString.at(hccl_mode);
}

bool HcclAdapter::InitHccl(uint32_t device_id, std::string_view rank_id) {
  MS_LOG(INFO) << "Start init hccl adapter.";
  common::SetEnv("HCCL_WHITELIST_DISABLE", "1");
  std::lock_guard<std::mutex> lock(init_mutex_);
  if (init_flag_) {
    MS_LOG(INFO) << "Hccl has been inited, skip.";
    return true;
  }
  InitPlugin();
  auto options = GenHcclOptions(device_id, rank_id);
  bool ret = InitKernelInfoStore(options);
  if (!ret) {
    return false;
  }
  ret = InitHcclExec();
  if (!ret) {
    return false;
  }
  init_flag_ = true;
  MS_LOG(INFO) << "Init hccl adapter success.";
  return true;
}

bool HcclAdapter::InitHccl(uint32_t device_id, std::string_view rank_id, std::string_view rank_file,
                           HcclMode hccl_mode) {
  MS_LOG(INFO) << "Start init hccl adapter for " << GetHcclModeString(hccl_mode);
  std::lock_guard<std::mutex> lock(init_mutex_);
  if (init_flag_) {
    MS_LOG(INFO) << "Hccl has been inited, skip.";
    return true;
  }

  hccl_mode_ = hccl_mode;
  InitPlugin();
  if (hccl_mode_ == HcclMode::kGraph) {
    auto options = GenHcclOptions(device_id, rank_id, rank_file);
    bool ret = InitKernelInfoStore(options);
    if (!ret) {
      return false;
    }

    ret = InitHcclExec();
    if (!ret) {
      return false;
    }
  } else {
    bool ret = InitHcclComm(rank_id, rank_file);
    if (!ret) {
      return false;
    }
  }

  init_flag_ = true;
  MS_LOG(INFO) << "Init hccl adapter success.";
  return true;
}

bool HcclAdapter::FinalizeHccl() {
  std::lock_guard<std::mutex> lock(init_mutex_);
  MS_LOG(INFO) << "Start destroy hccl adapter for " << GetHcclModeString(hccl_mode_);
  if (!init_flag_) {
    MS_LOG(INFO) << "Hccl has never been inited, skip.";
    return true;
  }
  (void)FinalizeHcclExec();
  (void)FinalizeKernelInfoStore();
  (void)FinalizeHcclComm();
  if (hcom_destroy_ != nullptr) {
    hcom_destroy_();
  }
  FinalizePlugin();
  init_flag_ = false;
  MS_LOG(INFO) << "Destroy hccl adapter success.";
  return true;
}

HcclResult HcclAdapter::HcclBroadcast(void *buf, uint64_t count, HcclDataType dataType, uint32_t root,
                                      aclrtStream stream, HcclComm hccl_comm) const {
  return launch_hccl_broadcast_(buf, count, dataType, root, hccl_comm, stream);
}

HcclResult HcclAdapter::HcclAllReduce(void *send_buf, void *recv_buf, uint64_t count, HcclDataType dataType,
                                      const HcclReduceOp op, const aclrtStream stream, HcclComm hccl_comm) const {
  return launch_hccl_all_reduce_(send_buf, recv_buf, count, dataType, op, hccl_comm, stream);
}

HcclResult HcclAdapter::HcclReduce(void *send_buf, void *recv_buf, uint64_t count, HcclDataType dataType,
                                   HcclReduceOp op, uint32_t root, const aclrtStream stream, HcclComm hccl_comm) const {
  return launch_hccl_reduce_(send_buf, recv_buf, count, dataType, op, root, hccl_comm, stream);
}

HcclResult HcclAdapter::HcclReduceScatter(void *send_buf, void *recv_buf, uint64_t count, HcclDataType dataType,
                                          const HcclReduceOp op, const aclrtStream stream, HcclComm hccl_comm) const {
  return launch_hccl_reduce_scatter_(send_buf, recv_buf, count, dataType, op, hccl_comm, stream);
}

HcclResult HcclAdapter::HcclAllGather(void *send_buf, void *recv_buf, uint64_t count, HcclDataType dataType,
                                      const aclrtStream stream, HcclComm hccl_comm) const {
  return launch_hccl_all_gather_(send_buf, recv_buf, count, dataType, hccl_comm, stream);
}

HcclResult HcclAdapter::HcclSend(void *send_buf, uint64_t count, HcclDataType dataType, uint32_t destRank,
                                 const aclrtStream stream, HcclComm hccl_comm) const {
  return launch_hccl_send_(send_buf, count, dataType, destRank, hccl_comm, stream);
}

HcclResult HcclAdapter::HcclRecv(void *recv_buf, uint64_t count, HcclDataType dataType, uint32_t srcRank,
                                 const aclrtStream stream, HcclComm hccl_comm) const {
  return launch_hccl_recv_(recv_buf, count, dataType, srcRank, hccl_comm, stream);
}

HcclResult HcclAdapter::HcclBarrier(const aclrtStream stream, HcclComm hccl_comm) const {
  return launch_hccl_barrier_(hccl_comm, stream);
}

bool HcclAdapter::InitKernelInfoStore(const std::map<std::string, std::string> options) {
  MS_LOG(INFO) << "Start init hccl kernel info store.";
  MS_EXCEPTION_IF_NULL(init_hcom_graph_adapter_);
  MS_EXCEPTION_IF_NULL(get_hccl_kernel_info_store_);
  // get ops_kernel_builder
  std::map<std::string, std::shared_ptr<ge::OpsKernelBuilder>> all_builders;
  get_all_kernel_builder_(&all_builders);
  auto iter = all_builders.find(kHcclOpsKernelInfoStore);
  if (iter == all_builders.end()) {
    std::string all_builders_name = "[";
    for (const auto &it : all_builders) {
      all_builders_name += it.first + " ";
    }
    all_builders_name += "]";
    MS_LOG(EXCEPTION) << "Builders size " << all_builders.size() << ", cannot find " << kHcclOpsKernelInfoStore
                      << ", full list of builders: " << all_builders_name;
  }

  MS_LOG(INFO) << "Get builder " << iter->first;
  ops_kernel_builder_ = iter->second;
  MS_EXCEPTION_IF_NULL(ops_kernel_builder_);
  // init ops_kernel_builder
  auto ret = ops_kernel_builder_->Initialize(options);
  if (ret != ge::SUCCESS) {
    MS_LOG(EXCEPTION) << "Init hccl kernel builder failed.";
  }

  // get ops_kernel_info_store
  ret = init_hcom_graph_adapter_(options);
  if (ret != ge::SUCCESS) {
    MS_LOG(EXCEPTION) << "Init hccl graph adapter failed.";
  }

  get_hccl_kernel_info_store_(&ops_kernel_info_store_);
  MS_EXCEPTION_IF_NULL(ops_kernel_info_store_);
  ret = ops_kernel_info_store_->Initialize(options);
  if (ret != ge::SUCCESS) {
    MS_LOG(EXCEPTION) << "Init info store failed.";
  }
  init_kernel_info_store_ = true;
  MS_LOG(INFO) << "Init hccl kernel info store success.";
  return true;
}

bool HcclAdapter::FinalizeKernelInfoStore() {
  if (!init_kernel_info_store_) {
    return true;
  }
  MS_LOG(INFO) << "Start destroy hccl kernel info store.";
  if (ops_kernel_info_store_ != nullptr) {
    auto ret = ops_kernel_info_store_->Finalize();
    if (ret != ge::SUCCESS) {
      MS_LOG(ERROR) << "Destroy info store failed, ret = " << ret;
      return false;
    }
  }

  if (ops_kernel_builder_ != nullptr) {
    auto ret = ops_kernel_builder_->Finalize();
    if (ret != ge::SUCCESS) {
      MS_LOG(ERROR) << "Destroy builder failed, ret = " << ret;
      return false;
    }
  }

  MS_EXCEPTION_IF_NULL(finalize_hcom_graph_adapter_);
  finalize_hcom_graph_adapter_();
  ops_kernel_info_store_.reset();
  ops_kernel_builder_.reset();
  init_kernel_info_store_ = false;
  MS_LOG(INFO) << "Destroy hccl kernel info store success.";
  return true;
}

bool HcclAdapter::InitHcclComm(std::string_view rank_id, std::string_view rank_file) {
  MS_LOG(INFO) << "Start init hccl comm.";
  int rank_id_i = -1;
  try {
    rank_id_i = std::stoi(rank_id.data());
  } catch (std::invalid_argument &) {
    MS_LOG(EXCEPTION) << "Invalid rank id env:" << rank_id;
  }
  if (rank_id_i < 0) {
    MS_LOG(ERROR) << "rank_id cannot be negative";
    return false;
  }
  MS_EXCEPTION_IF_NULL(init_hccl_comm_);
  auto hccl_result = init_hccl_comm_(rank_file.data(), rank_id_i, &hccl_comm_);
  if (hccl_result != HCCL_SUCCESS) {
    MS_LOG(ERROR) << "HcclCommInitClusterInfo failed, ret:" << hccl_result;
    return false;
  }
  MS_LOG(INFO) << "InitHcclComm success";
  return true;
}

bool HcclAdapter::FinalizeHcclComm() {
  MS_LOG(INFO) << "Start finalize hccl comm.";
  if (hccl_comm_ == nullptr) {
    return true;
  }

  MS_EXCEPTION_IF_NULL(finalize_hccl_comm_);
  auto hccl_result = finalize_hccl_comm_(hccl_comm_);
  if (hccl_result != HCCL_SUCCESS) {
    MS_LOG(ERROR) << "HcclComm destroy failed, ret:" << hccl_result;
    return false;
  }
  hccl_comm_ = nullptr;
  MS_LOG(INFO) << "HcclComm destroy success";
  return true;
}

HcclResult HcclAdapter::HcclCreateGroup(const std::string &group, uint32_t rank_num, uint32_t *rank_ids) const {
  CheckExcutionMode();
  CHECK_SYMBOL_NULL(hccl_create_group_);
  return hccl_create_group_(group.c_str(), rank_num, rank_ids);
}

HcclResult HcclAdapter::HcclDestroyGroup(const std::string &group) const {
  CHECK_SYMBOL_NULL(hccl_destroy_group_);
  return hccl_destroy_group_(group.c_str());
}

HcclResult HcclAdapter::HcclGetRankId(const std::string &group, uint32_t *rank_id) const {
  CheckExcutionMode();
  if (hccl_mode_ != HcclMode::kGraph) {
    CHECK_SYMBOL_NULL(single_op_hccl_get_rank_id_);
    return single_op_hccl_get_rank_id_(hccl_comm_, rank_id);
  } else {
    CHECK_SYMBOL_NULL(hccl_get_rank_id_);
    return hccl_get_rank_id_(group.c_str(), rank_id);
  }
}

HcclResult HcclAdapter::HcclGetRankSize(const std::string &group, uint32_t *rank_size) const {
  CheckExcutionMode();
  if (hccl_mode_ != HcclMode::kGraph) {
    CHECK_SYMBOL_NULL(single_op_hccl_get_rank_size_);
    return single_op_hccl_get_rank_size_(hccl_comm_, rank_size);
  } else {
    CHECK_SYMBOL_NULL(hccl_get_rank_size_);
    return hccl_get_rank_size_(group.c_str(), rank_size);
  }
}

HcclResult HcclAdapter::HcclGetLocalRankId(const std::string &group, uint32_t *local_rank_id) const {
  CheckExcutionMode();
  CHECK_SYMBOL_NULL(hccl_get_local_rank_id_);
  return hccl_get_local_rank_id_(group.c_str(), local_rank_id);
}

HcclResult HcclAdapter::HcclGetLocalRankSize(const std::string &group, uint32_t *local_rank_size) const {
  CheckExcutionMode();
  if (hccl_mode_ != HcclMode::kGraph) {
    MS_LOG(ERROR) << "The pynative mode doesn't support get local rank szie.";
    return HCCL_E_NOT_SUPPORT;
  } else {
    CHECK_SYMBOL_NULL(hccl_get_local_rank_size_);
    return hccl_get_local_rank_size_(group.c_str(), local_rank_size);
  }
}

HcclResult HcclAdapter::HcclGetWorldRankFromGroupRank(const std::string &group, uint32_t local_rank,
                                                      uint32_t *world_rank) const {
  CheckExcutionMode();
  if (hccl_mode_ != HcclMode::kGraph) {
    MS_LOG(ERROR) << "The pynative mode doesn't support get world rank by group rank.";
    return HCCL_E_NOT_SUPPORT;
  } else {
    CHECK_SYMBOL_NULL(hccl_get_world_rank_by_group_rank_);
    return hccl_get_world_rank_by_group_rank_(group.c_str(), local_rank, world_rank);
  }
}

HcclResult HcclAdapter::HcclGetGroupRankFromWorldRank(uint32_t world_rank, const std::string &group,
                                                      uint32_t *local_rank) const {
  CheckExcutionMode();
  if (hccl_mode_ != HcclMode::kGraph) {
    MS_LOG(ERROR) << "The pynative mode doesn't support get group rank by world rank.";
    return HCCL_E_NOT_SUPPORT;
  } else {
    CHECK_SYMBOL_NULL(hccl_get_group_rank_by_world_rank_);
    return hccl_get_group_rank_by_world_rank_(world_rank, group.c_str(), local_rank);
  }
}

bool HcclAdapter::InitHcclExec() {
  MS_LOG(INFO) << "Start init hccl exec.";
  MS_EXCEPTION_IF_NULL(hccl_exec_initialize_);
  HcclResult hccl_ret = hccl_exec_initialize_();
  if (hccl_ret == HCCL_E_PTR) {
    MS_LOG(WARNING) << "Hccl comm is null, hcom executor initialize is not required";
  } else if (hccl_ret == HCCL_SUCCESS) {
    MS_LOG(INFO) << "Hcom DynamicKernel Initialize success";
  } else {
    MS_LOG(ERROR) << "Hcom DynamicKernel Initialize failed";
    return false;
  }
  init_hccl_exec_ = true;
  MS_LOG(INFO) << "InitHcclExec success";
  return true;
}

bool HcclAdapter::FinalizeHcclExec() {
  if (!init_hccl_exec_) {
    return true;
  }
  MS_LOG(INFO) << "Start finalize hccl exec.";
  MS_EXCEPTION_IF_NULL(hccl_exec_finalize_);
  HcclResult hccl_ret = hccl_exec_finalize_();
  if (hccl_ret != HCCL_SUCCESS) {
    MS_LOG(ERROR) << "Hcom DynamicKernel Finalize failed";
    return false;
  }
  init_hccl_exec_ = false;
  MS_LOG(INFO) << "HcclExec destroy success";
  return true;
}

HcclResult HcclAdapter::HcclExecEnqueueOp(const ::HcomOperation &op_info, const HExecCallBack &callback) const {
  CheckExcutionMode();
  CHECK_SYMBOL_NULL(hccl_exec_enqueue_op_);
  return hccl_exec_enqueue_op_(op_info, callback);
}

HcclResult HcclAdapter::HcclExecAllToAllv(const ::HcomAllToAllVParams &params, const HExecCallBack &callback) const {
  CheckExcutionMode();
  CHECK_SYMBOL_NULL(hccl_exec_enqueue_all_to_all_v_);
  return hccl_exec_enqueue_all_to_all_v_(params, callback);
}

bool HcclAdapter::UseHcclCM() const {
  // If environment variable 'MS_HCCL_CM_INIT' is set and using dynamic cluster, we consider this process should be
  // launched by CM methods provided by HCCL.
  return common::UseDynamicCluster() && !common::GetEnv("MS_HCCL_CM_INIT").empty();
}

HcclResult HcclAdapter::HcclAllToAll(void *send_buf, void *recv_buf, hccl::HcclAllToAllVParams params,
                                     HcclDataType dataType, aclrtStream stream, HcclComm hccl_comm) const {
  CheckExcutionMode();
  CHECK_SYMBOL_NULL(launch_hccl_all_to_allv_);
  MS_EXCEPTION_IF_NULL(hccl_comm);
  return launch_hccl_all_to_allv_(send_buf, params.sendcounts.data(), params.sdispls.data(), dataType, recv_buf,
                                  params.recvcounts.data(), params.rdispls.data(), dataType, hccl_comm, stream);
}

bool HcclAdapter::IsSameServer(const std::vector<uint32_t> &rank_ids) const {
  auto min_iter = min_element(rank_ids.begin(), rank_ids.end());
  uint32_t min = (min_iter != rank_ids.end()) ? *min_iter : 0;
  auto max_iter = max_element(rank_ids.begin(), rank_ids.end());
  uint32_t max = (max_iter != rank_ids.end()) ? *max_iter : 0;
  return ((max - min < kDeviceNumOfServer) && (min / kDeviceNumOfServer == max / kDeviceNumOfServer));
}

string HcclAdapter::GetHcomGroup(const CNodePtr &cnode) const {
  MS_EXCEPTION_IF_NULL(cnode);
  if (!common::AnfAlgo::HasNodeAttr(kAttrGroup, cnode)) {
    MS_LOG(EXCEPTION) << "Hcom node " << cnode->fullname_with_scope() << " has no group attribute.";
  }

  auto group_name = common::AnfAlgo::GetNodeAttr<std::string>(cnode, kAttrGroup);
  auto rank_ids = common::AnfAlgo::HasNodeAttr(kAttrGroupRankIds, cnode)
                    ? common::AnfAlgo::GetNodeAttr<std::vector<uint32_t>>(cnode, kAttrGroupRankIds)
                    : std::vector<uint32_t>();
  auto new_group = DoGetHcomGroup(group_name, rank_ids);

  MS_LOG(INFO) << "hcom node: " << cnode->fullname_with_scope() << ", old group: " << group_name
               << ", new group: " << new_group;

  if (cnode->HasAttr(parallel::FIRST_RECEIVE)) {
    return new_group;
  }
  const auto &node_name = common::AnfAlgo::GetCNodeName(cnode);
  static const auto send_recv_parallel = (common::GetEnv("SEND_RECV_PARALLEL") == "1");
  if ((node_name == kSendOpName || node_name == kReceiveOpName) && !send_recv_parallel) {
    MS_LOG(DEBUG) << "hcom node: " << cnode->fullname_with_scope() << "is set to group: -1.";
    return "-1";
  }

  return new_group;
}

string HcclAdapter::DoGetHcomGroup(const string &original_group, const std::vector<uint32_t> &rank_ids) const {
  string communi_parallel_mode = parallel::ParallelContext::GetInstance()->communi_parallel_mode();
  if (communi_parallel_mode == parallel::kAllGroupParallel) {
    return original_group;
  }

  if (communi_parallel_mode == parallel::kNoGroupParallel) {
    return kDefaultGroup;
  }

  if (rank_ids.empty() || original_group == kHcclWorldGroup) {
    return kDefaultGroup;
  }

  if (IsSameServer(rank_ids)) {
    return original_group;
  }

  return kDefaultGroup;
}

void HcclAdapter::AddCMEnvToHcclOption(std::map<std::string, std::string> *hccl_opt_map) {
  MS_EXCEPTION_IF_NULL(hccl_opt_map);
  // Before hccl initialization, the validation of these environment variables is already verified.
  hccl_opt_map->emplace(ge::OPTION_EXEC_CM_CHIEF_IP,
                        mindspore::common::GetEnv(mindspore::distributed::kEnvSchedulerHost));

  // We offset scheduler port by 1 as cm chief port.
  std::string sched_port = mindspore::common::GetEnv(mindspore::distributed::kEnvSchedulerPort);
  int sched_port_num = std::stoi(sched_port);
  int cm_port_num = sched_port_num + 1;
  hccl_opt_map->emplace(ge::OPTION_EXEC_CM_CHIEF_PORT, std::to_string(cm_port_num));
  hccl_opt_map->emplace(ge::OPTION_EXEC_CM_CHIEF_DEVICE, GenerateCMChiefWorkDevice());
  hccl_opt_map->emplace(ge::OPTION_EXEC_CM_WORKER_SIZE,
                        mindspore::common::GetEnv(mindspore::distributed::kEnvWorkerNum));
  hccl_opt_map->emplace(ge::OPTION_EXEC_CM_WORKER_IP, mindspore::common::GetEnv(mindspore::distributed::kEnvWorkerIp));
  MS_LOG(INFO) << "Set CM options to hccl. OPTION_EXEC_CM_CHIEF_IP: " << hccl_opt_map->at(ge::OPTION_EXEC_CM_CHIEF_IP)
               << ", OPTION_EXEC_CM_CHIEF_PORT: " << hccl_opt_map->at(ge::OPTION_EXEC_CM_CHIEF_PORT)
               << ", OPTION_EXEC_CM_CHIEF_DEVICE: " << hccl_opt_map->at(ge::OPTION_EXEC_CM_CHIEF_DEVICE)
               << ", OPTION_EXEC_CM_WORKER_SIZE: " << hccl_opt_map->at(ge::OPTION_EXEC_CM_WORKER_SIZE)
               << ", OPTION_EXEC_CM_WORKER_IP: " << hccl_opt_map->at(ge::OPTION_EXEC_CM_WORKER_IP);
}
}  // namespace mindspore::hccl
