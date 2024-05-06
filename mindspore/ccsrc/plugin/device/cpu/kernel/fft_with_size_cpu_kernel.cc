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
#include "plugin/device/cpu/kernel/fft_with_size_cpu_kernel.h"
#include <algorithm>
#include "ops/op_utils.h"
#include "kernel/kernel.h"

#define FFTWITHSIZE_SWITCH_DIM_CALCULATE(T1, T2, real, inverse)                                                    \
  if (signal_ndim_ == 1) {                                                                                         \
    FFTWithSizeCompute<T1, T2, 1, real, inverse>(p_x, p_y, onesided_, normalized_, checked_signal_size, x_shape_); \
  } else if (signal_ndim_ == 2) {                                                                                  \
    FFTWithSizeCompute<T1, T2, 2, real, inverse>(p_x, p_y, onesided_, normalized_, checked_signal_size, x_shape_); \
  } else {                                                                                                         \
    FFTWithSizeCompute<T1, T2, 3, real, inverse>(p_x, p_y, onesided_, normalized_, checked_signal_size, x_shape_); \
  }
using std::vector;
namespace mindspore {
namespace kernel {
namespace {
constexpr int kDimNum_FFT = 1;
constexpr int kDimNum_IFFT = 2;
constexpr int kDimNum_RFFT = 3;
constexpr int kDimNum_IRFFT = 4;
constexpr int kRealFFTSideNum = 2;

int64_t FFTWithSize_choose(bool real, bool inverse) {
  if (!real) {
    if (!inverse) {
      return kDimNum_FFT;  // fftz
    } else {
      return kDimNum_IFFT;  // ifft
    }
  } else {
    if (!inverse) {
      return kDimNum_RFFT;  // rfft
    } else {
      return kDimNum_IRFFT;  // irfft
    }
  }
}

int64_t get_element_num(const std::vector<int64_t> &shape, size_t rank) {
  size_t back_itr = shape.size();
  int64_t size = 1;
  for (size_t i = 1; i <= rank; i++) {
    auto dim = shape[back_itr - i];
    MS_EXCEPTION_IF_CHECK_FAIL(dim > 0, "The element in shape must be positive.");
    size *= dim;
  }
  return size;
}

template <unsigned int size, unsigned int from, unsigned int to>
void change_axes(Eigen::array<unsigned int, size> *axes) {
  for (unsigned i = from; i <= (unsigned)to; i++) {
    axes->operator[](i - 1) = i;
  }
  return;
}
}  // namespace

bool FFTWithSizeCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(EXCEPTION) << kernel_name_ << " valid cpu kernel does not support this kernel data type: " << kernel_attr;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int FFTWithSizeCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  x_shape_ = inputs[kIndex0]->GetShapeVector();
  signal_ndim_ = inputs[kIndex1]->GetValueWithCheck<int64_t>();
  inverse_ = inputs[kIndex2]->GetValueWithCheck<bool>();
  real_ = inputs[kIndex3]->GetValueWithCheck<bool>();
  normalized_ = inputs[kIndex4]->GetValueWithCheck<int64_t>();
  onesided_ = inputs[kIndex5]->GetValueWithCheck<bool>();
  raw_checked_signal_size_ = inputs[kIndex6]->GetValueWithCheck<std::vector<int64_t>>();

