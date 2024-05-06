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

#include "plugin/device/cpu/kernel/diagonal_cpu_kernel.h"
#include <algorithm>
#include <utility>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kDiagonalInputsNum = 4;
constexpr size_t kDiagonalOutputsNum = 1;
constexpr int64_t N2 = 2;
template <typename T>
class PositionIterator {
 public:
  PositionIterator() {}
  ~PositionIterator() {}
  PositionIterator(std::vector<T> stt, std::vector<T> sh) {
    MS_EXCEPTION_IF_CHECK_FAIL(stt.size() == sh.size(), "Inputs of PositionIterator must have same size.");
    for (unsigned int i = 0; i < sh.size(); i++) {
      MS_EXCEPTION_IF_CHECK_FAIL(sh[i] > 0, "The elements of input [sh] must be positive.");
      MS_EXCEPTION_IF_CHECK_FAIL(
        stt[i] < sh[i], "Each element of input [stt] must be less than the corresponding element of input [sh].");
    }
    pos_ = stt;
    shape_ = sh;
  }
  PositionIterator operator++() {
    pos_[shape_.size() - 1] += 1;
    for (unsigned int i = shape_.size() - 1; i > 0; i--) {
      if (pos_[i] / shape_[i] != 0) {
        pos_[i - 1] += pos_[i] / shape_[i];
        pos_[i] = pos_[i] % shape_[i];
      }
    }
    return *this;
  }

  bool is_end() const {
    if (pos_.empty() || shape_.empty()) {
      return true;
    }
    if (pos_[0] != shape_[0]) {
      return false;
    }
    return true;
  }

  std::vector<T> get_pos() { return pos_; }

  std::vector<T> get_shape() { return shape_; }

 private:
  std::vector<T> pos_;
  std::vector<T> shape_;
};

template <typename T>
T mul_sum(std::vector<T> v1, std::vector<T> v2) {
  T output = 0;
  for (unsigned int i = 0; i < v1.size(); i++) {
    output += v1[i] * v2[i];
  }
  return output;
}

template <typename T>
std::vector<T> construct_stride(std::vector<T> t_shape) {
  std::vector<T> t_stride(t_shape.size(), 1);
  int initial = 1;
  for (unsigned int i = t_shape.size(); i > 0; i--) {
    t_stride[i - 1] = initial;
    initial = initial * t_shape[i - 1];
  }
  return t_stride;
}
}  // namespace

bool DiagonalCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (inputs.empty() || outputs.empty()) {
    MS_LOG(ERROR) << "For 'Diagonal', it got empty inputs or outputs, which is invalid.";
    return false;
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For 'Diagonal', the data type of input must be float32 or double, but got: " << kernel_attr
                  << ".";
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int DiagonalCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kDiagonalInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kDiagonalOutputsNum, kernel_name_);
  offset_ = inputs[kIndex1]->GetValueWithCheck<int64_t>();
  dim1_ = inputs[kIndex2]->GetValueWithCheck<int64_t>();
  dim2_ = inputs[kIndex3]->GetValueWithCheck<int64_t>();
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  input_shape = inputs[0]->GetShapeVector();
  int64_t input_size = input_shape.size();
  if (input_size < N2) {
    MS_LOG(ERROR) << "For 'Diagonal', input must be at least 2-dimensional, but got : " << input_size << ".";
    return false;
  }
  if (dim1_ > input_size - 1 || dim1_ < -input_size) {
    MS_LOG(ERROR) << "For 'Diagonal', dim1 should be in range of [" << -input_size << "," << (input_size - 1)
                  << "], but got : " << dim1_ << ".";
    return false;
  }
  if (dim2_ > input_size - 1 || dim2_ < -input_size) {
    MS_LOG(ERROR) << "For 'Diagonal', dim2 should be in range of [" << -input_size << "," << (input_size - 1)
                  << "], but got : " << dim2_ << ".";
    return false;
  }
  dim1_ = (dim1_ < 0) ? dim1_ + input_size : dim1_;
  dim2_ = (dim2_ < 0) ? dim2_ + input_size : dim2_;
  if (dim1_ == dim2_) {
    MS_LOG(ERROR) << "For 'Diagonal', dim1 and dim2 cannot be identical, but got : dim1 =" << dim1_
                  << " and dim2 = " << dim2_ << ".";
  }
  if (offset_ >= 0) {
    dsize = std::max<int64_t>(std::min(input_shape[dim1_], input_shape[dim2_] - offset_), 0);
  } else {
    dsize = std::max<int64_t>(std::min(input_shape[dim1_] + offset_, input_shape[dim2_]), 0);
  }
  return KRET_OK;
}

