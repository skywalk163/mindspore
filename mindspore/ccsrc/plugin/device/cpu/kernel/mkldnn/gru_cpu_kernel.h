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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MKLDNN_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MKLDNN_H_

#include <vector>
#include <memory>
#include <map>
#include "plugin/device/cpu/kernel/mkldnn/mkl_cpu_kernel.h"

namespace mindspore {
namespace kernel {
class GRUCpuKernelMod : public MKLCpuKernelMod {
 public:
  GRUCpuKernelMod() = default;
  ~GRUCpuKernelMod() override = default;
  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override {
    static std::vector<KernelAttr> support_list = {
      {KernelAttr()
         .AddInputAttr(kNumberTypeFloat32)
         .AddInputAttr(kNumberTypeFloat32)
         .AddInputAttr(kNumberTypeFloat32)
         .AddInputAttr(kNumberTypeInt32)
         .AddOutputAttr(kNumberTypeFloat32)
         .AddOutputAttr(kNumberTypeFloat32)
         .AddOutputAttr(kNumberTypeFloat32)
         .AddOutputAttr(kNumberTypeFloat32)},
      {KernelAttr()
         .AddInputAttr(kNumberTypeFloat16)
         .AddInputAttr(kNumberTypeFloat16)
         .AddInputAttr(kNumberTypeFloat16)
         .AddInputAttr(kNumberTypeInt32)
         .AddOutputAttr(kNumberTypeFloat16)
         .AddOutputAttr(kNumberTypeFloat16)
         .AddOutputAttr(kNumberTypeFloat16)
         .AddOutputAttr(kNumberTypeFloat16)},
    };
    return support_list;
  }

 private:
  void InitOutputSize(const std::vector<KernelTensor *> &outputs);
  int weight_size_{0};
  int weight_h_size_{0};
  int input_size_{0};
  int hidden_size_{0};
  int num_layers_{0};
  int batch_size_{0};
  int seq_len_{0};
  int num_directions_{0};
  bool bidirectional_{false};
  bool has_bias_{false};
  bool is_training_{false};
  size_t reserve_size_{1};

  dnnl::memory::dims weights_dims_;
  dnnl::memory::dims weights_h_dims_;
  dnnl::memory::dims bias_dims_;
  dnnl::gru_forward::primitive_desc prim_desc_;
  dnnl::memory::desc bias_desc_;
  dnnl::memory user_weights_memory_;
  dnnl::memory user_weights_h_memory_;
  dnnl::memory weights_memory_;
  dnnl::memory weights_h_memory_;
  dnnl::memory bias_memory_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MKLDNN_H_
