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

#include "plugin/device/gpu/kernel/math/ormqr_gpu_kernel.h"
#include <complex>
#include <map>
#include <utility>
#include <vector>
#include "abstract/utils.h"
#include "include/common/utils/convert_utils.h"
#include "kernel/common_utils.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/complex.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/real_to_complex_impl.cuh"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_public/cusolver.h"
namespace mindspore {
namespace kernel {
bool OrmqrGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the kernel type should be in [ "
                  << "float32, float64, complex64, complex128], but got: " << kernel_attr << ".";
    return false;
  }
  launch_kernel_func_ = func_list_[index].second;
  unit_size_ = abstract::TypeIdSize(inputs[kIndex0]->dtype_id());
  left_ = GetValue<bool>(primitive_->GetAttr(ops::kAttrLeft));
  transpose_ = GetValue<bool>(primitive_->GetAttr(ops::kAttrTranspose));
  handle_ = device::gpu::GPUDeviceManager::GetInstance().GetCusolverDnHandle();
  return true;
}

int OrmqrGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  x_shape_ = inputs[kIndex0]->GetShapeVector();
  auto tau_shape = inputs[kIndex1]->GetShapeVector();
  other_shape_ = inputs[kIndex2]->GetShapeVector();

  batch_size_ = 1;
  for (size_t i = 0; i < x_shape_.size() - kDim2; i++) {
    batch_size_ = batch_size_ * x_shape_[i];
  }
  side_ = left_ ? CUBLAS_SIDE_LEFT : CUBLAS_SIDE_RIGHT;
  bool is_complex =
    (inputs[kIndex0]->dtype_id() == kNumberTypeComplex64) || (inputs[kIndex0]->dtype_id() == kNumberTypeComplex128);
  trans_ = transpose_ ? (is_complex ? CUBLAS_OP_C : CUBLAS_OP_T) : CUBLAS_OP_N;
  m_ = other_shape_[other_shape_.size() - kDim2];
  n_ = other_shape_[other_shape_.size() - kDim1];
  x_m_ = x_shape_[x_shape_.size() - kDim2];
  x_n_ = x_shape_[x_shape_.size() - kDim1];
  tau_n_ = tau_shape[tau_shape.size() - kDim1];

  transpose_x_axis_.resize(x_shape_.size());
  for (size_t idx = 0; idx < transpose_x_axis_.size(); ++idx) {
    transpose_x_axis_[idx] = static_cast<int64_t>(idx);
  }
  transpose_output_shape_ = other_shape_;
  std::swap(transpose_x_axis_[x_shape_.size() - kDim1], transpose_x_axis_[x_shape_.size() - kDim2]);
  std::swap(transpose_output_shape_[other_shape_.size() - kDim1], transpose_output_shape_[other_shape_.size() - kDim2]);

  workspace_size_list_.push_back(batch_size_ * sizeof(int));               // dev_info
  workspace_size_list_.push_back(batch_size_ * x_m_ * x_n_ * unit_size_);  // x data
  workspace_size_list_.push_back(batch_size_ * m_ * n_ * unit_size_);      // other data
  return 0;
}
template <typename T>
void OrmqrGpuKernelMod::RunOrmqr(T *d_x, T *tau, T *d_other, int *info) {
  int64_t lda = std::max<int64_t>((left_ ? m_ : n_), 1);
  int64_t ldc = m_ > 1 ? m_ : 1;
  int lwork = 0;
  cusolver::ormqr_buffersize<T>(handle_, side_, trans_, m_, n_, tau_n_, d_x, lda, tau, d_other, ldc, &lwork);
  void *d_work = device::gpu::GPUMemoryAllocator::GetInstance().AllocTensorMem(sizeof(T) * lwork);
  if (d_work == nullptr) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the memory of d_work alloc failed.";
  }
  for (int64_t batch = 0; batch < batch_size_; ++batch) {
    cusolver::ormqr<T>(handle_, side_, trans_, m_, n_, tau_n_, d_x, lda, tau, d_other, ldc, static_cast<T *>(d_work),
                       lwork, info);
    d_x += x_m_ * x_n_;
    tau += tau_n_;
    d_other += m_ * n_;
    info += 1;
  }
  device::gpu::GPUMemoryAllocator::GetInstance().FreeTensorMem(d_work);
}