  return KRET_OK;
}

double Getnormalized(int64_t element_num, const std::string &normalized, bool is_reverse) {
  double result = 1.0;
  if (!is_reverse) {
    if (normalized == "forward") result = 1.0 / element_num;
    if (normalized == "backward") result = 1.0;
    if (normalized == "ortho") result = 1.0 / sqrt(static_cast<double>(element_num));
  }
  if (is_reverse) {
    if (normalized == "forward") result = 1.0 * element_num;
    if (normalized == "backward") result = 1.0;
    if (normalized == "ortho") result = 1.0 * sqrt(static_cast<double>(element_num));
  }
  return result;
}

template <int signal_ndim>
inline Eigen::DSizes<Eigen::DenseIndex, signal_ndim + 1> GetFlatShape(const std::vector<int64_t> &x_shape,
                                                                      size_t x_dims) {
  Eigen::DSizes<Eigen::DenseIndex, signal_ndim + 1> tensor_shape;
  if (x_dims == signal_ndim) {
    tensor_shape[0] = 1;
    for (size_t i = 0; i < x_dims; i++) {
      tensor_shape[i + 1] = x_shape[i];
    }
  } else if (x_dims == signal_ndim + 1) {
    for (size_t i = 0; i < x_dims; i++) {
      tensor_shape[i] = x_shape[i];
    }
  } else if (x_dims > signal_ndim + 1) {
    tensor_shape[0] = 1;
    for (size_t i = 0; i < x_dims - signal_ndim; i++) {
      tensor_shape[0] *= x_shape[i];
    }
    for (size_t j = x_dims - static_cast<size_t>(signal_ndim), i = 1; j < x_dims; j++, i++) {
      tensor_shape[i] = x_shape[j];
    }
  } else {
    MS_LOG(EXCEPTION) << "x_dims must not be less than signal_ndim.";
  }
  return tensor_shape;
}

template <typename T1, typename T2, int signal_ndim, bool is_real, bool is_inverse>
bool FFTWithSizeCompute(T1 *input_x, T2 *output_y, bool onesided, std::string normalized,
                        const vector<int64_t> &checked_signal_size, const vector<int64_t> &x_shape) {
  Eigen::DSizes<Eigen::DenseIndex, signal_ndim + 1> tensor_shape = GetFlatShape<signal_ndim>(x_shape, x_shape.size());
  Eigen::TensorMap<Eigen::Tensor<T1, signal_ndim + 1, Eigen::RowMajor>, Eigen::RowMajor> in(&input_x[0], tensor_shape);
  Eigen::array<unsigned int, signal_ndim> axes;
  change_axes<signal_ndim, 1, signal_ndim>(&axes);
  Eigen::Tensor<T2, signal_ndim + 1, Eigen::RowMajor> out;
  vector<int64_t> norm_shape(x_shape);
  if constexpr (is_real) {       // for rfft and irfft COMPILE TIME EXPANSION
    if constexpr (is_inverse) {  // irfft
      Eigen::Tensor<T1, signal_ndim + 1, Eigen::RowMajor> complex_out;
      if (onesided) {
        // compute the full fft tensor shape: full_fft_shape[-1] / 2 + 1
        Eigen::DSizes<Eigen::DenseIndex, signal_ndim + 1> temp_tensor_shape(tensor_shape);
        if (checked_signal_size.empty()) {
          if (temp_tensor_shape[signal_ndim] == 1) {
            MS_EXCEPTION(ValueError) << "For 'FFTWithSize', the last dimension of the input cannot be 1, but got: "
                                     << temp_tensor_shape[signal_ndim];
          }
          temp_tensor_shape[signal_ndim] = (temp_tensor_shape[signal_ndim] - 1) * kRealFFTSideNum;
        } else {
          if (checked_signal_size.back() / kRealFFTSideNum + 1 == temp_tensor_shape[signal_ndim]) {
            temp_tensor_shape[static_cast<size_t>(signal_ndim)] = checked_signal_size.back();
          }
        }
        if (temp_tensor_shape.back() == tensor_shape.back()) {
          // fake there is no need to reconstruct signal tensor
          complex_out = in.template fft<Eigen::BothParts, Eigen::FFT_REVERSE>(axes);
        } else {
          // Reconstruct the full fft tensor: temp_tensor
          Eigen::Tensor<T1, signal_ndim + 1, Eigen::RowMajor> temp_tensor(temp_tensor_shape);
          temp_tensor.setZero();
          Eigen::DSizes<Eigen::DenseIndex, signal_ndim + 1> zero_offsets;
          Eigen::DSizes<Eigen::DenseIndex, signal_ndim + 1> input_slice_sizes(in.dimensions());
          temp_tensor.slice(zero_offsets, input_slice_sizes) = in;
          // do ifft at outer axes
          if (signal_ndim > 1) {
            Eigen::array<unsigned int, signal_ndim - 1> outer_axes;
            change_axes<signal_ndim - 1, 1, signal_ndim - 1>(&outer_axes);
            temp_tensor = temp_tensor.template fft<Eigen::BothParts, Eigen::FFT_REVERSE>(outer_axes);
          }
          // rebuild the last axis with symmetrical data
          Eigen::array<bool, signal_ndim + 1> reverse_last_axis;
          for (auto i = 0; i <= signal_ndim; i++) {
            reverse_last_axis[i] = i == signal_ndim;
          }
          auto reverse_size = input_slice_sizes;
          reverse_size[signal_ndim] = temp_tensor_shape[signal_ndim] - input_slice_sizes[signal_ndim];
          Eigen::DSizes<Eigen::DenseIndex, signal_ndim + 1> reverse_start_indices;
          reverse_start_indices[signal_ndim] = 1;
          Eigen::DSizes<Eigen::DenseIndex, signal_ndim + 1> reverse_target_indices;
          reverse_target_indices[signal_ndim] = input_slice_sizes[signal_ndim];
          temp_tensor.slice(reverse_target_indices, reverse_size) =
            temp_tensor.slice(reverse_start_indices, reverse_size).reverse(reverse_last_axis).conjugate();
          // do irfft at the last axis:
          auto inner_axis = Eigen::array<unsigned int, 1>{signal_ndim};
          complex_out = temp_tensor.template fft<Eigen::BothParts, Eigen::FFT_REVERSE>(inner_axis);
        }
        norm_shape.back() = static_cast<int64_t>(temp_tensor_shape.back());
      } else {
        complex_out = in.template fft<Eigen::BothParts, Eigen::FFT_REVERSE>(axes);
      }
      out.resize(complex_out.dimensions());
      T1 *complex_out_ptr = complex_out.data();
      for (int i = 0; i < complex_out.size(); i++) {
        *(out.data() + i) = (complex_out_ptr + i)->real();
      }
    } else {  // rfft
      Eigen::Tensor<T2, signal_ndim + 1, Eigen::RowMajor> complex_in(in.dimensions());
      T2 *in_data_ptr = complex_in.data();
      for (int i = 0; i < in.size(); i++) {
        (in_data_ptr + i)->real(*(input_x + i));
        (in_data_ptr + i)->imag(0);
      }
      Eigen::Tensor<T2, signal_ndim + 1, Eigen::RowMajor> full_fft =
        complex_in.template fft<Eigen::BothParts, Eigen::FFT_FORWARD>(axes);
      if (onesided) {
        auto dims = in.dimensions();
        Eigen::DSizes<Eigen::DenseIndex, signal_ndim + 1> offsets;
        Eigen::DSizes<Eigen::DenseIndex, signal_ndim + 1> input_slice_sizes;
        for (auto i = 0; i <= signal_ndim; i++) {
          input_slice_sizes[i] = (i == signal_ndim) ? (dims[i] / kRealFFTSideNum + 1) : dims[i];
        }
        out = full_fft.slice(offsets, input_slice_sizes);
      } else {
        out = full_fft;
      }
    }
  } else {  // fft and ifft
    if (is_inverse) {
      out = in.template fft<Eigen::BothParts, Eigen::FFT_REVERSE>(axes);
    } else {
      out = in.template fft<Eigen::BothParts, Eigen::FFT_FORWARD>(axes);
    }
  }

  int64_t element_num = get_element_num(norm_shape, static_cast<size_t>(signal_ndim));
  double norm = Getnormalized(element_num, normalized, is_inverse);
  T2 *out_ptr = out.data();
  for (int i = 0; i < out.size(); i++) {
    T2 temp_value = *(out_ptr + i);
    temp_value *= norm;
    *(output_y + i) = temp_value;
  }
  return true;
}

template <typename T1, typename T2>
bool FFTWithSizeCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                           const std::vector<kernel::KernelTensor *> &outputs) {
  std::vector<int64_t> checked_signal_size(raw_checked_signal_size_.begin(), raw_checked_signal_size_.end());
  const int64_t choose = FFTWithSize_choose(real_, inverse_);
  auto p_x = reinterpret_cast<T1 *>(inputs[kIndex0]->device_ptr());
  auto p_y = reinterpret_cast<T2 *>(outputs[kIndex0]->device_ptr());
  if constexpr (std::is_same<T1, T2>::value) {  // fft and ifft
    if (choose == kDimNum_FFT) {
      FFTWITHSIZE_SWITCH_DIM_CALCULATE(T1, T2, false, false);
    } else {
      FFTWITHSIZE_SWITCH_DIM_CALCULATE(T1, T2, false, true);
    }
  } else {  // rfft and irfft
    if constexpr (std::is_same<T1, std::complex<float>>::value ||
                  std::is_same<T1, std::complex<double>>::value) {  // irfft
      FFTWITHSIZE_SWITCH_DIM_CALCULATE(T1, T2, true, true);
    } else {  // rfft
      FFTWITHSIZE_SWITCH_DIM_CALCULATE(T1, T2, true, false);
    }
  }
  return true;
}

