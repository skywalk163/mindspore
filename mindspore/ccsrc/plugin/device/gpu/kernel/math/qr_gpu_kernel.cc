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

#include "plugin/device/gpu/kernel/math/qr_gpu_kernel.h"
#include <type_traits>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_public/cusolver.h"

namespace mindspore {
namespace kernel {
template <typename R>
using Complex = mindspore::utils::Complex<R>;

constexpr size_t kNum2 = 2;
bool QrGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (inputs.empty() || outputs.empty()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' got empty inputs or outputs, which is invalid.";
    return false;
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the kernel type should be in [float32, float64, complex64, "
                  << "complex128], but got: " << kernel_attr << ".";
    return false;
  }
  kernel_func_ = func_list_[index].second;
  unit_input_size_ = abstract::TypeIdSize(kernel_attr.GetInputAttr(kIndex0).dtype);
  full_matrices_ = inputs[kIndex1]->GetValueWithCheck<bool>();
  cusolverH_ = device::gpu::GPUDeviceManager::GetInstance().GetCusolverDnHandle();
  return true;
}

int QrGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  ResetResource();
  std::vector<int64_t> output_shape = outputs.at(kIndex0)->GetShapeVector();
  size_t output_elements = std::accumulate(output_shape.begin(), output_shape.end(), 1, std::multiplies<int64_t>());
  if (output_elements == 0) {
    is_null_input_ = true;
  }

  std::vector<size_t> x_shape = std::vector<size_t>(inputs.at(kIndex0)->GetDeviceShapeVector().begin(),
                                                    inputs.at(kIndex0)->GetDeviceShapeVector().end());
  total_size_ = std::accumulate(x_shape.begin(), x_shape.end(), size_t(1), std::multiplies<size_t>());
  input_dims_ = x_shape.size();
  if (input_dims_ < kDim2) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', dimensions must be greater than or equal to 2"
                  << ", but got [" << input_dims_ << "].";
    return KRET_RESIZE_FAILED;
  }
  m_ = x_shape[input_dims_ - kDim2];
  n_ = x_shape[input_dims_ - kDim1];
  if (full_matrices_) {
    p_ = m_;
  } else {
    p_ = std::min(m_, n_);
  }
  s_ = std::max(m_, n_);

  batch_size_ = 1;
  for (size_t i = 0; i < input_dims_ - kDim2; i++) {
    batch_size_ = batch_size_ * x_shape[i];
  }

  // transpose row and col
  for (size_t i = 0; i < input_dims_; ++i) {
    transpose_input_shape_[i] = x_shape[i];
    transpose_q_shape_[i] = transpose_input_shape_[i];
    if (i == input_dims_ - kDim2) {
      transpose_input_axis_[i] = input_dims_ - kDim1;
    } else if (i == input_dims_ - kDim1) {
      transpose_input_axis_[i] = input_dims_ - kDim2;
    } else {
      transpose_input_axis_[i] = i;
    }
  }
  transpose_q_shape_[input_dims_ - kDim2] = p_;
  transpose_q_shape_[input_dims_ - kDim1] = m_;

  output_size_list_ = {batch_size_ * m_ * p_ * unit_input_size_, batch_size_ * p_ * n_ * unit_input_size_};
  workspace_size_list_ = {
    batch_size_ * sizeof(int),
    total_size_ * unit_input_size_,
    batch_size_ * m_ * p_ * unit_input_size_,
    batch_size_ * m_ * n_ * unit_input_size_,
    batch_size_ * n_ * unit_input_size_,
    batch_size_ * m_ * s_ * unit_input_size_,
    batch_size_ * m_ * n_ * unit_input_size_,
  };

  return 0;
}

template <typename T>
void QrGpuKernelMod::RunQr(T *d_input, T *d_A, T *d_tau, int *dev_info, T *d_output_q, T *d_output_r) {
  const size_t lda = m_;
  cudaStream_t stream = reinterpret_cast<cudaStream_t>(cuda_stream_);
  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(
    cudaMemcpyAsync(d_A, d_input, sizeof(T) * m_ * n_, cudaMemcpyDeviceToDevice, stream),
    "copy device A result to host failed");
  int geqrf_work_size = 0;
  cusolver::geqrf_buffersize<T>(cusolverH_, m_, n_, d_A, lda, &geqrf_work_size);
  int orgqr_work_size = 0;
  cusolver::orgqr_buffersize<T>(cusolverH_, m_, p_, p_, d_A, lda, d_tau, &orgqr_work_size);
  int lwork = geqrf_work_size > orgqr_work_size ? geqrf_work_size : orgqr_work_size;

  void *d_work = device::gpu::GPUMemoryAllocator::GetInstance().AllocTensorMem(sizeof(T) * lwork);
  if (d_work == nullptr) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the memory of d_work alloc failed.";
  }
  // compute QR factorization
  cusolver::geqrf<T>(cusolverH_, m_, n_, d_A, lda, d_tau, static_cast<T *>(d_work), lwork, dev_info);
  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(
    cudaMemcpyAsync(d_output_r, d_A, sizeof(T) * m_ * n_, cudaMemcpyDeviceToDevice, stream),
    "Copy to QR factorization device result failed");
  // compute Q=H(1)*H(2)*...*H(K)
  cusolver::orgqr<T>(cusolverH_, m_, p_, p_, d_A, lda, d_tau, static_cast<T *>(d_work), lwork, dev_info);
  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(
    cudaMemcpyAsync(d_output_q, d_A, sizeof(T) * m_ * p_, cudaMemcpyDeviceToDevice, stream),
    "copy device Q result to host failed");
  device::gpu::GPUMemoryAllocator::GetInstance().FreeTensorMem(d_work);
}

