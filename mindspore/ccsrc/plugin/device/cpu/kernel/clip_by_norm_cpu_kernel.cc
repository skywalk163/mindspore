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

#include "plugin/device/cpu/kernel/clip_by_norm_cpu_kernel.h"

#include <limits>
#include <algorithm>

namespace mindspore {
namespace kernel {
namespace {
std::vector<std::pair<TypeId, TypeId>> supported_data_type{{kNumberTypeFloat32, kNumberTypeFloat32},
                                                           {kNumberTypeFloat32, kNumberTypeFloat16},
                                                           {kNumberTypeFloat16, kNumberTypeFloat32},
                                                           {kNumberTypeFloat16, kNumberTypeFloat16}};

static std::vector<KernelAttr> clip_by_norm_io_attr_list = {
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32)},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat32)},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32)},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat32)}};

size_t GetDeviceSize(const std::vector<KernelTensor *> &addr_list, size_t index) {
  if (index >= addr_list.size()) {
    MS_LOG(EXCEPTION) << "Address index(" << index << ") out of range(" << addr_list.size() << ")";
  }
  if (addr_list[index] == nullptr || addr_list[index]->size() == 0) {
    MS_LOG(EXCEPTION) << "index(" << index << ") address is `nullptr` or its size is zero.";
  }
  return addr_list[index]->size();
}
}  // namespace

bool ClipByNormCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &outputs) {
  // Get C++ primitive and kernel_name
  // Check whether current input and output data types are valid.
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  if (!MatchKernelAttr(kernel_attr, GetOpSupport()).first) {
    MS_LOG(ERROR) << "For `" << kernel_name_ << "`, its input or output data types are not supported.";
    return false;
  }
  data_type_ = std::make_pair(kernel_attr.GetInputAttr(0).dtype, kernel_attr.GetInputAttr(1).dtype);
  return true;
}

int ClipByNormCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  ResetResource();
  // Init basic variables
  InitIOShape(inputs, outputs);
  // Init the `l2_norm` reduce shape according to `axis`
  l2_norm_output_shape_ = x_shape_;
  InitAxisAndEpsilon();
  (void)std::for_each(axis_.begin(), axis_.end(), [this](const size_t &idx) { l2_norm_output_shape_[idx] = 1; });
  InitSizeLists();
  return KRET_OK;
}

bool ClipByNormCpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &workspace,
                                    const std::vector<KernelTensor *> &outputs) {
  constexpr size_t input_num_expected = 2;
  constexpr size_t workspace_num_expected = 2;
  MS_EXCEPTION_IF_CHECK_FAIL(inputs.size() == input_num_expected, "The input addr number of ClipByNorm should be 2.");
  MS_EXCEPTION_IF_CHECK_FAIL(workspace.size() == workspace_num_expected,
                             "The workspace addr number of ClipByNorm should be 2.");
  MS_EXCEPTION_IF_CHECK_FAIL(outputs.size() == 1, "The output addr number of ClipByNorm should be 1.");

  if (data_type_.first == kNumberTypeFloat32 && data_type_.second == kNumberTypeFloat32) {
    LaunchFunc<float, float>(inputs, workspace, outputs);
  } else if (data_type_.first == kNumberTypeFloat32 && data_type_.second == kNumberTypeFloat16) {
    LaunchFunc<float, float16>(inputs, workspace, outputs);
  } else if (data_type_.first == kNumberTypeFloat16 && data_type_.second == kNumberTypeFloat32) {
    LaunchFunc<float16, float>(inputs, workspace, outputs);
  } else if (data_type_.first == kNumberTypeFloat16 && data_type_.second == kNumberTypeFloat16) {
    LaunchFunc<float16, float16>(inputs, workspace, outputs);
  } else {
    MS_LOG(WARNING) << "The data type of input args is invalid.";
    return false;
  }
  return true;
}

std::vector<KernelAttr> ClipByNormCpuKernelMod::GetOpSupport() { return clip_by_norm_io_attr_list; }

void ClipByNormCpuKernelMod::ResetResource() {
  epsilon_ = 0.000001f;
  x_dim_ = 0;
  x_shape_.clear();
  clip_norm_shape_.clear();
  l2_norm_output_shape_.clear();
  output_shape_.clear();
  output_size_list_.clear();
  workspace_size_list_.clear();
}

