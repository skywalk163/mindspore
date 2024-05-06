/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/batch_norm_grad_grad_cpu_kernel.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <vector>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr float num_1 = 1.0;
constexpr float num_3 = 3.0;
}  // namespace

bool BatchNormGradGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                         const std::vector<KernelTensor *> &outputs) {
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', does not support this kernel data type: " << kernel_attr;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int BatchNormGradGradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                          const std::vector<KernelTensor *> &outputs) {
  int ret = KRET_OK;
  if ((ret = NativeCpuKernelMod::Resize(inputs, outputs)) != 0) {
    return ret;
  }

  std::vector<int64_t> x_shape = inputs.at(kIndex0)->GetShapeVector();
  (void)std::transform(x_shape.begin(), x_shape.end(), std::back_inserter(x_shape_), LongToSize);
  std::vector<int64_t> scale_shape = inputs.at(kIndex2)->GetShapeVector();
  (void)std::transform(scale_shape.begin(), scale_shape.end(), std::back_inserter(scale_shape_), LongToSize);
  x_num_ = static_cast<int>(std::accumulate(x_shape_.begin(), x_shape_.end(), 1, std::multiplies<size_t>()));
  N_num_ = static_cast<int>(x_shape_[0]);
  C_num_ = static_cast<int>(std::accumulate(scale_shape_.begin(), scale_shape_.end(), 1, std::multiplies<size_t>()));
  CHW_num_ = static_cast<int>(x_num_ / N_num_);
  NHW_num_ = static_cast<int>(x_num_ / C_num_);
  HW_num_ = static_cast<int>(NHW_num_ / N_num_);
  M_ = static_cast<float>(NHW_num_);

  is_training_ = inputs[kIndex8]->GetValueWithCheck<bool>();
  epsilon_ = inputs[kIndex9]->GetValueWithCheck<float>();
  data_format_ = static_cast<mindspore::Format>(inputs[kIndex10]->GetValueWithCheck<int64_t>());

  size_t float_type_size = sizeof(float);
  workspace_size_list_.clear();
  if (is_training_) {
    // Training model: total workspace number = 12; 6 x_num_ * float_type_size, 6 C_num_ * float_type_size.
    // kIndex0-kIndex5: This workspace size used for new float[x_num_].
    (void)workspace_size_list_.emplace_back(x_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(x_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(x_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(x_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(x_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(x_num_ * float_type_size);
    // kIndex6-kIndex11: This workspace size used for new float[C_num_].
    (void)workspace_size_list_.emplace_back(C_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(C_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(C_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(C_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(C_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(C_num_ * float_type_size);
  } else {
    // Inference model: total workspace number = 5; 4 x_num_ * float_type_size, 1 C_num_ * float_type_size.
    // kIndex0-kIndex3: This workspace size used for new float[x_num_].
    (void)workspace_size_list_.emplace_back(x_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(x_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(x_num_ * float_type_size);
    (void)workspace_size_list_.emplace_back(x_num_ * float_type_size);
    // kIndex4: This workspace size used for new float[C_num_].
    (void)workspace_size_list_.emplace_back(C_num_ * float_type_size);
  }
  return ret;
}

template <typename T>
bool BatchNormGradGradCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                                 const std::vector<kernel::KernelTensor *> &workspace,
                                                 const std::vector<kernel::KernelTensor *> &outputs) {
  auto reserve_space_2 = static_cast<float *>(inputs.at(kIndex4)->device_ptr());  // fp32
  for (int j = 0; j < C_num_; j++) {
    if (*(reserve_space_2 + j) < 0) {
      MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', 'reserve_space_2' must be no less than zero.";
    }
  }
  if (is_training_ && data_format_ == Format::NHWC) {
    TrainingComputeNHWC<T>(inputs, workspace, outputs);
  } else if (!is_training_ && data_format_ == Format::NHWC) {
    InferenceComputeNHWC<T>(inputs, workspace, outputs);
  } else if (is_training_ && data_format_ == Format::NCHW) {
    TrainingComputeNCHW<T>(inputs, workspace, outputs);
  } else if (!is_training_ && data_format_ == Format::NCHW) {
    InferenceComputeNCHW<T>(inputs, workspace, outputs);
  }

  return true;
}

template <typename T>
void BatchNormGradGradCpuKernelMod::TrainingComputeNHWC(const std::vector<kernel::KernelTensor *> &inputs,
                                                        const std::vector<kernel::KernelTensor *> &workspace,
                                                        const std::vector<kernel::KernelTensor *> &outputs) const {
  auto x_ori = static_cast<T *>(inputs.at(kIndex0)->device_ptr());
  auto dy_ori = static_cast<T *>(inputs.at(kIndex1)->device_ptr());
  auto reserve_space_1 = static_cast<float *>(inputs.at(kIndex3)->device_ptr());  // batch_mean  fp32
  auto reserve_space_2 = static_cast<float *>(inputs.at(kIndex4)->device_ptr());  // batch_var  fp32
  auto ddx_ori = static_cast<T *>(inputs.at(kIndex5)->device_ptr());

  // change dtype from 'T' to 'fp32'
  float *x = static_cast<float *>(workspace.at(kIndex0)->device_ptr());
  float *dy = static_cast<float *>(workspace.at(kIndex1)->device_ptr());
  float *ddx = static_cast<float *>(workspace.at(kIndex2)->device_ptr());

  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(x + index) = static_cast<float>(*(x_ori + index));
        *(dy + index) = static_cast<float>(*(dy_ori + index));
        *(ddx + index) = static_cast<float>(*(ddx_ori + index));
      }
    }
  }

  // create intermediate variables
  float *x_hat = static_cast<float *>(workspace.at(kIndex3)->device_ptr());
  float *inv_std = static_cast<float *>(workspace.at(kIndex11)->device_ptr());

  for (int j = 0; j < C_num_; j++) {
    *(inv_std + j) = num_1 / sqrt(*(reserve_space_2 + j) + epsilon_);
  }

  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(x_hat + index) = (*(inv_std + j)) * (*(x + index) - *(reserve_space_1 + j));
      }
    }
  }
  TrainingNHWCCalculateDx<T>(inputs, workspace, outputs, x_hat, inv_std);
  TrainingNHWCCalculateDdy<T>(inputs, workspace, outputs, x_hat, inv_std);
  TrainingNHWCCalculateDscale<T>(inputs, workspace, outputs, x_hat, inv_std);
  return;
}

template <typename T>
void BatchNormGradGradCpuKernelMod::InferenceComputeNHWC(const std::vector<kernel::KernelTensor *> &inputs,
                                                         const std::vector<kernel::KernelTensor *> &workspace,
                                                         const std::vector<kernel::KernelTensor *> &outputs) const {
  auto x_ori = static_cast<T *>(inputs.at(kIndex0)->device_ptr());
  auto dy_ori = static_cast<T *>(inputs.at(kIndex1)->device_ptr());
  auto scale = static_cast<float *>(inputs.at(kIndex2)->device_ptr());            // fp32
  auto reserve_space_1 = static_cast<float *>(inputs.at(kIndex3)->device_ptr());  // batch_mean  fp32
  auto reserve_space_2 = static_cast<float *>(inputs.at(kIndex4)->device_ptr());  // batch_var  fp32
  auto ddx_ori = static_cast<T *>(inputs.at(kIndex5)->device_ptr());
  auto ddscale = static_cast<float *>(inputs.at(kIndex6)->device_ptr());   // fp32
  auto ddoffset = static_cast<float *>(inputs.at(kIndex7)->device_ptr());  // fp32
  auto dx = static_cast<T *>(outputs.at(kIndex0)->device_ptr());
  auto ddy = static_cast<T *>(outputs.at(kIndex1)->device_ptr());
  auto dscale = static_cast<float *>(outputs.at(kIndex2)->device_ptr());  // fp32

  // change dtype from 'T' to 'fp32'
  float *x = static_cast<float *>(workspace.at(kIndex0)->device_ptr());
  float *dy = static_cast<float *>(workspace.at(kIndex1)->device_ptr());
  float *ddx = static_cast<float *>(workspace.at(kIndex2)->device_ptr());

  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(x + index) = static_cast<float>(*(x_ori + index));
        *(dy + index) = static_cast<float>(*(dy_ori + index));
        *(ddx + index) = static_cast<float>(*(ddx_ori + index));
      }
    }
  }

  // create intermediate variables
  float *x_hat = static_cast<float *>(workspace.at(kIndex3)->device_ptr());
  float *inv_std = static_cast<float *>(workspace.at(kIndex4)->device_ptr());

  for (int j = 0; j < C_num_; j++) {
    *(inv_std + j) = num_1 / sqrt(*(reserve_space_2 + j) + epsilon_);
  }

  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(x_hat + index) = (*(inv_std + j)) * (*(x + index) - *(reserve_space_1 + j));
      }
    }
  }

  // initialize dscale
  for (int j = 0; j < C_num_; j++) {
    *(dscale + j) = 0;
  }

  // calculate dx, ddy, dscale
  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(dx + index) = static_cast<T>((*(ddscale + j)) * (*(inv_std + j)) * (*(dy + index)));
        *(ddy + index) = static_cast<T>((*(ddx + index)) * (*(inv_std + j)) * (*(scale + j)) +
                                        (*(ddscale + j)) * (*(x_hat + index)) + *(ddoffset + j));
        *(dscale + j) += (*(ddx + index)) * (*(dy + index)) * (*(inv_std + j));
      }
    }
  }
  return;
}

