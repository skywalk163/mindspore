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

#ifndef MINDSPORE_CCSRC_RUNTIME_DEVICE_ASCEND_ASCEND_DEVICE_ADDRESS_H_
#define MINDSPORE_CCSRC_RUNTIME_DEVICE_ASCEND_ASCEND_DEVICE_ADDRESS_H_

#include <string>
#include <vector>
#include <memory>
#include "include/backend/device_address.h"
#include "runtime/device/loadable_device_address.h"
#include "runtime/device/kernel_runtime.h"
#include "plugin/device/ascend/hal/device/ascend_memory_pool.h"
#include "plugin/device/ascend/hal/device/launch_transdata.h"
#include "ir/dtype.h"
#include "kernel/kernel.h"
#include "utils/shape_utils.h"
#include "acl/acl_rt.h"

namespace mindspore {
#ifdef ENABLE_DEBUGGER
class Debugger;
#endif
namespace device {
class LaunchKernel;
namespace ascend {
class AscendDeviceAddress : public LoadableDeviceAddress {
 public:
  explicit AscendDeviceAddress(const KernelTensorPtr &kernel_tensor) : LoadableDeviceAddress(kernel_tensor) {
    SetDevicePtrDeleter();
  }
  explicit AscendDeviceAddress(void *ptr, size_t size) : LoadableDeviceAddress(ptr, size) { SetDevicePtrDeleter(); }
  explicit AscendDeviceAddress(void *ptr, size_t size, const std::string &device_name, uint32_t device_id)
      : LoadableDeviceAddress(ptr, size, device_name, device_id) {
    SetDevicePtrDeleter();
  }
  explicit AscendDeviceAddress(void *ptr, size_t size, const std::string &format, TypeId type_id,
                               const std::string &device_name, uint32_t device_id)
      : LoadableDeviceAddress(ptr, size, format, type_id, device_name, device_id) {
    SetDevicePtrDeleter();
  }
  explicit AscendDeviceAddress(void *ptr, size_t size, const std::string &format, TypeId type_id,
                               const KernelWithIndex &node_index, const std::string &device_name, uint32_t device_id)
      : LoadableDeviceAddress(ptr, size, format, type_id, node_index, device_name, device_id) {
    SetDevicePtrDeleter();
  }
  explicit AscendDeviceAddress(void *ptr, size_t size, const std::string &format, TypeId type_id)
      : LoadableDeviceAddress(ptr, size, format, type_id) {
    SetDevicePtrDeleter();
  }
  ~AscendDeviceAddress() override;
  bool SyncDeviceToHost(size_t size, void *const host_ptr) const override;
  bool SyncHostToDevice(size_t size, const void *host_ptr) const override;
  bool SyncDeviceToHost(const ShapeVector &shape, size_t size, TypeId type, void *host_ptr) const override;
  bool SyncHostToDevice(const ShapeVector &shape, size_t size, TypeId type, const void *host_ptr,
                        const std::string &format) const override;
  bool SyncHostToDevice(const ShapeVector &shape, size_t size, TypeId type, const std::string &format,
                        const tensor::TensorDataPtr &tensor_data) const override;
  bool AsyncDeviceToDevice(const ShapeVector &shape, size_t size, TypeId type, const void *src_ptr,
                           const std::string &format) const override;
  bool SyncDeviceToDevice(const ShapeVector &shape, size_t size, TypeId type, const void *src_ptr,
                          const std::string &format) const override;
  bool AsyncHostToDevice(size_t size, TypeId /* type */,
                                            const void *host_ptr) const override;
  bool SyncDeviceToDevice(const DeviceSync *src_device_addr) const override;
  bool CopyDeviceToHost(void *dst, const void *src, const size_t &size) const override;
  bool CopyHostToDevice(void *dst, const void *src, const size_t &size) const override;
  void ClearDeviceMemory() override;
  DeviceType GetDeviceType() const override { return DeviceType::kAscend; }
#ifndef ENABLE_SECURITY
  bool DumpMemToFile(const std::string &filepath, const std::string &host_fmt, const ShapeVector &host_shape,
                     TypeId host_type, bool trans_flag) const override;
#endif
#ifdef ENABLE_DEBUGGER
  bool LoadMemToHost(const std::string &tensor_name, int execution_order, const std::string &host_fmt,
                     const ShapeVector &host_shape, TypeId host_type, size_t slot, bool keep_prev,
                     uint32_t root_graph_id, bool force_update, bool trans_flag) const override;
#endif

  // Asynchronously copy host memory to device side.
  bool AsyncHostToDevice(const ShapeVector &shape, size_t size, TypeId type, const void *host_ptr,
                         size_t stream_id) const;

  // Asynchronously copy device memory to host side.
  bool AsyncDeviceToHost(const ShapeVector &shape, size_t size, TypeId type, void *host_ptr, size_t stream_id) const;

