/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/unique_consecutive_cpu_kernel.h"
#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <complex>
#include <functional>
#include "include/common/thread_pool.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "mindspore/core/ops/unique_consecutive.h"

namespace mindspore {
namespace kernel {
namespace {
// Value check constant
constexpr size_t kUniqueConsecutiveInputsNum = 1;
constexpr size_t kUniqueConsecutiveOutputsNum = 3;
// Attr default value constant
constexpr int64_t kNone = 1000;

template <typename T>
class PositionIterator {
 public:
  PositionIterator() {}
  ~PositionIterator() {}
  PositionIterator(std::vector<T> stt, std::vector<T> sh) {
    if (stt.size() != sh.size()) {
      PositionIterator();
    } else {
      for (unsigned int i = 0; i < sh.size(); i++) {
        if (stt[i] >= sh[i]) {
          PositionIterator();
        }
      }
      pos_ = stt;
      shape_ = sh;
    }
  }
  const PositionIterator operator++() {
    pos_[shape_.size() - static_cast<size_t>(1)] += 1;
    for (size_t i = shape_.size() - static_cast<size_t>(1); i > static_cast<size_t>(0); i--) {
      if (pos_[i] / shape_[i] != 0) {
        pos_[i - 1] += pos_[i] / shape_[i];
        pos_[i] = pos_[i] % shape_[i];
      }
    }
    return *this;
  }
  bool End() {
    if (pos_[0] != shape_[0]) {
      return false;
    }
    return true;
  }
  std::vector<T> GetPos() { return pos_; }
  std::vector<T> GetShape() { return shape_; }

 private:
  std::vector<T> pos_;
  std::vector<T> shape_;
};

template <typename T>
std::vector<T> ConstructStride(std::vector<T> t_shape) {
  std::vector<T> t_stride(t_shape.size(), 1);
  int initial = 1;
  for (size_t i = t_shape.size(); i > 0; i--) {
    t_stride[i - 1] = initial;
    initial = initial * static_cast<int>(t_shape[i - static_cast<size_t>(1)]);
  }
  return t_stride;
}

template <typename T>
T MulSum(std::vector<T> v1, std::vector<T> v2) {
  T mul_sum = 0;
  for (unsigned int i = 0; i < v1.size(); i++) {
    mul_sum += v1[i] * v2[i];
  }
  return mul_sum;
}

template <typename T1>
std::vector<std::vector<T1>> ReshapeInput(std::vector<int64_t> input_shape_, int32_t axis, T1 *x_dataptr) {
  int64_t dim0 = input_shape_[static_cast<size_t>(axis)];
  std::vector<int64_t> input_stride = ConstructStride<int64_t>(input_shape_);
  std::vector<int64_t> v_shape = input_shape_;
  (void)v_shape.erase(v_shape.begin() + axis);
  std::vector<int64_t> v_start(v_shape.size(), 0);
  std::vector<int64_t> v_stride = input_stride;
  (void)v_stride.erase(v_stride.begin() + axis);
  std::vector<std::vector<T1>> data_;
  for (int64_t i = 0; i < dim0; i++) {
    std::vector<T1> tmp_v1;
    for (PositionIterator<int64_t> mit(v_start, v_shape); !mit.End(); (void)++mit) {
      auto pos = mit.GetPos();
      tmp_v1.push_back(
        x_dataptr[static_cast<size_t>(MulSum<int64_t>(pos, v_stride) + i * input_stride[static_cast<size_t>(axis)])]);
    }
    data_.push_back(tmp_v1);
  }
  return data_;
}

template <typename T1>
void OutputYSet(const std::vector<int64_t> &y_shape_, const std::vector<int64_t> &input_shape_, int32_t axis,
                T1 *y_dataptr, std::vector<std::vector<T1>> out_data_) {
  std::vector<int64_t> y_stride = ConstructStride<int64_t>(y_shape_);
  std::vector<int64_t> y_v_shape = y_shape_;
  (void)y_v_shape.erase(y_v_shape.begin() + axis);
  std::vector<int64_t> y_v_start(y_v_shape.size(), 0);
  std::vector<int64_t> y_v_stride = y_stride;
  (void)y_v_stride.erase(y_v_stride.begin() + axis);
  std::vector<int64_t> v_shape = input_shape_;
  (void)v_shape.erase(v_shape.begin() + axis);
  std::vector<int64_t> trans_stride = ConstructStride<int64_t>(v_shape);
  int64_t size0 = static_cast<int64_t>(out_data_.size());
  for (int64_t i = 0; i < size0; i++) {
    auto tmp_v = out_data_[static_cast<size_t>(i)];
    for (PositionIterator<int64_t> mit(y_v_start, y_v_shape); !mit.End(); (void)++mit) {
      auto pos = mit.GetPos();
      y_dataptr[static_cast<size_t>(MulSum<int64_t>(pos, y_v_stride) + i * y_stride[axis])] =
        tmp_v[static_cast<size_t>(MulSum<int64_t>(pos, trans_stride))];
    }
  }
}
}  // namespace

bool UniqueConsecutiveCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                         const std::vector<KernelTensor *> &outputs) {
  if (!MatchKernelFunc(kernel_name_, inputs, outputs)) {
    return false;
  }
  // Get attrs from primitive.
  auto axis_ptr = primitive_->GetAttr("axis");
  return_idx_ = GetValue<bool>(primitive_->GetAttr("return_idx"));
  return_counts_ = GetValue<bool>(primitive_->GetAttr("return_counts"));
  // Get input shape
  if (axis_ptr == nullptr || GetValue<int64_t>(axis_ptr) == kNone) {
    axis_ = kNone;
  } else {
    axis_ = GetValue<int64_t>(axis_ptr);
  }
  return true;
}

int UniqueConsecutiveCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                          const std::vector<KernelTensor *> &outputs) {
  auto ret = KernelMod::Resize(inputs, outputs);
  input_shape_ = inputs[0]->GetShapeVector();
  int64_t input_size = SizeToLong(input_shape_.size());
  axis_ = axis_ < 0 ? (axis_ + input_size) : axis_;
  return ret;
}