template <typename T>
void BatchNormGradGradCpuKernelMod::TrainingComputeNCHW(const std::vector<kernel::KernelTensor *> &inputs,
                                                        const std::vector<kernel::KernelTensor *> &workspace,
                                                        const std::vector<kernel::KernelTensor *> &outputs) const {
  auto x_ori = static_cast<T *>(inputs.at(kIndex0)->device_ptr());
  auto dy_ori = static_cast<T *>(inputs.at(kIndex1)->device_ptr());
  auto reserve_space_1 = static_cast<float *>(inputs.at(kIndex3)->device_ptr());  // batch_mean  fp32
  auto reserve_space_2 = static_cast<float *>(inputs.at(kIndex4)->device_ptr());  // batch_var  fp32
  auto ddx_ori = static_cast<T *>(inputs.at(kIndex5)->device_ptr());

  // change dtype from 'T' to 'fp32'
  float *x = static_cast<float *>(workspace.at(kIndex0)->device_ptr());
  float *dy = static_cast<float *>(workspace.at(kIndex1)->device_ptr());
  float *ddx = static_cast<float *>(workspace.at(kIndex2)->device_ptr());

  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(x + index) = static_cast<float>(*(x_ori + index));
        *(dy + index) = static_cast<float>(*(dy_ori + index));
        *(ddx + index) = static_cast<float>(*(ddx_ori + index));
      }
    }
  }

  // create intermediate variables
  float *x_hat = static_cast<float *>(workspace.at(kIndex3)->device_ptr());
  float *inv_std = static_cast<float *>(workspace.at(kIndex11)->device_ptr());

  for (int j = 0; j < C_num_; j++) {
    *(inv_std + j) = num_1 / sqrt(*(reserve_space_2 + j) + epsilon_);
  }

  for (int i = 0; i < N_num_; i++) {
    for (int j = 0; j < C_num_; j++) {
      for (int k = 0; k < HW_num_; k++) {
        int index = i * CHW_num_ + j * HW_num_ + k;
        *(x_hat + index) = (*(inv_std + j)) * (*(x + index) - *(reserve_space_1 + j));
      }
    }
  }
  TrainingNCHWCalculateDx<T>(inputs, workspace, outputs, x_hat, inv_std);
  TrainingNCHWCalculateDdy<T>(inputs, workspace, outputs, x_hat, inv_std);
  TrainingNCHWCalculateDscale<T>(inputs, workspace, outputs, x_hat, inv_std);
  return;
}