template <typename T>
void QrGpuKernelMod::LaunchQr(T *d_input, T *d_A, T *d_tau, T *d_output_q, T *d_output_r, int *dev_info,
                              T *d_output_r_t, T *output_r) {
  TransposeInfo info;
  info.input_shape = std::vector<int64_t>{n_, m_};
  info.perm = std::vector<int32_t>{1, 0};
  for (size_t batch = 0; batch < batch_size_; ++batch) {
    cudaStream_t stream = reinterpret_cast<cudaStream_t>(cuda_stream_);
    RunQr(d_input + batch * m_ * n_, d_A + batch * m_ * s_, d_tau + batch * n_, dev_info + batch,
          d_output_q + batch * m_ * p_, d_output_r + batch * m_ * n_);
    auto status =
      CalTranspose<T, true>(m_ * n_, d_output_r + batch * m_ * n_, info, d_output_r_t + batch * m_ * n_, stream);
    CHECK_CUDA_STATUS(status, "Transpose called by " + kernel_name_);
    status =
      CalTriu(p_ * n_, d_output_r_t + batch * m_ * n_, 0, p_, n_, output_r + batch * p_ * n_, device_id_, stream);
    CHECK_CUDA_STATUS(status, kernel_name_);
  }
}

template <typename T>
bool QrGpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                  const std::vector<kernel::KernelTensor *> &workspace,
                                  const std::vector<kernel::KernelTensor *> &outputs) {
  cudaStream_t stream = reinterpret_cast<cudaStream_t>(cuda_stream_);
  CHECK_CUSOLVER_RET_WITH_ERROR(cusolverDnSetStream(cusolverH_, stream), "CusolverDnSetStream failed");
  T *input = GetDeviceAddress<T>(inputs, kIndex0);
  T *output_q = GetDeviceAddress<T>(outputs, kIndex0);
  T *output_r = GetDeviceAddress<T>(outputs, kIndex1);
  MS_EXCEPTION_IF_NULL(input);
  MS_EXCEPTION_IF_NULL(output_q);
  MS_EXCEPTION_IF_NULL(output_r);

  int *dev_info = GetDeviceAddress<int>(workspace, kIndex0);
  T *d_input = GetDeviceAddress<T>(workspace, kIndex1);
  T *d_output_q = GetDeviceAddress<T>(workspace, kIndex2);
  T *d_output_r = GetDeviceAddress<T>(workspace, kIndex3);
  T *d_tau = GetDeviceAddress<T>(workspace, kIndex4);
  T *d_A = GetDeviceAddress<T>(workspace, kIndex5);
  T *d_output_r_t = GetDeviceAddress<T>(workspace, kIndex6);
  MS_EXCEPTION_IF_NULL(dev_info);
  MS_EXCEPTION_IF_NULL(d_input);
  MS_EXCEPTION_IF_NULL(d_output_q);
  MS_EXCEPTION_IF_NULL(d_output_r);
  MS_EXCEPTION_IF_NULL(d_tau);
  MS_EXCEPTION_IF_NULL(d_A);
  MS_EXCEPTION_IF_NULL(d_output_r_t);

  TransposeInfo x_info;
  TransposeInfo y_info;
  for (size_t i = 0; i < input_dims_; ++i) {
    x_info.input_shape.push_back(static_cast<int64_t>(transpose_input_shape_[i]));
    x_info.perm.push_back(static_cast<int>(transpose_input_axis_[i]));
    y_info.input_shape.push_back(static_cast<int64_t>(transpose_q_shape_[i]));
    y_info.perm.push_back(static_cast<int>(transpose_input_axis_[i]));
  }

  auto s1 = CalTranspose<T, true>(total_size_, input, x_info, d_input, stream);
  CHECK_CUDA_STATUS(s1, "Transpose called by " + kernel_name_);
  LaunchQr(d_input, d_A, d_tau, d_output_q, d_output_r, dev_info, d_output_r_t, output_r);
  auto s2 = CalTranspose<T, true>(batch_size_ * m_ * p_, d_output_q, y_info, output_q, stream);
  CHECK_CUDA_STATUS(s2, "Transpose called by " + kernel_name_);
  return true;
}

std::vector<std::pair<KernelAttr, QrGpuKernelMod::LaunchKernelFunc>> QrGpuKernelMod::func_list_ = {
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddOutputAttr(kNumberTypeFloat32)
     .AddOutputAttr(kNumberTypeFloat32),
   &QrGpuKernelMod::LaunchKernel<float>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddOutputAttr(kNumberTypeFloat64)
     .AddOutputAttr(kNumberTypeFloat64),
   &QrGpuKernelMod::LaunchKernel<double>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeComplex64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddOutputAttr(kNumberTypeComplex64)
     .AddOutputAttr(kNumberTypeComplex64),
   &QrGpuKernelMod::LaunchKernel<Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeComplex128)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddOutputAttr(kNumberTypeComplex128)
     .AddOutputAttr(kNumberTypeComplex128),
   &QrGpuKernelMod::LaunchKernel<Complex<double>>}};

std::vector<KernelAttr> QrGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, LaunchKernelFunc> &pair) { return pair.first; });
  return support_list;
}
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, Qr, QrGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
