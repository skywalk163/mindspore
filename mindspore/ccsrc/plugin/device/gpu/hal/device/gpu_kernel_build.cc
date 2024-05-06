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
#include "plugin/device/gpu/hal/device/gpu_kernel_build.h"
#include <map>
#include <memory>
#include <string>
#include "kernel/kernel.h"
#include "mindspore/core/ops/framework_ops.h"
#include "mindspore/core/ops/sequence_ops.h"
#ifdef ENABLE_AKG
#include "plugin/device/gpu/kernel/akg/akg_gpu_kernel_build.h"
#endif
#include "backend/common/session/kernel_build_client.h"
#include "frontend/operator/ops.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "kernel/framework_utils.h"
#include "kernel/graph_kernel/kernel_packet/kernel_packet_kernel_mod.h"
#include "plugin/device/gpu/hal/device/cuda_env_checker.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
namespace mindspore {
namespace device {
namespace gpu {
namespace {
void SetGpuRefMapToKernelInfo(const CNodePtr &apply_kernel, const std::vector<kernel::KernelAttr> &kernel_attrs) {
  MS_EXCEPTION_IF_NULL(apply_kernel);
  if (kernel_attrs.empty()) {
    return;
  }

  auto kernel_attr = kernel::GetKernelAttrFromNode(apply_kernel);
  auto [is_match, index] = kernel::MatchKernelAttr(kernel_attr, kernel_attrs);
  if (kernel_attrs[0].GetSkipCheck()) {
    is_match = true;
    index = 0;
  }
  if (!is_match) {
    MS_LOG(EXCEPTION) << common::AnfAlgo::GetCNodeName(apply_kernel)
                      << " does not support this kernel data type: " << kernel_attr;
  }

  auto kernel_info = dynamic_cast<device::KernelInfo *>(apply_kernel->kernel_info());
  MS_EXCEPTION_IF_NULL(kernel_info);
  const auto &matched_kernel_attr = kernel_attrs[index];
  if (!matched_kernel_attr.GetOutInRefMap().empty() || matched_kernel_attr.GetAllOutInRef()) {
    kernel_info->set_ref_map(matched_kernel_attr.GetAllOutInRef(), matched_kernel_attr.GetOutInRefMap());
  }
}

// Create KernelPacketKernelMod and return the real inner kernel
void CreateKernelPacketKernelMods(const std::vector<CNodePtr> &kernels) {
  for (auto &kernel : kernels) {
    MS_LOG(DEBUG) << "kernel name: " << kernel->DebugString();
    auto real_node = kernel::GetKernelPacketRealNode(kernel);
    auto kernel_mod = std::make_shared<kernel::KernelPacketKernelMod>(
      [](void *dst, const void *src, size_t count, void *stream_ptr) -> bool {
        cudaError_t status =
          cudaMemcpyAsync(dst, src, count, cudaMemcpyHostToDevice, reinterpret_cast<cudaStream_t>(stream_ptr));
        if (status != cudaSuccess) {
          MS_LOG(ERROR) << "#umsg#CUDA Error:#umsg#cudaMemcpyAsync for KernelPacketdNode failed | Error Number: "
                        << status << " " << cudaGetErrorString(status);
          return false;
        }
        return true;
      });
    std::vector<KernelTensor *> input_kernel_tensors = AnfAlgo::GetOrCreateAllInputKernelTensors(kernel);
    std::vector<KernelTensor *> output_kernel_tensors = AnfAlgo::GetOrCreateAllOutputKernelTensors(kernel);

    auto ms_context = MsContext::GetInstance();
    MS_EXCEPTION_IF_NULL(ms_context);
    auto device_id = ms_context->get_param<uint32_t>(MS_CTX_DEVICE_ID);
    kernel_mod->SetDevicedId(device_id);
    if (!kernel_mod->KernelMod::Init(common::AnfAlgo::GetCNodePrimitive(kernel), input_kernel_tensors,
                                     output_kernel_tensors) ||
        !kernel::kernelpacket::Init(kernel_mod.get(), real_node)) {
      MS_LOG(EXCEPTION) << "#dmsg#Kernel build failed:#dmsg#Initialize gpu kernel op[" << kernel->fullname_with_scope()
                        << "] failed.";
    }
    session::AnfRuntimeAlgorithm::SetKernelMod(kernel_mod, kernel.get());
  }
}
}  // namespace

void CreateGPUKernel(const std::vector<CNodePtr> &kernels) {
  kernel::KernelMeta *bin_map = kernel::KernelMeta::GetInstance();
  MS_EXCEPTION_IF_NULL(bin_map);
  bool already_check_nvcc = false;
  std::vector<AnfNodePtr> akg_nodes;
  std::vector<CNodePtr> kernel_packet_nodes;
  for (auto kernel : kernels) {
    MS_EXCEPTION_IF_NULL(kernel);
    if (IsPrimitiveCNode(kernel, prim::kPrimKernelPacket)) {
      kernel_packet_nodes.push_back(kernel);
      kernel = kernel::GetKernelPacketRealNode(kernel);
    }
    // Need backoff to create CPU kernel.
    if (AnfAlgo::IsKernelSelectBackoffOp(kernel)) {
      continue;
    }
    const mindspore::HashSet<PrimitivePtr, PrimitiveHasher, PrimitiveEqual> virtual_prims = {
      prim::kPrimTupleGetItem, prim::kPrimMakeTuple, prim::kPrimDepend, prim::kPrimStateSetItem};
    if (IsOneOfPrimitiveCNode(kernel, virtual_prims)) {
      continue;
    }

    if (session::AnfRuntimeAlgorithm::GetKernelType(kernel) == KernelType::AKG_KERNEL) {
      if (!bin_map->initialized()) {
        bin_map->Initialize();
      }
      if (!already_check_nvcc) {
        already_check_nvcc = true;
        if (!CudaEnvChecker::GetInstance().CheckNvccInPath()) {
          MS_LOG(EXCEPTION)
            << "#umsg#Failed to find nvcc compiler:#umsg#Please add nvcc position to the PATH environment variable, "
               "run the command: export PATH=${CUDA_PATH}/bin:${PATH}, CUDA_PATH is the installation path of the "
               "cuda library(eg. /usr/local/cuda).";
        }
      }
      akg_nodes.push_back(kernel);
    } else if (!common::AnfAlgo::IsBpropCutOpExecInBackend(kernel)) {
      std::shared_ptr<kernel::NativeGpuKernelMod> gpu_kernel_mod = nullptr;
      bool new_factory = true;
      const auto &kernel_name = common::AnfAlgo::GetCNodeName(kernel);
      if (kernel::Factory<kernel::NativeGpuKernelMod>::Instance().IsRegistered(kernel_name)) {
        gpu_kernel_mod = kernel::Factory<kernel::NativeGpuKernelMod>::Instance().Create(kernel_name);
      } else {
        gpu_kernel_mod =
          (std::shared_ptr<kernel::NativeGpuKernelMod>)(kernel::NativeGpuKernelModFactory::GetInstance().Create(
            kernel_name, kernel));
        new_factory = false;
      }
      if (!gpu_kernel_mod) {
        MS_LOG(INTERNAL_EXCEPTION) << "#dmsg#Kernel build failed:#dmsg#Build gpu kernel op["
                                   << kernel->fullname_with_scope() << "] failed";
      }
      MS_EXCEPTION_IF_NULL(kernel);

      if (new_factory) {
        auto kernel_attrs = gpu_kernel_mod->GetOpSupport();
        SetGpuRefMapToKernelInfo(kernel, kernel_attrs);
      }
      auto ms_context = MsContext::GetInstance();
      MS_EXCEPTION_IF_NULL(ms_context);
      auto device_id = ms_context->get_param<uint32_t>(MS_CTX_DEVICE_ID);
      gpu_kernel_mod->SetDevicedId(device_id);
      std::vector<KernelTensor *> input_kernel_tensors = AnfAlgo::GetOrCreateAllInputKernelTensors(kernel);
      std::vector<KernelTensor *> output_kernel_tensors = AnfAlgo::GetOrCreateAllOutputKernelTensors(kernel);

      MS_LOG(DEBUG) << "Begin Init kernel: " << kernel->fullname_with_scope();
      if (!gpu_kernel_mod->Init(common::AnfAlgo::GetCNodePrimitive(kernel), input_kernel_tensors,
                                output_kernel_tensors)) {
        MS_LOG(EXCEPTION) << "#dmsg#Kernel build failed:#dmsg#Initialize gpu kernel op["
                          << kernel->fullname_with_scope() << "] failed.";
      }
      MS_LOG(DEBUG) << "End Init kernel: " << kernel->fullname_with_scope();
      if (kernel::CheckResizeCondition(kernel)) {
        MS_LOG(DEBUG) << "Begin Resize in compile phase for kernel: " << kernel->fullname_with_scope();
        if (gpu_kernel_mod->Resize(input_kernel_tensors, output_kernel_tensors) == kernel::KRET_RESIZE_FAILED) {
          MS_LOG(EXCEPTION) << "#dmsg#Kernel build failed:#dmsg#Gpu kernel op[" << kernel->fullname_with_scope()
                            << "] Resize failed.";
        }
        MS_LOG(DEBUG) << "End Resize in compile phase for kernel: " << kernel->fullname_with_scope();
      }
      session::AnfRuntimeAlgorithm::SetKernelMod(gpu_kernel_mod, kernel.get());
    }
  }
#ifdef ENABLE_AKG
  kernel::AkgGpuKernelBuilder akg_gpu_kernel_builder;
  (void)akg_gpu_kernel_builder.SingleOpParallelBuild(akg_nodes);
#endif
  CreateKernelPacketKernelMods(kernel_packet_nodes);
}
}  // namespace gpu
}  // namespace device
}  // namespace mindspore