void ClipByNormCpuKernelMod::InitIOShape(const std::vector<KernelTensor *> &inputs,
                                         const std::vector<KernelTensor *> &outputs) {
  constexpr size_t input_num_expected = 2;
  MS_EXCEPTION_IF_CHECK_FAIL(inputs.size() == input_num_expected, "The size of input tensors should be 2.");
  MS_EXCEPTION_IF_CHECK_FAIL(outputs.size() == 1, "The size of output tensors should be 1.");
  // Init `input_x` shape
  MS_EXCEPTION_IF_NULL(inputs[0]);
  x_shape_ = inputs[0]->GetShapeVector();
  if (!IsValidShape(x_shape_)) {
    MS_EXCEPTION(ValueError) << "For " << kernel_name_ << ", x_shape not supports dynamic shape.";
  }
  x_dim_ = x_shape_.size();
  // Init 'clip_norm' shape
  MS_EXCEPTION_IF_NULL(inputs[1]);
  clip_norm_shape_ = inputs[1]->GetShapeVector();
  if (!IsValidShape(clip_norm_shape_)) {
    MS_EXCEPTION(ValueError) << "For " << kernel_name_ << ", clip_norm_shape not support dynamic shape.";
  }
  // Init output shape
  MS_EXCEPTION_IF_NULL(outputs[0]);
  output_shape_ = outputs[0]->GetShapeVector();
  if (!IsValidShape(output_shape_)) {
    MS_EXCEPTION(ValueError) << "For " << kernel_name_ << ", output_shape not supports dynamic shape.";
  }
  MS_EXCEPTION_IF_CHECK_FAIL(output_shape_ == x_shape_, "Output shape should be same with input x shape.");
}

void ClipByNormCpuKernelMod::InitAxisAndEpsilon() {
  epsilon_ = 0.000001f;
  // Get axis vector from attribute
  auto axis_value = primitive_->GetAttr(kAttrAxis);
  MS_EXCEPTION_IF_NULL(axis_value);
  std::vector<int64_t> temp_axis;
  if (axis_value->isa<ValueSequence>()) {
    temp_axis = GetValue<std::vector<int64_t>>(axis_value);
  } else if (axis_value->isa<Int64Imm>()) {
    (void)temp_axis.emplace_back(GetValue<int64_t>(axis_value));
  } else {
    MS_EXCEPTION(TypeError) << "For `" << kernel_name_ << "`, the type of attribute `axis` is invalid.";
  }
  // Init `axis_`
  axis_.clear();
  if (temp_axis.empty()) {
    for (size_t i = 0; i < x_dim_; ++i) {
      (void)axis_.emplace_back(i);  // Reduce for all dimensions.
    }
  } else {  // Convert negative `axis` to positive `axis` and keep number unique
    int64_t temp_x_dim = SizeToLong(x_dim_);
    (void)std::for_each(temp_axis.begin(), temp_axis.end(), [this, &temp_x_dim](const int64_t &value) {
      value < 0 ? axis_.emplace_back(LongToSize(value + temp_x_dim)) : axis_.emplace_back(LongToSize(value));
    });
    std::sort(axis_.begin(), axis_.end());
    (void)axis_.erase(std::unique(axis_.begin(), axis_.end()), axis_.end());
  }
}

