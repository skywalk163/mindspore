/**
 * Copyright 2019-2021 Huawei Technologies Co., Ltd
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
#include "extendrt/delegate/tensorrt/tensorrt_plugin_impl.h"
#ifdef LITE_CUDA_DISTRIBUTION
#include "extendrt/delegate/tensorrt/distribution/distribution_base.h"
#include "plugin/device/gpu/hal/device/distribution/collective_wrapper.h"
#endif

namespace mindspore::lite {
int TensorRTPluginImpl::GetGPUGroupSize() const {
#ifdef LITE_CUDA_DISTRIBUTION
  return GetGroupSize(NCCL_WORLD_GROUP);
#else
  return 1;
#endif
}

int TensorRTPluginImpl::GetRankID() const {
#ifdef LITE_CUDA_DISTRIBUTION
  return GetRankIDByGroup(NCCL_WORLD_GROUP);
#else
  return 0;
#endif
}
}  // namespace mindspore::lite

mindspore::lite::TensorRTExecutorPluginImplBase *CreateTensorRTPluginImpl() {
  return new mindspore::lite::TensorRTPluginImpl();
}