template <typename T>
void BatchNormGradGradCpuKernelMod::InferenceComputeNCHW(const std::vector<kernel::KernelTensor *> &inputs,
                                                         const std::vector<kernel::KernelTensor *> &workspace,
                                                         const std::vector<kernel::KernelTensor *> &outputs) const {
  auto x_ori = static_cast<T *>(inputs.at(kIndex0)->device_ptr());
  auto dy_ori = static_cast<T *>(inputs.at(kIndex1)->device_ptr());
  auto scale = static_cast<float *>(inputs.at(kIndex2)->device_ptr());            // fp32
  auto reserve_space_1 = static_cast<float *>(inputs.at(kIndex3)->device_ptr());  // batch_mean  fp32
  auto reserve_space_2 = static_cast<float *>(inputs.at(kIndex4)->device_ptr());  // batch_var  fp32
  auto ddx_ori = static_cast<T *>(inputs.at(kIndex5)->device_ptr());
  auto ddscale = static_cast<float *>(inputs.at(kIndex6)->device_ptr());   // fp32
  auto ddoffset = static_cast<float *>(inputs.at(kIndex7)->device_ptr());  // fp32
  auto dx = static_cast<T *>(outputs.at(kIndex0)->device_ptr());
  auto ddy = static_cast<T *>(outputs.at(kIndex1)->device_ptr());
  auto dscale = static_cast<float *>(outputs.at(kIndex2)->device_ptr());  // fp32

  // change dtype from 'T' to 'fp32'
  float *x = static_cast<float *>(workspace.at(kIndex0)->device_ptr());
  float *dy = static_cast<float *>(workspace.at(kIndex1)->device_ptr());
  float *ddx = static_cast<float *>(workspace.at(kIndex2)->device_ptr());

  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(x + index) = static_cast<float>(*(x_ori + index));
        *(dy + index) = static_cast<float>(*(dy_ori + index));
        *(ddx + index) = static_cast<float>(*(ddx_ori + index));
      }
    }
  }

  // create intermediate variables
  float *x_hat = static_cast<float *>(workspace.at(kIndex3)->device_ptr());
  float *inv_std = static_cast<float *>(workspace.at(kIndex4)->device_ptr());

  for (int j = 0; j < C_num_; j++) {
    *(inv_std + j) = num_1 / sqrt(*(reserve_space_2 + j) + epsilon_);
  }

  for (int i = 0; i < N_num_; i++) {
    for (int j = 0; j < C_num_; j++) {
      for (int k = 0; k < HW_num_; k++) {
        int index = i * CHW_num_ + j * HW_num_ + k;
        *(x_hat + index) = (*(inv_std + j)) * (*(x + index) - *(reserve_space_1 + j));
      }
    }
  }

  // initialize dscale
  for (int j = 0; j < C_num_; j++) {
    *(dscale + j) = 0;
  }

  // calculate dx, ddy, dscale
  for (int i = 0; i < N_num_; i++) {
    for (int j = 0; j < C_num_; j++) {
      for (int k = 0; k < HW_num_; k++) {
        int index = i * CHW_num_ + j * HW_num_ + k;
        *(dx + index) = static_cast<T>((*(ddscale + j)) * (*(inv_std + j)) * (*(dy + index)));
        *(ddy + index) = static_cast<T>((*(ddx + index)) * (*(inv_std + j)) * (*(scale + j)) +
                                        (*(ddscale + j)) * (*(x_hat + index)) + *(ddoffset + j));
        *(dscale + j) += (*(ddx + index)) * (*(dy + index)) * (*(inv_std + j));
      }
    }
  }
  return;
}

