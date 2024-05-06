/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_RUNTIME_HARDWARE_CPU_CPU_DEVICE_CONTEXT_H_
#define MINDSPORE_CCSRC_RUNTIME_HARDWARE_CPU_CPU_DEVICE_CONTEXT_H_

#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include "runtime/hardware/device_context.h"
#include "runtime/hardware/device_context_manager.h"
#include "runtime/device/memory_manager.h"

namespace mindspore {
namespace device {
namespace cpu {
class CPUDeviceResManager : public DeviceResManager {
 public:
  CPUDeviceResManager() {}
  ~CPUDeviceResManager() override = default;

  void Initialize() override;

  void Destroy() override;

  std::vector<void *> AllocateContinuousMemory(const std::vector<size_t> &size_list,
                                               uint32_t stream_id = kDefaultStreamIndex) const override;

  DeviceAddressPtr CreateDeviceAddress(const KernelTensorPtr &kernel_tensor) const override;

  bool LoadCollectiveCommLib() override;

  // Relevant function to allocate and free device memory of raw ptr.
  void *AllocateMemory(size_t size, uint32_t stream_id = kDefaultStreamIndex) const override;
  void FreeMemory(void *ptr) const override;
  void FreePartMemorys(const std::vector<void *> &free_addrs, const std::vector<void *> &keep_addrs,
                       const std::vector<size_t> &keep_addr_sizes) const override;
};

class CPUKernelExecutor : public KernelExecutor {
 public:
  CPUKernelExecutor() = default;
  ~CPUKernelExecutor() override = default;

  void OptimizeGraph(const FuncGraphPtr &graph) const override;

  void CreateKernel(const std::vector<CNodePtr> &nodes) const override;
  kernel::KernelModPtr CreateKernelMod(const std::string &op_name) const override;

  // Kernel that is not supported by other device can be backed off and rebuilt on the CPU.
  // The function will set kernel info and create kernel mod.
  void RebuildKernelSelectBackoffOp(const std::vector<CNodePtr> &nodes) const;

  void PreprocessBeforeRun(const FuncGraphPtr &graph) const override;

  bool LaunchKernel(const CNodePtr &kernel, const std::vector<KernelTensor *> &inputs,
                    const std::vector<KernelTensor *> &workspace, const std::vector<KernelTensor *> &outputs,
                    KernelMod *kernel_mod, void * /*stream*/) const override;

  bool ExecuteKernelTask(const runtime::KernelTaskType &task_type, const device::DeviceAddressPtrList &input_addr_list,
                         const device::DeviceAddressPtrList &output_addr_list, const size_t &stream_id) const override;

 private:
  // Select the matching backend kernels according to the data type and format of input and output for all
  // execution operators, and set final device data type and format information for backend kernels, device
  // data type and format which replace original data type and format will use for executing kernels.
  void SetOperatorInfo(const KernelGraphPtr &graph) const;
  void SingleOpGraphOptimize(const KernelGraphPtr &graph) const;
  void OptimizeGraphImpl(const KernelGraphPtr &graph) const;
  void OptimizeMindIR(const KernelGraphPtr &graph) const;
#ifndef ENABLE_SECURITY
  // Launch a kernel and record the elapsed time end to end.
  bool LaunchKernelWithProfiling(const CNodePtr &kernel, const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &workspace,
                                 const std::vector<KernelTensor *> &outputs, KernelMod *kernel_mod) const;
#endif
  // Launch a kernel by 'KernelMod' of the kernel.
  bool DoLaunchKernel(const CNodePtr &kernel, const std::vector<KernelTensor *> &inputs,
                      const std::vector<KernelTensor *> &workspace, const std::vector<KernelTensor *> &outputs,
                      KernelMod *kernel_mod) const;
  void UpdateKernelRefInfo(const KernelGraphPtr &graph) const;

  mutable std::mutex launch_mutex_;
};

class CPUDeviceContext : public DeviceInterface<CPUKernelExecutor, CPUDeviceResManager> {
 public:
  explicit CPUDeviceContext(const DeviceContextKey &device_context_key) : DeviceInterface(device_context_key) {}
  ~CPUDeviceContext() override = default;

  void Initialize() override;

  void Destroy() override;

  RunMode GetRunMode(const FuncGraphPtr &func_graph) const override { return RunMode::kKernelMode; }

 private:
  DISABLE_COPY_AND_ASSIGN(CPUDeviceContext);
};
}  // namespace cpu
}  // namespace device
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_RUNTIME_HARDWARE_CPU_CPU_DEVICE_CONTEXT_H_
