/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/mkldnn/pooling_grad_cpu_kernel.h"
#include <functional>
#include <unordered_map>
#include "ops/conv_pool_op_name.h"
#include "utils/profile.h"
#include "kernel/ops_utils.h"
#include "mindspore/core/ops/op_utils.h"
#include "mindspore/ccsrc/kernel/common_utils.h"
#include "mindspore/ccsrc/kernel/format_utils.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kMaxPoolingGradWorkSpaceNum = 2;
constexpr size_t kAvgPoolingGradWorkSpaceNum = 1;
// avgpoolgrad and maxpoolgrad input indexes
constexpr size_t kGradIndex = 2;
constexpr size_t kKernelSizeIdx = 3;
constexpr size_t kStridesIdx = 4;
constexpr size_t kPadModeIdx = 5;
constexpr size_t kDataFormatIdx = 6;

// avgpool3dgrad input indexes
constexpr size_t kAvg3DGradIndex = 1;
constexpr size_t kAvg3DKernelSizeIdx = 2;
constexpr size_t kAvg3DStridesIdx = 3;
constexpr size_t kAvg3DPadModeIdx = 4;
constexpr size_t kAvg3DPadsIdx = 5;
constexpr size_t kAvg3DCeilModeIdx = 6;
constexpr size_t kAvg3DCountIncludePadIdx = 7;
constexpr size_t kAvg3DDivisorOverrideIdx = 8;
constexpr size_t kAvg3DDataFormatIdx = 9;

// maxpool3dgrad input indexes are different from those of avgpool3dgrad
// the input indexes of maxpool3dgrad are roughly listed, which need to be determined later.
constexpr size_t kMax3DGradIndex = kGradIndex;
constexpr size_t kMax3DKernelSizeIdx = kKernelSizeIdx;
constexpr size_t kMax3DStridesIdx = kStridesIdx;
constexpr size_t kMax3DPadModeIdx = kPadModeIdx;
constexpr size_t kMax3DPadsIdx = 6;
constexpr size_t kMax3DCeilModeIdx = 7;
constexpr size_t kMax3DDataFormatIdx = 8;

}  // namespace

bool PoolingGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  if (kernel_name_ != kAvgPoolGradOpName) {
    if (KernelMod::primitive_->HasAttr(CEIL_MODE)) {
      ValuePtr ceil_mode = KernelMod::primitive_->GetAttr(CEIL_MODE);
      ceil_mode_ = (ceil_mode->isa<BoolImm>() && GetValue<bool>(ceil_mode)) ||
                   (ceil_mode->isa<Int64Imm>() && GetValue<int64_t>(ceil_mode) == 1);
    }
    if (kernel_name_ == kAvgPool3DGradOpName) {
      algorithm_ = dnnl::algorithm::pooling_avg;
      if (KernelMod::primitive_->HasAttr(COUNT_INCLUDE_PAD) &&
          GetValue<bool>(KernelMod::primitive_->GetAttr(COUNT_INCLUDE_PAD))) {
        algorithm_ = dnnl::algorithm::pooling_avg_include_padding;
      }
      if (KernelMod::primitive_->HasAttr(DIVISOR_OVERRIDE) &&
          GetValue<int64_t>(KernelMod::primitive_->GetAttr(DIVISOR_OVERRIDE)) != 0) {
        divisor_override_ = GetValue<int64_t>(KernelMod::primitive_->GetAttr(DIVISOR_OVERRIDE));
      }
    }
    grad_index_ = kernel_name_ == kAvgPool3DGradOpName ? 1 : kGradIndex;
    format_ = GetFormatFromStrToEnum(GetValue<std::string>(KernelMod::primitive_->GetAttr(FORMAT)));
    pad_mode_ = static_cast<mindspore::PadMode>(
      ops::PadModeStringToInt(GetValue<std::string>(KernelMod::primitive_->GetAttr(PAD_MODE))));
    kernel_include_nc_ = GetValue<std::vector<int64_t>>(KernelMod::primitive_->GetAttr(KERNEL_SIZE));
    strides_include_nc_ = GetValue<std::vector<int64_t>>(KernelMod::primitive_->GetAttr(STRIDES));
    dtype_ = inputs[grad_index_]->dtype_id();
    return true;
  }
  grad_index_ = kGradIndex;
  dtype_ = inputs[grad_index_]->dtype_id();
  // AvgPoolGrad input
  algorithm_ = dnnl::algorithm::pooling_avg;
  return true;
}

int PoolingGradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  if (kernel_name_ == kAvgPoolGradOpName) {
    pad_mode_ = static_cast<mindspore::PadMode>(inputs[kPadModeIdx]->GetValueWithCheck<int64_t>());
    kernel_include_nc_ = inputs[kKernelSizeIdx]->GetValueWithCheck<std::vector<int64_t>>();
    strides_include_nc_ = inputs[kStridesIdx]->GetValueWithCheck<std::vector<int64_t>>();
    format_ = static_cast<mindspore::Format>(inputs[kDataFormatIdx]->GetValueWithCheck<int64_t>());
    kernel_include_nc_.insert(kernel_include_nc_.begin(), 2, 1);
    strides_include_nc_.insert(strides_include_nc_.begin(), 2, 1);
  }
  auto src_shape = outputs[0]->GetShapeVector();
  dst_shape_ = inputs[grad_index_]->GetShapeVector();
  const size_t src_dim = src_shape.size();
  if (src_dim != SHAPE_4D && src_dim != SHAPE_5D) {
    MS_LOG(EXCEPTION) << "PoolingGrad only supports 4D/5D input, but got " << src_dim << "D";
  }
  src_desc_ = GetDefaultMemDesc(src_shape);
  dst_desc_ = GetDefaultMemDesc(dst_shape_);
  if (src_dim == SHAPE_4D && format_ != mindspore::Format::NCHW) {
    MS_LOG(EXCEPTION) << kernel_name_ << " only supports 4D input with NCHW format, but got format "
                      << GetFormatFromEnumToStr(format_);
  }
  if (src_dim == SHAPE_5D && format_ != mindspore::Format::NCDHW) {
    MS_LOG(EXCEPTION) << kernel_name_ << " only supports 5D input with NCDHW format, but got format"
                      << GetFormatFromEnumToStr(format_);
  }
  if (kernel_include_nc_.size() != src_dim) {
    MS_LOG(EXCEPTION) << kernel_name_ << " requires kernel_size must be " << src_dim << "D, but got "
                      << kernel_include_nc_.size() << "D!";
  }
  if (strides_include_nc_.size() != src_dim) {
    MS_LOG(EXCEPTION) << kernel_name_ << " requires strides must be " << src_dim << "D, but got "
                      << strides_include_nc_.size() << "D!";
  }
  const dnnl::memory::dims kernel(kernel_include_nc_.begin() + NC_LEN, kernel_include_nc_.end());
  const dnnl::memory::dims strides(strides_include_nc_.begin() + NC_LEN, strides_include_nc_.end());
  const dnnl::memory::dims dilation(kernel.size(), kPoolingDilation);
  dnnl::memory::dims padding_l;
  dnnl::memory::dims padding_r;
  kernel_ = kernel;
  PaddingInfo padding_info{pad_mode_, kernel, strides, dilation, &padding_l, &padding_r, &padding_invalid_, ceil_mode_};
  std::vector<int64_t> pad_list;
  if (kernel_name_ == kAvgPool3DGradOpName || kernel_name_ == kMaxPool3DGradOpName) {
    pad_list = GetValue<std::vector<int64_t>>(KernelMod::primitive_->GetAttr(PAD_LIST));
  }
  GetPadding(src_shape, padding_info, pad_list);

  // Pooling_avg forward description
  const auto desc = CreateDesc<dnnl::pooling_forward::desc>(dnnl::prop_kind::forward_training, algorithm_, src_desc_,
                                                            dst_desc_, strides, kernel, padding_l, padding_r);
  auto forward_prim_desc = CreateDesc<dnnl::pooling_forward::primitive_desc>(desc, engine_);

  // Pooling_avg backward description
  const auto backward_desc =
    CreateDesc<dnnl::pooling_backward::desc>(algorithm_, src_desc_, dst_desc_, strides, kernel, padding_l, padding_r);
  const auto backward_prim_desc =
    CreateDesc<dnnl::pooling_backward::primitive_desc>(backward_desc, engine_, forward_prim_desc);
  primitive_ = CreatePrimitive<dnnl::pooling_backward>(backward_prim_desc);
  AddArgument(DNNL_ARG_DIFF_SRC, src_desc_);
  AddArgument(DNNL_ARG_DIFF_DST, dst_desc_);

  // For pooling_max, need a workspace that generated in forward and stored the max value indexes to compute grad.
  if (algorithm_ == dnnl::algorithm::pooling_max) {
    primitive_forward_ = CreatePrimitive<dnnl::pooling_forward>(forward_prim_desc);
    workspace_desc_ = GetWorkspaceDesc(forward_prim_desc);
    AddArgument(DNNL_ARG_WORKSPACE, workspace_desc_);
    size_t work_space = GetSize(workspace_desc_);
    workspace_size_list_.push_back(work_space);
  }
  workspace_size_list_.push_back(inputs[grad_index_]->size());
  return KRET_OK;
}

