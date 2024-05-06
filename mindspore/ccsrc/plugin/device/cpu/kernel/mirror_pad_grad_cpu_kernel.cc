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

#include "plugin/device/cpu/kernel/mirror_pad_grad_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
// preset size of paddings
constexpr int MAX_PADDINGS = 5;
constexpr int PADDING_SIZE = 2;

// define constants for kernel indexing use
constexpr size_t kMirrorPadGradInputsNum = 2;
constexpr size_t kMirrorPadGradOutputsNum = 1;
constexpr size_t kPadMaxSupportDim = 5;
}  // namespace

bool MirrorPadGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  std::string mode = GetValue<std::string>(primitive_->GetAttr("mode"));
  dtype_ = inputs[0]->dtype_id();
  pad_dtype_ = inputs[1]->dtype_id();
  if (mode == "REFLECT") {
    mode_ = 1;
  } else if (mode == "SYMMETRIC") {
    mode_ = 0;
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the 'mode' should be 'REFLECT' or 'SYMMETRIC', but got "
                      << mode;
  }
  return true;
}

int MirrorPadGradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  input_shape_ = inputs.at(kIndex0)->GetShapeVector();
  dims_ = int64_t(input_shape_.size());
  if (dims_ <= 0) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dim must greater than 0, but got " << dims_;
  }
  input_size_ = 1;
  for (auto x : input_shape_) {
    input_size_ *= x;
  }

  ShapeVector padding_shape = inputs.at(kIndex1)->GetShapeVector();
  num_paddings_ = padding_shape[0];

  output_shape_ = outputs.at(kIndex0)->GetShapeVector();
  output_size_ = 1;
  for (auto x : output_shape_) {
    if (x == 0) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', all the dims in output shape can not be 0, but got " << x;
    }
    output_size_ *= x;
  }
  return ret;
}

template <typename T>
void MirrorPadGradCpuKernelMod::paddings_type(const std::vector<KernelTensor *> &inputs,
                                              const std::vector<KernelTensor *> &outputs) {
  if (pad_dtype_ == kNumberTypeInt32) {
    LaunchKernel<T, int32_t>(inputs, outputs);
  } else if (pad_dtype_ == kNumberTypeInt64) {
    LaunchKernel<T, int64_t>(inputs, outputs);
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dtype of 'paddings' should be int32 or int64, but got "
                      << TypeIdLabel(pad_dtype_);
  }
}

bool MirrorPadGradCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                       const std::vector<kernel::KernelTensor *> &,
                                       const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kMirrorPadGradInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kMirrorPadGradOutputsNum, kernel_name_);
  if (dtype_ == kNumberTypeFloat16) {
    paddings_type<float16>(inputs, outputs);
  } else if (dtype_ == kNumberTypeFloat32) {
    paddings_type<float>(inputs, outputs);
  } else if (dtype_ == kNumberTypeFloat64) {
    paddings_type<double>(inputs, outputs);
  } else if (dtype_ == kNumberTypeInt8) {
    paddings_type<int8_t>(inputs, outputs);
  } else if (dtype_ == kNumberTypeInt16) {
    paddings_type<int16_t>(inputs, outputs);
  } else if (dtype_ == kNumberTypeInt32) {
    paddings_type<int32_t>(inputs, outputs);
  } else if (dtype_ == kNumberTypeInt64) {
    paddings_type<int64_t>(inputs, outputs);
  } else if (dtype_ == kNumberTypeUInt8) {
    paddings_type<uint8_t>(inputs, outputs);
  } else if (dtype_ == kNumberTypeUInt16) {
    paddings_type<uint16_t>(inputs, outputs);
  } else if (dtype_ == kNumberTypeComplex64) {
    paddings_type<std::complex<float>>(inputs, outputs);
  } else if (dtype_ == kNumberTypeComplex128) {
    paddings_type<std::complex<double>>(inputs, outputs);
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the dtype of 'input_x' should be float16, float32, float64, or int8, int16, int32, int64, "
                         "uint8, uint16, complex64, complex128, but got "
                      << TypeIdLabel(dtype_);
  }
  return true;
}

template <typename T>
void MirrorPadGradCpuKernelMod::slice(std::vector<int64_t> extents, std::vector<int64_t> rhs_offsets,
                                      std::vector<int64_t> input_strides, std::vector<T> inputs,
                                      const std::vector<KernelTensor *> &outputs) {
  auto *outputs_addr = static_cast<T *>(outputs[0]->device_ptr());
  auto inputs_addr = reinterpret_cast<T *>(inputs.data());
  size_t index = LongToSize(dims_ - 1);
  auto copy_size = sizeof(T) * LongToSize(extents[index]);
  for (int64_t i = 0; i < output_size_; i += extents[index]) {
    std::vector<int64_t> pos(LongToSize(dims_), 0);
    auto idx = i / extents[index];
    for (int j = LongToInt(dims_ - 2); j >= 0; --j) {
      if (idx == 0) {
        break;
      }
      pos[j] = idx % extents[j];
      idx /= extents[j];
    }
    int64_t input_index = 0;
    for (size_t j = 0; j < pos.size(); j++) {
      input_index += (pos[j] + rhs_offsets[j]) * input_strides[j];
    }
    int ret = memcpy_s(outputs_addr + LongToSize(i), copy_size, inputs_addr + LongToSize(input_index), copy_size);
    if (ret != 0) {
      MS_LOG(EXCEPTION) << "The memcpy_s error, errorno(" << ret << ")";
    }
  }
}

