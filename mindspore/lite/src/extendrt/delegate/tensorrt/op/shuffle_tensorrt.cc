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

#include "src/extendrt/delegate/tensorrt/op/shuffle_tensorrt.h"
#include <vector>
#include <numeric>
#include <functional>
#include <algorithm>
#include "ops/auto_generate/gen_lite_ops.h"
#include "ops/unsqueeze.h"
#include "ops/squeeze.h"
#include "ops/auto_generate/gen_ops_name.h"

namespace mindspore::lite {
int ShuffleTensorRT::IsSqueezeSupport() {
  constexpr size_t input_count_without_constant = 1;
  constexpr size_t input_count_with_constant = 2;
  if (in_tensors_.size() == input_count_without_constant) {
    auto squeeze_op = AsOps<ops::Squeeze>();
    if (squeeze_op == nullptr) {
      MS_LOG(ERROR) << "SqueezeOp convert failed";
      return RET_ERROR;
    }
    param_axis_ = squeeze_op->get_axis();
  } else if (in_tensors_.size() == input_count_with_constant) {
    if (!in_tensors_[1].IsConst()) {
      MS_LOG(ERROR) << "Expect input 1 to be const when input size is 2, type: " << type_ << ", op: " << op_name_;
      return RET_ERROR;
    }
    auto axis = ConvertTensorAsIntVector(in_tensors_[1]);
    std::copy(axis.begin(), axis.end(), std::back_inserter(param_axis_));
  } else {
    MS_LOG(ERROR) << "Unsupported in_tensors size " << in_tensors_.size() << " of " << type_;
    return RET_ERROR;
  }
  if (param_axis_.empty()) {
    MS_LOG(WARNING) << op_name_ << " is a full dim squeeze, don't support dynamic input shape.";
    dynamic_shape_params_.support_dynamic_ = false;
    dynamic_shape_params_.support_hw_dynamic_ = false;
  }
  return RET_OK;
}
int ShuffleTensorRT::IsSupport(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &in_tensors,
                               const std::vector<TensorInfo> &out_tensors) {
  if (type_ == ops::kNameFlatten || type_ == ops::kNameUnsqueeze) {
    if (in_tensors.size() != 1) {
      MS_LOG(ERROR) << "Unsupported in_tensors size " << in_tensors.size() << " of " << type_;
      return RET_ERROR;
    }
  } else if (type_ == ops::kNameSqueeze) {
    return IsSqueezeSupport();
  } else if (type_ == ops::kNameReshape) {
    if (in_tensors.size() != INPUT_SIZE2) {
      MS_LOG(ERROR) << "PrimitiveType_Transpose Unsupported in_tensors size: " << in_tensors.size();
      return RET_ERROR;
    }
    dynamic_shape_params_.support_hw_dynamic_ = false;
  } else if (type_ == ops::kNameTranspose || type_ == ops::kNameExpandDims) {
    if (in_tensors.size() != INPUT_SIZE2) {
      MS_LOG(ERROR) << "PrimitiveType_Transpose Unsupported in_tensors size: " << in_tensors.size();
      return RET_ERROR;
    }
    if (!in_tensors[1].IsConst()) {
      MS_LOG(ERROR) << "Unsupported shape tensor of " << type_;
      return RET_ERROR;
    }
  } else if (type_ == ops::kNameBroadcastTo) {
    if (in_tensors.size() > INPUT_SIZE2) {
      MS_LOG(ERROR) << "PrimitiveType_Transpose Unsupported in_tensors size: " << in_tensors.size();
      return RET_ERROR;
    }
  } else {
    MS_LOG(ERROR) << "Unsupported op type:" << type_;
    return RET_ERROR;
  }
  return RET_OK;
}

int ShuffleTensorRT::AddInnerOp(TensorRTContext *ctx) {
  if (ctx == nullptr || ctx->network() == nullptr) {
    MS_LOG(ERROR) << "context or network is invalid";
    return RET_ERROR;
  }
  ctx_ = ctx;

  int ret = InputTensorPreprocess(ctx);
  if (ret != RET_OK || shuffler_input_ == nullptr) {
    MS_LOG(ERROR) << "InputTensorPreprocess failed for " << op_name_;
    return RET_ERROR;
  }

  nvinfer1::IShuffleLayer *shuffle_layer = ctx->network()->addShuffle(*shuffler_input_);
  if (shuffle_layer == nullptr) {
    MS_LOG(ERROR) << "add Shuffle op failed for TensorRT.";
    return RET_ERROR;
  }
  shuffle_layer->setName(op_name_.c_str());
  this->layer_ = shuffle_layer;

  ret = RET_OK;
  if (type_ == ops::kNameUnsqueeze) {
    ret = AddUnsqueezeOp(shuffle_layer);
  } else if (type_ == ops::kNameSqueeze) {
    ret = AddSqueezeOp(shuffle_layer);
  } else if (type_ == ops::kNameTranspose) {
    ret = AddTransposeOp(shuffle_layer);
  } else if (type_ == ops::kNameReshape) {
    ret = AddReshapeOp(shuffle_layer);
  } else if (type_ == ops::kNameFlatten) {
    ret = AddFlattenOp(shuffle_layer);
  } else if (type_ == ops::kNameExpandDims) {
    ret = AddExpandDimsOp(shuffle_layer);
  } else if (type_ == ops::kNameBroadcastTo) {
    ret = AddBroadcastToOp(shuffle_layer);
  } else {
    MS_LOG(ERROR) << "Unsupported op type for " << op_name_;
    return RET_ERROR;
  }
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "AddOp failed for " << op_name_;
    return ret;
  }

