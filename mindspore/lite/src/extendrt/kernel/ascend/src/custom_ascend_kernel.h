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

#ifndef MINDSPORE_LITE_SRC_EXTENDRT_KERNEL_ASCEND_CUSTOM_ASCEND_KERNEL_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_KERNEL_ASCEND_CUSTOM_ASCEND_KERNEL_H_

#include <vector>
#include <string>
#include <memory>
#include <map>
#include "extendrt/kernel/ascend/options/acl_model_options.h"
#include "extendrt/kernel/ascend/model/model_infer.h"
#include "include/api/types.h"
#include "include/api/context.h"
#include "kernel/kernel.h"
#include "kernel/common_utils.h"
#include "include/errorcode.h"

namespace mindspore::kernel {
namespace acl {
class CustomAscendKernelMod : public kernel::KernelMod {
 public:
  CustomAscendKernelMod();
  ~CustomAscendKernelMod() override;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Finalize() override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override;

  std::vector<KernelAttr> GetOpSupport() override { return {}; }

 private:
  void RecordInputDataIndex(const std::vector<KernelTensor *> &inputs);
  AclModelOptionsPtr GenAclOptions();
  void UpdateInputKernelTensorInfo();
  void UpdateOutputKernelTensorInfo();
  bool OnNewInputShapes(const std::vector<KernelTensor *> &new_shapes);

  bool load_model_;
  AclModelOptionsPtr acl_options_;
  ModelInferPtr model_infer_;
  size_t input_data_idx_;
  bool is_multi_model_sharing_mem_prepare_ = false;
  std::vector<KernelTensor *> inputs_;
  std::vector<KernelTensor *> outputs_;
};
}  // namespace acl
}  // namespace mindspore::kernel

#endif  // MINDSPORE_LITE_SRC_EXTENDRT_KERNEL_ASCEND_CUSTOM_ASCEND_KERNEL_H_