template <typename T>
void PoolingGradCpuKernelMod::ReComputeDivisor(T *dst) {
  const int64_t kernel_size = std::accumulate(kernel_.begin(), kernel_.end(), int64_t(1), std::multiplies<int64_t>());
  const size_t size = std::accumulate(dst_shape_.begin(), dst_shape_.end(), size_t(1), std::multiplies<size_t>());
  CTask task = [&](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      dst[i] = dst[i] * static_cast<T>(LongToFloat(kernel_size)) / static_cast<T>(LongToFloat(divisor_override_));
    }
  };
  ParallelLaunchAutoSearch(task, size, this, &parallel_search_info_);
}

template <typename T>
void PoolingGradCpuKernelMod::EliminateInvalidPadding(T *dst) {
  if (dst_shape_.size() < SHAPE_5D || kernel_.size() + NC_LEN < SHAPE_5D ||
      padding_invalid_.size() + NC_LEN < SHAPE_5D) {
    MS_LOG(ERROR) << "The dst_shape must be 5D, the kernel and the padding_invalid must be 3D!";
  }
  const auto d_max = LongToSize(dst_shape_[D_INDEX] - 1);
  const auto h_max = LongToSize(dst_shape_[H_INDEX] - 1);
  const auto w_max = LongToSize(dst_shape_[W_INDEX] - 1);
  const size_t d_index = D_INDEX - NC_LEN;
  const size_t h_index = H_INDEX - NC_LEN;
  const size_t w_index = W_INDEX - NC_LEN;
  const int64_t valid_d = kernel_[d_index] - padding_invalid_[d_index];
  const int64_t valid_h = kernel_[h_index] - padding_invalid_[h_index];
  const int64_t valid_w = kernel_[w_index] - padding_invalid_[w_index];
  const std::vector<int64_t> valid_kernel_array{kernel_[d_index] * kernel_[h_index] * kernel_[w_index],
                                                kernel_[d_index] * kernel_[h_index] * valid_w,
                                                kernel_[d_index] * valid_h * kernel_[w_index],
                                                kernel_[d_index] * valid_h * valid_w,
                                                valid_d * kernel_[h_index] * kernel_[w_index],
                                                valid_d * kernel_[h_index] * valid_w,
                                                valid_d * valid_h * kernel_[w_index],
                                                valid_d * valid_h * valid_w};
  const int base = 2;
  const int64_t kernel_size = std::accumulate(kernel_.begin(), kernel_.end(), int64_t(1), std::multiplies<int64_t>());
  CTask task = [&](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      for (size_t d = 0; d <= d_max; d++) {
        for (size_t h = 0; h <= h_max; h++) {
          for (size_t w = 0; w <= w_max; w++) {
            const char d_bound = d == d_max ? '1' : '0';
            const char h_bound = h == h_max ? '1' : '0';
            const char w_bound = w == w_max ? '1' : '0';
            const std::string bin{d_bound, h_bound, w_bound};
            int kernel_index = 0;
            try {
              kernel_index = std::stoi(bin, nullptr, base);
            } catch (const std::invalid_argument &e) {
              MS_LOG(EXCEPTION) << "invalid string to be cast to int!";
            } catch (const std::out_of_range &e) {
              MS_LOG(EXCEPTION) << "value is out of the range of int!";
            }
            const int64_t valid_kernel_size = valid_kernel_array[kernel_index];
            if (valid_kernel_size != kernel_size) {
              const size_t index =
                static_cast<size_t>(i * dst_shape_[D_INDEX] * dst_shape_[H_INDEX] * dst_shape_[W_INDEX] +
                                    d * dst_shape_[H_INDEX] * dst_shape_[W_INDEX] + h * dst_shape_[W_INDEX] + w);
              dst[index] =
                dst[index] * static_cast<T>(LongToFloat(kernel_size)) / static_cast<T>(LongToFloat(valid_kernel_size));
            }
          }
        }
      }
    }
  };
  ParallelLaunchAutoSearch(task, static_cast<size_t>(dst_shape_[N_INDEX] * dst_shape_[C_INDEX]), this,
                           &parallel_search_info_);
}