  if (shuffler_output_ == nullptr) {
    MS_LOG(ERROR) << "output tensor create failed for " << op_name_;
    return RET_ERROR;
  }
  auto output_helper = ITensorHelper{shuffler_output_, out_format_, true};
  ctx->RegisterTensor(output_helper, out_tensors_[0].Name());
  MS_LOG(DEBUG) << "output " << GetTensorFormat(output_helper);
  return RET_OK;
}

int ShuffleTensorRT::InputTensorPreprocess(TensorRTContext *ctx) {
  auto input_0 = in_tensors_[0];
  if (!ctx->HasTensor(input_0.Name()) && input_0.IsConst()) {
    shuffler_input_ = lite::ConvertConstantTensor(ctx_, input_0, op_name_);
    out_format_ = NCHW;
    ctx->RegisterTensor({shuffler_input_}, input_0.Name());
    return RET_OK;
  }
  shuffler_input_ = input(ctx, 0).trt_tensor_;
  MS_LOG(DEBUG) << "before transpose " << GetTensorFormat(input(ctx, 0));
  out_format_ = input(ctx, 0).format_;

  MS_LOG(DEBUG) << "after transpose " << GetTensorFormat(shuffler_input_, out_format_, true);
  return RET_OK;
}

int ShuffleTensorRT::AddSqueezeOp(nvinfer1::IShuffleLayer *shuffle_layer) {
  // axis
  auto squeeze_shape = shuffler_input_->getDimensions();
  std::vector<int64_t> new_shape(squeeze_shape.d, squeeze_shape.d + squeeze_shape.nbDims);
  if (param_axis_.empty()) {
    MS_LOG(WARNING) << op_name_ << " has null axis.";
    for (int i = SizeToInt(new_shape.size()) - 1; i >= 0; i--) {
      if (new_shape[i] == 1) {
        new_shape.erase(new_shape.begin() + i);
      }
    }
    if (new_shape.empty()) {
      new_shape = {1};
    }
  } else {
    for (int i = SizeToInt(param_axis_.size()) - 1; i >= 0; i--) {
      if (param_axis_[i] >= SizeToInt(new_shape.size()) || new_shape[param_axis_[i]] != 1) {
        MS_LOG(WARNING) << "squeeze_shape value at " << i << " is " << param_axis_[i] << ", need check " << op_name_;
      }
      if (param_axis_[i] < 0) {
        param_axis_[i] += shuffler_input_->getDimensions().nbDims;
      }
      new_shape.erase(new_shape.begin() + param_axis_[i]);
    }
  }

  std::vector<int> subscripts(shuffler_input_->getDimensions().nbDims);
  std::iota(subscripts.begin(), subscripts.end(), 0);
  auto p = std::remove_if(subscripts.begin(), subscripts.end(), [&](int x) {
    return std::find(param_axis_.begin(), param_axis_.end(), x) != param_axis_.end();
  });
  subscripts.resize(p - subscripts.begin());
  bool anyValuesUnknown = std::any_of(new_shape.begin(), new_shape.end(), [](int shape) { return shape == -1; });
  if (!anyValuesUnknown) {
    nvinfer1::Dims squeeze_dims = lite::ConvertCudaDims(new_shape);
    shuffle_layer->setReshapeDimensions(squeeze_dims);
  } else {
    auto squeeze_shape_tensor = ctx_->network()->addShape(*shuffler_input_)->getOutput(0);
    auto subscripts_tensor = ctx_->ConvertTo1DTensor(subscripts);
    auto newDims = ctx_->network()->addGather(*squeeze_shape_tensor, *subscripts_tensor, 0)->getOutput(0);
    shuffle_layer->setInput(1, *newDims);
  }
  shuffler_output_ = shuffle_layer->getOutput(0);
  return shuffler_output_ == nullptr ? RET_ERROR : RET_OK;
}

