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

#include "plugin/device/ascend/kernel/hccl/hcom_barrier.h"
#include "plugin/device/ascend/hal/hccl_adapter/hccl_adapter.h"

namespace mindspore {
namespace kernel {
bool HcomBarrierKernel::Launch(const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &,
                               const std::vector<KernelTensor *> &, void *stream_ptr) {
  MS_EXCEPTION_IF_NULL(stream_ptr);
  auto hccl_result = hccl::HcclAdapter::GetInstance().HcclBarrier(stream_ptr, comm_);
  if (hccl_result != HCCL_SUCCESS) {
    MS_LOG(ERROR) << "HcclBarrier failed, ret:" << hccl_result;
    return false;
  }
  return true;
}

const std::vector<size_t> &HcomBarrierKernel::GetOutputSizeList() const {
  // Operators must have output, so give Barrier a dummy output.
  static const std::vector<size_t> dummy_output_size_list{kDim1 * sizeof(float)};
  return dummy_output_size_list;
}
}  // namespace kernel
}  // namespace mindspore
