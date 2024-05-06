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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_CLASS_DILATION2D_HELPER_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_CLASS_DILATION2D_HELPER_H_
#include <memory>
#include <string>
#include <vector>
#include <list>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_class/helper_base.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/dilation2d_impl.cuh"

namespace mindspore {
namespace cukernel {
constexpr size_t kDimSize3 = 3;
constexpr size_t kDimSize4 = 4;
constexpr size_t kInputIndex = 0;
constexpr size_t kFilterIndex = 1;
constexpr size_t kOutputIndex = 0;
constexpr size_t kFormatNCHWIndexC = 1;
constexpr size_t kFormatNCHWIndexH = 2;
constexpr size_t kFormatNCHWIndexW = 3;
constexpr size_t kFormatCHWIndexH = 1;
constexpr size_t kFormatCHWIndexW = 2;

class Dilation2DAttr : public GpuKernelAttrBase {
 public:
  Dilation2DAttr() = default;
  ~Dilation2DAttr() override = default;
  std::vector<int64_t> stride;
  std::vector<int64_t> dilation;
  std::string pad_mode;
  std::string format;
};

template <typename T>
class Dilation2DHelperGpuKernel : public GpuKernelHelperBase {
 public:
  explicit Dilation2DHelperGpuKernel(const std::string &kernel_name, const uint32_t &device_id)
      : GpuKernelHelperBase(kernel_name, device_id) {
    is_null_input_ = false;
  }

  virtual ~Dilation2DHelperGpuKernel() = default;
  int CalMemSize(const std::vector<std::vector<int64_t>> &input_shapes,
                 const std::vector<std::vector<int64_t>> &output_shapes) override {
    constexpr size_t OUTPUT_NUM = 1;
    ResetResource();

    input_shape_ = input_shapes[kIndex0];
    filter_shape_ = input_shapes[kIndex1];

    int out_flag =
      CalShapesSizeInBytes<T>(output_shapes, OUTPUT_NUM, kernel_name_, "output_shapes", &output_size_list_);
    if (out_flag == -1) {
      return out_flag;
    }
    output_shape_ = output_shapes[kIndex0];
    is_null_input_ = (HasZeroInShapes(input_shapes) || out_flag == 1);
    return CheckKernelParam();
  }

  int Process(const std::vector<void *> &input_ptrs, const std::vector<void *> &output_ptrs,
              const std::vector<void *> &work_ptrs, void *cuda_stream) override {
    if (is_null_input_) {
      return 0;
    }

    T *input_ptr = nullptr;
    T *filter = nullptr;
    T *output_ptr = nullptr;

    int flag = GetDeviceAddress<T>(input_ptrs, kInputIndex, kernel_name_, &input_ptr);
    if (flag != 0) {
      return flag;
    }
    flag = GetDeviceAddress<T>(input_ptrs, kFilterIndex, kernel_name_, &filter);
    if (flag != 0) {
      return flag;
    }
    flag = GetDeviceAddress<T>(output_ptrs, kOutputIndex, kernel_name_, &output_ptr);
    if (flag != 0) {
      return flag;
    }

    int64_t dims = static_cast<int64_t>(output_shape_.size());
    int64_t outer_size = 1;
    for (int64_t i = dims - 1; i >= 0; i--) {
      outer_size *= output_shape_[i];
    }
    int64_t pads[kIndex2];
    if (pad_mode_.compare("VALID") == 0 || pad_mode_.compare("valid") == 0) {
      pads[kIndex0] = 0;
      pads[kIndex1] = 0;
    }
    if (pad_mode_.compare("SAME") == 0 || pad_mode_.compare("same") == 0) {
      int64_t pad_height = (output_shape_[kFormatNCHWIndexH] - 1) * stride_[kFormatNCHWIndexH] +
                                 dilation_[kFormatNCHWIndexH] * (filter_shape_[kFormatCHWIndexH] - 1) + 1 -
                                 input_shape_[kFormatNCHWIndexH] >
                               0
                             ? (output_shape_[kFormatNCHWIndexH] - 1) * stride_[kFormatNCHWIndexH] +
                                 dilation_[kFormatNCHWIndexH] * (filter_shape_[kFormatCHWIndexH] - 1) + 1 -
                                 input_shape_[kFormatNCHWIndexH]
                             : 0;
      int64_t pad_width = (output_shape_[kFormatNCHWIndexW] - 1) * stride_[kFormatNCHWIndexW] +
                                dilation_[kFormatNCHWIndexW] * (filter_shape_[kFormatCHWIndexW] - 1) + 1 -
                                input_shape_[kFormatNCHWIndexW] >
                              0
                            ? (output_shape_[kFormatNCHWIndexW] - 1) * stride_[kFormatNCHWIndexW] +
                                dilation_[kFormatNCHWIndexW] * (filter_shape_[kFormatCHWIndexW] - 1) + 1 -
                                input_shape_[kFormatNCHWIndexW]
                            : 0;
      pads[kIndex0] = pad_height / kIndex2;
      pads[kIndex1] = pad_width / kIndex2;
    }

    // call cuda kernel
    auto status = CalDilation2D(input_ptr, filter, output_ptr, input_shape_, filter_shape_, output_shape_, stride_,
                                dilation_, pads, outer_size, device_id_, reinterpret_cast<cudaStream_t>(cuda_stream));
    CHECK_CUDA_STATUS(status, kernel_name_);
    return 0;
  }