int ShuffleTensorRT::AddUnsqueezeOp(nvinfer1::IShuffleLayer *shuffle_layer) {
  // Unsqueeze
  auto unsqueeze_op = AsOps<ops::Unsqueeze>();
  if (unsqueeze_op == nullptr) {
    MS_LOG(ERROR) << "AddUnsqueezeOp convert failed";
    return RET_ERROR;
  }
  // axis
  param_axis_ = unsqueeze_op->get_axis();
  if (param_axis_.empty()) {
    MS_LOG(ERROR) << "axis is invalid for " << op_name_;
    return RET_ERROR;
  }
  if (param_axis_.size() != 1) {
    MS_LOG(WARNING) << op_name_ << " has unsqueeze axis size: " << param_axis_.size();
  }
  nvinfer1::ITensor *expand_input = shuffler_input_;
  if (input(ctx_, 0).is_tensor == true) {
    for (size_t i = 0; i < param_axis_.size(); i++) {
      expand_input = ExpandDim(ctx_, expand_input, param_axis_[i]);
    }
  }
  shuffler_output_ = expand_input;
  return shuffler_output_ == nullptr ? RET_ERROR : RET_OK;
}

int ShuffleTensorRT::AddTransposeOp(nvinfer1::IShuffleLayer *shuffle_layer) {
  if (shuffler_input_->getDimensions().nbDims != in_tensors_[1].ElementNum()) {
    MS_LOG(WARNING) << "transpose perm is invalid for input, ignore " << op_name_;
    shuffler_output_ = shuffler_input_;
    return RET_OK;
  }
  auto transpose_op = AsOps<ops::Transpose>();
  if (transpose_op == nullptr) {
    MS_LOG(ERROR) << "AddTransposeOp convert failed";
    return RET_ERROR;
  }
  // perm
  auto perm_ternsor = in_tensors_[1];
  if (!perm_ternsor.IsConst()) {
    MS_LOG(ERROR) << "AddTransposeOp perm_ternsor data is invalid: " << op_name_;
    return RET_ERROR;
  }

  nvinfer1::Permutation perm{};
  if (perm_ternsor.DataType() == DataType::kNumberTypeInt64) {
    auto perm_data = reinterpret_cast<const int64_t *>(perm_ternsor.Data());
    for (int64_t i = 0; i < perm_ternsor.ElementNum(); i++) {
      perm.order[i] = perm_data[i];
    }
  } else if (perm_ternsor.DataType() == DataType::kNumberTypeInt32) {
    auto perm_data = reinterpret_cast<const int32_t *>(perm_ternsor.Data());
    for (int64_t i = 0; i < perm_ternsor.ElementNum(); i++) {
      perm.order[i] = perm_data[i];
    }
  } else {
    MS_LOG(ERROR) << op_name_ << " perm tensor data type is " << static_cast<int>(perm_ternsor.DataType());
    return RET_ERROR;
  }

  shuffle_layer->setFirstTranspose(perm);

  shuffler_output_ = shuffle_layer->getOutput(0);
  return RET_OK;
}

