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

#include "plugin/device/cpu/kernel/mkldnn/conv3d_transpose_cpu_kernel.h"
#include <map>
#include <string>
#include <algorithm>
#include "ops/conv3d_transpose.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr auto kConv3DTranspose = "Conv3DTranspose";
constexpr size_t kConv3DTransposeInputsNum = 2;
constexpr size_t kConv3DTransposeOutputsNum = 1;
}  // namespace

bool Conv3DTransposeCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  group = LongToSize(GetValue<int64_t>(KernelMod::primitive_->GetAttr(GROUP)));
  format = GetValue<std::string>(KernelMod::primitive_->GetAttr(FORMAT));
  auto pad_mode_str = GetValue<std::string>(KernelMod::primitive_->GetAttr(PAD_MODE));
  std::map<std::string, mindspore::PadMode> str2padmode_map = {
    {PAD_MODE_LOWER_SAME, PadMode::SAME},   {PAD_MODE_UPPER_SAME, PadMode::SAME},
    {PAD_MODE_LOWER_VALID, PadMode::VALID}, {PAD_MODE_UPPER_VALID, PadMode::VALID},
    {PAD_MODE_LOWER_PAD, PadMode::PAD},     {PAD_MODE_UPPER_PAD, PadMode::PAD}};
  auto iter = str2padmode_map.find(pad_mode_str);
  if (iter == str2padmode_map.end()) {
    MS_LOG(EXCEPTION) << "For " << kernel_name_ << ", pad_mode is illegal, got " << pad_mode_str;
  } else {
    pad_mode = iter->second;
  }
  strides_include_nc = GetValue<std::vector<int64_t>>(KernelMod::primitive_->GetAttr(STRIDES));
  dilation_include_nc = GetValue<std::vector<int64_t>>(KernelMod::primitive_->GetAttr(DILATIONS));
  return true;
}

int Conv3DTransposeCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  std::vector<int64_t> src_shape = outputs.at(kIndex0)->GetDeviceShapeVector();
  std::vector<int64_t> weight_shape = inputs.at(kIndex1)->GetDeviceShapeVector();
  std::vector<int64_t> dst_shape = inputs.at(kIndex0)->GetDeviceShapeVector();
  size_t src_dim = src_shape.size();
  if (src_dim != SHAPE_5D) {
    MS_LOG(EXCEPTION) << "Conv3DTranspose only supports 5D input, but got " << src_dim << "D!";
  }

  if (src_dim == SHAPE_5D && format != NCDHW) {
    MS_LOG(EXCEPTION) << kernel_name_ << " only supports 5D input with format NCDHW, but got format " << format;
  }

  dnnl::memory::dims kernel_size(weight_shape.begin() + NC_LEN, weight_shape.end());
  if (group > 1) {
    if (src_shape[1] % group != 0) {
      MS_LOG(EXCEPTION) << kernel_name_ << " requires channels must be divided by group!";
    }
    (void)weight_shape.insert(weight_shape.begin(), group);
    weight_shape[1] = weight_shape[1] / group;
  }
  const dnnl::memory::desc src_desc = GetDefaultMemDesc(src_shape);
  const dnnl::memory::desc weights_desc = GetDefaultMemDesc(weight_shape);
  const dnnl::memory::desc dst_desc = GetDefaultMemDesc(dst_shape);
  if (strides_include_nc.size() != src_dim) {
    MS_LOG(EXCEPTION) << kernel_name_ << "requires strides must be " << src_dim << "D, but got "
                      << strides_include_nc.size() << "D!";
  }
  if (dilation_include_nc.size() != src_dim) {
    MS_LOG(EXCEPTION) << kernel_name_ << " requires dilation must be " << src_dim << "D, but got "
                      << dilation_include_nc.size() << "D!";
  }
  const dnnl::memory::dims strides(strides_include_nc.begin() + NC_LEN, strides_include_nc.end());
  const dnnl::memory::dims dilation(dilation_include_nc.begin() + NC_LEN, dilation_include_nc.end());
  dnnl::memory::dims dilates;
  dnnl::memory::dims padding_l;
  dnnl::memory::dims padding_r;
  (void)std::transform(dilation.begin(), dilation.end(), std::back_inserter(dilates),
                       [](const int64_t &value) { return value - 1; });
  PaddingInfo padding_info{pad_mode, kernel_size, strides, dilation, &padding_l, &padding_r};
  GetPadding(src_shape, padding_info);

  const auto forward_desc = CreateDesc<dnnl::convolution_forward::desc>(
    dnnl::prop_kind::forward_training, dnnl::algorithm::convolution_auto, src_desc, weights_desc, dst_desc, strides,
    dilates, padding_l, padding_r);
  const auto forward_prim_desc = CreateDesc<dnnl::convolution_forward::primitive_desc>(forward_desc, engine_);
  const auto backward_desc = CreateDesc<dnnl::convolution_backward_data::desc>(
    dnnl::algorithm::convolution_auto, src_desc, weights_desc, dst_desc, strides, dilates, padding_l, padding_r);
  const auto backward_prim_desc =
    CreateDesc<dnnl::convolution_backward_data::primitive_desc>(backward_desc, engine_, forward_prim_desc);
  primitive_ = CreatePrimitive<dnnl::convolution_backward_data>(backward_prim_desc);
  AddArgument(DNNL_ARG_DIFF_SRC, src_desc);
  AddArgument(DNNL_ARG_WEIGHTS, weights_desc);
  AddArgument(DNNL_ARG_DIFF_DST, dst_desc);
  return KRET_OK;
}

bool Conv3DTransposeCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                         const std::vector<kernel::KernelTensor *> &,
                                         const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kConv3DTransposeInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kConv3DTransposeOutputsNum, kernel_name_);
  SetArgumentHandle(DNNL_ARG_DIFF_SRC, outputs[0]->device_ptr());
  SetArgumentHandle(DNNL_ARG_WEIGHTS, inputs[1]->device_ptr());
  SetArgumentHandle(DNNL_ARG_DIFF_DST, inputs[0]->device_ptr());
  ExecutePrimitive();
  return true;
}

std::vector<KernelAttr> Conv3DTransposeCpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> support_list = {
    KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32)};
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Conv3DTranspose, Conv3DTransposeCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
