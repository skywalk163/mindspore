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

#include "nnacl/nnacl_manager.h"

namespace mindspore::nnacl {
bool NNACLSupportKernel(int op_type, TypeId data_type) {
  auto creator = KernelRegistry::GetInstance()->Creator({op_type, data_type});
  if (creator != nullptr) {
    return true;
  }
  return SupportKernelC(op_type, data_type);
}

NNACLKernel *NNACLKernelRegistry(OpParameter *parameter, const std::vector<lite::Tensor *> &inputs,
                                 const std::vector<lite::Tensor *> &outputs, const lite::InnerContext *ctx,
                                 const kernel::KernelKey &key) {
  auto creator = KernelRegistry::GetInstance()->Creator({key.type, key.data_type});
  NNACLKernel *kernel = nullptr;
  if (creator != nullptr) {
    kernel = creator(parameter, inputs, outputs, ctx);
  }
  if (kernel == nullptr) {
    kernel = new (std::nothrow) NNACLKernel(parameter, inputs, outputs, ctx);
  }
  if (kernel == nullptr) {
    MS_LOG(ERROR) << "Create nnacl kernel failed:  " << parameter->name_;
    return nullptr;
  }

  auto ret = kernel->InitKernel(key.data_type, ctx);
  if (ret != RET_OK) {
    MS_LOG(WARNING) << "Init nnacl kernel failed:  " << parameter->name_;
    kernel->set_parameter(nullptr);  // Do not Free parameter here, Free where it was malloced.
    delete kernel;
    return nullptr;
  }

  return kernel;
}
}  // namespace mindspore::nnacl