int ShuffleTensorRT::AddReshapeOp(nvinfer1::IShuffleLayer *shuffle_layer) {
  auto &shape_tensor = in_tensors_[1];
  if (shape_tensor.IsConst()) {
    // static shuffle layer
    auto reshape_dims = lite::ConvertCudaDims(shape_tensor);
    if (reshape_dims.nbDims == -1) {
      MS_LOG(ERROR) << "ConvertCudaDims failed for " << op_name_;
      return RET_ERROR;
    }
    shuffle_layer->setReshapeDimensions(reshape_dims);
  } else {
    if (in_tensors_.size() != INPUT_SIZE2) {
      MS_LOG(ERROR) << "invalid shape tensor for reshape " << op_name_;
      return RET_ERROR;
    }
    shuffle_layer->setInput(1, *input(ctx_, 1).trt_tensor_);
  }
  shuffler_output_ = shuffle_layer->getOutput(0);
  return RET_OK;
}

int ShuffleTensorRT::AddFlattenOp(nvinfer1::IShuffleLayer *shuffle_layer) {
  nvinfer1::Dims flatten_dims;
  nvinfer1::Dims dims = input(ctx_, 0).trt_tensor_->getDimensions();
  flatten_dims.nbDims = DIMENSION_2D;
  flatten_dims.d[0] = dims.d[0] == -1 ? 0 : dims.d[0];
  flatten_dims.d[1] = std::accumulate(dims.d + 1, dims.d + dims.nbDims, 1, std::multiplies<int32_t>());
  if (flatten_dims.d[1] <= 0) {
    MS_LOG(ERROR) << op_name_ << "infer shape failed";
  }
  shuffle_layer->setReshapeDimensions(flatten_dims);
  shuffler_output_ = shuffle_layer->getOutput(0);
  return RET_OK;
}

int ShuffleTensorRT::AddExpandDimsOp(nvinfer1::IShuffleLayer *shuffle_layer) {
  if (!input(ctx_, 0).is_tensor) {
    shuffler_output_ = shuffler_input_;
    return RET_OK;
  }
  auto axis_vec = ConvertTensorAsIntVector(in_tensors_[1]);
  if (axis_vec.size() != 1) {
    MS_LOG(ERROR) << "Failed to get axis input, dim count " << axis_vec.size() << ", node: " << op_name_;
    return RET_ERROR;
  }
  int axis = axis_vec[0];
  if (axis > (-1 - shuffler_input_->getDimensions().nbDims) && axis < -1) {
    axis = shuffler_input_->getDimensions().nbDims + axis + 1;
  }

  shuffler_output_ = ExpandDim(ctx_, shuffler_input_, axis);
  return shuffler_output_ == nullptr ? RET_ERROR : RET_OK;
}