void ClipByNormCpuKernelMod::InitSizeLists() {
  size_t x_type_size = (data_type_.first == kNumberTypeFloat32) ? sizeof(float) : sizeof(float16);
  // Init input size list
  auto x_size = std::accumulate(x_shape_.begin(), x_shape_.end(), x_type_size, std::multiplies<size_t>());
  x_size = std::max(x_size, x_type_size);
  size_t clip_norm_type_size = GetTypeByte(TypeIdToType(data_type_.second));
  size_t clip_norm_size = SizeOf(clip_norm_shape_);
  clip_norm_size = std::max(clip_norm_size, clip_norm_type_size);
  // Init workspace size list
  size_t float_type_size = sizeof(float);
  auto l2_norm_out_size = float_type_size * SizeOf(l2_norm_output_shape_);
  l2_norm_out_size = std::max(l2_norm_out_size, float_type_size);
  // This workspace size used for l2_norm calculation
  (void)workspace_size_list_.emplace_back(l2_norm_out_size);
  // This workspace size used for `x/l2_norm(x)` calculation
  (void)workspace_size_list_.emplace_back((x_size / x_type_size) * float_type_size);
  // Init output size list
  auto output_size = float_type_size * SizeOf(output_shape_);
  output_size = std::max(output_size, float_type_size);
  (void)output_size_list_.emplace_back(output_size);

  // Calculate l2norm iter
  // Calculate transpose axes and stride
  size_t j = 0;
  size_t k = 0;
  stride_ = 1;
  size_t axis_size = axis_.size();
  std::vector<size_t> axes(x_shape_.size());
  for (size_t i = 0; i < x_dim_; ++i) {
    if (j == axis_size || i != axis_[j]) {
      axes[k] = i;
      ++k;
    } else {
      stride_ *= LongToSize(x_shape_[i]);
      ++j;
    }
  }
  for (const auto &v : axis_) {
    axes[k] = v;
    ++k;
  }
  // Calculate transpose shape
  ShapeVector transpose_shape(x_shape_.size());
  for (size_t i = 0; i < x_dim_; ++i) {
    transpose_shape[i] = x_shape_[axes[i]];
  }
  auto l2_norm_out_ele = l2_norm_out_size / sizeof(float);
  l2_norm_index_.clear();
  l2_norm_index_.resize(l2_norm_out_ele * stride_);
  TransposeIterator l2n_base_iter(std::move(transpose_shape), std::move(axes), x_shape_);
  l2n_base_iter.SetPos(0);
  for (size_t i = 0; i < l2_norm_index_.size(); ++i) {
    l2_norm_index_[i] = l2n_base_iter.GetPos();
    l2n_base_iter.GenNextPos();
  }

  // Cal Div iter
  if (!x_shape_.empty()) {
    BroadcastIterator div_base_iter(x_shape_, l2_norm_output_shape_, x_shape_);
    div_base_iter.SetPos(0);
    div_index1_.clear();
    div_index2_.clear();
    auto div_ele = x_size / x_type_size;
    div_index1_.resize(div_ele);
    div_index2_.resize(div_ele);
    for (size_t i = 0; i < div_ele; ++i) {
      div_index1_[i] = div_base_iter.GetInputPosA();
      div_index2_[i] = div_base_iter.GetInputPosB();
      div_base_iter.GenNextPos();
    }
  }

  // Cal Mul iter
  if (!x_shape_.empty()) {
    BroadcastIterator mul_base_iter(x_shape_, clip_norm_shape_, output_shape_);
    mul_base_iter.SetPos(0);
    mul_index1_.clear();
    mul_index2_.clear();
    auto output_ele = output_size / sizeof(float);
    mul_index1_.resize(output_ele);
    mul_index2_.resize(output_ele);
    for (size_t i = 0; i < output_ele; ++i) {
      mul_index1_[i] = mul_base_iter.GetInputPosA();
      mul_index2_[i] = mul_base_iter.GetInputPosB();
      mul_base_iter.GenNextPos();
    }
  }
}

template <typename T, typename S>
void ClipByNormCpuKernelMod::LaunchFunc(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &workspace,
                                        const std::vector<KernelTensor *> &outputs) {
  // Launch `l2_norm(x)` calculate function
  T *x_addr = GetDeviceAddress<T>(inputs, 0);
  float *l2_norm_output_addr = GetDeviceAddress<float>(workspace, 0);
  MS_EXCEPTION_IF_NULL(x_addr);
  MS_EXCEPTION_IF_NULL(l2_norm_output_addr);
  size_t l2_norm_output_size = GetDeviceSize(workspace, 0);
  L2NormLaunch<T>(x_addr, l2_norm_output_addr, l2_norm_output_size);
  // Launch `x / l2_norm(x)` calculate function
  float *div_output_addr = GetDeviceAddress<float>(workspace, 1);
  MS_EXCEPTION_IF_NULL(div_output_addr);
  size_t div_output_size = GetDeviceSize(workspace, 1);
  DivLaunch<T>(x_addr, l2_norm_output_addr, div_output_addr, div_output_size);
  // Launch `clip_norm * (x / l2_norm(x))` calculate function and chose bigger one compared with `x`
  S *clip_norm_addr = GetDeviceAddress<S>(inputs, 1);
  float *output_addr = GetDeviceAddress<float>(outputs, 0);
  MS_EXCEPTION_IF_NULL(clip_norm_addr);
  MS_EXCEPTION_IF_NULL(output_addr);
  size_t output_size = GetDeviceSize(outputs, 0);
  ClipNormMulAndCmpLaunch<T, S>(x_addr, div_output_addr, clip_norm_addr, output_addr, output_size);
}