  void set_communication_ptr(uint8_t *communication_ptr) override {
    communication_ptr_ = communication_ptr;
    // The communication_ptr_ should free to memory pool instead of GetDevicePtr(), so must update device pointer
    // deleter.
    SetDevicePtrDeleter();
  }

 protected:
  bool CopyDeviceToHost(void *dst, const void *src, size_t size, bool async, size_t stream_id) const override;
  bool CopyHostToDevice(void *dst, const void *src, size_t size, bool async, size_t stream_id) const override;

  bool DeviceToFileDirectly(void *ptr, size_t size, const std::string &file_name, size_t stream_id) const override;

  bool FileToDeviceDirectly(void *ptr, size_t size, const std::string &file_name, size_t stream_id) const override;

  void DeviceToDevice(void *dst, void *src, size_t size, size_t stream_id) const;

 private:
  bool SyncDeviceToHostAndConvertFormat(const ShapeVector &shape, size_t size, TypeId type, void *host_ptr) const;
  bool ConvertFormatAndSyncHostToDevice(const ShapeVector &shape, size_t size, TypeId type, const void *host_ptr,
                                        const tensor::TensorDataPtr &tensor_data) const;
  bool SyncDeviceToHostAndConvertFormatBasedOnTransData(const ShapeVector &host_shape, size_t size,
                                                        mindspore::TypeId type, void *host_ptr) const;
  bool SyncDeviceToDeviceWithDiffFormatType(const DeviceSync *src_device_addr) const;

  bool SyncHostToDeviceImpl(const ShapeVector &shape, size_t size, mindspore::TypeId type, const void *host_ptr,
                            const std::string &format, const tensor::TensorDataPtr &tensor_data = nullptr) const;
  void SyncStream() const;
  bool SyncStream(size_t stream_id) const;
  bool Float64ToFloatAndSyncHostToDevice(void *dst, size_t dst_size, const void *src, size_t src_size,
                                         const tensor::TensorDataPtr &tensor_data) const;
  bool SyncDeviceToHostAndFloatToFloat64(void *dst, size_t dst_size, const void *src, size_t src_size) const;
  void SyncMemory(void *dst, const void *src, uint64_t size, aclrtMemcpyKind kind,
                  const tensor::TensorDataPtr &tensor_data = nullptr) const;
  void SyncHostMemoryToDeviceWithCopySrc(void *dst, const void *src, uint64_t size, aclrtMemcpyKind kind,
                                         KernelRuntime *runtime_instance) const;
  void SyncHostMemoryToDeviceForTensorFromNumpy(void *dst, const void *src, uint64_t size, aclrtMemcpyKind kind,
                                                KernelRuntime *runtime_instance) const;
  void SyncHostMemoryToDeviceWithTensorData(void *dst, const void *src, uint64_t size, aclrtMemcpyKind kind,
                                            const tensor::TensorDataPtr &tensor_data,
                                            KernelRuntime *runtime_instance) const;
  ShapeVector GetDeviceShape(ShapeVector *host_shape) const;
  std::shared_ptr<LaunchTransData> CreateLaunchTransData(const ShapeVector &host_shape, const std::string &ori_format,
                                                         const std::string &dst_format) const;
  mutable std::shared_ptr<LaunchTransData> launch_transdata_{nullptr};
  void BindDevice() const;
  void CopyHostToDevice(const void *src, uint64_t size, const tensor::TensorDataPtr &tensor_data) const;
  void CopyDeviceToHost(void *dst, uint64_t size) const;
  bool CopyBetweenHostDevice(void *dst, const void *src, size_t size, bool async, size_t stream_id,
                             bool host_to_device) const;
  bool CopyBetweenFileDeviceDirectly(void *ptr, const std::string &file_name, size_t size, size_t stream_id,
                                     bool file_to_device) const;

  // The 'const' for this class is irrational, but I abide by it
  int64_t GetGroupsWithCache() const;

  // Set a device pointer destructor to kernel tensor, used to release resource reclaiming of the device pointer
  // automatically when DeviceAddress destructed.
  void SetDevicePtrDeleter();

  mutable int64_t groups_ = 1;

  // When the device address is used by communication node, create protect area [kMemAlignSize -- data -- kMemAlignSize]
  // memory buffer, communication_ptr_(allocated from ascend memory pool) + kMemAlignSize = device pointer (could get by
  // GetDevicePtr()), device pointer is to really used by communication node, and communication_ptr_ is used to free
  // memory to Ascend memory pool.
  uint8_t *communication_ptr_{nullptr};
};
using AscendDeviceAddressPtr = std::shared_ptr<AscendDeviceAddress>;
}  // namespace ascend
}  // namespace device
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_RUNTIME_DEVICE_ASCEND_ASCEND_DEVICE_ADDRESS_H_