template <typename T>
void BatchNormGradGradCpuKernelMod::TrainingNHWCCalculateDx(const std::vector<kernel::KernelTensor *> &inputs,
                                                            const std::vector<kernel::KernelTensor *> &workspace,
                                                            const std::vector<kernel::KernelTensor *> &outputs,
                                                            float *x_hat, float *inv_std) const {
  auto dy_ori = static_cast<T *>(inputs.at(kIndex1)->device_ptr());
  auto scale = static_cast<float *>(inputs.at(kIndex2)->device_ptr());            // fp32
  auto reserve_space_2 = static_cast<float *>(inputs.at(kIndex4)->device_ptr());  // batch_var  fp32
  auto ddx_ori = static_cast<T *>(inputs.at(kIndex5)->device_ptr());
  auto ddscale = static_cast<float *>(inputs.at(kIndex6)->device_ptr());  // fp32
  auto dx = static_cast<T *>(outputs.at(kIndex0)->device_ptr());

  // create intermediate variables
  float *sum_dy = static_cast<float *>(workspace.at(kIndex6)->device_ptr());
  float *sum_dy_x_hat = static_cast<float *>(workspace.at(kIndex7)->device_ptr());
  float *sum_ddx = static_cast<float *>(workspace.at(kIndex8)->device_ptr());
  float *sum_ddx_x_hat = static_cast<float *>(workspace.at(kIndex9)->device_ptr());
  float *sum_dy_ddx = static_cast<float *>(workspace.at(kIndex10)->device_ptr());

  // initialize
  for (int j = 0; j < C_num_; j++) {
    *(sum_dy + j) = 0;
    *(sum_dy_x_hat + j) = 0;
    *(sum_ddx + j) = 0;
    *(sum_ddx_x_hat + j) = 0;
    *(sum_dy_ddx + j) = 0;
  }

  // compute np.sum(to C dim)
  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(sum_dy + j) += static_cast<float>(*(dy_ori + index));
        *(sum_dy_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(dy_ori + index)));
        *(sum_ddx + j) += static_cast<float>(*(ddx_ori + index));
        *(sum_ddx_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(ddx_ori + index)));
        *(sum_dy_ddx + j) += (static_cast<float>(*(dy_ori + index))) * (static_cast<float>(*(ddx_ori + index)));
      }
    }
  }

  float *dx_term = static_cast<float *>(workspace.at(kIndex4)->device_ptr());
  float *scale_term = static_cast<float *>(workspace.at(kIndex5)->device_ptr());

  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(dx_term + index) =
          (*(scale + j)) / ((*(reserve_space_2 + j)) + epsilon_) *
          (((*(x_hat + index)) *
            ((*(sum_ddx + j)) * (*(sum_dy + j)) / M_ - (*(sum_dy_ddx + j)) +
             num_3 * (*(sum_dy_x_hat + j)) * (*(sum_ddx_x_hat + j)) / M_) /
            M_) +
           ((*(sum_ddx_x_hat + j)) * ((*(sum_dy + j)) / M_ - (static_cast<float>(*(dy_ori + index)))) / M_) +
           (*(sum_dy_x_hat + j)) * ((*(sum_ddx + j)) / M_ - (static_cast<float>(*(ddx_ori + index)))) / M_);
        *(scale_term + index) = (*(ddscale + j)) * (*(inv_std + j)) *
                                ((static_cast<float>(*(dy_ori + index))) - (*(sum_dy + j)) / M_ -
                                 (*(sum_dy_x_hat + j)) / M_ * (*(x_hat + index)));
      }
    }
  }

  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(dx + index) = static_cast<T>((*(dx_term + index)) + (*(scale_term + index)));
      }
    }
  }
  return;
}