  void SetKernelParam(const GpuKernelAttrBasePtr &kernel_attr) override {
    attr_ptr_ = std::dynamic_pointer_cast<Dilation2DAttr>(kernel_attr);
  }

 protected:
  int CheckKernelParam() override {
    stride_ = attr_ptr_->stride;
    dilation_ = attr_ptr_->dilation;
    pad_mode_ = attr_ptr_->pad_mode;
    format_ = attr_ptr_->format;
    size_t input_shape_dims = input_shape_.size();
    size_t filter_shape_dims = filter_shape_.size();
    size_t output_shape_dims = output_shape_.size();
    size_t stride_dims = stride_.size();
    size_t dilation_dims = dilation_.size();
    if (input_shape_dims != kDimSize4) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'input_shape' must be equal to 4, but got "
                    << input_shape_dims << ".";
      return -1;
    }
    if (filter_shape_dims != kDimSize3) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'filter_shape' must be equal to 3, but got "
                    << filter_shape_dims << ".";
      return -1;
    }
    if (output_shape_dims != kDimSize4) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'output_shape' must be equal to 4, but got "
                    << output_shape_dims << ".";
      return -1;
    }
    if (stride_dims != kDimSize4) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'stride' must be equal to 4, but got "
                    << stride_dims << ".";
      return -1;
    }
    if (dilation_dims != kDimSize4) {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'dilation' must be equal to 4, but got "
                    << dilation_dims << ".";
      return -1;
    }
    if (pad_mode_ != "VALID" && pad_mode_ != "valid" && pad_mode_ != "SAME" && pad_mode_ != "same") {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', pad_mode_ must be VALID, valid, SAME or same, but got "
                    << pad_mode_ << ".";
      return -1;
    }
    if (format_ != "NCHW") {
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', data_format must be NCHW, but got " << format_ << ".";
      return -1;
    }
    return 0;
  }

 private:
  std::shared_ptr<Dilation2DAttr> attr_ptr_;
  std::vector<int64_t> input_shape_;
  std::vector<int64_t> filter_shape_;
  std::vector<int64_t> output_shape_;
  std::vector<int64_t> stride_;
  std::vector<int64_t> dilation_;
  std::string pad_mode_;
  std::string format_;
  bool is_null_input_;
};
}  // namespace cukernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_CLASS_DILATION2D_HELPER_H_