template <typename T1, typename T2>
void UniqueConsecutiveCpuKernelMod::UniqueConsecutiveNone(const std::vector<KernelTensor *> &inputs,
                                                          const std::vector<KernelTensor *> &outputs) {
  // Get the input and output
  const T1 *input_x = GetDeviceAddress<T1>(inputs, kIndex0);
  T1 *output_y = GetDeviceAddress<T1>(outputs, kIndex0);
  T2 *output_idx = GetDeviceAddress<T2>(outputs, kIndex1);
  T2 *output_count = GetDeviceAddress<T2>(outputs, kIndex2);
  MS_EXCEPTION_IF_NULL(input_x);
  MS_EXCEPTION_IF_NULL(output_y);
  MS_EXCEPTION_IF_NULL(output_count);
  int64_t input_total = std::accumulate(input_shape_.begin(), input_shape_.end(), 1, std::multiplies<int64_t>());
  if (input_total > 0) {
    *output_y = *input_x;
    T1 *p = output_y;
    T2 *q = output_count;
    T2 last = 0;
    for (T2 i = 0; i < input_total; i++) {
      if (static_cast<T1>(input_x[i]) != static_cast<T1>(*p)) {
        *(++p) = input_x[i];
        if (return_counts_) {
          *(q++) = i - last;
        }
        last = i;
      }
      if (return_idx_) {
        output_idx[i] = static_cast<T2>(p - output_y);
      }
    }
    if (return_counts_) {
      *q = input_total - last;
    }
    // Set the shape of output and count, the idx has the same shape of input
    output_shape_.push_back((p - output_y) + 1);
    if (return_idx_) {
      idx_shape_ = input_shape_;
    } else {
      idx_shape_.clear();
      idx_shape_.push_back(0);
    }
    if (return_counts_) {
      count_shape_ = output_shape_;
    } else {
      count_shape_.clear();
      count_shape_.push_back(0);
    }
  } else {
    output_shape_.push_back(0);
    if (return_idx_) {
      idx_shape_ = input_shape_;
    } else {
      idx_shape_.clear();
      idx_shape_.push_back(0);
    }
    if (return_counts_) {
      count_shape_ = input_shape_;
    } else {
      count_shape_.clear();
      count_shape_.push_back(0);
    }
  }
}

template <typename T1, typename T2>
void UniqueConsecutiveCpuKernelMod::UniqueConsecutiveDim(const std::vector<KernelTensor *> &inputs,
                                                         const std::vector<KernelTensor *> &outputs) {
  // Get the inuput and output
  T1 *input_x = GetDeviceAddress<T1>(inputs, kIndex0);
  T1 *output_y = GetDeviceAddress<T1>(outputs, kIndex0);
  T2 *output_idx = GetDeviceAddress<T2>(outputs, kIndex1);
  T2 *output_count = GetDeviceAddress<T2>(outputs, kIndex2);
  MS_EXCEPTION_IF_NULL(output_count);
  auto num_zero_dims = std::count(input_shape_.begin(), input_shape_.end(), 0);
  int64_t dim0 = input_shape_[static_cast<size_t>(axis_)];
  // Set the idx shape
  if (return_idx_) {
    idx_shape_.clear();
    idx_shape_.push_back(dim0);
  } else {
    idx_shape_.clear();
    idx_shape_.push_back(0);
  }
  // Some check
  if (dim0 == 0) {
    if (num_zero_dims != 1) {
      MS_LOG(EXCEPTION)
        << "For 'UniqueConsecutive', the number of zero sized dimensions > 1, so unique cannot be applied.";
    } else {
      output_shape_.push_back(0);
      count_shape_.push_back(0);
      return;
    }
  }
  if (num_zero_dims != 0) {
    MS_LOG(EXCEPTION) << "For 'UniqueConsecutive', there are 0 sized dimensions, and they aren't selected by 'axis', "
                         "so unique cannot be applied.";
  }
  // If the input is 1D, return UniqueConsecutiveNone
  if (input_shape_.size() != 1) {
    std::vector<std::vector<T1>> data_ = ReshapeInput<T1>(input_shape_, axis_, input_x);
    std::vector<std::vector<T1>> out_data_;
    out_data_.push_back(data_[0]);
    auto p = data_[0];
    T2 *q = output_count;
    T2 last = 0;
    for (size_t i = 0; i < static_cast<size_t>(dim0); i++) {
      if (!std::equal(data_[i].begin(), data_[i].end(), p.begin())) {
        p = data_[i];
        out_data_.push_back(data_[i]);
        if (return_counts_) {
          *(q++) = static_cast<T2>(i) - last;
        }
        last = static_cast<T2>(i);
      }
      if (return_idx_) {
        output_idx[i] = static_cast<T2>(static_cast<int32_t>(out_data_.size()) - 1);
      }
    }
    if (return_counts_) {
      *q = static_cast<T2>(dim0) - last;
    }
    output_shape_ = input_shape_;
    output_shape_[static_cast<size_t>(axis_)] = static_cast<int64_t>(out_data_.size());
    OutputYSet(output_shape_, input_shape_, axis_, output_y, out_data_);
    // Set the output and count shape
    if (return_counts_) {
      count_shape_.clear();
      count_shape_.push_back(out_data_.size());
    } else {
      count_shape_.clear();
      count_shape_.push_back(0);
    }
  } else {
    return UniqueConsecutiveNone<T1, T2>(inputs, outputs);
  }
}

