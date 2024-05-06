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

#include "plugin/device/ascend/hal/device/ascend_data_queue.h"
#include <string>
#include <map>
#include <utility>
#include "graph/types.h"
#include "include/backend/data_queue/data_queue_mgr.h"
#include "include/common/utils/python_adapter.h"
#include "utils/log_adapter.h"
#include "ops/structure_op_name.h"
#include "plugin/device/ascend/hal/common/ascend_utils.h"
#include "runtime/device/kernel_runtime.h"
#include "runtime/device/kernel_runtime_manager.h"
#include "include/backend/distributed/ps/ps_cache/ps_data_prefetch.h"
#include "include/backend/distributed/embedding_cache/embedding_cache_utils.h"
#include "transform/symbol/acl_rt_symbol.h"
#include "transform/symbol/acl_tdt_symbol.h"
#include "transform/symbol/symbol_utils.h"

namespace mindspore {
namespace device {
namespace {
const std::map<aclDataType, std::string> kAclTypeToString = {
  {ACL_INT8, "int8"},       {ACL_UINT8, "uint8"},   {ACL_INT16, "int16"},    {ACL_UINT16, "uint16"},
  {ACL_INT32, "int32"},     {ACL_UINT32, "uint32"}, {ACL_INT64, "int64"},    {ACL_UINT64, "uint64"},
  {ACL_FLOAT16, "float16"}, {ACL_FLOAT, "float32"}, {ACL_DOUBLE, "float64"}, {ACL_BOOL, "bool"}};

const std::map<std::string, aclDataType> kStringTypeToAclType = []() -> std::map<std::string, aclDataType> {
  std::map<std::string, aclDataType> ret;
  for (const auto &[acl_type, type_str] : kAclTypeToString) {
    ret.emplace(type_str, acl_type);
  }
  return ret;
}();

std::vector<std::pair<void **, std::thread *>> g_acl_handle_map = {};

std::mutex g_acl_destroy_all_mutex = {};
bool g_acl_destroy_all = false;

bool GetAclDataType(const std::string &str_type, aclDataType *acl_type) {
  MS_EXCEPTION_IF_NULL(acl_type);
  auto iter = kStringTypeToAclType.find(str_type);
  if (iter == kStringTypeToAclType.end()) {
    MS_LOG(EXCEPTION) << "Invalid type " << str_type;
  }
  *acl_type = iter->second;
  return true;
}

void CheckRtRetWithError(aclError error, const std::string &msg) {
  if (error != ACL_ERROR_NONE) {
    MS_LOG(ERROR) << "Rt error: " << msg << " | Error number: " << error;
  }
}

bool IsGetNextOp(const std::string &op_name) { return op_name == kGetNextOpName || op_name == kDynamicGetNextV2OpName; }
}  // namespace

namespace tdt_handle {
void AddHandle(acltdtChannelHandle **handle, std::thread *use_thread) {
  void **void_handle = reinterpret_cast<void **>(handle);
  if (*handle == nullptr) {
    return;
  }

  for (auto iter = g_acl_handle_map.cbegin(); iter != g_acl_handle_map.cend(); ++iter) {
    if (iter->first == void_handle) {
      return;
    }
  }

  g_acl_handle_map.emplace_back(void_handle, use_thread);
  {
    std::lock_guard<std::mutex> lock(g_acl_destroy_all_mutex);
    g_acl_destroy_all = false;
  }
}

void DelHandle(acltdtChannelHandle **handle) {
  void **void_handle = reinterpret_cast<void **>(handle);
  for (auto iter = g_acl_handle_map.begin(); iter != g_acl_handle_map.end();) {
    if (iter->first == void_handle) {
      iter = g_acl_handle_map.erase(iter);
    } else {
      ++iter;
    }
  }
}

bool DestroyHandle() {
  std::lock_guard<std::mutex> lock(g_acl_destroy_all_mutex);
  bool destroy_all = true;
  for (auto &item : g_acl_handle_map) {
    acltdtChannelHandle **handle = reinterpret_cast<acltdtChannelHandle **>(item.first);
    if (*handle != nullptr) {
      aclError stop_status = CALL_ASCEND_API(acltdtStopChannel, *handle);
      if (stop_status != ACL_SUCCESS) {
        MS_LOG(ERROR) << "Failed stop acl data channel and the stop status is " << stop_status << std::endl;
        return false;
      }
      if (item.second != nullptr && item.second->joinable()) {
        item.second->join();
      }
      if (CALL_ASCEND_API(acltdtDestroyChannel, *handle) != ACL_SUCCESS) {
        MS_LOG(INFO) << "acltdtDestroyChannel failed.";
        destroy_all = false;
      } else {
        *handle = nullptr;
      }
    }
  }

  // clear the map container when all the handle has been destroyed
  if (destroy_all) {
    g_acl_handle_map.clear();
    g_acl_destroy_all = true;
  }
  return destroy_all;
}

bool IsClosed() {
  std::lock_guard<std::mutex> lock(g_acl_destroy_all_mutex);
  return g_acl_destroy_all;
}
}  // namespace tdt_handle

AscendDataQueueDynamic::AscendDataQueueDynamic(const std::string &channel_name, const size_t capacity)
    : DataQueue(channel_name, capacity), stream_(nullptr), node_info_(nullptr) {
  auto context_key = device_context_->device_context_key();
  auto runtime_instance =
    device::KernelRuntimeManager::Instance().GetKernelRuntime(context_key.device_name_, context_key.device_id_);
  node_info_ = std::make_unique<NodeInfo[]>(capacity);
  stream_ = runtime_instance->compute_stream();
}

DataQueueStatus AscendDataQueueDynamic::Push(std::vector<DataQueueItem> data) {
  for (size_t i = 0; i < data.size(); i++) {
    auto &item = data[i];
    if (item.data_ptr == nullptr) {
      MS_LOG(ERROR) << "Invalid Input: ptr: " << item.data_ptr << ", len: " << item.data_len;
      return DataQueueStatus::ERROR_INPUT;
    }
    void *addr = device_context_->device_res_manager_->AllocateMemory(item.data_len);
    if (addr == nullptr) {
      MS_LOG(ERROR) << "Allocate device memory of data queue failed";
    }
    CheckRtRetWithError(CALL_ASCEND_API(aclrtMemcpyAsync, addr, item.data_len, item.data_ptr, item.data_len,
                                        ACL_MEMCPY_HOST_TO_DEVICE, stream_),
                        "Rt Memcpy Error");
    item.device_addr = addr;
  }
  CheckRtRetWithError(CALL_ASCEND_API(aclrtSynchronizeStreamWithTimeout, stream_, -1),
                      "Call runtime aclrtSynchronizeStreamWithTimeout failed");
  node_info_[tail_].data_ = std::move(data);
  tail_ = (tail_ + 1) % (capacity_);
  ++size_;
  return DataQueueStatus::SUCCESS;
}

DataQueueStatus AscendDataQueueDynamic::Front(std::vector<DataQueueItem> *data) const {
  for (auto &item : node_info_[head_].data_) {
    host_release_(item.data_ptr, item.worker_id);
  }
  *data = node_info_[head_].data_;
  return DataQueueStatus::SUCCESS;
}

DataQueueStatus AscendDataQueueDynamic::Pop() {
  head_ = (head_ + 1) % (capacity_);
  --size_;
  return DataQueueStatus::SUCCESS;
}

AscendTdtQueue::AscendTdtQueue(const std::string &channel_name) : DataQueue(channel_name, 0), acl_handle_(nullptr) {
  // Init ErrorManager
  if (!ascend::ErrorManagerAdapter::Init()) {
    MS_LOG(WARNING) << "[Internal Error] Init ErrorManager failed.";
  }
  // get device id
  MS_EXCEPTION_IF_NULL(MsContext::GetInstance());
  device_id_ = MsContext::GetInstance()->get_param<uint32_t>(MS_CTX_DEVICE_ID);

  aclError ret = CALL_ASCEND_API(aclrtSetDevice, device_id_);
  if (ret != ACL_ERROR_NONE) {
    MS_LOG(ERROR) << "Acl open device " << device_id_ << " failed.";
  }

#if defined(ENABLE_PYTHON) && !defined(ENABLE_ANDROID)
  // There is a python flag in MindSpore to recognize if the runtime env is python.
  // If we only use MD feature, python_env_flag will not set to true,
  // then init.cc will not call ClearResAtexit at the end of MindSpore to clean resource.
  // The original case is [only MD + mbuf + device_queue + send], the ascend stream release
  // failed if we don't call ClearResAtexit first.
  mindspore::python_adapter::set_python_env_flag(true);
#endif

  // create acl tdt handle
  if (!channel_name_.empty()) {
    // When "capacity" is too large, device memory will be exploded
    size_t data_queue_capacity = 128;
    std::string env_capacity_str = common::GetEnv("MS_DATASET_SINK_QUEUE");
    if (!env_capacity_str.empty()) {
      int32_t env_capacity = atoi(env_capacity_str.c_str());
      if (env_capacity <= 0) {
        MS_LOG(EXCEPTION) << "Invalid data queue capacity.#umsg#User Help Message:#umsg#"
                             "Expect env variable MS_DATASET_SINK_QUEUE > 0.";
      }
      data_queue_capacity = env_capacity;
    }
    // Create device channel
    acl_handle_ =
      CALL_ASCEND_API(acltdtCreateChannelWithCapacity, device_id_, channel_name_.c_str(), data_queue_capacity);
    if (acl_handle_ != nullptr) {
      MS_LOG(INFO) << "Select MBUF channel, the capacity of data queue is: " << data_queue_capacity;
      queue_type_ = "Ascend_MBUF";
    } else {
      MS_LOG(INFO) << "Select TDT channel.";
      acl_handle_ = CALL_ASCEND_API(acltdtCreateChannel, device_id_, channel_name_.c_str());
      queue_type_ = "Ascend_TDT";
      if (acl_handle_ == nullptr) {
        MS_LOG(EXCEPTION) << "Create channel for sending data failed.#umsg#User Help Message:#umsg#"
                             "Please check DEVICE ID setting, DEVICE ID that passed into dataset"
                             "(from context) and training process should be the same.";
      }
    }
    tdt_handle::AddHandle(&acl_handle_, nullptr);
  }

  // a wingman of tdt to help with transferring data shapes on host
  auto wingman_queue = std::make_shared<BlockingQueue>();
  std::shared_ptr<DataQueue> data_queue = std::make_shared<WingmanQueue>(channel_name);
  auto rt = wingman_queue->Create(data_queue);
  if (rt != DataQueueStatus::SUCCESS) {
    MS_LOG(EXCEPTION) << "Wingman queue: " << channel_name << "create failed: " << rt;
  }
  DataQueueMgr::GetInstance().Manage(channel_name, wingman_queue);
}

AscendTdtQueue::~AscendTdtQueue() {
  if (acl_handle_ != nullptr) {
    if (CALL_ASCEND_API(acltdtDestroyChannel, acl_handle_) != ACL_SUCCESS) {
      MS_LOG(EXCEPTION) << "Failed to destroy channel for tdt queue. The details refer to 'Ascend Error Message'.";
    } else {
      tdt_handle::DelHandle(&acl_handle_);
      acl_handle_ = nullptr;
    }
  }
  if (DataQueueMgr::GetInstance().IsCreated(channel_name_)) {
    DataQueueMgr::GetInstance().Free(channel_name_);
  }
  aclError rt_ret = CALL_ASCEND_API(aclrtResetDevice, device_id_);
  if (rt_ret != ACL_ERROR_NONE) {
    MS_LOG(ERROR) << "Reset device " << device_id_ << " failed.";
  }
}

size_t AscendTdtQueue::QueryQueueSize() const {
  size_t size = 0;
  if (!IsOpen()) {
    MS_LOG(INFO) << "Mbuf channel has been closed, should not query size.";
    return 0;
  }
  auto status = CALL_ASCEND_API(acltdtQueryChannelSize, acl_handle_, &size);
  if (status != ACL_SUCCESS) {
    MS_LOG(EXCEPTION) << "Unable to query real-time size of Mbuf channel: " << channel_name_
                      << ", error code: " << status;
  }
  return size;
}

bool AscendTdtQueue::IsOpen() const { return !tdt_handle::IsClosed(); }

DataQueueStatus AscendTdtQueue::Push(std::vector<DataQueueItem> data) {
  MS_LOG(DEBUG) << "TDT channel name is " << channel_name_ << ".";
  acltdtDataset *acl_dataset = nullptr;
  auto ret = Translate(data, &acl_dataset);
  if (!ret) {
    DestroyAclDataset(acl_dataset);
    MS_LOG(ERROR) << "Converting into TDT tensor failed!";
    return DataQueueStatus::INTERNAL_ERROR;
  }

  // Data prefetch only when PS mode enables cache.
  if (CALL_ASCEND_API(acltdtGetDatasetSize, acl_dataset) > 0) {
    acltdtDataItem *item0 = CALL_ASCEND_API(acltdtGetDataItem, acl_dataset, 0);
    std::string item_type;
    ParseType(CALL_ASCEND_API(acltdtGetDataTypeFromItem, item0), &item_type);
  }
  auto status = CALL_ASCEND_API(acltdtSendTensor, acl_handle_, acl_dataset, -1);
  DestroyAclDataset(acl_dataset);
  if (status != ACL_SUCCESS) {
    // if the device_queue thread had been interrupted by master, just print warning and return success
    if (tdt_handle::IsClosed()) {
      MS_LOG(WARNING) << "Device queue thread had been interrupted by TdtHandle::DestroyHandle, you can ignore "
                      << "the above error: 'failed to send...'. In this scenario, the training ends first without "
                      << "using all epoch(s) data, and the data preprocessing is blocked by the data "
                      << "transmission channel on the device side. So we force the data transmission channel to stop.";
      return DataQueueStatus::SUCCESS;
    }
    MS_LOG(EXCEPTION) << "Tdt Send data failed. The details refer to 'Ascend Error Message'.";
  }
  auto wingman = DataQueueMgr::GetInstance().GetDataQueue(channel_name_);
  if (wingman != nullptr && wingman->IsOpen() && !data.empty()) {
    (void)wingman->Push(data);
  }
  return DataQueueStatus::SUCCESS;
}

void AscendTdtQueue::ParseType(aclDataType acl_data_type, std::string *data_type) const {
  auto type_iter = kAclTypeToString.find(acl_data_type);
  if (type_iter == kAclTypeToString.end()) {
    MS_LOG(EXCEPTION) << "Got unsupported acl datatype: " << acl_data_type;
  }
  *data_type = type_iter->second;
}

bool AscendTdtQueue::Translate(const std::vector<DataQueueItem> &data, acltdtDataset **output_acl_dataset) const {
  auto acl_dataset = CALL_ASCEND_API(acltdtCreateDataset);
  if (acl_dataset == nullptr) {
    MS_LOG(ERROR) << "Create tdt dataset failed.";
    return false;
  }
  bool status = AssembleTensor2AclDataset(data, acl_dataset);
  if (!status) {
    DestroyAclDataset(acl_dataset);
    MS_LOG(ERROR) << "Assemble tensor row to tdt dataset failed.";
    return false;
  }

  *output_acl_dataset = acl_dataset;
  return true;
}

bool AscendTdtQueue::AssembleTensor2AclDataset(const std::vector<DataQueueItem> &data,
                                               acltdtDataset *acl_dataset) const {
  if (data.empty()) {
    acltdtDataItem *acl_data = CALL_ASCEND_API(acltdtCreateDataItem, acltdtTensorType::ACL_TENSOR_DATA_END_OF_SEQUENCE,
                                               nullptr, 0, ACL_BOOL, nullptr, 0);
    if (acl_data == nullptr) {
      MS_LOG(ERROR) << "Create data item failed when send empty data.";
      return false;
    }
    if (CALL_ASCEND_API(acltdtAddDataItem, acl_dataset, acl_data) != ACL_SUCCESS) {
      if (CALL_ASCEND_API(acltdtDestroyDataItem, acl_data) != ACL_SUCCESS) {
        MS_LOG(ERROR) << "Destroy data item failed when send empty data.";
      }
      MS_LOG(ERROR) << "Add data item to tdt dataset failed when send data.";
      return false;
    }
    return true;
  }

  for (const auto &ts : data) {
    aclDataType acl_type;
    acltdtDataItem *acl_data = nullptr;
    if (!GetAclDataType(ts.data_type, &acl_type)) {
      MS_LOG(ERROR) << "Convert type " << ts.data_type << " to acl type failed.";
      return false;
    }

    const auto &shape = ts.shapes;
    std::string shape_str = "[";
    for (auto dim : shape) {
      (void)shape_str.append(std::to_string(dim)).append(",");
    }
    shape_str.pop_back();
    (void)shape_str.append("]");

    void *data_ptr = ts.data_ptr;
    size_t data_size = ts.data_len;

    acl_data = CALL_ASCEND_API(acltdtCreateDataItem, acltdtTensorType::ACL_TENSOR_DATA_TENSOR,
                               (shape.empty() ? nullptr : &shape[0]), shape.size(), acl_type, data_ptr, data_size);
    if (acl_data == nullptr) {
      MS_LOG(ERROR) << "Create data item failed when send data.";
      return false;
    }
    if (CALL_ASCEND_API(acltdtAddDataItem, acl_dataset, acl_data) != ACL_SUCCESS) {
      if (CALL_ASCEND_API(acltdtDestroyDataItem, acl_data) != ACL_SUCCESS) {
        MS_LOG(ERROR) << "Destroy data item failed when send data with type ACL_TENSOR_DATA_TENSOR.";
      }
      MS_LOG(INFO) << "Add data item to tdt dataset failed when send data.";
      return false;
    }

    MS_LOG(DEBUG) << "TDT data type is TDT_TENSOR, tensor type is " << acl_type << ", tensor shape is " << shape_str
                  << ", data length is " << data_size << ".";
  }

  return true;
}

void AscendTdtQueue::DestroyAclDataset(acltdtDataset *acl_dataset, bool include_data_item) const {
  if (include_data_item) {
    for (size_t i = 0; i < CALL_ASCEND_API(acltdtGetDatasetSize, acl_dataset); i++) {
      auto data_item = CALL_ASCEND_API(acltdtGetDataItem, acl_dataset, i);
      if (CALL_ASCEND_API(acltdtDestroyDataItem, data_item) != ACL_SUCCESS) {
        MS_LOG(EXCEPTION) << "Destroy data item failed when send data.";
      }
    }
  }

  if (CALL_ASCEND_API(acltdtDestroyDataset, acl_dataset) != ACL_SUCCESS) {
    MS_LOG(EXCEPTION) << "Destroy tdt dataset failed when send data.";
  }
}

DataQueueStatus WingmanQueue::Push(const std::vector<DataQueueItem> data) {
  queue_.emplace(data);
  return DataQueueStatus::SUCCESS;
}

DataQueueStatus WingmanQueue::Pop() {
  queue_.pop();
  return DataQueueStatus::SUCCESS;
}

DataQueueStatus WingmanQueue::Front(std::vector<DataQueueItem> *data) const {
  *data = queue_.front();
  return DataQueueStatus::SUCCESS;
}

DataQueueStatus WingmanQueue::FrontAsync(std::vector<DataQueueItem> *data) const { return this->Front(data); }

void WingmanQueue::Close() {
  queue_ = {};
  closed_ = true;
}

std::shared_ptr<BlockingQueue> GetTdtWingManQueue(const PrimitivePtr &prim) {
  if (!IsGetNextOp(prim->name())) return nullptr;
  auto queue_name = GetValue<std::string>(prim->GetAttr("shared_name"));
  if (!DataQueueMgr::GetInstance().IsCreated(queue_name)) {
    return nullptr;
  }
  return DataQueueMgr::GetInstance().GetDataQueue(queue_name);
}

std::shared_ptr<BlockingQueue> GetTdtWingManQueue(const std::shared_ptr<AnfNode> &node) {
  if (!common::AnfAlgo::IsGetNextNode(node)) return nullptr;
  return GetTdtWingManQueue(common::AnfAlgo::GetCNodePrimitive(node));
}

void CloseTdtWingManQueue(const PrimitivePtr &prim) {
  if (!IsGetNextOp(prim->name())) return;
  auto wingman = GetTdtWingManQueue(prim);
  if (wingman && wingman->IsOpen()) {
    wingman->Close();
  }
}

void CloseTdtWingManQueue(const std::shared_ptr<AnfNode> &node) {
  if (!common::AnfAlgo::IsGetNextNode(node)) return;
  return CloseTdtWingManQueue(common::AnfAlgo::GetCNodePrimitive(node));
}

namespace {
std::shared_ptr<DataQueue> CreateAscendDataQueue(const std::string &channel_name, bool dynamic_shape, size_t capacity,
                                                 const std::vector<size_t> &) {
  return std::make_shared<AscendTdtQueue>(channel_name);
}

REGISTER_DATA_QUEUE_CREATOR(kAscendDevice, CreateAscendDataQueue);
struct DevicePlugFuncRegister {
  DevicePlugFuncRegister() noexcept {
    DataQueueMgr::SetDestoryTdtHandleHandler([]() -> bool { return tdt_handle::DestroyHandle(); });
  }
} ascend_device_func_register;
}  // namespace
}  // namespace device
}  // namespace mindspore
