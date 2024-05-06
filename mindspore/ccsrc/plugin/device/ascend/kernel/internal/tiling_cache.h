/**
 * Copyright 2024 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_KERNEL_INTERNAL_TILING_CACHE_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_KERNEL_INTERNAL_TILING_CACHE_H_

#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <mutex>

#include "mindspore/core/ir/primitive.h"
#include "kernel/kernel.h"
#include "acl/acl.h"
#include "plugin/device/ascend/hal/device/ascend_memory_pool.h"
#include "mindspore/core/utils/ms_context.h"
#include "mindspore/ccsrc/runtime/hardware/device_context.h"
#include "mindspore/ccsrc/runtime/hardware/device_context_manager.h"
#include "./internal_kernel.h"

namespace mindspore::kernel {

struct TilingInfo {
  internal::DeviceRawBuf device_buf_;
  internal::CacheInfo cache_info_;

  TilingInfo() {
    this->device_buf_.size_ = 0;
    this->device_buf_.addr_ = nullptr;
  }

  TilingInfo(const TilingInfo &other) {
    this->device_buf_ = other.device_buf_;
    cache_info_ = other.cache_info_;
  }

  const TilingInfo &operator=(const TilingInfo &other) {
    this->device_buf_ = other.device_buf_;
    cache_info_ = other.cache_info_;
    return *this;
  }
};

// Set the max cache size to avoid memory leak. one buf_ 8192/8/1024 = 1kb, total kMaxSize kb.
constexpr int kMaxSize = 10240;
// Detail buffer parameters.
constexpr int kBufSize = 8192;
constexpr int kBufMaxSize = kBufSize + 1024;
constexpr size_t kMemAlignSize = 64;
constexpr size_t kMaxDevBlockSize = kMaxSize * kMemAlignSize;

class TilingCacheMgr {
 public:
  // TilingCacheMgr is used to manage the tiling caches.
  TilingCacheMgr() { SetDeviceContext(); }
  ~TilingCacheMgr() { Clear(); }
  static TilingCacheMgr &GetInstance() noexcept {
    static TilingCacheMgr instance;
    return instance;
  }

  TilingCacheMgr(const TilingCacheMgr &) = delete;
  TilingCacheMgr(const TilingCacheMgr &&) = delete;
  TilingCacheMgr &operator=(const TilingCacheMgr &&) = delete;
  TilingCacheMgr &operator=(const TilingCacheMgr &) = delete;

  // Generate tiling cache key by input, kernel should provide the specific data which will change
  // the tiling tactics.
  template <typename T, typename... Args>
  uint64_t GenTilingCacheKey(const T &arg, const Args &... args) {
    std::lock_guard<std::mutex> lock(key_mtx_);
    ResetCacheKey();
    GenCache(arg, args...);
    return calc_hash_id();
  }

  // Generate tiling cache key by default inputs. All of the input shape, type, format and depend value
  // will be the cache key. It's a normal way, which means some non-essential data will be part of the key.
  // In this situation, it will cache miss if only the non-essential data changed. So this function is not
  // recommended unless we don't know the data which influence the tiling result.
  uint64_t GenTilingCacheKey(const std::string &name, PrimitivePtr prim, const std::vector<KernelTensor *> &inputs);

  // Get the device tiling buffer form cache if exist, else create a new one.
  TilingInfo GetOrCreateTilingInfo(const uint64_t key,
                                   const std::function<int(internal::HostRawBuf &, internal::CacheInfo &)> &tiling_func,
                                   size_t tiling_size);
  // Clear all the resource used in tiling cache.
  void Clear();
  // Set device context for bind device, otherwise aclrtMemcpy will hang in multi-threads.
  void SetDeviceContext() {
    auto context_ptr = mindspore::MsContext::GetInstance();
    uint32_t device_id = context_ptr->get_param<uint32_t>(MS_CTX_DEVICE_ID);
    std::string device_name = context_ptr->get_param<std::string>(MS_CTX_DEVICE_TARGET);
    device_context_ = device::DeviceContextManager::GetInstance().GetOrCreateDeviceContext({device_name, device_id});
  }

 private:
  // Store the device tiling buffer with cache key.
  std::unordered_map<uint64_t, TilingInfo> cache_buf_;
  char buf_[kBufSize];
  int offset_{0};
  int dev_offset_{0};
  void *dev_addr_{nullptr};
  internal::HostRawBuf host_tiling_buf_;
  device::DeviceContext *device_context_;
  std::mutex key_mtx_, cache_mtx_;

  uint64_t calc_hash_id();

  inline size_t AlignMemorySize(size_t size) {
    if (size == 0) {
      return kMemAlignSize;
    }
    return ((size + kMemAlignSize - 1) / kMemAlignSize) * kMemAlignSize;
  }

  inline void FreeMemoryIfFull() {
    if (cache_buf_.size() >= kMaxSize && dev_addr_ != nullptr) {
      device::ascend::AscendMemoryPool::GetInstance().FreeDeviceMem(&dev_addr_);
      dev_addr_ = nullptr;
    }
  }

  inline void SetDevAddr(TilingInfo *tiling_cache_elem, size_t tiling_size) {
    tiling_cache_elem->device_buf_.size_ = tiling_size;
    auto aligned_size = AlignMemorySize(tiling_size);
    void *tiling_cache_addr = nullptr;
    if (dev_addr_ != nullptr && dev_offset_ + aligned_size < kMaxDevBlockSize && cache_buf_.size() < kMaxSize) {
      tiling_cache_addr = static_cast<void *>(static_cast<char *>(dev_addr_) + dev_offset_);
    } else {
      dev_addr_ = nullptr;
      dev_offset_ = 0;
      device::ascend::AscendMemoryPool::GetInstance().AllocDeviceMem(kMaxDevBlockSize, &dev_addr_);
      tiling_cache_addr = dev_addr_;
    }
    dev_offset_ += aligned_size;
    tiling_cache_elem->device_buf_.addr_ = tiling_cache_addr;
  }

  // concat all the data which influence tiling result into an array[char].
  inline void ConcatKey(const void *data, size_t size) {
    if (size == 0) {
      return;
    }
    if (offset_ + size >= kBufSize) {
      offset_ = kBufMaxSize;
      return;
    }
    auto ret = memcpy_sp(buf_ + offset_, kBufSize - offset_, data, size);
    if (ret != EOK) {
      MS_LOG(EXCEPTION) << "Memcpy Failed !";
    }
    offset_ += size;
  }
  // cache for string, eg. some attributes
  void GenCache(const string &s) { ConcatKey(s.c_str(), static_cast<int64_t>(s.size())); }
  // cache for scalar, eg. depend value
  void GenCache(const ScalarPtr &scalar) {
    MS_EXCEPTION_IF_NULL(scalar);
    if (scalar->isa<BoolImm>()) {
      auto value = GetValue<bool>(scalar);
      GenCache(&value);
    } else if (scalar->isa<Int64Imm>()) {
      auto value = GetValue<int64_t>(scalar);
      GenCache(&value);
    } else if (scalar->isa<FP32Imm>()) {
      auto value = GetValue<float>(scalar);
      GenCache(&value);
    } else if (scalar->isa<Int32Imm>()) {
      auto value = GetValue<int32_t>(scalar);
      GenCache(&value);
    } else {
      MS_LOG(EXCEPTION) << "Currently not support value: " << scalar->ToString();
    }
  }
  // end
  void GenCache() {}
  // cache for type, eg. input type
  void GenCache(const TypePtr &type) {
    const auto type_id = type->type_id();
    ConcatKey(&type_id, sizeof(int));
  }
  // cache for vector<T>, eg. input shape
  template <typename T>
  void GenCache(const std::vector<T> &values) {
    ConcatKey(values.data(), values.size() * sizeof(T));
  }
  // cache for value, eg. const value from attribute
  template <typename T>
  void GenCache(const T &value) {
    ConcatKey(&value, sizeof(T));
  }
  // vector<KernelTesnor>
  void GenCache(std::vector<mindspore::kernel::KernelTensor *> inputs) {
    for (auto item : inputs) {
      GenCache(item);
    }
  }
  // cache for KernelTensor, eg. raw depend value
  void GenCache(mindspore::kernel::KernelTensor *input) {
    if (input == nullptr) {
      return;
    }
    // input is a type
    auto obj_type = input->GetType()->type_id();
    if (obj_type == kMetaTypeType) {
      GenCache(input->GetValue()->cast<TypePtr>()->type_id());
    }
    // shape
    auto shape = input->GetShapeVector();
    if (!shape.empty()) {
      GenCache(shape);
    }
    // type
    auto type = input->dtype_id();
    GenCache(type);
    // format
    auto format = input->format();
    GenCache(format);
  }

  // cache for Format, eg. NCHW..
  void GenCache(const mindspore::Format &format) { ConcatKey(&format, sizeof(int)); }
  // cache for typeid, eg. enum kNumberFloat32
  void GenCache(TypeId type_id) { ConcatKey(&type_id, sizeof(int)); }
  // cache input recursively
  template <typename T, typename... Args>
  void GenCache(const T &arg, const Args &... args) {
    GenCache(arg);
    GenCache(args...);
  }
  void ResetCacheKey() {
    offset_ = 0;
    auto ret = memset_s(buf_, sizeof(buf_), 0, sizeof(buf_));
    if (ret != EOK) {
      MS_LOG(EXCEPTION) << "Init cache failed!";
    }
  }
  void AppendToCache(const uint64_t key, TilingInfo device_tiling_buf) {
    if (cache_buf_.size() >= kMaxSize) {
      MS_LOG(WARNING) << "Cache buffer is full, stop cache tiling buf_.";
      return;
    }
    cache_buf_[key] = device_tiling_buf;
  }
};
}  // namespace mindspore::kernel
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_KERNEL_INTERNAL_TILING_CACHE_H_
