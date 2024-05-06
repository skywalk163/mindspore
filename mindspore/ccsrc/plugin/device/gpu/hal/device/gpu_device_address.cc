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

#include "plugin/device/gpu/hal/device/gpu_device_address.h"
#include <vector>
#include <memory>
#include "utils/log_adapter.h"
#include "utils/ms_context.h"
#include "ir/tensor.h"
#include "plugin/device/gpu/hal/device/gpu_device_manager.h"
#include "plugin/device/gpu/hal/device/gpu_memory_allocator.h"
#include "plugin/device/gpu/hal/hardware/gpu_device_context.h"
#include "plugin/device/gpu/hal/device/gpu_hash_table_util.h"
#include "plugin/device/gpu/hal/device/gpu_common.h"
#include "plugin/device/gpu/hal/device/gpu_event.h"
#ifdef ENABLE_DEBUGGER
#include "debug/debug_services.h"
#include "debug/tensor_load.h"
#include "include/backend/debug/debugger/debugger.h"
#endif
#ifdef ENABLE_DUMP_IR
#include "include/common/debug/rdr/recorder_manager.h"
#endif

namespace mindspore {
namespace device {
namespace gpu {
void GPUDeviceAddress::SetDevicePtrDeleter() {
  const auto &kernel_tensor = this->kernel_tensor();
  if (!kernel_tensor) {
    return;
  }

  kernel_tensor->set_deleter([](void *ptr, bool from_mem_pool) {
    if (ptr != nullptr && from_mem_pool) {
      GPUMemoryAllocator::GetInstance().FreeTensorMem(ptr);
    }
  });
}

bool GPUDeviceAddress::SyncDeviceToHost(size_t size, void *host_ptr) const {
  // The input or output may be empty.
  if ((size == 0) || (GetSize() == 0)) {
    MS_LOG(INFO) << "No need sync, host size: " << size << ", device size: " << GetSize();
    return true;
  }
  if (size > GetSize()) {
    MS_LOG(WARNING) << "Please check whether need sync data, host size: " << size << ", device size: " << GetSize();
    return true;
  }

  MS_EXCEPTION_IF_NULL(host_ptr);
  auto ret = GPUDeviceManager::GetInstance().SyncAllStreams();
  if (!ret) {
#ifdef ENABLE_DUMP_IR
    mindspore::RDR::TriggerAll();
#endif
    MS_LOG(ERROR) << "SyncStream failed";
    return ret;
  }
  if (size != GetSize()) {
    // nccl kernel input and output device address is aligned, may lead to host size is not equal to device size
    MS_LOG(INFO) << "Sync memory size is inconsistent, host size: " << size << ", device size " << GetSize();
  }
  MoveToDevice(false);
  std::lock_guard<std::recursive_mutex> lock(ptr_mutex_);
  if (mem_offloaded()) {
    MS_EXCEPTION_IF_NULL(offload_ptr_);
    return GPUDeviceManager::GetInstance().CopyHostMemToHost(host_ptr, offload_ptr_, size);
  } else {
    MS_EXCEPTION_IF_NULL(GetDevicePtr());
    return GPUDeviceManager::GetInstance().CopyDeviceMemToHost(host_ptr, GetDevicePtr(), size);
  }
}

bool GPUDeviceAddress::SyncHostToDevice(size_t size, const void *host_ptr) const {
  // The input or output may be empty.
  if ((size == 0) || (GetSize() == 0)) {
    MS_LOG(INFO) << "No need sync, host size: " << size << ", device size: " << GetSize();
    return true;
  }
  if (size > GetSize()) {
    MS_LOG(WARNING) << "Please check whether need sync data, host size: " << size << ", device size: " << GetSize();
    return true;
  }
  MS_EXCEPTION_IF_NULL(host_ptr);
  if (size != GetSize()) {
    // nccl kernel input and output device address is aligned, may lead to host size is not equal to device size
    MS_LOG(INFO) << "Sync memory size is inconsistent, host size: " << size << ", device size " << GetSize();
  }

  // Bind device by device name and device id on the current thread.
  if (!device_name().empty()) {
    auto device_context = GetDeviceContext();
    auto gpu_device_context = dynamic_cast<GPUDeviceContext *>(device_context);
    MS_EXCEPTION_IF_NULL(gpu_device_context);
    if (!gpu_device_context->device_res_manager_->BindDeviceToCurrentThread(false)) {
      MS_LOG(EXCEPTION) << "BindDeviceToCurrentThread failed.";
    }
  }

  MoveToDevice(false);
  std::lock_guard<std::recursive_mutex> lock(ptr_mutex_);
  if (mem_offloaded()) {
    MS_EXCEPTION_IF_NULL(offload_ptr_);
    return GPUDeviceManager::GetInstance().CopyHostMemToHost(offload_ptr_, host_ptr, size);
  } else {
    MS_EXCEPTION_IF_NULL(GetDevicePtr());
    auto stream = GPUDeviceManager::GetInstance().GetStream(this->stream_id());
    MS_EXCEPTION_IF_NULL(stream);
    if (!GPUDeviceManager::GetInstance().CopyHostMemToDeviceAsync(GetDevicePtr(), host_ptr, size, stream)) {
      MS_LOG(ERROR) << "CopyHostMemToDeviceAsync failed";
      return false;
    }
    return GPUDeviceManager::GetInstance().SyncStream(stream);
  }
}

bool GPUDeviceAddress::SyncDeviceToHost(const ShapeVector &, size_t size, TypeId, void *host_ptr) const {
  return SyncDeviceToHost(size, host_ptr);
}

namespace {
bool SyncUserDataToDevice(const UserDataPtr &user_data, const void *host_ptr, size_t size) {
  MS_EXCEPTION_IF_NULL(user_data);
  MS_EXCEPTION_IF_NULL(host_ptr);
  const auto &user_data_type = user_data->get<UserDataType>(kUserDataType);
  MS_EXCEPTION_IF_NULL(user_data_type);

  if (*user_data_type == UserDataType::kUserTypeHashTable) {
#if CUDA_VERSION > 11000 && defined(__linux__)
    auto key_type = user_data->get<TypeId>(kHashTableKeyType);
    auto value_type = user_data->get<TypeId>(kHashTableValueType);
    MS_EXCEPTION_IF_NULL(key_type);
    MS_EXCEPTION_IF_NULL(value_type);
    const auto &iter = hashtable_func_list.find({*key_type, *value_type});
    if (iter != hashtable_func_list.end()) {
      return std::get<kSyncFuncIndex>(iter->second)(user_data, host_ptr, size);
    } else {
      MS_LOG(EXCEPTION) << "Unsupported hash table type:" << *key_type << " and:" << *value_type;
    }
#else
    MS_LOG(EXCEPTION) << "Invalid platform or cuda version for gpu hash table.";
#endif
  }
  return true;
}
}  // namespace

bool GPUDeviceAddress::SyncHostToDevice(const ShapeVector &, size_t size, TypeId, const void *host_ptr,
                                        const std::string &format) const {
  if (user_data() != nullptr && user_data()->has(kUserDataType)) {
    return SyncUserDataToDevice(user_data(), host_ptr, size);
  }

  MoveToDevice(false);
  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  bool execution_mode = ms_context->get_param<int>(MS_CTX_EXECUTION_MODE);
  if (execution_mode != kPynativeMode) {
    return SyncHostToDevice(size, host_ptr);
  }

  // PyNative mode need copy async to improve performance.
  MS_EXCEPTION_IF_NULL(host_ptr);
  bool need_sync = (size != 0) && (GetSize() != 0) && (size <= GetSize());
  if (!need_sync) {
    return true;
  }
  auto stream = GPUDeviceManager::GetInstance().GetStream(this->stream_id());
  MS_EXCEPTION_IF_NULL(stream);
  return GPUDeviceManager::GetInstance().CopyHostMemToDeviceAsync(GetDevicePtr(), host_ptr, size, stream);
}

bool GPUDeviceAddress::SyncDeviceToDevice(const DeviceSync *src_device_addr) const {
  MS_EXCEPTION_IF_NULL(src_device_addr);
  auto src_gpu_device = dynamic_cast<const GPUDeviceAddress *>(src_device_addr);
  MS_EXCEPTION_IF_NULL(src_gpu_device);
  MS_LOG(DEBUG) << "Sync gpu device address from:" << src_device_addr << " to:" << this;
  src_gpu_device->MoveToDevice(false);
  if (src_gpu_device->mem_offloaded()) {
    return SyncHostToDevice(src_gpu_device->host_shape(), src_gpu_device->GetSize(), src_gpu_device->type_id(),
                            src_gpu_device->GetOffloadPtr(), src_gpu_device->format());
  } else {
    return SyncDeviceToDevice(src_gpu_device->host_shape(), src_gpu_device->GetSize(), src_gpu_device->type_id(),
                              src_gpu_device->GetPtr(), src_gpu_device->format());
  }
}

bool GPUDeviceAddress::SyncDeviceToDevice(const ShapeVector &, size_t size, TypeId type, const void *src_ptr,
                                          const std::string &format) const {
  MS_LOG(DEBUG) << "SyncDeviceToDevice, dst(address:" << GetDevicePtr() << " format:" << DeviceAddress::format()
                << ", type_id:" << TypeIdLabel(type_id()) << ", size:" << GetSize() << "), src(address:" << src_ptr
                << "format:" << format << ", type_id:" << TypeIdLabel(type) << ", size:" << size << ")";
  if (GetDevicePtr() == src_ptr) {
    MS_LOG(INFO) << "Dst addr is same with src addr, no need memcpy data.";
    return true;
  }
  if (type_id() > kMonadTypeBegin && type_id() < kMonadTypeEnd) {
    return true;
  }
  // The input or output may be empty.
  if ((size == 0) || (GetSize() == 0)) {
    MS_LOG(INFO) << "No need sync, src device size: " << size << ", dst device size: " << GetSize();
    return true;
  }
  if (GetSize() < size) {
    MS_LOG(ERROR) << "Src size is greater than det size, src size is: " << size << ", dst size is: " << GetSize();
    return false;
  }
  if (DeviceAddress::format() != format || type_id() != type) {
    MS_LOG(ERROR) << "Format or type is different, src(format:" << format << ", type_id:" << TypeIdLabel(type)
                  << "), dst(format:" << DeviceAddress::format() << "), type_id:" << TypeIdLabel(type_id());
    return false;
  }

  MoveToDevice(false);
  MS_EXCEPTION_IF_NULL(src_ptr);
  MS_EXCEPTION_IF_NULL(GetDevicePtr());
  auto &stream = GPUDeviceManager::GetInstance().default_stream();
  MS_EXCEPTION_IF_NULL(stream);
  if (mem_offloaded()) {
    if (!GPUDeviceManager::GetInstance().CopyDeviceMemToHostAsync(offload_ptr_, src_ptr, size, stream)) {
      MS_LOG(ERROR) << "CopyDeviceMemToDeviceAsync failed";
      return false;
    }
  } else if (!GPUDeviceManager::GetInstance().CopyDeviceMemToDeviceAsync(GetDevicePtr(), src_ptr, size, stream)) {
    MS_LOG(ERROR) << "CopyDeviceMemToDeviceAsync failed";
    return false;
  }
  return GPUDeviceManager::GetInstance().SyncStream(stream);
}

bool GPUDeviceAddress::AsyncHostToDevice(const ShapeVector &, size_t size, TypeId, const void *host_ptr,
                                         size_t stream_id) const {
  MS_ERROR_IF_NULL(host_ptr);
  MoveToDevice(false);
  MS_ERROR_IF_NULL(GetDevicePtr());
  const auto stream = GPUDeviceManager::GetInstance().GetStream(stream_id);
  MS_ERROR_IF_NULL(stream);

  CHECK_RET_WITH_RETURN_ERROR(CudaDriver::CopyHostMemToDeviceAsync(GetDevicePtr(), host_ptr, size, stream),
                              "CopyHostMemToDeviceAsync failed");
  return true;
}

bool GPUDeviceAddress::AsyncDeviceToHost(const ShapeVector &, size_t size, TypeId, void *host_ptr,
                                         size_t stream_id) const {
  MS_ERROR_IF_NULL(host_ptr);
  MoveToDevice(false);
  MS_ERROR_IF_NULL(GetDevicePtr());
  const auto stream = GPUDeviceManager::GetInstance().GetStream(stream_id);
  MS_ERROR_IF_NULL(stream);

  CHECK_RET_WITH_RETURN_ERROR(CudaDriver::CopyDeviceMemToHostAsync(host_ptr, GetDevicePtr(), size, stream),
                              "CopyHostMemToDeviceAsync failed");
  return true;
}

void GPUDeviceAddress::ClearDeviceMemory() {
  std::lock_guard<std::recursive_mutex> lock(ptr_mutex_);
  if (offload_ptr_ != nullptr) {
    auto device_context = GetDeviceContext();
    MS_EXCEPTION_IF_NULL(device_context);
    device_context->device_res_manager_->FreeOffloadMemory(offload_ptr_);
    offload_ptr_ = nullptr;
  }
  if (GetDevicePtr() != nullptr && from_mem_pool()) {
    GPUMemoryAllocator::GetInstance().FreeTensorMem(GetDevicePtr());
    SetDevicePtr(nullptr);
  }
}

void GPUDeviceAddress::ClearUserData() {
  if (user_data() == nullptr || (!user_data()->has(kUserDataType))) {
    return;
  }

  auto user_data_type = user_data()->get<UserDataType>(kUserDataType);
  MS_EXCEPTION_IF_NULL(user_data_type);
  if (*user_data_type == UserDataType::kUserTypeHashTable) {
#if CUDA_VERSION > 11000 && defined(__linux__)
    auto key_type = user_data()->get<TypeId>(kHashTableKeyType);
    auto value_type = user_data()->get<TypeId>(kHashTableValueType);
    MS_EXCEPTION_IF_NULL(key_type);
    MS_EXCEPTION_IF_NULL(value_type);
    const auto &iter = hashtable_func_list.find({*key_type, *value_type});
    if (iter != hashtable_func_list.end()) {
      return std::get<kClearFuncIndex>(iter->second)(user_data());
    } else {
      MS_LOG(EXCEPTION) << "Unsupported hash table type:" << *key_type << " and:" << *value_type;
    }
#else
    MS_LOG(EXCEPTION) << "Invalid platform or cuda version for gpu hash table.";
#endif
  }
}

GPUDeviceAddress::~GPUDeviceAddress() {
  // Only release offload memory, release device memory when `kernel_tensor_` in base class destroyed, because maybe
  // multi GPUDeviceAddress objects use same device pointer in ref case.
  std::lock_guard<std::recursive_mutex> lock(ptr_mutex_);
  if (offload_ptr_ != nullptr) {
    auto device_context = GetDeviceContext();
    MS_EXCEPTION_IF_NULL(device_context);
    device_context->device_res_manager_->FreeOffloadMemory(offload_ptr_);
    offload_ptr_ = nullptr;
  }
  LoadableDeviceAddress::ReleaseResource();
}

bool GPUDeviceAddress::CopyBetweenHostDevice(void *dst, const void *src, size_t size, bool async, size_t stream_id,
                                             bool host_to_device) const {
  MS_ERROR_IF_NULL(dst);
  MS_ERROR_IF_NULL(src);
  const auto stream = GPUDeviceManager::GetInstance().GetStream(stream_id);
  MS_ERROR_IF_NULL(stream);
  if (host_to_device) {
    CHECK_RET_WITH_RETURN_ERROR(CudaDriver::CopyHostMemToDeviceAsync(dst, src, size, stream),
                                "CopyHostMemToDeviceAsync failed");
  } else {
    CHECK_RET_WITH_RETURN_ERROR(CudaDriver::CopyDeviceMemToHostAsync(dst, src, size, stream),
                                "CopyDeviceMemToHostAsync failed");
  }
  if (async) {
    auto record_event = std::make_shared<GpuEvent>();
    record_event->set_record_stream(stream);
    record_event->RecordEvent();
    swap_event_.device_event_ = record_event;
  } else {
    GPUDeviceManager::GetInstance().SyncStream(stream);
  }
  return true;
}

bool GPUDeviceAddress::CopyDeviceToHost(void *dst, const void *src, size_t size, bool async, size_t stream_id) const {
  return CopyBetweenHostDevice(dst, src, size, async, stream_id, false);
}

bool GPUDeviceAddress::CopyHostToDevice(void *dst, const void *src, size_t size, bool async, size_t stream_id) const {
  return CopyBetweenHostDevice(dst, src, size, async, stream_id, true);
}

bool GPUDeviceAddress::CopyHostToDevice(void *dst, const void *src, const size_t &size) const {
  return GPUDeviceManager::GetInstance().CopyHostMemToDevice(dst, src, size);
}

bool GPUDeviceAddress::CopyDeviceToHost(void *dst, const void *src, const size_t &size) const {
  return GPUDeviceManager::GetInstance().CopyDeviceMemToHost(dst, const_cast<void *>(src), size);
}

/*
 * Feature group: Dump, Online debugger.
 * Target device group: GPU.
 * Runtime category: Old runtime, MindRT.
 * Description: Load tensor to host and create tensor_data object for the loaded tensor.
 */
#ifdef ENABLE_DEBUGGER
bool GPUDeviceAddress::LoadMemToHost(const std::string &tensor_name, int execution_order, const std::string &host_fmt,
                                     const ShapeVector &host_shape, TypeId host_type, size_t slot, bool keep_prev,
                                     uint32_t root_graph_id, bool force_update, bool) const {
  bool ret = false;
  if (GetSize() == 0) {
    return true;
  }
  auto debugger = Debugger::GetInstance();
  MS_EXCEPTION_IF_NULL(debugger);
  if (debugger->TensorExistsInCurrent(tensor_name) && !force_update) {
    MS_LOG(INFO) << tensor_name << " already loaded for this step so not loading it again.";
    return true;
  }

  if (host_type > TypeId::kNumberTypeEnd || host_type < TypeId::kNumberTypeBegin || host_type == kNumberTypeComplex64) {
    MS_LOG(INFO) << "Cannot create tensor with type: " << TypeIdLabel(host_type);
    return false;
  }
  mindspore::tensor::TensorPtr out_tensor = std::make_shared<tensor::Tensor>(host_type, host_shape);
  size_t host_size = out_tensor->data().nbytes();
  if (host_size == 0) {
    MS_LOG(INFO) << "Host size is 0 for tensor: " << tensor_name << ", no need to load.";
  }
  auto ret_rt_memcpy = SyncDeviceToHost(host_shape, host_size, host_type, out_tensor->data_c());
  if (!ret_rt_memcpy) {
    MS_LOG(ERROR) << "Copy device mem to host failed";
    return ret;
  }
  auto tensor_data = std::make_shared<mindspore::TensorData>();
  MS_EXCEPTION_IF_NULL(tensor_data);
  tensor_data->SetName(tensor_name);
  tensor_data->SetExecutionOrder(execution_order);
  tensor_data->SetSlot(slot);
  tensor_data->SetTensor(out_tensor);
  tensor_data->SetDataPtr(static_cast<char *>(out_tensor->data_c()));
  tensor_data->SetByteSize(out_tensor->data().nbytes());
  tensor_data->SetType(host_type);
  tensor_data->SetShape(out_tensor->shape());
  tensor_data->SetRootGraphId(root_graph_id);
  tensor_data->SetFormat(host_fmt);
  ret = debugger->LoadNewTensor(tensor_data, keep_prev);
  MS_LOG(INFO) << "E2E tensor name is " << tensor_name;
  return ret;
}
#endif
}  // namespace gpu
}  // namespace device
}  // namespace mindspore
