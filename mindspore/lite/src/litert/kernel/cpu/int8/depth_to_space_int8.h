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
#ifndef MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_INT8_DEPTH_TO_SPACE_INT8_H_
#define MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_INT8_DEPTH_TO_SPACE_INT8_H_

#include <vector>
#include "include/errorcode.h"
#include "src/litert/lite_kernel.h"
#include "nnacl/base/depth_to_space_base.h"
#include "nnacl/int8/depth_to_space_int8.h"
#include "nnacl/int8/quantize.h"
#include "nnacl/kernel/depth_to_space.h"

namespace mindspore::kernel {
class DepthToSpaceInt8CPUKernel : public LiteKernel {
 public:
  DepthToSpaceInt8CPUKernel(OpParameter *parameter, const std::vector<lite::Tensor *> &inputs,
                            const std::vector<lite::Tensor *> &outputs, const lite::InnerContext *ctx)
      : LiteKernel(parameter, inputs, outputs, ctx) {}
  ~DepthToSpaceInt8CPUKernel() override;

  int Prepare() override;
  int ReSize() override;
  int Run() override;

 private:
  QuantArg *in_quant_arg_ = nullptr;
  QuantArg *out_quant_arg_ = nullptr;
  DepthToSpaceArgs args_;
};
}  // namespace mindspore::kernel

#endif  // MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_INT8_DEPTH_TO_SPACE_INT8_H_
