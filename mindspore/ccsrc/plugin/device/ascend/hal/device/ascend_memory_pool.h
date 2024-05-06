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

#ifndef MINDSPORE_CCSRC_RUNTIME_DEVICE_ASCEND_ASCEND_MEMORY_POOL_H_
#define MINDSPORE_CCSRC_RUNTIME_DEVICE_ASCEND_ASCEND_MEMORY_POOL_H_

#include <memory>
#include <string>
#include "utils/hash_map.h"
#include "include/backend/mem_reuse/mem_dynamic_allocator.h"

namespace mindspore {
namespace device {
namespace ascend {
class AscendMemoryPool : public DynamicMemPoolBestFit {
 public:
  ~AscendMemoryPool() override = default;
  AscendMemoryPool(const AscendMemoryPool &) = delete;
  AscendMemoryPool &operator=(const AscendMemoryPool &) = delete;

  size_t AllocDeviceMem(size_t size, DeviceMemPtr *addr) override;
  bool FreeDeviceMem(const DeviceMemPtr &addr) override;
  size_t GetMaxUsedMemSize() const override;
  size_t free_mem_size() override;
  uint64_t total_mem_size() const override;
  std::string GetMemoryPoolType() const override { return "Ascend"; }
  // Set mem pool block size
  void SetMemPoolBlockSize(size_t available_device_mem_size) override;

  void ResetIdleMemBuf() const;

  // The main program entry of memory alloc.
  DeviceMemPtr AllocOverflowTensorMem(size_t size, bool from_persistent_mem = false);

  static AscendMemoryPool &GetInstance() {
    static AscendMemoryPool instance;
    return instance;
  }

 protected:
  // Calculate memory block required alloc size when adding the memory block.
  size_t CalMemBlockAllocSize(size_t size, bool from_persistent_mem, bool need_recycle) override;

  // The related interface of device memory eager free.
  const bool IsEnableEagerFree() const override;
  const bool SyncAllStreams() override;
  size_t AllocDeviceMemByEagerFree(size_t size, DeviceMemPtr *addr) override;
  size_t FreeDeviceMemByEagerFree(const DeviceMemPtr addr, const size_t size) override;

 private:
  AscendMemoryPool() = default;
  std::mutex mutex_;
  // overflow memory info, key is kernel, val is memory ptr
  mindspore::HashMap<std::string, void *> overflow_memory_info_map_;
};
}  // namespace ascend
}  // namespace device
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_RUNTIME_DEVICE_ASCEND_ASCEND_MEMORY_POOL_H_