#define FFT_CPU_REG(MS_I, MS_O, I, O)                                     \
  KernelAttr()                                                            \
    .AddInputAttr(MS_I)                                /* x */            \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64) /* signal_ndim */  \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)  /* inverse */      \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)  /* real */         \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64) /* norm */         \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)  /* onesided */     \
    .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)  /* signal_sizes */ \
    .AddOutputAttr(MS_O),                                                 \
    &FFTWithSizeCpuKernelMod::LaunchKernel<I, O>

std::vector<std::pair<KernelAttr, FFTWithSizeCpuKernelMod::FFTWithSizeFunc>> FFTWithSizeCpuKernelMod::func_list_ = {
  {FFT_CPU_REG(kNumberTypeComplex64, kNumberTypeComplex64, std::complex<float>, std::complex<float>)},
  {FFT_CPU_REG(kNumberTypeComplex128, kNumberTypeComplex128, std::complex<double>, std::complex<double>)},
  {FFT_CPU_REG(kNumberTypeFloat32, kNumberTypeComplex64, float, std::complex<float>)},
  {FFT_CPU_REG(kNumberTypeComplex64, kNumberTypeFloat32, std::complex<float>, float)},
  {FFT_CPU_REG(kNumberTypeFloat64, kNumberTypeComplex128, double, std::complex<double>)},
  {FFT_CPU_REG(kNumberTypeComplex128, kNumberTypeFloat64, std::complex<double>, double)},
  {FFT_CPU_REG(kNumberTypeUInt8, kNumberTypeComplex64, uint8_t, std::complex<float>)},
  {FFT_CPU_REG(kNumberTypeInt8, kNumberTypeComplex64, int8_t, std::complex<float>)},
  {FFT_CPU_REG(kNumberTypeInt16, kNumberTypeComplex64, int16_t, std::complex<float>)},
  {FFT_CPU_REG(kNumberTypeInt32, kNumberTypeComplex64, int32_t, std::complex<float>)},
  {FFT_CPU_REG(kNumberTypeInt64, kNumberTypeComplex64, int64_t, std::complex<float>)},
  {FFT_CPU_REG(kNumberTypeBool, kNumberTypeComplex64, bool, std::complex<float>)}};

std::vector<KernelAttr> FFTWithSizeCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, FFTWithSizeFunc> &pair) { return pair.first; });

  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, FFTWithSize, FFTWithSizeCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
