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
#include "src/extendrt/subgraph_kernel.h"
namespace mindspore::kernel {
bool SubgraphKernel::Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                            const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  std::vector<tensor::Tensor> in;
  std::vector<tensor::Tensor> out;
  std::map<string, string> compile_options;
  executor_->RunGraph(subgraph_, in, &out, compile_options);
  return true;
}
bool SubgraphKernel::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  std::map<string, string> compile_options;
  return executor_->CompileGraph(subgraph_, compile_options);
}

int SubgraphKernel::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  return 0;
}
}  // namespace mindspore::kernel