template <typename T>
void BatchNormGradGradCpuKernelMod::TrainingNHWCCalculateDdy(const std::vector<kernel::KernelTensor *> &inputs,
                                                             const std::vector<kernel::KernelTensor *> &workspace,
                                                             const std::vector<kernel::KernelTensor *> &outputs,
                                                             float *x_hat, float *inv_std) const {
  auto dy_ori = static_cast<T *>(inputs.at(kIndex1)->device_ptr());
  auto scale = static_cast<float *>(inputs.at(kIndex2)->device_ptr());  // fp32
  auto ddx_ori = static_cast<T *>(inputs.at(kIndex5)->device_ptr());
  auto ddscale = static_cast<float *>(inputs.at(kIndex6)->device_ptr());   // fp32
  auto ddoffset = static_cast<float *>(inputs.at(kIndex7)->device_ptr());  // fp32
  auto ddy = static_cast<T *>(outputs.at(kIndex1)->device_ptr());

  // create intermediate variables
  float *sum_dy = static_cast<float *>(workspace.at(kIndex6)->device_ptr());
  float *sum_dy_x_hat = static_cast<float *>(workspace.at(kIndex7)->device_ptr());
  float *sum_ddx = static_cast<float *>(workspace.at(kIndex8)->device_ptr());
  float *sum_ddx_x_hat = static_cast<float *>(workspace.at(kIndex9)->device_ptr());
  float *sum_dy_ddx = static_cast<float *>(workspace.at(kIndex10)->device_ptr());

  // initialize
  for (int j = 0; j < C_num_; j++) {
    *(sum_dy + j) = 0;
    *(sum_dy_x_hat + j) = 0;
    *(sum_ddx + j) = 0;
    *(sum_ddx_x_hat + j) = 0;
    *(sum_dy_ddx + j) = 0;
  }

  // compute np.sum(to C dim)
  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(sum_dy + j) += static_cast<float>(*(dy_ori + index));
        *(sum_dy_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(dy_ori + index)));
        *(sum_ddx + j) += static_cast<float>(*(ddx_ori + index));
        *(sum_ddx_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(ddx_ori + index)));
        *(sum_dy_ddx + j) += (static_cast<float>(*(dy_ori + index))) * (static_cast<float>(*(ddx_ori + index)));
      }
    }
  }

  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(ddy + index) = static_cast<T>((*(scale + j)) * (*(inv_std + j)) / M_ *
                                          (M_ * (static_cast<float>(*(ddx_ori + index))) - (*(sum_ddx + j)) -
                                           (*(x_hat + index)) * (*(sum_ddx_x_hat + j))) +
                                        (*(ddscale + j)) * (*(x_hat + index)) + (*(ddoffset + j)));
      }
    }
  }
  return;
}

