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

#include "plugin/device/gpu/kernel/sparse_grad/sparse_segment_mean_grad_gpu_kernel.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kNumber1 = 1;
constexpr size_t kNumber4 = 4;
}  // namespace

bool SparseSegmentMeanGradGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                             const std::vector<KernelTensor *> &outputs) {
  size_t inputs_num = kNumber4;
  size_t outputs_num = kNumber1;
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), inputs_num, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), outputs_num, kernel_name_);

  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << kernel_name_ << " does not support this kernel data type: " << kernel_attr << ".";
    return false;
  }
  kernel_func_ = kernel_attr_map_[index].second;
  unit_grad_size_ = abstract::TypeIdSize(inputs[kIndex0]->dtype_id());
  unit_idx_seg_size_ = abstract::TypeIdSize(inputs[kIndex1]->dtype_id());
  return true;
}

int SparseSegmentMeanGradGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                              const std::vector<KernelTensor *> &outputs) {
  for (const auto &input : inputs) {
    // If any input shape contains -1, means input shape is dynamic, so just return do nothing.
    auto input_shape = input->GetShapeVector();
    if (!IsValidShape(input_shape)) {
      return KRET_UNKNOWN_SHAPE;
    }
  }
  for (const auto &output : outputs) {
    // If any input shape contains -1, means input shape is dynamic, so just return do nothing.
    auto output_shape = output->GetShapeVector();
    if (!IsValidShape(output_shape)) {
      return KRET_UNKNOWN_SHAPE;
    }
  }
  ResetResource();
  std::vector<int64_t> output_shape = outputs.at(kIndex0)->GetShapeVector();
  output_elements_ = std::accumulate(output_shape.begin(), output_shape.end(), 1, std::multiplies<int64_t>());
  if (output_elements_ == 0) {
    is_null_input_ = true;
  }
  std::vector<int64_t> grad_shape = inputs.at(kIndex0)->GetShapeVector();
  grad_shape_0_ = grad_shape[0];
  grad_elements_ = std::accumulate(grad_shape.begin(), grad_shape.end(), 1, std::multiplies{});
  outer_size_ = grad_shape.front();
  inner_size_ = grad_elements_ / outer_size_;
  std::vector<int64_t> indices_shape = inputs.at(kIndex1)->GetShapeVector();
  idx_seg_elements_ = std::accumulate(indices_shape.begin(), indices_shape.end(), 1, std::multiplies{});
  output_dim0_ = LongToSize(output_shape.front());

  size_t output_size = output_elements_ * unit_grad_size_;
  output_size_list_.push_back(output_size);
  workspace_size_list_.push_back((outer_size_ + 1) * sizeof(size_t));
  return KRET_OK;
}

template <typename R, typename S>
bool SparseSegmentMeanGradGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                                     const std::vector<KernelTensor *> &workspace,
                                                     const std::vector<KernelTensor *> &outputs) {
  R *grad_ptr = GetDeviceAddress<R>(inputs, kIndex0);
  S *indices_ptr = GetDeviceAddress<S>(inputs, kIndex1);
  S *segment_ids_ptr = GetDeviceAddress<S>(inputs, kIndex2);
  R *y_ptr = GetDeviceAddress<R>(outputs, kIndex0);
  size_t *segment_pos_ptr = GetDeviceAddress<size_t>(workspace, kIndex0);
  auto any = [](auto... args) -> bool { return ((args == nullptr) || ...); };
  if (any(grad_ptr, indices_ptr, segment_ids_ptr, segment_pos_ptr, y_ptr)) {
    cudaMemset(y_ptr, 0, outputs[0]->size());
    return true;
  }
  cudaStream_t stream = reinterpret_cast<cudaStream_t>(cuda_stream_);
  std::vector<S> indices_host;
  std::vector<S> segment_ids_host;
  indices_host.resize(idx_seg_elements_);
  segment_ids_host.resize(idx_seg_elements_);
  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(
    cudaMemcpyAsync(indices_host.data(), indices_ptr, idx_seg_elements_ * sizeof(S), cudaMemcpyDeviceToHost, stream),
    "For 'SparseSegmentMeanGrad', cudaMemcpy indices failed.");
  CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(cudaMemcpyAsync(segment_ids_host.data(), segment_ids_ptr,
                                                     idx_seg_elements_ * sizeof(S), cudaMemcpyDeviceToHost, stream),
                                     "For 'SparseSegmentMeanGrad', cudaMemcpy segment_ids failed.");
  if (cudaStreamQuery(stream) != cudaSuccess) {
    CHECK_CUDA_RET_WITH_EXCEPT_NOTRACE(cudaStreamSynchronize(stream),
                                       "For 'SparseSegmentMeanGrad', cudaStreamSyncFailed");
  }
  for (size_t i = 1; i < idx_seg_elements_; i++) {
    if (segment_ids_host[i] < segment_ids_host[i - 1]) {
      MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', segment_ids should be sorted.";
    }
  }
  for (size_t i = 0; i < idx_seg_elements_; i++) {
    if (indices_host[i] >= static_cast<int>(output_dim0_)) {
      MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', indices out of range of output_dim0.";
    }
    if (segment_ids_host[i] >= static_cast<S>(grad_shape_0_)) {
      MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', segment_ids out of range of grad's first shape.";
    }
  }
  cudaMemset(y_ptr, 0, output_elements_ * unit_grad_size_);
  auto status = CalSparseSegmentMeanGrad(grad_ptr, segment_ids_ptr, indices_ptr, segment_pos_ptr, outer_size_,
                                         inner_size_, idx_seg_elements_, output_dim0_, y_ptr, device_id_, stream);
  CHECK_CUDA_STATUS(status, kernel_name_);
  return true;
}

std::vector<std::pair<KernelAttr, SparseSegmentMeanGradGpuKernelMod::SSMGLaunchFunc>>
  SparseSegmentMeanGradGpuKernelMod::kernel_attr_map_ = {{
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat16)
       .AddInputAttr(kNumberTypeInt32)
       .AddInputAttr(kNumberTypeInt32)
       .AddInputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeFloat16),
     &SparseSegmentMeanGradGpuKernelMod::LaunchKernel<half, int32_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kNumberTypeInt32)
       .AddInputAttr(kNumberTypeInt32)
       .AddInputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeFloat32),
     &SparseSegmentMeanGradGpuKernelMod::LaunchKernel<float, int32_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat64)
       .AddInputAttr(kNumberTypeInt32)
       .AddInputAttr(kNumberTypeInt32)
       .AddInputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeFloat64),
     &SparseSegmentMeanGradGpuKernelMod::LaunchKernel<double, int32_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat16)
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeFloat16),
     &SparseSegmentMeanGradGpuKernelMod::LaunchKernel<half, int64_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeFloat32),
     &SparseSegmentMeanGradGpuKernelMod::LaunchKernel<float, int64_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat64)
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeFloat64),
     &SparseSegmentMeanGradGpuKernelMod::LaunchKernel<double, int64_t>},
  }};

std::vector<KernelAttr> SparseSegmentMeanGradGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(
    kernel_attr_map_.begin(), kernel_attr_map_.end(), std::back_inserter(support_list),
    [](const std::pair<KernelAttr, SparseSegmentMeanGradGpuKernelMod::SSMGLaunchFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, SparseSegmentMeanGrad, SparseSegmentMeanGradGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
