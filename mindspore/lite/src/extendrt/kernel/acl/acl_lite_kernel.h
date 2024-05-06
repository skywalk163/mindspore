/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_LITE_SRC_EXTENDRT_KERNEL_ACL_ACL_LITE_KERNEL_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_KERNEL_ACL_ACL_LITE_KERNEL_H_

#include <memory>
#include <utility>
#include <vector>
#include <string>
#include "src/extendrt/kernel/base_kernel.h"
#include "kernel/kernel.h"
#include "ops/base_operator.h"

namespace mindspore::kernel {
class AclLiteKernel : public BaseKernel {
 public:
  explicit AclLiteKernel(std::shared_ptr<mindspore::kernel::KernelMod> kernel_mod, BaseOperatorPtr base_operator,
                         std::vector<lite::Tensor *> in_tensors, std::vector<lite::Tensor *> out_tensors,
                         const lite::InnerContext *ctx);
  ~AclLiteKernel() override = default;

  int Prepare() override;
  int ReSize() override;
  int Run() override;
  int InferShape();

 public:
  std::string KernelType() const { return base_operator_->name(); }

 private:
  KernelModPtr kernel_mod_;
  BaseOperatorPtr base_operator_;
  std::vector<KernelTensor *> inputs_;
  std::vector<KernelTensor *> outputs_;
};
}  // namespace mindspore::kernel
#endif  // MINDSPORE_LITE_SRC_EXTENDRT_KERNEL_ACL_ACL_LITE_KERNEL_H_