#ifdef USE_MS_THREADPOOL_FOR_DNNL
void PoolingGradCpuKernelMod::ExecuteForwardByMSThreadPool(const std::unordered_map<int, dnnl::memory> &arguments) {
  const size_t MAX_POW = 6;
  const size_t AVG_COUNT = 5;
  const size_t DIFF = 2;
  size_t current_pow = forward_parallel_info_.search_count / AVG_COUNT;
  int current_thread_nums = static_cast<int>(std::pow(2.0f, current_pow));
  auto mkl_pool = dynamic_cast<mkl_threadpool *>(mkl_threadpool_.get());
  if (current_pow >= MAX_POW) {
    int best_thread_nums = static_cast<int>(std::pow(2.0f, forward_parallel_info_.best_pow));
    mkl_pool->set_num_threads(best_thread_nums);
    MS_LOG(DEBUG) << "begin to invoke primitive::execute";
    primitive_forward_->execute(stream_, arguments);
    MS_LOG(DEBUG) << "end to invoke primitive::execute";
    return;
  }

  if (forward_parallel_info_.search_count % AVG_COUNT == 0) {
    forward_parallel_info_.tmp_sum_cost_time = 0;
  }
  double start_time = GetTime();
  mkl_pool->set_num_threads(current_thread_nums);
  MS_LOG(DEBUG) << "begin to invoke primitive::execute";
  primitive_forward_->execute(stream_, arguments);
  MS_LOG(DEBUG) << "end to invoke primitive::execute";
  double cost_time = GetTime() - start_time;
  forward_parallel_info_.tmp_sum_cost_time += cost_time;
  forward_parallel_info_.search_count++;
  if (forward_parallel_info_.search_count % AVG_COUNT == 0) {
    if (forward_parallel_info_.min_cost_time > forward_parallel_info_.tmp_sum_cost_time) {
      forward_parallel_info_.min_cost_time = forward_parallel_info_.tmp_sum_cost_time;
      forward_parallel_info_.best_pow = current_pow;
    } else if (current_pow - forward_parallel_info_.best_pow >= DIFF) {
      forward_parallel_info_.search_count = AVG_COUNT * MAX_POW;
    }
  }
}
#endif

void PoolingGradCpuKernelMod::ComputeMaxValueIndex(void *src, void *dst, void *work_array) {
  // Compute maxvalue index for pooling_backward_max.
  MS_LOG(INFO) << "Compute maxvalue index for " << kernel_name_;
  std::unordered_map<int, dnnl::memory> arguments;
  dnnl::memory src_mem = dnnl::memory(src_desc_, engine_, nullptr);
  dnnl::memory dst_mem = dnnl::memory(dst_desc_, engine_, nullptr);
  dnnl::memory work_mem = dnnl::memory(workspace_desc_, engine_, nullptr);
  src_mem.set_data_handle(src);
  dst_mem.set_data_handle(dst);
  work_mem.set_data_handle(work_array);
  arguments[DNNL_ARG_SRC] = src_mem;
  arguments[DNNL_ARG_DST] = dst_mem;
  arguments[DNNL_ARG_WORKSPACE] = work_mem;

#ifdef USE_MS_THREADPOOL_FOR_DNNL
  ExecuteForwardByMSThreadPool(arguments);
#else
  MS_LOG(DEBUG) << "begin to invoke primitive::execute";
  primitive_forward_->execute(stream_, arguments);
  MS_LOG(DEBUG) << "end to invoke primitive::execute";
#endif
  (void)stream_.wait();
}