template <typename T>
void BatchNormGradGradCpuKernelMod::TrainingNHWCCalculateDscale(const std::vector<kernel::KernelTensor *> &inputs,
                                                                const std::vector<kernel::KernelTensor *> &workspace,
                                                                const std::vector<kernel::KernelTensor *> &outputs,
                                                                float *x_hat, float *inv_std) const {
  auto dy_ori = static_cast<T *>(inputs.at(kIndex1)->device_ptr());
  auto ddx_ori = static_cast<T *>(inputs.at(kIndex5)->device_ptr());
  auto dscale = static_cast<float *>(outputs.at(kIndex2)->device_ptr());  // fp32

  // create intermediate variables
  float *sum_dy = static_cast<float *>(workspace.at(kIndex6)->device_ptr());
  float *sum_dy_x_hat = static_cast<float *>(workspace.at(kIndex7)->device_ptr());
  float *sum_ddx = static_cast<float *>(workspace.at(kIndex8)->device_ptr());
  float *sum_ddx_x_hat = static_cast<float *>(workspace.at(kIndex9)->device_ptr());
  float *sum_dy_ddx = static_cast<float *>(workspace.at(kIndex10)->device_ptr());

  // initialize
  for (int j = 0; j < C_num_; j++) {
    *(sum_dy + j) = 0;
    *(sum_dy_x_hat + j) = 0;
    *(sum_ddx + j) = 0;
    *(sum_ddx_x_hat + j) = 0;
    *(sum_dy_ddx + j) = 0;
  }

  // compute np.sum(to C dim)
  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(sum_dy + j) += static_cast<float>(*(dy_ori + index));
        *(sum_dy_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(dy_ori + index)));
        *(sum_ddx + j) += static_cast<float>(*(ddx_ori + index));
        *(sum_ddx_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(ddx_ori + index)));
        *(sum_dy_ddx + j) += (static_cast<float>(*(dy_ori + index))) * (static_cast<float>(*(ddx_ori + index)));
      }
    }
  }

  for (int j = 0; j < C_num_; j++) {
    *(dscale + j) = 0;
  }

  for (int i = 0; i < N_num_; i++) {
    for (int k = 0; k < HW_num_; k++) {
      for (int j = 0; j < C_num_; j++) {
        int index = i * HW_num_ * C_num_ + k * C_num_ + j;
        *(dscale + j) += (static_cast<float>(*(ddx_ori + index))) * (*(inv_std + j)) *
                         ((static_cast<float>(*(dy_ori + index))) - (*(sum_dy + j)) / M_ -
                          (*(sum_dy_x_hat + j)) / M_ * (*(x_hat + index)));
      }
    }
  }
  return;
}

template <typename T>
void BatchNormGradGradCpuKernelMod::TrainingNCHWCalculateDx(const std::vector<kernel::KernelTensor *> &inputs,
                                                            const std::vector<kernel::KernelTensor *> &workspace,
                                                            const std::vector<kernel::KernelTensor *> &outputs,
                                                            float *x_hat, float *inv_std) const {
  auto dy_ori = static_cast<T *>(inputs.at(kIndex1)->device_ptr());
  auto scale = static_cast<float *>(inputs.at(kIndex2)->device_ptr());            // fp32
  auto reserve_space_2 = static_cast<float *>(inputs.at(kIndex4)->device_ptr());  // batch_var  fp32
  auto ddx_ori = static_cast<T *>(inputs.at(kIndex5)->device_ptr());
  auto ddscale = static_cast<float *>(inputs.at(kIndex6)->device_ptr());  // fp32
  auto dx = static_cast<T *>(outputs.at(kIndex0)->device_ptr());

  // create intermediate variables
  float *sum_dy = static_cast<float *>(workspace.at(kIndex6)->device_ptr());
  float *sum_dy_x_hat = static_cast<float *>(workspace.at(kIndex7)->device_ptr());
  float *sum_ddx = static_cast<float *>(workspace.at(kIndex8)->device_ptr());
  float *sum_ddx_x_hat = static_cast<float *>(workspace.at(kIndex9)->device_ptr());
  float *sum_dy_ddx = static_cast<float *>(workspace.at(kIndex10)->device_ptr());

  // initialize
  for (int j = 0; j < C_num_; j++) {
    *(sum_dy + j) = 0;
    *(sum_dy_x_hat + j) = 0;
    *(sum_ddx + j) = 0;
    *(sum_ddx_x_hat + j) = 0;
    *(sum_dy_ddx + j) = 0;
  }

  // compute np.sum(to C dim)
  for (int i = 0; i < N_num_; i++) {
    for (int j = 0; j < C_num_; j++) {
      for (int k = 0; k < HW_num_; k++) {
        int index = i * CHW_num_ + j * HW_num_ + k;
        *(sum_dy + j) += static_cast<float>(*(dy_ori + index));
        *(sum_dy_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(dy_ori + index)));
        *(sum_ddx + j) += static_cast<float>(*(ddx_ori + index));
        *(sum_ddx_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(ddx_ori + index)));
        *(sum_dy_ddx + j) += (static_cast<float>(*(dy_ori + index))) * (static_cast<float>(*(ddx_ori + index)));
      }
    }
  }

  float *dx_term = static_cast<float *>(workspace.at(kIndex4)->device_ptr());
  float *scale_term = static_cast<float *>(workspace.at(kIndex5)->device_ptr());

  for (int i = 0; i < N_num_; i++) {
    for (int j = 0; j < C_num_; j++) {
      for (int k = 0; k < HW_num_; k++) {
        int index = i * CHW_num_ + j * HW_num_ + k;
        *(dx_term + index) =
          (*(scale + j)) / ((*(reserve_space_2 + j)) + epsilon_) *
          (((*(x_hat + index)) *
            ((*(sum_ddx + j)) * (*(sum_dy + j)) / M_ - (*(sum_dy_ddx + j)) +
             num_3 * (*(sum_dy_x_hat + j)) * (*(sum_ddx_x_hat + j)) / M_) /
            M_) +
           ((*(sum_ddx_x_hat + j)) * ((*(sum_dy + j)) / M_ - (static_cast<float>(*(dy_ori + index)))) / M_) +
           (*(sum_dy_x_hat + j)) * ((*(sum_ddx + j)) / M_ - (static_cast<float>(*(ddx_ori + index)))) / M_);
        *(scale_term + index) = (*(ddscale + j)) * (*(inv_std + j)) *
                                ((static_cast<float>(*(dy_ori + index))) - (*(sum_dy + j)) / M_ -
                                 (*(sum_dy_x_hat + j)) / M_ * (*(x_hat + index)));
      }
    }
  }

  for (int i = 0; i < N_num_; i++) {
    for (int j = 0; j < C_num_; j++) {
      for (int k = 0; k < HW_num_; k++) {
        int index = i * CHW_num_ + j * HW_num_ + k;
        *(dx + index) = static_cast<T>((*(dx_term + index)) + (*(scale_term + index)));
      }
    }
  }
  return;
}

