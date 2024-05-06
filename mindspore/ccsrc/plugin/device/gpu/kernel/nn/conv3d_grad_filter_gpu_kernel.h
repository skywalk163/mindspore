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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_CONV3D_GRAD_FILTER_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_CONV3D_GRAD_FILTER_GPU_KERNEL_H_

#include <algorithm>
#include <string>
#include <vector>
#include <map>

#include "mindspore/core/ops/grad/conv3d_backprop_filter.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/pad_impl.cuh"
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/kernel_constants.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cast_impl.cuh"
#include "plugin/device/gpu/kernel/nn/convolution/conv_gpu_common.h"

namespace mindspore {
namespace kernel {
constexpr int kDynamicInputNum = 3;
constexpr int kOutputNum = 1;
constexpr int kNumDims = 5;
constexpr int kConvDims = 3;
constexpr size_t kInDimIdxForN = 0;
constexpr size_t kInDimIdxForC = 1;
constexpr size_t kInDimIdxForD = 2;
constexpr size_t kInDimIdxForH = 3;
constexpr size_t kInDimIdxForW = 4;

constexpr size_t k3DPadSize = 6;
constexpr size_t kHead3DPadIdx = 0;
constexpr size_t kTail3DPadIdx = 1;
constexpr size_t kTop3DPadIdx = 2;
constexpr size_t kBottom3DPadIdx = 3;
constexpr size_t kLeft3DPadIdx = 4;
constexpr size_t kRight3DPadIdx = 5;

constexpr size_t kPadDepthIdx = 0;
constexpr size_t kPadHeightIdx = 1;
constexpr size_t kPadWidthIdx = 2;

constexpr size_t k3DStrideSize = 5;
constexpr size_t kDepth3DStrideIdx = 2;
constexpr size_t kHeight3DStrideIdx = 3;
constexpr size_t kWidth3DStrideIdx = 4;

constexpr size_t k3DDilationSize = 5;
constexpr size_t kDepth3DDilationIdx = 2;
constexpr size_t kHeight3DDilationIdx = 3;
constexpr size_t kWidth3DDilationIdx = 4;

template <typename T>
class Conv3dGradFilterGpuKernelMod : public NativeGpuKernelMod {
 public:
  Conv3dGradFilterGpuKernelMod() { ResetResource(); }
  ~Conv3dGradFilterGpuKernelMod() override { DestroyResource(); }

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    T *x = GetDeviceAddress<T>(inputs, 0);
    T *dy = GetDeviceAddress<T>(inputs, 1);
    T *work_space = GetPossiblyNullDeviceAddress<T>(workspace, 0);

    T *dw = nullptr;
    float *dw_float32 = nullptr;
    if (cudnn_data_type_ == CUDNN_DATA_HALF) {
      dw = GetDeviceAddress<T>(workspace, 1);
      dw_float32 = GetDeviceAddress<float>(outputs, 0);
    } else {
      dw = GetDeviceAddress<T>(outputs, 0);
    }

    const float alpha = 1;
    const float beta = 0;
    if (use_pad_) {
      T *padded = GetDeviceAddress<T>(workspace, 1);
      auto status = CalPad3d(padded_size_ / sizeof(T), x, n_, c_, old_depth_, old_height_, old_width_,
                             old_depth_ + pad_depth_, old_height_ + pad_height_, old_width_ + pad_width_, pad_head_,
                             pad_top_, pad_left_, pad_value_, padded, reinterpret_cast<cudaStream_t>(stream_ptr));
      CHECK_CUDA_STATUS(status, kernel_name_);
      CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
        cudnnConvolutionBackwardFilter(cudnn_handle_, &alpha, padded_descriptor_, padded, dy_desc_, dy, conv_desc_,
                                       algo_, work_space, workspace_size_, &beta, dw_desc_, dw),
        "ConvolutionBackwardFilter failed");
      return true;
    }
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
      cudnnConvolutionBackwardFilter(cudnn_handle_, &alpha, x_desc_, x, dy_desc_, dy, conv_desc_, algo_, work_space,
                                     workspace_size_, &beta, dw_desc_, dw),
      "ConvolutionBackwardFilter failed");

