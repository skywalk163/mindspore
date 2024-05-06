/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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
#include "src/litert/kernel/cpu/int8/argminmax_int8.h"
#include "schema/model_generated.h"
#include "src/litert/kernel_registry.h"

using mindspore::lite::RET_ERROR;
using mindspore::lite::RET_OK;

using mindspore::lite::KernelRegistrar;
using mindspore::lite::RET_FORMAT_ERR;
using mindspore::lite::RET_PARAM_INVALID;
using mindspore::schema::PrimitiveType_ArgMaxFusion;
using mindspore::schema::PrimitiveType_ArgMinFusion;

namespace mindspore::kernel {
ArgMinMaxInt8CPUKernel::~ArgMinMaxInt8CPUKernel() {
  if (in_quant_arg_ != nullptr) {
    free(in_quant_arg_);
    in_quant_arg_ = nullptr;
  }
  if (out_quant_arg_ != nullptr) {
    free(out_quant_arg_);
    out_quant_arg_ = nullptr;
  }
  if (compute_param_ != nullptr) {
    free(compute_param_);
    compute_param_ = nullptr;
  }
}

int ArgMinMaxInt8CPUKernel::Prepare() {
  CHECK_LESS_RETURN(in_tensors_.size(), C1NUM);
  CHECK_LESS_RETURN(out_tensors_.size(), C1NUM);
  CHECK_NULL_RETURN(in_tensors_[0]);
  CHECK_NULL_RETURN(out_tensors_[0]);
  if (in_tensors_[0]->data_type() != mindspore::kNumberTypeInt8 ||
      out_tensors_[0]->data_type() != mindspore::kNumberTypeInt8) {
    MS_LOG(ERROR) << "Datatype error, input0 data_type is " << in_tensors_[0]->data_type() << ", output data_type is "
                  << out_tensors_[0]->data_type();
    return RET_ERROR;
  }
  in_quant_arg_ = reinterpret_cast<QuantArg *>(malloc(sizeof(QuantArg)));
  if (in_quant_arg_ == nullptr) {
    MS_LOG(ERROR) << "Malloc QuantArg for argmin or argmax int8 op failed!";
    return RET_ERROR;
  }
  auto *input_tensor = in_tensors_.at(kInputIndex);
  auto in_quant_args = input_tensor->quant_params();
  CHECK_LESS_RETURN(in_quant_args.size(), 1);
  in_quant_arg_->scale_ = in_quant_args.front().scale;
  in_quant_arg_->zp_ = in_quant_args.front().zeroPoint;

  auto *out_tensor = out_tensors_.at(kOutputIndex);
  auto out_quant_args = out_tensor->quant_params();
  CHECK_LESS_RETURN(out_quant_args.size(), 1);
  out_quant_arg_ = reinterpret_cast<QuantArg *>(malloc(sizeof(QuantArg)));
  out_quant_arg_->scale_ = out_quant_args.front().scale;
  out_quant_arg_->zp_ = out_quant_args.front().zeroPoint;
  if (out_quant_arg_ == nullptr) {
    MS_LOG(ERROR) << "Malloc QuantArg for argmin or argmax int8 op failed!";
    return RET_ERROR;
  }

  compute_param_ = reinterpret_cast<ArgMinMaxComputeParam *>(sizeof(ArgMinMaxComputeParam));
  if (compute_param_ == nullptr) {
    MS_LOG(ERROR) << "Malloc ArgMinMaxComputeParam for argmin or argmax int8 op failed!";
    return RET_ERROR;
  }
  auto param = reinterpret_cast<ArgMinMaxParameter *>(op_parameter_);
  CHECK_NULL_RETURN(param);
  compute_param_->axis_ = param->axis_;
  compute_param_->topk_ = param->topk_;
  compute_param_->out_value_ = param->out_value_;
  compute_param_->keep_dims_ = param->keep_dims_;

  if (!InferShapeDone()) {
    return RET_OK;
  }
  return ReSize();
}

int ArgMinMaxInt8CPUKernel::ReSize() {
  auto in_shape = in_tensors_.at(0)->shape();
  auto dims_size = in_shape.size();
  CHECK_LESS_RETURN(in_shape.size(), 1);
  auto param = reinterpret_cast<ArgMinMaxParameter *>(op_parameter_);
  CHECK_NULL_RETURN(param);
  int axis = param->axis_ < 0 ? param->axis_ + dims_size : param->axis_;
  compute_param_->axis_ = axis;
  compute_param_->dims_size_ = dims_size;
  if (compute_param_->topk_ <= 0) {
    MS_LOG(ERROR) << "Invalid topk " << param->topk_;
    return RET_ERROR;
  }
  compute_param_->topk_ = MSMIN(param->topk_, in_shape.at(axis));
  CHECK_NULL_RETURN(in_shape.data());
  ComputeStrides(in_shape.data(), compute_param_->in_strides_, in_shape.size());
  auto out_shape = out_tensors_.at(0)->shape();
  CHECK_NULL_RETURN(out_shape.data());
  ComputeStrides(out_shape.data(), compute_param_->out_strides_, out_shape.size());
  return RET_OK;
}

int ArgMinMaxInt8CPUKernel::Run() {
  auto input = in_tensors_.at(0);

  const int8_t *input_data = reinterpret_cast<const int8_t *>(in_tensors_.at(0)->MutableData());
  int8_t *output_data = reinterpret_cast<int8_t *>(out_tensors_.at(0)->MutableData());
  int8_t *output_value = nullptr;
  if (out_tensors_.size() == C2NUM) {
    output_value = reinterpret_cast<int8_t *>(out_tensors_.at(C1NUM)->MallocData());
  }
  CHECK_NULL_RETURN(input_data);
  CHECK_NULL_RETURN(output_data);
  int *in_shape = input->ConvertToTensorC()->shape_;
  CHECK_NULL_RETURN(in_shape);
  if (compute_param_->topk_ == 1) {
    Int8ArgMinMaxQuant(input_data, output_data, output_value, in_shape, compute_param_, in_quant_arg_, out_quant_arg_);
    return RET_OK;
  }
  CHECK_NULL_RETURN(in_quant_arg_);
  CHECK_NULL_RETURN(out_quant_arg_);
  switch (compute_param_->axis_) {
    case 0:
      Int8ArgMinMaxDim0(input_data, output_data, output_value, in_shape, compute_param_, in_quant_arg_, out_quant_arg_);
      break;
    case 1:
      Int8ArgMinMaxDim1(input_data, output_data, output_value, in_shape, compute_param_, in_quant_arg_, out_quant_arg_);
      break;
    case 2:
      Int8ArgMinMaxDim2(input_data, output_data, output_value, in_shape, compute_param_, in_quant_arg_, out_quant_arg_);
      break;
    case 3:
      Int8ArgMinMaxDim3(input_data, output_data, output_value, in_shape, compute_param_, in_quant_arg_, out_quant_arg_);
      break;
    default:
      MS_LOG(ERROR) << "axis is invalid";
      return RET_ERROR;
  }
  return RET_OK;
}

REG_KERNEL(kCPU, kNumberTypeInt8, PrimitiveType_ArgMaxFusion, LiteKernelCreator<ArgMinMaxInt8CPUKernel>)
REG_KERNEL(kCPU, kNumberTypeInt8, PrimitiveType_ArgMinFusion, LiteKernelCreator<ArgMinMaxInt8CPUKernel>)
}  // namespace mindspore::kernel