template <typename T>
void BatchNormGradGradCpuKernelMod::TrainingNCHWCalculateDdy(const std::vector<kernel::KernelTensor *> &inputs,
                                                             const std::vector<kernel::KernelTensor *> &workspace,
                                                             const std::vector<kernel::KernelTensor *> &outputs,
                                                             float *x_hat, float *inv_std) const {
  auto dy_ori = static_cast<T *>(inputs.at(kIndex1)->device_ptr());
  auto scale = static_cast<float *>(inputs.at(kIndex2)->device_ptr());  // fp32
  auto ddx_ori = static_cast<T *>(inputs.at(kIndex5)->device_ptr());
  auto ddscale = static_cast<float *>(inputs.at(kIndex6)->device_ptr());   // fp32
  auto ddoffset = static_cast<float *>(inputs.at(kIndex7)->device_ptr());  // fp32
  auto ddy = static_cast<T *>(outputs.at(kIndex1)->device_ptr());

  // create intermediate variables
  float *sum_dy = static_cast<float *>(workspace.at(kIndex6)->device_ptr());
  float *sum_dy_x_hat = static_cast<float *>(workspace.at(kIndex7)->device_ptr());
  float *sum_ddx = static_cast<float *>(workspace.at(kIndex8)->device_ptr());
  float *sum_ddx_x_hat = static_cast<float *>(workspace.at(kIndex9)->device_ptr());
  float *sum_dy_ddx = static_cast<float *>(workspace.at(kIndex10)->device_ptr());

  // initialize
  for (int j = 0; j < C_num_; j++) {
    *(sum_dy + j) = 0;
    *(sum_dy_x_hat + j) = 0;
    *(sum_ddx + j) = 0;
    *(sum_ddx_x_hat + j) = 0;
    *(sum_dy_ddx + j) = 0;
  }

  // compute np.sum(to C dim)
  for (int i = 0; i < N_num_; i++) {
    for (int j = 0; j < C_num_; j++) {
      for (int k = 0; k < HW_num_; k++) {
        int index = i * CHW_num_ + j * HW_num_ + k;
        *(sum_dy + j) += static_cast<float>(*(dy_ori + index));
        *(sum_dy_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(dy_ori + index)));
        *(sum_ddx + j) += static_cast<float>(*(ddx_ori + index));
        *(sum_ddx_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(ddx_ori + index)));
        *(sum_dy_ddx + j) += (static_cast<float>(*(dy_ori + index))) * (static_cast<float>(*(ddx_ori + index)));
      }
    }
  }

  for (int i = 0; i < N_num_; i++) {
    for (int j = 0; j < C_num_; j++) {
      for (int k = 0; k < HW_num_; k++) {
        int index = i * CHW_num_ + j * HW_num_ + k;
        *(ddy + index) = static_cast<T>((*(scale + j)) * (*(inv_std + j)) / M_ *
                                          (M_ * (static_cast<float>(*(ddx_ori + index))) - (*(sum_ddx + j)) -
                                           (*(x_hat + index)) * (*(sum_ddx_x_hat + j))) +
                                        (*(ddscale + j)) * (*(x_hat + index)) + (*(ddoffset + j)));
      }
    }
  }
  return;
}