template <typename T1, typename T2>
bool UniqueConsecutiveCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                                 const std::vector<KernelTensor *> &,
                                                 const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kUniqueConsecutiveInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kUniqueConsecutiveOutputsNum, kernel_name_);
  output_shape_.clear();
  idx_shape_.clear();
  count_shape_.clear();
  if (axis_ == kNone) {
    UniqueConsecutiveNone<T1, T2>(inputs, outputs);
  } else {
    UniqueConsecutiveDim<T1, T2>(inputs, outputs);
  }

  return true;
}

void UniqueConsecutiveCpuKernelMod::UpdateOutputShapeAndSize(const std::vector<KernelTensor *> &inputs,
                                                             const std::vector<KernelTensor *> &outputs) {
  // Update output shape and size
  outputs[kIndex0]->SetShapeVector(output_shape_);
  auto ele_size =
    LongToSize(std::accumulate(output_shape_.begin(), output_shape_.end(), 1, std::multiplies<int64_t>()));
  outputs[kIndex0]->set_size(ele_size * UnitSizeInBytes(outputs[kIndex0]->dtype_id()));
  outputs[kIndex1]->SetShapeVector(idx_shape_);
  ele_size = LongToSize(std::accumulate(idx_shape_.begin(), idx_shape_.end(), 1, std::multiplies<int64_t>()));
  outputs[kIndex1]->set_size(ele_size * UnitSizeInBytes(outputs[kIndex1]->dtype_id()));
  outputs[kIndex2]->SetShapeVector(count_shape_);
  ele_size = LongToSize(std::accumulate(count_shape_.begin(), count_shape_.end(), 1, std::multiplies<int64_t>()));
  outputs[kIndex2]->set_size(ele_size * UnitSizeInBytes(outputs[kIndex2]->dtype_id()));
}

#define CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(ms_index_type, ms_value_type, index_type, value_type) \
  {                                                                                                  \
    KernelAttr()                                                                                     \
      .AddInputAttr(ms_value_type)                                                                   \
      .AddOutputAttr(ms_value_type)                                                                  \
      .AddOutputAttr(ms_index_type)                                                                  \
      .AddOutputAttr(ms_index_type),                                                                 \
      &UniqueConsecutiveCpuKernelMod::LaunchKernel<value_type, index_type>                           \
  }

using UCKernelRunFunc = UniqueConsecutiveCpuKernelMod::KernelRunFunc;
const std::vector<std::pair<KernelAttr, UCKernelRunFunc>> &UniqueConsecutiveCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, UCKernelRunFunc>> func_list = {
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeComplex64, int64_t, std::complex<float>),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeComplex128, int64_t, std::complex<double>),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeFloat16, int64_t, float16),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeFloat32, int64_t, float),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeFloat64, int64_t, double),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeInt8, int64_t, int8_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeInt16, int64_t, int16_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeInt32, int64_t, int32_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeInt64, int64_t, int64_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeUInt8, int64_t, uint8_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeUInt16, int64_t, uint16_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeUInt32, int64_t, uint32_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeUInt64, int64_t, uint64_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeComplex64, int32_t, std::complex<float>),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeComplex128, int32_t, std::complex<double>),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeFloat16, int32_t, float16),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeFloat32, int32_t, float),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeFloat64, int32_t, double),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeInt8, int32_t, int8_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeInt16, int32_t, int16_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeInt32, int32_t, int32_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeInt64, int32_t, int64_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeUInt8, int32_t, uint8_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeUInt16, int32_t, uint16_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeUInt32, int32_t, uint32_t),
    CPU_UNIQUE_CONSECUTIVE_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeUInt64, int32_t, uint64_t)};
  return func_list;
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, UniqueConsecutive, UniqueConsecutiveCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
