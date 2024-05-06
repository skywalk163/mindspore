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
#ifndef MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_INT8_SUB_INT8_H_
#define MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_INT8_SUB_INT8_H_

#include <vector>
#include <limits>
#include <algorithm>
#include "nnacl/int8/arithmetic_int8.h"
#include "nnacl/int8/quantize.h"
#include "src/litert/lite_kernel.h"
#include "nnacl/int8/sub_int8.h"

namespace mindspore::kernel {
class SubInt8CPUKernel : public LiteKernel {
 public:
  explicit SubInt8CPUKernel(OpParameter *parameter, const std::vector<lite::Tensor *> &inputs,
                            const std::vector<lite::Tensor *> &outputs, const lite::InnerContext *ctx)
      : LiteKernel(parameter, inputs, outputs, ctx) {}
  ~SubInt8CPUKernel() override;

  int Prepare() override;
  int ReSize() override;
  int Run() override;
  int DoExecute(int task_id);

 private:
  SubQuantArg *quant_param_ = nullptr;
  int8_t *tile0_data_ = nullptr;
  int8_t *tile1_data_ = nullptr;
  bool broadcast_ = false;
};
}  // namespace mindspore::kernel

#endif  // MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_INT8_SUB_INT8_H_
