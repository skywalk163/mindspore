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

#include "plugin/device/cpu/kernel/fast_gelu_grad_cpu_kernel.h"
#include <algorithm>
#include <functional>
#include "mindspore/core/ops/nn_optimizer_ops.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore::kernel {
namespace {
constexpr auto kFastGeLUGrad = "FastGeLUGrad";
constexpr const size_t kFastGeluGradInputsNum = 2;
constexpr const size_t kFastGeluGradOutputsNum = 1;
using KernelRunFunc = FastGeLUGradCpuKernelMod::KernelRunFunc;
}  // namespace
template <typename T>
bool FastGeLUGradCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &,
                                            const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kFastGeluGradInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kFastGeluGradOutputsNum, kernel_name_);
  T *input1 = reinterpret_cast<T *>(inputs[kIndex0]->device_ptr());
  MS_ERROR_IF_NULL_W_RET_VAL(input1, false);
  T *input2 = reinterpret_cast<T *>(inputs[kIndex1]->device_ptr());
  MS_ERROR_IF_NULL_W_RET_VAL(input2, false);
  T *output = reinterpret_cast<T *>(outputs[kIndex0]->device_ptr());
  MS_ERROR_IF_NULL_W_RET_VAL(output, false);

  const size_t lens = outputs[0]->size() > 0 ? static_cast<size_t>(outputs[0]->size() / sizeof(T)) : 1;
  auto task = [&input1, &input2, &output](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      T x = input2[i];
      double double_x = static_cast<double>(x);
      T res_e = static_cast<T>(std::exp(-1.702 * double_x));
      T div_up = res_e + static_cast<T>(1.702) * x * res_e + static_cast<T>(1);
      T div_down = (res_e + static_cast<T>(1)) * (res_e + static_cast<T>(1));
      T y_res = div_up / div_down;
      output[i] = input1[i] * y_res;
    }
  };
  ParallelLaunchAutoSearch(task, lens, this, &parallel_search_info_);
  return true;
}

const std::vector<std::pair<KernelAttr, KernelRunFunc>> &FastGeLUGradCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, KernelRunFunc>> func_list = {
    {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
     &FastGeLUGradCpuKernelMod::LaunchKernel<float16>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
     &FastGeLUGradCpuKernelMod::LaunchKernel<float>},
  };
  return func_list;
}

bool FastGeLUGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kFastGeluGradInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kFastGeluGradOutputsNum, kernel_name_);
  if (!MatchKernelFunc(kernel_name_, inputs, outputs)) {
    return false;
  }

  return true;
}

int FastGeLUGradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  int ret = KRET_OK;
  if ((ret = KernelMod::Resize(inputs, outputs)) != 0) {
    return ret;
  }
  std::vector<int64_t> input_shape = inputs[kIndex0]->GetShapeVector();
  std::vector<int64_t> input_shape_2 = inputs[kIndex1]->GetShapeVector();
  std::vector<int64_t> output_shape = outputs[kIndex0]->GetShapeVector();
  auto in_shape_size_1 = input_shape.size();
  if (in_shape_size_1 > max_dims_) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the dimension of input should be less than or equal to max_dims 7, but got " << in_shape_size_1
                  << ".";
    return KRET_RESIZE_FAILED;
  }
  auto in_shape_size_2 = input_shape_2.size();
  auto output_shape_size = output_shape.size();
  if (in_shape_size_1 != output_shape_size || in_shape_size_1 != in_shape_size_2) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', input one shape size should be the same as input two shape size and"
                  << " output shape size, but got input one shape size " << in_shape_size_1 << " input two shape size "
                  << in_shape_size_2 << " output shape size" << output_shape_size;
    return KRET_RESIZE_FAILED;
  }
  return KRET_OK;
}

MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, FastGeLUGrad,
                                 []() { return std::make_shared<FastGeLUGradCpuKernelMod>(kFastGeLUGrad); });
}  // namespace mindspore::kernel