bool PoolingGradCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                     const std::vector<kernel::KernelTensor *> &workspace,
                                     const std::vector<kernel::KernelTensor *> &outputs) {
  SetArgumentHandle(DNNL_ARG_DIFF_SRC, outputs[0]->device_ptr());

  // For pooling_max, get the workspace that store the max value indexes.
  if (algorithm_ == dnnl::algorithm::pooling_max) {
    SetArgumentHandle(DNNL_ARG_DIFF_DST, inputs[grad_index_]->device_ptr());
    CHECK_KERNEL_WORKSPACE_SIZE(workspace.size(), kMaxPoolingGradWorkSpaceNum, kernel_name_);
    ComputeMaxValueIndex(inputs[0]->device_ptr(), workspace[1]->device_ptr(), workspace[0]->device_ptr());
    SetArgumentHandle(DNNL_ARG_WORKSPACE, workspace[0]->device_ptr());
    ExecutePrimitive();
    return true;
  }
  if (dtype_ == kNumberTypeFloat32) {
    return LaunchKernel<float>(inputs, workspace, outputs);
  } else if (dtype_ == kNumberTypeFloat16) {
    return LaunchKernel<float16>(inputs, workspace, outputs);
  } else if (dtype_ == kNumberTypeFloat64) {
    return LaunchKernel<double>(inputs, workspace, outputs);
  } else {
    MS_LOG(ERROR) << "For '" << kernel_name_ << " error get " << TypeIdToType(dtype_)->ToString();
    return false;
  }
  return true;
}

template <typename T>
bool PoolingGradCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                           const std::vector<kernel::KernelTensor *> &workspace,
                                           const std::vector<kernel::KernelTensor *> &outputs) {
  // Copy data of inputs[grad_index_] to workspace to avoid input data being changed
  CHECK_KERNEL_WORKSPACE_SIZE(workspace.size(), kAvgPoolingGradWorkSpaceNum, kernel_name_);
  auto dst_work_addr = workspace[0]->device_ptr();
  auto cpy_ret =
    memcpy_s(dst_work_addr, workspace[0]->size(), inputs[grad_index_]->device_ptr(), inputs[grad_index_]->size());
  if (cpy_ret != EOK) {
    MS_LOG_ERROR << "For '" << kernel_name_ << "', input memcpy to workspace error!";
    return false;
  }
  SetArgumentHandle(DNNL_ARG_DIFF_DST, dst_work_addr);
  T *dst = reinterpret_cast<T *>(dst_work_addr);
  if (divisor_override_ != 0) {
    ReComputeDivisor(dst);
  } else {
    bool has_invalid_padding = std::any_of(padding_invalid_.begin(), padding_invalid_.end(),
                                           [](const int64_t &padding) { return padding != 0; });
    if (algorithm_ == dnnl::algorithm::pooling_avg_include_padding && has_invalid_padding) {
      EliminateInvalidPadding(dst);
    }
  }
  ExecutePrimitive();
  return true;
}

MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, AvgPoolGrad,
                                 []() { return std::make_shared<PoolingGradCpuKernelMod>(kAvgPoolGrad); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, AvgPool3DGrad,
                                 []() { return std::make_shared<PoolingGradCpuKernelMod>(kAvgPool3DGrad); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, MaxPoolGrad,
                                 []() { return std::make_shared<PoolingGradCpuKernelMod>(kMaxPoolGrad); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, MaxPool3DGrad,
                                 []() { return std::make_shared<PoolingGradCpuKernelMod>(kMaxPool3DGrad); });
}  // namespace kernel
}  // namespace mindspore