template <typename T>
bool OrmqrGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &workspace,
                                     const std::vector<KernelTensor *> &outputs) {
  if (outputs[0]->size() == 0) {
    return true;
  }
  CHECK_CUSOLVER_RET_WITH_ERROR(cusolverDnSetStream(handle_, reinterpret_cast<cudaStream_t>(cuda_stream_)),
                                "CusolverDnSetStream failed");

  T *x = GetDeviceAddress<T>(inputs, kIndex0);
  T *tau = GetDeviceAddress<T>(inputs, kIndex1);
  T *other = GetDeviceAddress<T>(inputs, kIndex2);
  T *output_y = GetDeviceAddress<T>(outputs, kIndex0);
  MS_EXCEPTION_IF_NULL(x);
  MS_EXCEPTION_IF_NULL(tau);
  MS_EXCEPTION_IF_NULL(other);
  MS_EXCEPTION_IF_NULL(output_y);

  int *dev_info = GetDeviceAddress<int>(workspace, kIndex0);
  T *d_x = GetDeviceAddress<T>(workspace, kIndex1);
  T *d_other = GetDeviceAddress<T>(workspace, kIndex2);
  MS_EXCEPTION_IF_NULL(dev_info);
  MS_EXCEPTION_IF_NULL(d_x);
  MS_EXCEPTION_IF_NULL(d_other);

  TransposeInfo x_info;
  TransposeInfo y_info;
  TransposeInfo o_info;
  for (size_t i = 0; i < x_shape_.size(); ++i) {
    x_info.input_shape.push_back(static_cast<int64_t>(x_shape_[i]));
    x_info.perm.push_back(static_cast<int32_t>(transpose_x_axis_[i]));
    o_info.perm.push_back(static_cast<int32_t>(transpose_x_axis_[i]));
    y_info.perm.push_back(static_cast<int32_t>(transpose_x_axis_[i]));
  }
  for (size_t i = 0; i < other_shape_.size(); ++i) {
    o_info.input_shape.push_back(static_cast<int64_t>(other_shape_[i]));
    y_info.input_shape.push_back(static_cast<int64_t>(transpose_output_shape_[i]));
  }
  size_t trans_size = static_cast<size_t>(batch_size_ * x_m_ * x_n_);
  auto s1 = CalTranspose<T, true>(trans_size, x, x_info, d_x, reinterpret_cast<cudaStream_t>(cuda_stream_));
  CHECK_CUDA_STATUS(s1, "Transpose called by " + kernel_name_);
  trans_size = static_cast<size_t>(batch_size_ * m_ * n_);
  auto s2 = CalTranspose<T, true>(trans_size, other, o_info, d_other, reinterpret_cast<cudaStream_t>(cuda_stream_));
  CHECK_CUDA_STATUS(s2, "Transpose called by " + kernel_name_);
  RunOrmqr(d_x, tau, d_other, dev_info);
  auto s3 = CalTranspose<T, true>(trans_size, d_other, y_info, output_y, reinterpret_cast<cudaStream_t>(cuda_stream_));
  CHECK_CUDA_STATUS(s3, "Transpose called by " + kernel_name_);
  return true;
}

std::vector<std::pair<KernelAttr, OrmqrGpuKernelMod::LaunchKernelFunc>> OrmqrGpuKernelMod::func_list_ = {
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeFloat32)
     .AddOutputAttr(kNumberTypeFloat32),
   &OrmqrGpuKernelMod::LaunchKernel<float>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeFloat64)
     .AddOutputAttr(kNumberTypeFloat64),
   &OrmqrGpuKernelMod::LaunchKernel<double>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeComplex64)
     .AddOutputAttr(kNumberTypeComplex64),
   &OrmqrGpuKernelMod::LaunchKernel<Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeComplex128)
     .AddOutputAttr(kNumberTypeComplex128),
   &OrmqrGpuKernelMod::LaunchKernel<Complex<double>>},
};

std::vector<KernelAttr> OrmqrGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, LaunchKernelFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, Ormqr, OrmqrGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
