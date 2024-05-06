/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "cpu_kernel/utils/sampling_kernels.h"
#include <algorithm>
#include <string>
#include "inc/kernel_log.h"
#include "context/common/status.h"

namespace aicpu {
SamplingKernelType SamplingKernelTypeFromString(const std::string &str) {
  if (str == "lanczos1") return Lanczos1Kernel;
  if (str == "lanczos3") return Lanczos3Kernel;
  if (str == "lanczos5") return Lanczos5Kernel;
  if (str == "gaussian") return GaussianKernel;
  if (str == "box") return BoxKernel;
  if (str == "triangle") return TriangleKernel;
  if (str == "keyscubic") return KeysCubicKernel;
  if (str == "mitchellcubic") return MitchellCubicKernel;
  return SamplingKernelTypeEnd;
}
}  // namespace aicpu
