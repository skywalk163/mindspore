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

#include "plugin/device/cpu/kernel/cdist_cpu_kernel.h"
#include <utility>
#include <algorithm>
#include "plugin/device/cpu/kernel/nnacl/op_base.h"
#include "plugin/device/cpu/kernel/nnacl/fp32/cdist_fp32.h"
namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kCdistInputDimsMin = 2;

const std::vector<KernelAttr> kernel_attr = {
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32)}};
}  // namespace

void CdistCpuKernelMod::InitFunc(float p) {
  if (p == 0.0) {
    dist_func_ = CdistZeroNormalOpt;
  } else if (p == 1.0) {
    dist_func_ = CdistOneNormalOpt;
  } else if (p == 2.0) {
    dist_func_ = CdistTwoNormalOpt;
  } else if (std::isinf(p)) {
    dist_func_ = CdistInfNormalOpt;
  } else {
    dist_func_ = CdistPNormalOpt;
  }
}

bool CdistCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  p_ = GetValue<float>(primitive_->GetAttr(ops::kP));
  auto input_type_id = inputs[0]->dtype_id();
  switch (input_type_id) {
    case kNumberTypeFloat32:
      InitFunc(p_);
      break;
    default:
      MS_LOG(ERROR) << "cdist kernel does not support " << TypeIdToString(input_type_id);
      return false;
  }
  return true;
}

int CdistCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  int ret = 0;
  if ((ret = KernelMod::Resize(inputs, outputs)) != 0) {
    return ret;
  }
  std::vector<int64_t> in_shape0 = inputs[0]->GetShapeVector();
  std::vector<int64_t> in_shape1 = inputs[1]->GetShapeVector();
  auto in_shape_size = in_shape0.size();
  if (in_shape1.size() != in_shape_size || in_shape_size < kCdistInputDimsMin) {
    MS_LOG(ERROR) << "invalid input shape, input0 shape size " << in_shape_size << ", input1 shape size "
                  << in_shape1.size() << ", kernel_name_ " << kernel_name_;
    return KRET_RESIZE_FAILED;
  }
  batch_ = 1;
  for (size_t i = 0; i < in_shape_size - kCdistInputDimsMin; i++) {
    batch_ *= in_shape0[i];
  }

  r0_ = in_shape0[in_shape_size - 2];
  m_ = in_shape0[in_shape_size - 1];
  r1_ = in_shape1[in_shape_size - 2];

  thread_num_ = std::min(static_cast<size_t>(batch_), pool_->GetKernelThreadNum());

  return 0;
}

bool CdistCpuKernelMod::LaunchKernel(int64_t start, int64_t end) {
  const auto *in_data0 = reinterpret_cast<float *>(in_data0_) + start * r0_ * m_;
  const auto *in_data1 = reinterpret_cast<float *>(in_data1_) + start * r1_ * m_;
  auto *out_data = reinterpret_cast<float *>(out_data_) + start * r0_ * r1_;

  for (int64_t b_i = 0; b_i < end - start; b_i++) {
    for (int64_t p_i = 0; p_i < r0_; p_i++) {
      auto in_data_tmp1 = in_data1;
      for (int64_t r_i = 0; r_i < r1_; r_i++) {
        dist_func_(in_data0, in_data_tmp1, &(out_data[r_i]), m_, p_);
        in_data_tmp1 = in_data_tmp1 + m_;
      }
      in_data0 = in_data0 + m_;
      out_data = out_data + r1_;
    }
    in_data1 = in_data1 + r1_ * m_;
  }

  return true;
}

std::vector<KernelAttr> CdistCpuKernelMod::GetOpSupport() { return kernel_attr; }

bool CdistCpuKernelMod::DoLaunch(int task_id) {
  auto batch_per_thread = UP_DIV(batch_, thread_num_);
  int64_t start = batch_per_thread * task_id;
  int64_t end = start + batch_per_thread;
  end = std::min(end, batch_);
  return LaunchKernel(start, end);
}

int CdistRun(void *cdata, int task_id, float lhs_scale, float rhs_scale) {
  auto cdist_kernel = reinterpret_cast<CdistCpuKernelMod *>(cdata);
  if (!cdist_kernel->DoLaunch(task_id)) {
    MS_LOG(ERROR) << "cdist_kernel DoLaunch failed, task_id:" << task_id;
    return -1;
  }
  return 0;
}

bool CdistCpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                               const std::vector<KernelTensor *> &outputs) {
  in_data0_ = inputs[0]->device_ptr();
  in_data1_ = inputs[1]->device_ptr();
  out_data_ = outputs[0]->device_ptr();
  int ret = pool_->ParallelLaunch(CdistRun, this, thread_num_);
  if (ret != 0) {
    MS_LOG(ERROR) << "CdistCpuKernelMod ParallelLaunch failed, error_code[" << ret << "]";
    return false;
  }
  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Cdist, CdistCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
