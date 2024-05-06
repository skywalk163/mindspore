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

#include <algorithm>
#include <utility>
#include "plugin/device/ascend/hal/device/ascend_memory_pool.h"
#include "plugin/device/ascend/hal/device/ascend_memory_adapter.h"
#include "plugin/device/ascend/hal/device/ascend_gmem_adapter.h"
#include "plugin/device/ascend/hal/device/ascend_stream_manager.h"
#include "utils/log_adapter.h"
#include "utils/convert_utils_base.h"
#include "transform/symbol/acl_rt_symbol.h"
#include "transform/symbol/symbol_utils.h"

namespace mindspore {
namespace device {
namespace ascend {
// The minimum unit size (8MB) of memory block used for dynamic extend in graph run mode.
static const size_t ASCEND_COMMON_POOL_ALLOC_UNIT_SIZE_FOR_GRAPH_RUN_MODE = 8 << 20;
constexpr char kGlobalOverflowWorkspace[] = "GLOBAL_OVERFLOW_WORKSPACE";

void AscendMemoryPool::SetMemPoolBlockSize(size_t available_device_mem_size) {
  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  float mem_block_size = ms_context->get_param<float>(MS_CTX_MEMPOOL_BLOCK_SIZE);
  // set from context configuration
  if (!common::IsFloatEqual(mem_block_size, kDefaultMempoolBlockSize)) {
    size_t config_size = FloatToSize(mem_block_size * kGBToByte);
    if (config_size > available_device_mem_size) {
      MS_LOG(WARNING) << "Memory pool block size " << config_size
                      << " is bigger than currently available maximum memory " << available_device_mem_size
                      << ", and the actual effective value will be " << available_device_mem_size;
    }
    // Reserve 1G for persistent_mem
    if (available_device_mem_size > kDynamicMemAllocUnitSize) {
      available_device_mem_size -= kDynamicMemAllocUnitSize;
    }
    size_t real_block_size = std::min(config_size, available_device_mem_size);
    SetMemAllocUintSize(real_block_size, kDynamicMemAllocUnitSize);
    return;
  }

  // set by default configuration
  const auto graph_mode = (ms_context->get_param<int>(MS_CTX_EXECUTION_MODE) == kGraphMode);
  const bool is_graph_run_mode = ms_context->get_param<bool>(MS_CTX_ENABLE_TASK_SINK);
  if (graph_mode && is_graph_run_mode) {
    SetMemAllocUintSize(ASCEND_COMMON_POOL_ALLOC_UNIT_SIZE_FOR_GRAPH_RUN_MODE,
                        ASCEND_COMMON_POOL_ALLOC_UNIT_SIZE_FOR_GRAPH_RUN_MODE);
  } else {
    SetMemAllocUintSize(kDynamicMemAllocUnitSize, kDynamicMemAllocUnitSize);
  }
}

namespace {
bool NoAdditionalMemory() {
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  const auto is_cell_reuse = context->CellReuseLevel() != CellReuseLevel::kNoCellReuse;
  const auto is_multi_graph_sink = context->get_param<bool>(MS_CTX_IS_MULTI_GRAPH_SINK);
  const auto is_task_sink = context->get_param<bool>(MS_CTX_ENABLE_TASK_SINK);
  return (is_cell_reuse || is_multi_graph_sink) && is_task_sink;
}
}  // namespace

size_t AscendMemoryPool::CalMemBlockAllocSize(size_t size, bool from_persistent_mem, bool need_recycle) {
  auto device_free_mem_size = free_mem_size();
  if (device_free_mem_size < size && common::IsNeedProfileMemory()) {
    device_free_mem_size = size;
  }
  if (device_free_mem_size < size) {
    MS_LOG(INFO) << "The device memory is not enough, the free memory size is " << device_free_mem_size
                 << ", but the alloc size is " << size;
    MS_LOG(INFO) << "The dynamic memory pool total size is "
                 << device::ascend::AscendMemoryPool::GetInstance().TotalMemStatistics() / kMBToByte
                 << "M, total used size is "
                 << device::ascend::AscendMemoryPool::GetInstance().TotalUsedMemStatistics() / kMBToByte
                 << "M, used peak size is "
                 << device::ascend::AscendMemoryPool::GetInstance().UsedMemPeakStatistics() / kMBToByte << "M.";
    MS_LOG(INFO) << "Memory Statistics:" << AscendMemAdapter::GetInstance().DevMemStatistics();
    return 0;
  }

  size_t alloc_mem_size;
  SetMemPoolBlockSize(device_free_mem_size);
  auto alloc_mem_unit_size = MemAllocUnitSize(from_persistent_mem);
  if (need_recycle) {
    alloc_mem_unit_size = kDynamicMemAllocUnitSize;
  }
  MS_LOG(DEBUG) << "Get unit block size " << alloc_mem_unit_size;
  alloc_mem_size = alloc_mem_unit_size;

  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  const bool is_graph_run_mode = ms_context->get_param<bool>(MS_CTX_ENABLE_TASK_SINK);
  if (is_graph_run_mode) {
    // Growing at adding alloc unit size
    while (alloc_mem_size < size) {
      alloc_mem_size = alloc_mem_size + alloc_mem_unit_size;
    }
  } else {
    // Growing at twice of alloc unit size
    constexpr size_t kDouble = 2;
    while (alloc_mem_size < size) {
      alloc_mem_size = alloc_mem_size * kDouble;
    }
  }

  alloc_mem_size = std::min(alloc_mem_size, device_free_mem_size);
  if (NoAdditionalMemory() && !need_recycle) {
    alloc_mem_size = std::min(alloc_mem_size, size);
  }
  return alloc_mem_size;
}

size_t AscendMemoryPool::AllocDeviceMem(size_t size, DeviceMemPtr *addr) {
  MS_LOG(INFO) << "Malloc Memory for Pool, size: " << size;
  if (size == 0) {
    MS_LOG(EXCEPTION) << "Failed to alloc memory pool resource, the size is zero!";
  }
  *addr = AscendMemAdapter::GetInstance().MallocStaticDevMem(size);
  if (*addr == nullptr) {
    MS_LOG(EXCEPTION) << "Alloc device memory pool address is nullptr, failed to alloc memory pool resource!";
  }
  return size;
}

DeviceMemPtr AscendMemoryPool::AllocOverflowTensorMem(size_t size, bool from_persistent_mem) {
  size_t align_size = AlignMemorySize(size);
  std::lock_guard<std::mutex> locker(mutex_);
  auto iter = overflow_memory_info_map_.find(kGlobalOverflowWorkspace);
  if (iter != overflow_memory_info_map_.cend()) {
    return iter->second;
  }
  DeviceMemPtr overflow_memory_ptr = AllocTensorMem(align_size, from_persistent_mem);
  MS_EXCEPTION_IF_NULL(overflow_memory_ptr);
  auto acl_ret = CALL_ASCEND_API(aclrtMemset, overflow_memory_ptr, align_size, 0, align_size);
  if (acl_ret != ACL_RT_SUCCESS) {
    MS_LOG(EXCEPTION) << "Clear overflow memory failed, aclrtMemset size = " << align_size << ", ret = " << acl_ret;
  }
  (void)overflow_memory_info_map_.emplace(kGlobalOverflowWorkspace, overflow_memory_ptr);
  return overflow_memory_ptr;
}

size_t AscendMemoryPool::GetMaxUsedMemSize() const {
  void *min_used_addr = GetMinUsingMemoryAddr();
  if (min_used_addr == nullptr) {
    return 0;
  }
  auto max_used_hbm = AscendMemAdapter::GetInstance().GetMsUsedHbmSize();
  size_t static_offset = reinterpret_cast<uint8_t *>(min_used_addr) - AscendMemAdapter::GetInstance().GetBaseAddr();
  return LongToSize(max_used_hbm) - static_offset;
}

const bool AscendMemoryPool::IsEnableEagerFree() const {
  return AscendGmemAdapter::GetInstance().is_eager_free_enabled();
}

const bool AscendMemoryPool::SyncAllStreams() { return AscendStreamMng::GetInstance().SyncAllStreams(); }

size_t AscendMemoryPool::AllocDeviceMemByEagerFree(size_t size, DeviceMemPtr *addr) {
  return AscendGmemAdapter::GetInstance().AllocDeviceMem(size, addr);
}

size_t AscendMemoryPool::FreeDeviceMemByEagerFree(const DeviceMemPtr addr, const size_t size) {
  return AscendGmemAdapter::GetInstance().EagerFreeDeviceMem(addr, size);
}

bool AscendMemoryPool::FreeDeviceMem(const DeviceMemPtr &addr) {
  MS_EXCEPTION_IF_NULL(addr);
  int64_t max_actual = ActualPeakStatistics();
  MS_LOG(INFO) << "Max actual used memory size is " << max_actual;
  AscendMemAdapter::GetInstance().UpdateActualPeakMemory(max_actual);
  int64_t max_peak = UsedMemPeakStatistics();
  MS_LOG(INFO) << "Max peak used memory size is " << max_peak;
  AscendMemAdapter::GetInstance().UpdateUsedPeakMemory(max_peak);
  return AscendMemAdapter::GetInstance().FreeStaticDevMem(addr);
}

void AscendMemoryPool::ResetIdleMemBuf() const {
  auto fn = [this](const MemStatusManagerPtr &mem_mng) {
    MS_EXCEPTION_IF_NULL(mem_mng);
    if (mem_mng->mem_block_list_.empty()) {
      return;
    }
    const auto &stream_ids = mem_mng->GetStreamIds();
    for (const auto stream_id : stream_ids) {
      auto key = std::make_pair(stream_id, DynamicMemBufStatus::kMemBufIdle);
      const auto &&iter = mem_mng->mem_bufs_.find(key);
      if (iter == mem_mng->mem_bufs_.end()) {
        continue;
      }
      const auto &mem_buf_map = iter->second;
      for (auto &&idle_iter = mem_buf_map.begin(); idle_iter != mem_buf_map.end(); idle_iter++) {
        auto &mem_buf = idle_iter->second;
        MS_EXCEPTION_IF_NULL(mem_buf);
        (void)CALL_ASCEND_API(aclrtMemset, mem_buf->device_addr_, mem_buf->size_, 0, mem_buf->size_);
      }
    }
  };
  fn(persistent_mem());
  fn(common_mem());
}

size_t AscendMemoryPool::free_mem_size() { return AscendMemAdapter::GetInstance().FreeDevMemSize(); }

uint64_t AscendMemoryPool::total_mem_size() const { return AscendMemAdapter::GetInstance().MaxHbmSizeForMs(); }
}  // namespace ascend
}  // namespace device
}  // namespace mindspore