    if (cudnn_data_type_ == CUDNN_DATA_HALF) {
      Cast(num_output_elements_, dw, dw_float32, reinterpret_cast<cudaStream_t>(stream_ptr));
    }
    return true;
  }

  void CheckSize(const size_t value, const size_t expect_value, const string arg_name) {
    if (value != expect_value) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dimension of " << arg_name << " must be " << expect_value
                        << ", but got " << value;
    }
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override {
    InitResource();
    size_t input_num = inputs.size();
    if (input_num != kDynamicInputNum) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of inputs must be 3, but got " << input_num;
    }
    size_t output_num = outputs.size();
    if (output_num != kOutputNum) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of outputs must be 1, but got " << output_num;
    }

    cudnn_data_type_ = GetCudnnDataType(TypeIdLabel(inputs[kIndex0]->dtype_id()));
    data_format_ = kOpFormat_NCDHW;
    return true;
  }

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
    int ret = KernelMod::Resize(inputs, outputs);
    if (ret != KRET_OK) {
      return ret;
    }
    workspace_size_list_.clear();
    output_size_list_.clear();

    auto input_shape = inputs[kIndex0]->GetShapeVector();
    auto dy_shape = inputs[kIndex1]->GetShapeVector();
    auto filter_shape = outputs[kIndex0]->GetShapeVector();
    compute_format_ = CUDNN_TENSOR_NCHW;
    CheckTensorSize({input_shape});
    (void)CheckSize(input_shape.size(), kNumDims, "input shape");

    const size_t kFilterDimSize = 5;
    if (filter_shape.size() < kFilterDimSize) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dimension of filter must be greater than or equal to 5, "
                        << "but got " << filter_shape.size();
    }
    num_output_elements_ = 1;
    for (auto x : filter_shape) {
      num_output_elements_ *= x;
    }

    n_ = LongToInt(input_shape[kInDimIdxForN]);
    c_ = LongToInt(input_shape[kInDimIdxForC]);
    old_depth_ = LongToInt(input_shape[kInDimIdxForD]);
    old_height_ = LongToInt(input_shape[kInDimIdxForH]);
    old_width_ = LongToInt(input_shape[kInDimIdxForW]);
    SetNDDesc(dy_shape, input_shape, filter_shape);
    group_ = GetValue<int64_t>(primitive_->GetAttr("group"));
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnSetConvolutionGroupCount(conv_desc_, group_),
                                        "cudnnSetConvGroupCount failed");

    pad_mode_ = GetValue<std::string>(primitive_->GetAttr("pad_mode"));
    std::vector<int> pad_list;
    std::vector<int64_t> pad_list_me;
    if (pad_mode_ == kValidPadModeUpperCase || pad_mode_ == kValidPadModeLowerCase) {
      pad_list_me = {0, 0, 0, 0, 0, 0};
    } else if (pad_mode_ == kSamePadModeUpperCase || pad_mode_ == kSamePadModeLowerCase) {
      pad_list_me =
        primitive_->HasAttr("pad_list")
          ? GetValue<std::vector<int64_t>>(primitive_->GetAttr("pad_list"))
          : GetSameModePadList(dy_shape, input_shape, GetValue<std::vector<int64_t>>(primitive_->GetAttr("stride")),
                               GetValue<std::vector<int64_t>>(primitive_->GetAttr("dilation")),
                               GetValue<std::vector<int64_t>>(primitive_->GetAttr("kernel_size")));
    } else if (pad_mode_ == "PAD" || pad_mode_ == "pad") {
      pad_list_me = GetValue<std::vector<int64_t>>(primitive_->GetAttr("pad"));
    }
    (void)std::transform(pad_list_me.begin(), pad_list_me.end(), std::back_inserter(pad_list),
                         [](const int64_t &value) { return static_cast<int>(value); });
    SetPad(pad_list);
    SetStrideAndDilation(GetValue<std::vector<int64_t>>(primitive_->GetAttr("stride")),
                         GetValue<std::vector<int64_t>>(primitive_->GetAttr("dilation")));
    auto x_desc_real = GetXDescReal(pad_list);

    SetConvolutionMathType(conv_desc_, cudnn_data_type_);
    algo_ = SelectBackwardFilterAlgorithm(cudnn_handle_, cudnn_data_type_, x_desc_real, dy_desc_, conv_desc_, dw_desc_,
                                          group_);
    InitSizeLists();
    return KRET_OK;
  }

  std::vector<size_t> GetLaunchIgnoredInputAddressIdx() const override { return {kIndex2}; }

  void ResetResource() noexcept {
    cudnn_handle_ = nullptr;
    dw_desc_ = nullptr;
    conv_desc_ = nullptr;
    dy_desc_ = nullptr;
    x_desc_ = nullptr;
    padded_descriptor_ = nullptr;
    cudnn_data_type_ = CUDNN_DATA_FLOAT;
    compute_format_ = CUDNN_TENSOR_NCHW;
    old_depth_ = 0;
    old_height_ = 0;
    old_width_ = 0;
    pad_depth_ = 0;
    pad_height_ = 0;
    pad_width_ = 0;
    pad_head_ = 0;
    pad_top_ = 0;
    pad_left_ = 0;
    n_ = 0;
    c_ = 0;
    group_ = 1;
    kernel_name_ = "Conv3dGradFilter";
    dy_size_ = 0;
    input_size_ = 0;
    output_size_ = 0;
    padded_size_ = 0;
    workspace_size_ = 0;
    use_pad_ = false;
    num_output_elements_ = 1;
  }

  void DestroyResource() noexcept override {
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnDestroyConvolutionDescriptor(conv_desc_),
                                        "cudnnDestroyConvolutionDescriptor failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnDestroyFilterDescriptor(dw_desc_), "cudnnDestroyFilterDescriptor failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnDestroyTensorDescriptor(padded_descriptor_),
                                        "cudnnDestroyTensorDescriptor failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnDestroyTensorDescriptor(dy_desc_), "cudnnDestroyTensorDescriptor failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnDestroyTensorDescriptor(x_desc_), "cudnnDestroyTensorDescriptor failed");
  }

 protected:
  void InitResource() override {
    cudnn_handle_ = device::gpu::GPUDeviceManager::GetInstance().GetCudnnHandle();
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateTensorDescriptor(&x_desc_), "cudnnCreateTensorDescriptor failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateTensorDescriptor(&dy_desc_), "cudnnCreateTensorDescriptor failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateTensorDescriptor(&padded_descriptor_),
                                        "cudnnCreateTensorDescriptor failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateFilterDescriptor(&dw_desc_), "cudnnCreateFilterDescriptor failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnCreateConvolutionDescriptor(&conv_desc_),
                                        "cudnnCreateConvolutionDescriptor failed");
  }

  void InitSizeLists() {
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnGetTensorSizeInBytes(dy_desc_, reinterpret_cast<size_t *>(&dy_size_)),
                                        "cudnnGetTensorSizeInBytes failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnGetTensorSizeInBytes(x_desc_, reinterpret_cast<size_t *>(&input_size_)),
                                        "cudnnGetTensorSizeInBytes failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnGetFilterSizeInBytes(dw_desc_, reinterpret_cast<size_t *>(&output_size_)),
                                        "cudnnGetFilterSizeInBytes failed");

    if (use_pad_) {
      CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
        cudnnGetTensorSizeInBytes(padded_descriptor_, reinterpret_cast<size_t *>(&padded_size_)),
        "cudnnGetTensorSizeInBytes failed");
      CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
        cudnnGetConvolutionBackwardFilterWorkspaceSize(cudnn_handle_, padded_descriptor_, dy_desc_, conv_desc_,
                                                       dw_desc_, algo_, reinterpret_cast<size_t *>(&workspace_size_)),
        "cudnnGetConvolutionBackwardFilterWorkspaceSize failed");
      workspace_size_list_.push_back(padded_size_);
    } else {
      CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
        cudnnGetConvolutionBackwardFilterWorkspaceSize(cudnn_handle_, x_desc_, dy_desc_, conv_desc_, dw_desc_, algo_,
                                                       reinterpret_cast<size_t *>(&workspace_size_)),
        "cudnnGetConvolutionBackwardFilterWorkspaceSize failed");
    }
    (void)workspace_size_list_.insert(workspace_size_list_.begin(), workspace_size_);

    if (cudnn_data_type_ == CUDNN_DATA_HALF) {
      workspace_size_list_.push_back(output_size_);
      output_size_list_.push_back(num_output_elements_ * sizeof(float));
    } else {
      output_size_list_.push_back(output_size_);
    }
  }

 private:
  void SetNDDesc(const ShapeVector &dy_shape, const ShapeVector &input_shape, const ShapeVector &filter_shape) {
    const int kDims = 5;
    int dimA[kDims];
    int strideAin[kDims];
    int dimAdy[kDims];
    int strideAdy[kDims];
    int filterDimA[kDims];
    SetDimA(input_shape, dimA, kDims, data_format_);
    SetStrideA(input_shape, strideAin, kDims, data_format_);
    SetDimA(dy_shape, dimAdy, kDims, data_format_);
    SetStrideA(dy_shape, strideAdy, kDims, data_format_);
    SetDimA(filter_shape, filterDimA, kDims, data_format_);
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
      cudnnSetTensorNdDescriptor(dy_desc_, cudnn_data_type_, kDims, dimAdy, strideAdy),
      "cudnnSetTensorNdDescriptor failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
      cudnnSetFilterNdDescriptor(dw_desc_, cudnn_data_type_, compute_format_, kDims, filterDimA),
      "cudnnSetFilterNdDescriptor failed");
    CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnSetTensorNdDescriptor(x_desc_, cudnn_data_type_, kDims, dimA, strideAin),
                                        "cudnnSetTensorNdDescriptor failed");
  }

  void SetStrideAndDilation(std::vector<int64_t> stride_me, std::vector<int64_t> dilation_me) {
    stride_.clear();
    dilation_.clear();
    (void)std::transform(stride_me.begin(), stride_me.end(), std::back_inserter(stride_),
                         [](const int64_t &value) { return static_cast<int>(value); });
    (void)std::transform(dilation_me.begin(), dilation_me.end(), std::back_inserter(dilation_),
                         [](const int64_t &value) { return static_cast<int>(value); });
    if (stride_.size() != k3DStrideSize) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the length of 'stride' must be 5, but got " << stride_.size();
    }
    if (stride_[0] != 1 || stride_[1] != 1) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the value of 'stride' at 0 and 1 axis must be 1, but got "
                        << "stride[0]: " << stride_[0] << ", stride[1]: " << stride_[1];
    }
    if (dilation_.size() != k3DDilationSize) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the length of 'dilation' must be 5, but got "
                        << dilation_.size();
    }
    if (dilation_[0] != 1 || dilation_[1] != 1) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the value of 'dilation' at 0 and 1 axis must be 1, but got "
                        << "dilation[0]: " << dilation_[0] << ", dilation[1]: " << dilation_[1];
    }
  }

  std::vector<int64_t> GetSameModePadList(const ShapeVector &dout_shape_norm, const ShapeVector &x_size_v,
                                          std::vector<int64_t> stride, std::vector<int64_t> dilation,
                                          std::vector<int64_t> kernel_size) {
    constexpr auto kConv3DBackpropFilterPadHalf = 2;
    auto kernel_d = kernel_size[kIndex0];
    auto kernel_h = kernel_size[kIndex1];
    auto kernel_w = kernel_size[kIndex2];
    auto stride_d = stride[kIndex2];
    auto stride_h = stride[kIndex3];
    auto stride_w = stride[kIndex4];
    auto dilation_d = dilation[kIndex2];
    auto dilation_h = dilation[kIndex3];
    auto dilation_w = dilation[kIndex4];
    int64_t pad_head, pad_tail, pad_top, pad_bottom, pad_left, pad_right;

    auto pad_needed_d = (dout_shape_norm[kIndex2] - 1) * stride_d + dilation_d * (kernel_d - 1) + 1 - x_size_v[kIndex2];
    pad_needed_d = 0 > pad_needed_d ? 0 : pad_needed_d;
    pad_head = pad_needed_d / kConv3DBackpropFilterPadHalf;
    pad_tail = pad_needed_d - pad_head;

    auto pad_needed_h = (dout_shape_norm[kIndex3] - 1) * stride_h + dilation_h * (kernel_h - 1) + 1 - x_size_v[kIndex3];
    pad_needed_h = 0 > pad_needed_h ? 0 : pad_needed_h;
    pad_top = pad_needed_h / kConv3DBackpropFilterPadHalf;
    pad_bottom = pad_needed_h - pad_top;

    auto pad_needed_w = (dout_shape_norm[kIndex4] - 1) * stride_w + dilation_w * (kernel_w - 1) + 1 - x_size_v[kIndex4];
    pad_needed_w = 0 > pad_needed_w ? 0 : pad_needed_w;
    pad_left = pad_needed_w / kConv3DBackpropFilterPadHalf;
    pad_right = pad_needed_w - pad_left;

    return std::vector<int64_t>{pad_head, pad_tail, pad_top, pad_bottom, pad_left, pad_right};
  }

  void SetPad(const std::vector<int> &pad_list) {
    (void)CheckSize(pad_list.size(), k3DPadSize, "pad");
    pad_depth_ = pad_list[kHead3DPadIdx];
    pad_height_ = pad_list[kTop3DPadIdx];
    pad_width_ = pad_list[kLeft3DPadIdx];
    use_pad_ = !((pad_depth_ == pad_list[kTail3DPadIdx]) && (pad_height_ == pad_list[kBottom3DPadIdx]) &&
                 (pad_width_ == pad_list[kRight3DPadIdx]));
  }

  cudnnTensorDescriptor_t GetXDescReal(const std::vector<int> &pad_list) {
    cudnnTensorDescriptor_t x_desc_real = nullptr;
    int padA[kConvDims];
    int strideA[kConvDims] = {stride_[kDepth3DStrideIdx], stride_[kHeight3DStrideIdx], stride_[kWidth3DStrideIdx]};
    int dilaA[kConvDims] = {dilation_[kDepth3DDilationIdx], dilation_[kHeight3DDilationIdx],
                            dilation_[kWidth3DDilationIdx]};
    if (use_pad_) {
      pad_depth_ = pad_list[kHead3DPadIdx] + pad_list[kTail3DPadIdx];
      pad_height_ = pad_list[kTop3DPadIdx] + pad_list[kBottom3DPadIdx];
      pad_width_ = pad_list[kLeft3DPadIdx] + pad_list[kRight3DPadIdx];
      pad_head_ = pad_list[kHead3DPadIdx];
      pad_top_ = pad_list[kTop3DPadIdx];
      pad_left_ = pad_list[kLeft3DPadIdx];
      int dimA[kNumDims];
      int strideApadded[kNumDims];
      if (data_format_ != kOpFormat_NCDHW) {
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the value of 'data_format' only support 'NCDHW' right now "
                          << ", but got " << data_format_;
      }
      ShapeVector padded_shape = {n_, c_, old_depth_ + pad_depth_, old_height_ + pad_height_, old_width_ + pad_width_};
      SetDimA(padded_shape, dimA, kNumDims, data_format_);
      SetStrideA(padded_shape, strideApadded, kNumDims, data_format_);
      CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(
        cudnnSetTensorNdDescriptor(padded_descriptor_, cudnn_data_type_, kNumDims, dimA, strideApadded),
        "cudnnSetTensorNdDescriptor failed");
      padA[kPadDepthIdx] = 0;
      padA[kPadHeightIdx] = 0;
      padA[kPadWidthIdx] = 0;
      CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnSetConvolutionNdDescriptor(conv_desc_, kConvDims, padA, strideA, dilaA,
                                                                          CUDNN_CROSS_CORRELATION, CUDNN_DATA_FLOAT),
                                          "cudnnSetConvolutionNdDescriptor failed");
      x_desc_real = padded_descriptor_;
    } else {
      if (pad_mode_ == kValidPadModeUpperCase || pad_mode_ == kValidPadModeLowerCase) {
        pad_depth_ = 0;
        pad_height_ = 0;
        pad_width_ = 0;
      }
      padA[kPadDepthIdx] = pad_depth_;
      padA[kPadHeightIdx] = pad_height_;
      padA[kPadWidthIdx] = pad_width_;
      CHECK_CUDNN_RET_WITH_EXCEPT_NOTRACE(cudnnSetConvolutionNdDescriptor(conv_desc_, kConvDims, padA, strideA, dilaA,
                                                                          CUDNN_CROSS_CORRELATION, CUDNN_DATA_FLOAT),
                                          "cudnnSetConvolutionNdDescriptor failed");
      x_desc_real = x_desc_;
    }

    return x_desc_real;
  }

  cudnnHandle_t cudnn_handle_;
  cudnnFilterDescriptor_t dw_desc_;
  cudnnConvolutionDescriptor_t conv_desc_;
  cudnnTensorDescriptor_t dy_desc_;
  cudnnTensorDescriptor_t x_desc_;
  cudnnTensorDescriptor_t padded_descriptor_;
  cudnnConvolutionBwdFilterAlgo_t algo_;
  std::string pad_mode_;
  std::string data_format_ = kOpFormat_NCDHW;

  const float pad_value_ = 0.0;
  cudnnDataType_t cudnn_data_type_;
  cudnnTensorFormat_t compute_format_;
  int old_depth_;
  int old_height_;
  int old_width_;
  int pad_depth_;
  int pad_height_;
  int pad_width_;
  int pad_head_;
  int pad_top_;
  int pad_left_;
  int n_;
  int c_;
  std::vector<int> stride_;
  std::vector<int> dilation_;
  int group_;
  size_t input_size_;
  size_t dy_size_;
  size_t output_size_;
  size_t padded_size_;
  size_t workspace_size_;
  bool use_pad_;
  size_t num_output_elements_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_CONV3D_GRAD_FILTER_GPU_KERNEL_H_
