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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_HCCL_HCOM_MATMUL_ALL_REDUCE_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_HCCL_HCOM_MATMUL_ALL_REDUCE_H_

#include <memory>
#include <vector>
#include "plugin/device/ascend/kernel/hccl/hccl_kernel.h"

namespace mindspore {
namespace kernel {
constexpr uint32_t kMatMulAllReduceInputNum = 2;
constexpr uint32_t kMatMulAllReduceOutputNum = 1;
constexpr char kAttrNameTransposeA[] = "transpose_a";
constexpr char kAttrNameTransposeB[] = "transpose_b";

class HcomMatMulAllReduceKernel : public HcclKernel {
 public:
  HcomMatMulAllReduceKernel() = default;
  ~HcomMatMulAllReduceKernel() override = default;

  /* Inherit from kernelmod */
  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override;

 private:
  bool transpose_a_{false};
  bool transpose_b_{false};

  std::shared_ptr<Lcal::Lcoc> lcoc_;
  Lcal::CoCDataTypeDesc lcoc_dtype_{Lcal::CoCDataTypeDesc::FP16FP16_FP32_FP16};
  Lcal::LcalType lcoc_type_{Lcal::LcalType::MATMUL_ALL_REDUCE};
  Lcal::QuantInfo quant_info_{};
  Lcal::CoCTiling tiling_{};
  Lcal::MatMulInfo matmul_info_{};
  Lcal::CoCParamDesc param_desc_{};
};

MS_HCCL_REG_KERNEL(MatMulAllReduce, HcomMatMulAllReduceKernel);
}  // namespace kernel
}  // namespace mindspore
#endif