template <typename T>
std::vector<std::pair<int64_t, int64_t>> MirrorPadGradCpuKernelMod::extract_paddings(const T *paddings_arg) const {
  std::vector<std::pair<int64_t, int64_t>> paddings;
  for (int64_t i = 0; i < dims_; ++i) {
    paddings.push_back(
      std::make_pair(int64_t(paddings_arg[i * PADDING_SIZE]), int64_t(paddings_arg[i * PADDING_SIZE + 1])));
  }
  return paddings;
}

template <typename T1, typename T2>
void MirrorPadGradCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                             const std::vector<KernelTensor *> &outputs) {
  auto *inputs_data = static_cast<T1 *>(inputs[0]->device_ptr());
  auto *paddings_arg = static_cast<T2 *>(inputs[1]->device_ptr());
  int64_t block_num = 1;

  std::vector<T1> inputs_addr;
  inputs_addr.resize(input_size_);

  int ret = memcpy_s(inputs_addr.data(), input_size_ * sizeof(T1), inputs_data, input_size_ * sizeof(T1));
  if (ret != 0) {
    MS_LOG(EXCEPTION) << "The memcpy_s error, errorno(" << ret << ")";
  }

  std::vector<std::pair<int64_t, int64_t>> paddings = extract_paddings<T2>(paddings_arg);

  std::vector<int64_t> input_strides(dims_, 0);
  std::vector<int64_t> lhs_offsets(dims_, 0);
  std::vector<int64_t> rhs_offsets(dims_, 0);
  std::vector<int64_t> extents(dims_, 0);

  input_strides[dims_ - 1] = 1;
  for (int64_t i = dims_ - 1; i >= 0; --i) {
    if (i > 0) {
      input_strides[i - 1] = input_strides[i] * input_shape_[i];
    }
    extents[i] = input_shape_[i];
  }

  for (int64_t i = 0; i < dims_; ++i) {
    if (paddings[i].first > 0) {
      rhs_offsets[i] = 0;
      lhs_offsets[i] = paddings[i].first + mode_;
      extents[i] = paddings[i].first;
      int64_t inx = 0;
      int64_t total_block = block_num * extents[i];
      std::vector<int64_t> block_pos(i + 1, 0);
      while (inx < total_block) {
        for (int64_t j = i; j >= 0; --j) {
          bool newline = j > 0 && block_pos[j] >= extents[j];
          if (newline) {
            block_pos[j] -= extents[j];
            block_pos[j - 1] += 1;
          } else {
            break;
          }
        }
        int64_t src_addr = 0;
        int64_t des_addr = 0;
        for (int64_t j = 0; j < i; ++j) {
          des_addr += (lhs_offsets[j] + block_pos[j]) * input_strides[j];
          src_addr += (rhs_offsets[j] + block_pos[j]) * input_strides[j];
        }
        des_addr += (lhs_offsets[i] + block_pos[i]) * input_strides[i];
        src_addr += (rhs_offsets[i] + extents[i] - block_pos[i] - 1) * input_strides[i];
        for (int64_t j = 0; j < input_strides[i]; ++j) {
          inputs_addr[des_addr + j] += inputs_addr[src_addr + j];
        }
        block_pos[i] += 1;
        ++inx;
      }
    }
    if (paddings[i].second > 0) {
      rhs_offsets[i] = input_shape_[i] - paddings[i].second;
      lhs_offsets[i] = rhs_offsets[i] - paddings[i].second - mode_;
      extents[i] = paddings[i].second;
      int64_t inx = 0;
      int64_t total_block = block_num * extents[i];
      std::vector<int64_t> block_pos(i + 1, 0);
      while (inx < total_block) {
        for (int64_t j = i; j >= 0; --j) {
          bool newline = j > 0 && block_pos[j] >= extents[j];
          if (newline) {
            block_pos[j] -= extents[j];
            block_pos[j - 1] += 1;
          } else {
            break;
          }
        }
        int64_t src_addr = 0;
        int64_t des_addr = 0;
        for (int64_t j = 0; j < i; ++j) {
          des_addr += (lhs_offsets[j] + block_pos[j]) * input_strides[j];
          src_addr += (rhs_offsets[j] + block_pos[j]) * input_strides[j];
        }
        des_addr += (lhs_offsets[i] + block_pos[i]) * input_strides[i];
        src_addr += (rhs_offsets[i] + extents[i] - block_pos[i] - 1) * input_strides[i];
        for (int64_t j = 0; j < input_strides[i]; ++j) {
          inputs_addr[des_addr + j] += inputs_addr[src_addr + j];
        }
        block_pos[i] += 1;
        ++inx;
      }
    }
    lhs_offsets[i] = paddings[i].first;
    rhs_offsets[i] = paddings[i].first;
    extents[i] = output_shape_[i];
    block_num = block_num * extents[i];
  }
  slice<T1>(extents, rhs_offsets, input_strides, inputs_addr, outputs);
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, MirrorPadGrad, MirrorPadGradCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
