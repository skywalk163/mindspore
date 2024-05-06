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

#ifndef MINDSPORE_MINDSPORE_CCSRC_RUNTIME_DEVICE_LOADABLE_DEVICE_ADDRESS_H_
#define MINDSPORE_MINDSPORE_CCSRC_RUNTIME_DEVICE_LOADABLE_DEVICE_ADDRESS_H_

#include <memory>
#include <string>
#include "include/backend/device_address.h"
#include "runtime/hardware/device_context.h"
#include "runtime/hardware/device_context_manager.h"

namespace mindspore {
namespace device {
struct SwapEvent {
  bool NeedWait() const {
    return aio_token_ != kInvalidAsyncIOToken || (device_event_ != nullptr && device_event_->NeedWait());
  }
  AsyncIOToken aio_token_{kInvalidAsyncIOToken};
  std::shared_ptr<DeviceEvent> device_event_{nullptr};
};
using SwapEventPtr = std::shared_ptr<SwapEvent>;

// LoadableDeviceAddress provide the ability to offload data on device to ddr or disk and load it back later.
class BACKEND_EXPORT LoadableDeviceAddress : public DeviceAddress {
 public:
  explicit LoadableDeviceAddress(const KernelTensorPtr &kernel_tensor) : DeviceAddress(kernel_tensor) {}
  LoadableDeviceAddress(void *ptr, size_t size) : DeviceAddress(ptr, size) {}
  LoadableDeviceAddress(void *ptr, size_t size, const string &format, TypeId type_id)
      : DeviceAddress(ptr, size, format, type_id) {}
  LoadableDeviceAddress(void *ptr, size_t size, const std::string &format, TypeId type_id,
                        const KernelWithIndex &node_index)
      : DeviceAddress(ptr, size, format, type_id, node_index) {}
  LoadableDeviceAddress(void *ptr, size_t size, const std::string &format, TypeId type_id,
                        const std::string &device_name, uint32_t device_id)
      : DeviceAddress(ptr, size, format, type_id, device_name, device_id) {}
  LoadableDeviceAddress(void *ptr, size_t size, const std::string &device_name, uint32_t device_id)
      : DeviceAddress(ptr, size, device_name, device_id) {}
  LoadableDeviceAddress(void *ptr, size_t size, const std::string &format, TypeId type_id,
                        const KernelWithIndex &node_index, const std::string &device_name, uint32_t device_id)
      : DeviceAddress(ptr, size, format, type_id, node_index, device_name, device_id) {}

  bool mem_offloaded() const final { return mem_offloaded_; }

  // Offload data from device to host and free device memory
  bool Offload(size_t stream_id) final;

  // Load data from host to device and free host memory
  bool Load(size_t stream_id) final;

  // Move data to destination hardware and free resource on source hardware
  bool MoveTo(StorageType dest, bool async, size_t stream_id) override;

  bool Wait() const override;

  void SetStorageInfo(const StorageInfo &storage_info) final;
  StorageInfo GetStorageInfo() const final;

  // Set host ptr data offloaded to
  void SetOffloadPtr(void *offload_ptr) final;
  // Get offloaded host ptr
  void *GetOffloadPtr() const final;

  // Return whether DeviceAddress has a valid ptr.
  bool IsPtrValid() const final;

  // Load first if data is offloaded and return the device ptr.
  void *GetValidPtr(size_t stream_id) final;

  void Swap(DeviceAddress *other) override;

  virtual bool DeviceToFileDirectly(void *ptr, size_t size, const std::string &file_name, size_t stream_id) const {
    return false;
  }

  virtual bool FileToDeviceDirectly(void *ptr, size_t size, const std::string &file_name, size_t stream_id) const {
    return false;
  }

  virtual void set_swappable(bool swappable) { swappable_ = swappable; }
  virtual bool swappable() {
    return swappable_ && !(status_ == DeviceAddressStatus::kInDevice && GetDevicePtr() == nullptr);
  }

 protected:
  DeviceContext *GetDeviceContext() const {
    DeviceContext *device_context = nullptr;
    device_context = DeviceContextManager::GetInstance().GetOrCreateDeviceContext({device_name(), device_id()});
    return device_context;
  }

  bool MoveToDevice(bool async, size_t stream_id = kDefaultStreamIndex) const;
  bool MoveToHost(bool async, size_t stream_id = kDefaultStreamIndex) const;
  bool MoveToFile(bool async, size_t stream_id = kDefaultStreamIndex) const;

  virtual bool CopyDeviceToHost(void *dst, const void *src, size_t size, bool async, size_t stream_id) const {
    return false;
  }
  virtual bool CopyHostToDevice(void *dst, const void *src, size_t size, bool async, size_t stream_id) const {
    return false;
  }
  virtual bool CopyHostToFile(const std::string &dst, const void *src, size_t size, bool async) const;
  virtual bool CopyFileToHost(void *dst, const std::string &src, size_t size, bool async) const;

  void ReleaseResource();

  std::string GetSwapFileName() const;
  size_t GetFileAlignSize() const;

  bool mem_offloaded_{false};
  void *offload_ptr_{nullptr};
  mutable SwapEvent swap_event_;
  mutable StorageInfo storage_info_;
  bool swappable_{false};
};
}  // namespace device
}  // namespace mindspore

#endif  // MINDSPORE_MINDSPORE_CCSRC_RUNTIME_DEVICE_LOADABLE_DEVICE_ADDRESS_H_
