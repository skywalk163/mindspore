/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_POOLING_GRAD_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_POOLING_GRAD_CPU_KERNEL_H_

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <unordered_map>
#include <map>

#include "plugin/device/cpu/kernel/mkldnn/mkl_cpu_kernel.h"

namespace mindspore {
namespace kernel {
constexpr auto kAvgPoolGrad = "AvgPoolGrad";
constexpr auto kAvgPool3DGrad = "AvgPool3DGrad";
constexpr auto kMaxPoolGrad = "MaxPoolGrad";
constexpr auto kMaxPool3DGrad = "MaxPool3DGrad";
constexpr auto kUnknown = "Unknown";
constexpr size_t kPoolingDilation = 1;

class PoolingGradCpuKernelMod : public MKLCpuKernelMod {
 public:
  PoolingGradCpuKernelMod() = default;
  explicit PoolingGradCpuKernelMod(const std::string &kernel_type) : kernel_type_(kernel_type) {}
  ~PoolingGradCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override {
    static std::unordered_map<std::string, std::vector<KernelAttr>> support_list = {
      {kAvgPoolGrad,
       {{KernelAttr()
           .AddInputAttr(kNumberTypeFloat32)                   // x
           .AddInputAttr(kNumberTypeFloat32)                   // out
           .AddInputAttr(kNumberTypeFloat32)                   // dout
           .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)   // kernel_size
           .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)   // strides
           .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)  // pad_mode
           .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)  // data_format
           .AddOutputAttr(kNumberTypeFloat32)}}},
      {kAvgPool3DGrad,
       {{KernelAttr()
           .AddInputAttr(kObjectTypeTuple, kNumberTypeInt32)
           .AddInputAttr(kNumberTypeFloat32)
           .AddOutputAttr(kNumberTypeFloat32)},
        {KernelAttr()
           .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
           .AddInputAttr(kNumberTypeFloat32)
           .AddOutputAttr(kNumberTypeFloat32)}}},
      // the registration of maxpoolgrad and maxpool3dgrad hasn't been modified.
      {kMaxPoolGrad,
       {{KernelAttr()
           .AddInputAttr(kNumberTypeFloat32)
           .AddInputAttr(kNumberTypeFloat32)
           .AddInputAttr(kNumberTypeFloat32)
           .AddOutputAttr(kNumberTypeFloat32)}}},
      {kMaxPool3DGrad,
       {{KernelAttr()
           .AddInputAttr(kNumberTypeFloat32)
           .AddInputAttr(kNumberTypeFloat32)
           .AddInputAttr(kNumberTypeFloat32)
           .AddOutputAttr(kNumberTypeFloat32)}}}};

    auto iter = support_list.find(kernel_type_);
    if (iter == support_list.end()) {
      MS_LOG(EXCEPTION) << "PoolingGrad does not support kernel type: " << kernel_type_;
    }
    return iter->second;
  }

 private:
  template <typename T>
  void EliminateInvalidPadding(T *output);
  template <typename T>
  void ReComputeDivisor(T *output);

  dnnl::algorithm algorithm_{dnnl::algorithm::pooling_max};
  bool ceil_mode_{false};
  int64_t divisor_override_{0};
  std::vector<int64_t> dst_shape_;
  std::vector<int64_t> kernel_;
  std::vector<int64_t> padding_invalid_;
  mindspore::Format format_;
  mindspore::PadMode pad_mode_;
  std::vector<int64_t> kernel_include_nc_{};
  std::vector<int64_t> strides_include_nc_{};

  void ComputeMaxValueIndex(void *src, void *dst, void *work_array);
#ifdef USE_MS_THREADPOOL_FOR_DNNL
  void ExecuteForwardByMSThreadPool(const std::unordered_map<int, dnnl::memory> &arguments);
#endif

  size_t grad_index_{0};
  dnnl::memory::desc src_desc_{};
  dnnl::memory::desc dst_desc_{};
  dnnl::memory::desc workspace_desc_{};
  std::shared_ptr<dnnl::pooling_forward> primitive_forward_{nullptr};
  ParallelSearchInfo forward_parallel_info_{};
  std::string kernel_type_{kUnknown};

  template <typename T>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &workspace,
                    const std::vector<kernel::KernelTensor *> &outputs);

  TypeId dtype_{kTypeUnknown};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_POOLING_GRAD_CPU_KERNEL_H_