template <typename T>
void BatchNormGradGradCpuKernelMod::TrainingNCHWCalculateDscale(const std::vector<kernel::KernelTensor *> &inputs,
                                                                const std::vector<kernel::KernelTensor *> &workspace,
                                                                const std::vector<kernel::KernelTensor *> &outputs,
                                                                float *x_hat, float *inv_std) const {
  auto dy_ori = static_cast<T *>(inputs.at(kIndex1)->device_ptr());
  auto ddx_ori = static_cast<T *>(inputs.at(kIndex5)->device_ptr());
  auto dscale = static_cast<float *>(outputs.at(kIndex2)->device_ptr());  // fp32

  // create intermediate variables
  float *sum_dy = static_cast<float *>(workspace.at(kIndex6)->device_ptr());
  float *sum_dy_x_hat = static_cast<float *>(workspace.at(kIndex7)->device_ptr());
  float *sum_ddx = static_cast<float *>(workspace.at(kIndex8)->device_ptr());
  float *sum_ddx_x_hat = static_cast<float *>(workspace.at(kIndex9)->device_ptr());
  float *sum_dy_ddx = static_cast<float *>(workspace.at(kIndex10)->device_ptr());

  // initialize
  for (int j = 0; j < C_num_; j++) {
    *(sum_dy + j) = 0;
    *(sum_dy_x_hat + j) = 0;
    *(sum_ddx + j) = 0;
    *(sum_ddx_x_hat + j) = 0;
    *(sum_dy_ddx + j) = 0;
  }

  // compute np.sum(to C dim)
  for (int i = 0; i < N_num_; i++) {
    for (int j = 0; j < C_num_; j++) {
      for (int k = 0; k < HW_num_; k++) {
        int index = i * CHW_num_ + j * HW_num_ + k;
        *(sum_dy + j) += static_cast<float>(*(dy_ori + index));
        *(sum_dy_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(dy_ori + index)));
        *(sum_ddx + j) += static_cast<float>(*(ddx_ori + index));
        *(sum_ddx_x_hat + j) += (*(x_hat + index)) * (static_cast<float>(*(ddx_ori + index)));
        *(sum_dy_ddx + j) += (static_cast<float>(*(dy_ori + index))) * (static_cast<float>(*(ddx_ori + index)));
      }
    }
  }

  for (int j = 0; j < C_num_; j++) {
    *(dscale + j) = 0;
  }

  for (int i = 0; i < N_num_; i++) {
    for (int j = 0; j < C_num_; j++) {
      for (int k = 0; k < HW_num_; k++) {
        int index = i * CHW_num_ + j * HW_num_ + k;
        *(dscale + j) += (static_cast<float>(*(ddx_ori + index))) * (*(inv_std + j)) *
                         ((static_cast<float>(*(dy_ori + index))) - (*(sum_dy + j)) / M_ -
                          (*(sum_dy_x_hat + j)) / M_ * (*(x_hat + index)));
      }
    }
  }
  return;
}

#define BATCH_NORM_GRAD_GRAD_REG(MS, S)                  \
  KernelAttr()                                           \
    .AddInputAttr(MS)                                    \
    .AddInputAttr(MS)                                    \
    .AddInputAttr(kNumberTypeFloat32)                    \
    .AddInputAttr(kNumberTypeFloat32)                    \
    .AddInputAttr(kNumberTypeFloat32)                    \
    .AddInputAttr(MS)                                    \
    .AddInputAttr(kNumberTypeFloat32)                    \
    .AddInputAttr(kNumberTypeFloat32)                    \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)    \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32) \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)   \
    .AddOutputAttr(MS)                                   \
    .AddOutputAttr(MS)                                   \
    .AddOutputAttr(kNumberTypeFloat32),                  \
    &BatchNormGradGradCpuKernelMod::LaunchKernel<S>

std::vector<std::pair<KernelAttr, BatchNormGradGradCpuKernelMod::BatchNormGradGradFunc>>
  BatchNormGradGradCpuKernelMod::func_list_ = {{BATCH_NORM_GRAD_GRAD_REG(kNumberTypeFloat32, float)},
                                               {BATCH_NORM_GRAD_GRAD_REG(kNumberTypeFloat16, float16)}};

std::vector<KernelAttr> BatchNormGradGradCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, BatchNormGradGradFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, BatchNormGradGrad, BatchNormGradGradCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