int ShuffleTensorRT::AddBroadcastToOp(nvinfer1::IShuffleLayer *shuffle_layer) {
  if (in_tensors_.size() > 1 && !in_tensors_[1].IsConst()) {
#if TRT_VERSION_GE(7, 2)
    auto shape_tensor = input(ctx_, 1).trt_tensor_;
    auto input = ctx_->network()->addShape(*shuffler_input_)->getOutput(0);
    auto one_tensor = ctx_->ConvertTo1DTensor(1);
    auto eq_one =
      ctx_->network()->addElementWise(*shape_tensor, *one_tensor, nvinfer1::ElementWiseOperation::kEQUAL)->getOutput(0);
    auto int_eq_one = TRTTensorCast(ctx_, eq_one, nvinfer1::DataType::kINT32, op_name_ + "_cast_int_one");
    auto x = ctx_->network()->addElementWise(*int_eq_one, *input, nvinfer1::ElementWiseOperation::kPROD)->getOutput(0);
    auto zero_tensor = ctx_->ConvertTo1DTensor(0);
    auto not_eq_one =
      ctx_->network()->addElementWise(*zero_tensor, *int_eq_one, nvinfer1::ElementWiseOperation::kEQUAL)->getOutput(0);
    auto int_not_eq_one = TRTTensorCast(ctx_, not_eq_one, nvinfer1::DataType::kINT32, op_name_ + "_cast_int_not_one");
    auto y = ctx_->network()
               ->addElementWise(*int_not_eq_one, *shape_tensor, nvinfer1::ElementWiseOperation::kPROD)
               ->getOutput(0);
    auto new_shape = ctx_->network()->addElementWise(*x, *y, nvinfer1::ElementWiseOperation::kSUM)->getOutput(0);
    shuffler_output_ = Broadcast(ctx_, shuffler_input_, new_shape);
#else
    MS_LOG(WARNING)
      << "low TensorRT version don't support broadcastto op, please upgrade TensorRT version to 7.2 or higher";
    return RET_ERROR;
#endif
  } else {
    std::vector<int> input_shape;
    if (in_tensors_.size() == 1) {
      auto broadcast_op = AsOps<ops::BroadcastTo>();
      auto shape_64 = broadcast_op->get_shape();
      std::transform(shape_64.begin(), shape_64.end(), std::back_inserter(input_shape), [](auto x) { return x; });
    } else {
      input_shape = ConvertTensorAsIntVector(in_tensors_[1]);
    }
    if (input_shape.empty()) {
      MS_LOG(ERROR) << "Failed to get input shape from const input 1, node: " << op_name_;
      return RET_ERROR;
    }

    nvinfer1::Dims in_tensor_dims = shuffler_input_->getDimensions();
    auto input_shape_tensor = ctx_->ConvertTo1DTensor(input_shape);

    while (in_tensor_dims.nbDims < static_cast<int64_t>(input_shape.size())) {
      shuffler_input_ = ExpandDim(ctx_, shuffler_input_, 0);
      if (shuffler_input_->getDimensions().nbDims == -1) {
        MS_LOG(ERROR) << "ConvertCudaDims failed for " << op_name_;
        return RET_ERROR;
      }
      shuffle_layer->setReshapeDimensions(shuffler_input_->getDimensions());
      shuffler_input_ = shuffle_layer->getOutput(0);
      in_tensor_dims = shuffler_input_->getDimensions();
    }

    auto size_tensor = ctx_->network()->addShape(*shuffler_input_)->getOutput(0);
    size_tensor = ctx_->network()
                    ->addElementWise(*input_shape_tensor, *size_tensor, nvinfer1::ElementWiseOperation::kMAX)
                    ->getOutput(0);
    shuffler_output_ = Broadcast(ctx_, shuffler_input_, size_tensor);
  }
  return shuffler_output_ == nullptr ? RET_ERROR : RET_OK;
}

REGISTER_TENSORRT_CREATOR(ops::kNameUnsqueeze, ShuffleTensorRT)
REGISTER_TENSORRT_CREATOR(ops::kNameSqueeze, ShuffleTensorRT)
REGISTER_TENSORRT_CREATOR(ops::kNameReshape, ShuffleTensorRT)
REGISTER_TENSORRT_CREATOR(ops::kNameTranspose, ShuffleTensorRT)
REGISTER_TENSORRT_CREATOR(ops::kNameFlatten, ShuffleTensorRT)
REGISTER_TENSORRT_CREATOR(ops::kNameExpandDims, ShuffleTensorRT)
REGISTER_TENSORRT_CREATOR(ops::kNameBroadcastTo, ShuffleTensorRT)
}  // namespace mindspore::lite