template <typename T>
bool DiagonalCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kDiagonalInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kDiagonalOutputsNum, kernel_name_);

  const T *input = GetDeviceAddress<T>(inputs, kIndex0);
  MS_EXCEPTION_IF_NULL(input);
  T *output = GetDeviceAddress<T>(outputs, kIndex0);
  if (dsize > 0) {
    MS_EXCEPTION_IF_NULL(output);
  }
  // Get some information of input
  size_t input_size = input_shape.size();
  // Compute
  std::vector<int64_t> x_stride = construct_stride<int64_t>(input_shape);
  if (input_size > N2) {
    // set the vx_shape and vx_stride
    std::vector<int64_t> vx_shape, vx_stride;
    for (unsigned int tmp_dim = 0; tmp_dim < input_size; tmp_dim++) {
      if (tmp_dim != dim1_ && tmp_dim != dim2_) {
        vx_shape.push_back(input_shape[tmp_dim]);
        vx_stride.push_back(x_stride[tmp_dim]);
      }
    }
    // set the y_shape, y_stride, vy_stride
    std::vector<int64_t> y_shape = vx_shape;
    y_shape.push_back(dsize);
    std::vector<int64_t> y_stride = construct_stride<int64_t>(y_shape);
    std::vector<int64_t> vy_stride = y_stride;
    vy_stride.pop_back();
    // diagonal
    std::vector<int64_t> v_start(vx_shape.size(), 0);
    for (PositionIterator<int64_t> myiter(v_start, vx_shape); !myiter.is_end(); (void)++myiter) {
      auto p = myiter.get_pos();
      int64_t base_pos1 = mul_sum<int64_t>(p, vx_stride);
      int64_t outbase_pos = mul_sum<int64_t>(p, vy_stride);
      for (int i = 0; i < dsize; i++) {
        int64_t base_pos2 = i * (x_stride[dim1_] + x_stride[dim2_]);
        int64_t arr[N2] = {x_stride[dim1_], x_stride[dim2_]};
        output[outbase_pos + i] = offset_ >= 0 ? input[base_pos1 + base_pos2 + offset_ * arr[1]]
                                               : input[base_pos1 + base_pos2 - offset_ * arr[0]];
      }
    }
  } else {
    for (int i = 0; i < dsize; i++) {
      int64_t base_pos = i * (x_stride[dim1_] + x_stride[dim2_]);
      int64_t arr[N2] = {x_stride[dim1_], x_stride[dim2_]};
      output[i] = offset_ >= 0 ? input[base_pos + offset_ * arr[1]] : input[base_pos - offset_ * arr[0]];
    }
  }
  return true;
}

std::vector<std::pair<KernelAttr, DiagonalCpuKernelMod::DiagonalLaunchFunc>> DiagonalCpuKernelMod::func_list_ = {
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat32)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &DiagonalCpuKernelMod::LaunchKernel<float>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &DiagonalCpuKernelMod::LaunchKernel<double>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeBool)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &DiagonalCpuKernelMod::LaunchKernel<bool>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeFloat16)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &DiagonalCpuKernelMod::LaunchKernel<float16>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeInt8)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &DiagonalCpuKernelMod::LaunchKernel<int8_t>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeInt16)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &DiagonalCpuKernelMod::LaunchKernel<int16_t>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeInt32)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &DiagonalCpuKernelMod::LaunchKernel<int32_t>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &DiagonalCpuKernelMod::LaunchKernel<int64_t>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeUInt8)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &DiagonalCpuKernelMod::LaunchKernel<uint8_t>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeUInt16)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &DiagonalCpuKernelMod::LaunchKernel<uint16_t>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeUInt32)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &DiagonalCpuKernelMod::LaunchKernel<uint32_t>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeUInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &DiagonalCpuKernelMod::LaunchKernel<uint64_t>}};

std::vector<KernelAttr> DiagonalCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, DiagonalLaunchFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Diagonal, DiagonalCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