template <typename T>
void ClipByNormCpuKernelMod::L2NormLaunch(const T *x_addr, float *l2_norm_output_addr, size_t l2_norm_output_size) {
  if (l2_norm_index_.size() == 0) {
    MS_LOG(EXCEPTION) << "l2_norm_index_ size is 0, bad input size.";
  }
  // Run `l2_norm(x)` calculation
  auto task = [&](size_t start, size_t end) {
    float zero = static_cast<float>(0);
    float temp = zero;
    float denominator = zero;
    for (size_t i = start; i < end; ++i) {
      denominator = static_cast<float>(x_addr[l2_norm_index_[i]]);
      denominator = denominator * denominator;
      for (size_t j = 1; j < stride_; ++j) {
        temp = static_cast<float>(x_addr[l2_norm_index_[i + j]]);
        denominator += (temp * temp);
      }
      denominator = (denominator > epsilon_) ? denominator : epsilon_;
      l2_norm_output_addr[i] = sqrt(denominator);
    }
  };
  ParallelLaunchAutoSearch(task, l2_norm_output_size / sizeof(float), this, &parallel_search_info_);
}

template <typename T>
void ClipByNormCpuKernelMod::DivLaunch(const T *x_addr, const float *l2_norm_output_addr, float *div_output_addr,
                                       size_t div_output_size) {
  // Run div calculation
  if (x_shape_.empty()) {  // The input x is a scalar tensor
    div_output_addr[0] = static_cast<float>(x_addr[0]) / l2_norm_output_addr[0];
    return;
  }
  auto task = [&](size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
      float zero = static_cast<float>(0);
      float dividend = static_cast<float>(x_addr[div_index1_[i]]);
      float divisor = l2_norm_output_addr[div_index2_[i]];
      if (divisor == zero) {
        if (dividend == zero) {
          div_output_addr[i] = std::numeric_limits<float>::quiet_NaN();
          continue;
        }
        if (std::numeric_limits<float>::has_infinity) {
          div_output_addr[i] =
            dividend > zero ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity();
        } else {
          div_output_addr[i] = dividend > zero ? std::numeric_limits<float>::max() : std::numeric_limits<float>::min();
        }
        continue;
      }
      div_output_addr[i] = dividend / divisor;
    }
  };
  ParallelLaunchAutoSearch(task, div_output_size / sizeof(float), this, &parallel_search_info_div_);
}

template <typename T, typename S>
void ClipByNormCpuKernelMod::ClipNormMulAndCmpLaunch(const T *x_addr, const float *div_output_addr,
                                                     const S *clip_norm_addr, float *output_addr, size_t output_size) {
  if (x_shape_.empty()) {  // The input x is a scalar tensor
    float mul_output = div_output_addr[0] * static_cast<float>(clip_norm_addr[0]);
    float x = static_cast<float>(x_addr[0]);
    if (x * mul_output >= 0) {
      output_addr[0] = (mul_output * mul_output) > (x * x) ? x : mul_output;
    } else {
      output_addr[0] = mul_output;
    }
    return;
  }
  auto task = [&](size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
      float div_out = div_output_addr[mul_index1_[i]];
      float clip_norm = static_cast<float>(clip_norm_addr[mul_index2_[i]]);
      float mul_output = clip_norm * div_out;
      float x = static_cast<float>(x_addr[mul_index1_[i]]);
      if (x * mul_output >= 0) {
        output_addr[i] = (mul_output * mul_output) > (x * x) ? x : mul_output;
      } else {
        output_addr[i] = mul_output;
      }
    }
  };
  ParallelLaunchAutoSearch(task, output_size / sizeof(float), this, &parallel_search_info_mul_);
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, ClipByNorm, ClipByNormCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
