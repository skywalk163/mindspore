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

#ifndef MINDSPORE_CCSRC_RUNTIME_DEVICE_GPU_GPU_DEVICE_ADDRESS_H_
#define MINDSPORE_CCSRC_RUNTIME_DEVICE_GPU_GPU_DEVICE_ADDRESS_H_

#include <string>
#include <vector>
#include "include/backend/device_address.h"
#include "runtime/device/loadable_device_address.h"

using ShapeVecotr = std::vector<int>;

namespace mindspore {
#ifdef ENABLE_DEBUGGER
class Debugger;
#endif
namespace device {
namespace gpu {
class GPUDeviceAddress : public LoadableDeviceAddress {
 public:
  explicit GPUDeviceAddress(const KernelTensorPtr &kernel_tensor) : LoadableDeviceAddress(kernel_tensor) {
    SetDevicePtrDeleter();
  }
  GPUDeviceAddress(void *ptr, size_t size) : LoadableDeviceAddress(ptr, size) { SetDevicePtrDeleter(); }
  GPUDeviceAddress(void *ptr, size_t size, const string &format, TypeId type_id)
      : LoadableDeviceAddress(ptr, size, format, type_id) {
    SetDevicePtrDeleter();
  }
  GPUDeviceAddress(void *ptr, size_t size, const std::string &format, TypeId type_id, const KernelWithIndex &node_index)
      : LoadableDeviceAddress(ptr, size, format, type_id, node_index) {
    SetDevicePtrDeleter();
  }
  GPUDeviceAddress(void *ptr, size_t size, const std::string &format, TypeId type_id, const std::string &device_name,
                   uint32_t device_id)
      : LoadableDeviceAddress(ptr, size, format, type_id, device_name, device_id) {
    SetDevicePtrDeleter();
  }
  ~GPUDeviceAddress() override;

  bool SyncDeviceToHost(size_t size, void *host_ptr) const override;
  bool SyncHostToDevice(size_t size, const void *host_ptr) const override;
  bool SyncDeviceToHost(const ShapeVector &shape, size_t size, TypeId type, void *host_ptr) const override;
  bool SyncHostToDevice(const ShapeVector &shape, size_t size, TypeId type, const void *host_ptr,
                        const std::string &format) const override;
  bool SyncDeviceToDevice(const DeviceSync *src_device_addr) const override;
  bool SyncDeviceToDevice(const ShapeVector &shape, size_t size, TypeId type, const void *src_ptr,
                          const std::string &format) const override;
  bool CopyDeviceToHost(void *dst, const void *src, const size_t &size) const override;
  bool CopyHostToDevice(void *dst, const void *src, const size_t &size) const override;

  void ClearDeviceMemory() override;
  DeviceType GetDeviceType() const override { return DeviceType::kGPU; }

#ifdef ENABLE_DEBUGGER
  bool LoadMemToHost(const std::string &tensor_name, int execution_order, const std::string &host_fmt,
                     const ShapeVector &host_shape, TypeId host_type, size_t slot, bool keep_prev,
                     uint32_t root_graph_id, bool force_update, bool trans_flag) const override;
#endif

  // Asynchronously copy host memory to device side.
  bool AsyncHostToDevice(const ShapeVector &, size_t size, TypeId, const void *host_ptr,
                         size_t stream_id) const override;

  // Asynchronously copy device memory to host side.
  bool AsyncDeviceToHost(const ShapeVector &, size_t size, TypeId, void *host_ptr, size_t stream_id) const override;
  void ClearUserData() override;

 protected:
  bool CopyDeviceToHost(void *dst, const void *src, size_t size, bool async, size_t stream_id) const override;
  bool CopyHostToDevice(void *dst, const void *src, size_t size, bool async, size_t stream_id) const override;

 private:
  bool CopyBetweenHostDevice(void *dst, const void *src, size_t size, bool async, size_t stream_id,
                             bool host_to_device) const;

  // Set a device pointer destructor to kernel tensor, used to release resource reclaiming of the device pointer
  // automatically when DeviceAddress destructed.
  void SetDevicePtrDeleter();
};
}  // namespace gpu
}  // namespace device
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_RUNTIME_DEVICE_GPU_GPU_DEVICE_ADDRESS_H_
