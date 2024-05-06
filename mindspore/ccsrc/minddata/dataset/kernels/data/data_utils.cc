/**
 * Copyright 2020-2023 Huawei Technologies Co., Ltd
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

#include "minddata/dataset/kernels/data/data_utils.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "minddata/dataset/core/data_type.h"
#ifdef ENABLE_PYTHON
#include "minddata/dataset/core/pybind_support.h"
#endif
#include "minddata/dataset/core/tensor.h"
#include "minddata/dataset/core/tensor_shape.h"
#include "minddata/dataset/include/dataset/constants.h"
#include "minddata/dataset/kernels/data/type_cast_op.h"
#include "minddata/dataset/util/status.h"

namespace mindspore {
namespace dataset {
template <typename T>
Status OneHotEncodingImpl(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, dsize_t num_classes,
                          int64_t index, double smoothing_rate) {
  RETURN_UNEXPECTED_IF_NULL(output);
  T class_idx;
  if (input->Rank() == 0) {
    RETURN_IF_NOT_OK(input->GetItemAt<T>(&class_idx, {}));
  } else {
    RETURN_IF_NOT_OK(input->GetItemAt<T>(&class_idx, {index}));
  }
  if (class_idx >= static_cast<int64_t>(num_classes)) {
    RETURN_STATUS_UNEXPECTED("OneHot: index values should not bigger than num classes: " + std::to_string(num_classes) +
                             ", but got: " + std::to_string(class_idx));
  }
  if ((*output)->type().IsInt()) {
    RETURN_IF_NOT_OK((*output)->SetItemAt<T>({index, static_cast<dsize_t>(class_idx)}, 1));
  } else if ((*output)->type() == DataType::DE_FLOAT64) {
    auto itr = (*output)->begin<double>();
    auto end = (*output)->end<double>();
    double val = smoothing_rate / static_cast<double>(num_classes);
    for (; itr != end; itr++) {
      *itr = val;
    }
    double value = (1. - smoothing_rate) + val;
    RETURN_IF_NOT_OK((*output)->SetItemAt<double>({index, static_cast<dsize_t>(class_idx)}, value));
  } else {
    RETURN_STATUS_UNEXPECTED("OneHot: signed input case only supports signed int as input but got:" +
                             input->type().ToString());
  }
  return Status::OK();
}

Status OneHotEncoding(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, dsize_t num_classes,
                      double smoothing_rate) {
  RETURN_UNEXPECTED_IF_NULL(output);
  input->Squeeze();

  CHECK_FAIL_RETURN_UNEXPECTED(input->Rank() <= 1,
                               "OneHot: Only support scalar or 1D input, got rank: " + std::to_string(input->Rank()));
  CHECK_FAIL_RETURN_UNEXPECTED(input->type().IsInt(),
                               "OneHot: Only support input of int type, but got: " + input->type().ToString());
  try {
    dsize_t num_elements = 1;
    if (input->Rank() == 1) {
      num_elements = input->shape()[0];
    }
    TensorShape out_shape({num_elements, num_classes});
    std::shared_ptr<Tensor> out;
    mindspore::dataset::DataType type = input->type();
    if (abs(smoothing_rate) > std::numeric_limits<double>::epsilon()) {
      type = DataType(DataType::DE_FLOAT64);
    }
    RETURN_IF_NOT_OK(Tensor::CreateEmpty(out_shape, type, &out));
    RETURN_IF_NOT_OK(out->Zero());
    for (dsize_t i = 0; i < num_elements; ++i) {
      if (input->type() == DataType::DE_INT8) {
        RETURN_IF_NOT_OK(OneHotEncodingImpl<int8_t>(input, &out, num_classes, i, smoothing_rate));
      } else if (input->type() == DataType::DE_INT16) {
        RETURN_IF_NOT_OK(OneHotEncodingImpl<int16_t>(input, &out, num_classes, i, smoothing_rate));
      } else if (input->type() == DataType::DE_INT32) {
        RETURN_IF_NOT_OK(OneHotEncodingImpl<int32_t>(input, &out, num_classes, i, smoothing_rate));
      } else if (input->type() == DataType::DE_INT64) {
        RETURN_IF_NOT_OK(OneHotEncodingImpl<int64_t>(input, &out, num_classes, i, smoothing_rate));
      } else if (input->type() == DataType::DE_UINT8) {
        RETURN_IF_NOT_OK(OneHotEncodingImpl<uint8_t>(input, &out, num_classes, i, smoothing_rate));
      } else if (input->type() == DataType::DE_UINT16) {
        RETURN_IF_NOT_OK(OneHotEncodingImpl<uint16_t>(input, &out, num_classes, i, smoothing_rate));
      } else if (input->type() == DataType::DE_UINT32) {
        RETURN_IF_NOT_OK(OneHotEncodingImpl<uint32_t>(input, &out, num_classes, i, smoothing_rate));
      } else if (input->type() == DataType::DE_UINT64) {
        RETURN_IF_NOT_OK(OneHotEncodingImpl<uint64_t>(input, &out, num_classes, i, smoothing_rate));
      } else {
        RETURN_STATUS_UNEXPECTED("OneHot: OneHot only supports input of int type, but got:" + input->type().ToString());
      }
    }
    out->Squeeze();
    *output = out;
    return Status::OK();
  } catch (const std::exception &e) {
    RETURN_STATUS_UNEXPECTED("OneHot: Unexpected errors occurred: " + std::string(e.what()));
  }
}

Status FillHelper(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *out,
                  const std::shared_ptr<Tensor> &fill_output) {
  RETURN_UNEXPECTED_IF_NULL(out);
  const DataType &input_type = input->type();
  const TensorShape &input_shape = input->shape();
  switch (input_type.value()) {
    case DataType::DE_BOOL: {
      bool value = false;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<bool>(value));
      break;
    }
    case DataType::DE_INT8: {
      int8_t value = 0;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<int8_t>(value));
      break;
    }
    case DataType::DE_UINT8: {
      uint8_t value = 0;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<uint8_t>(value));
      break;
    }
    case DataType::DE_UINT16: {
      uint16_t value = 0;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<uint16_t>(value));
      break;
    }
    case DataType::DE_INT16: {
      int16_t value = 0;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<int16_t>(value));
      break;
    }
    case DataType::DE_UINT32: {
      uint32_t value = 0;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<uint32_t>(value));
      break;
    }
    case DataType::DE_INT32: {
      int32_t value = 0;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<int32_t>(value));
      break;
    }
    case DataType::DE_UINT64: {
      uint64_t value = 0;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<uint64_t>(value));
      break;
    }
    case DataType::DE_INT64: {
      int64_t value = 0;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<int64_t>(value));
      break;
    }
    case DataType::DE_FLOAT16: {
      int64_t value = 0;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<float>(value));
      break;
    }
    case DataType::DE_FLOAT32: {
      float value = 0.;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<float>(value));
      break;
    }
    case DataType::DE_FLOAT64: {
      double value = 0.;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&value, {}));
      RETURN_IF_NOT_OK((*out)->Fill<double>(value));
      break;
    }
    case DataType::DE_STRING:
    case DataType::DE_BYTES: {
      std::vector<std::string> strings;
      std::string_view fill_string_view;
      RETURN_IF_NOT_OK(fill_output->GetItemAt(&fill_string_view, {}));
      std::string fill_string = std::string(fill_string_view);
      for (int i = 0; i < input_shape.NumOfElements(); i++) {
        (void)strings.emplace_back(fill_string);
      }
      RETURN_IF_NOT_OK(Tensor::CreateFromVector(strings, input_shape, DataType(input_type.value()), out));
      break;
    }
    default:
      RETURN_STATUS_UNEXPECTED("Fill: invalid data type, filling values into tensors of type " + input_type.ToString() +
                               " is not supported.");
      break;
  }
  return Status::OK();
}

Status Fill(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output,
            const std::shared_ptr<Tensor> &fill_value) {
  RETURN_UNEXPECTED_IF_NULL(output);
  const DataType &fill_type = fill_value->type();
  const DataType &input_type = input->type();
  const TensorShape &input_shape = input->shape();

  if (fill_type.IsString() || input_type.IsString()) {
    CHECK_FAIL_RETURN_UNEXPECTED(
      fill_type == input_type,
      "Fill: fill_value and the input tensor must be of the same data type when involving strings "
      "or bytes, but got fill_value data type " +
        fill_type.ToString() + " and input tensor data type " + input_type.ToString());
  }

  CHECK_FAIL_RETURN_UNEXPECTED(
    fill_value->shape() == TensorShape({}),
    "Fill: the shape of fill_value is not a scalar, got shape:" + fill_value->shape().ToString());

  std::shared_ptr<Tensor> out, fill_output;

  if (input_type.IsNumeric() && fill_type.IsNumeric() && input_type != fill_type) {
    RETURN_IF_NOT_OK(TypeCast(fill_value, &fill_output, input_type));
  } else {
    fill_output = fill_value;
  }

  if (input_type.IsNumeric()) {
    RETURN_IF_NOT_OK(Tensor::CreateEmpty(input_shape, input_type, &out));
  }
  RETURN_IF_NOT_OK(FillHelper(input, &out, fill_output));
  *output = out;
  return Status::OK();
}

template <typename FROM, typename TO>
void Cast(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output) {
  auto in_itr = input->begin<FROM>();
  auto out_itr = (*output)->begin<TO>();
  auto out_end = (*output)->end<TO>();

  for (; out_itr != out_end; ++in_itr, ++out_itr) {
    *out_itr = static_cast<TO>(*in_itr);
  }
}

template <typename T>
void CastFrom(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output) {
  switch ((*output)->type().value()) {
    case DataType::DE_BOOL:
      Cast<T, bool>(input, output);
      break;
    case DataType::DE_INT8:
      Cast<T, int8_t>(input, output);
      break;
    case DataType::DE_UINT8:
      Cast<T, uint8_t>(input, output);
      break;
    case DataType::DE_INT16:
      Cast<T, int16_t>(input, output);
      break;
    case DataType::DE_UINT16:
      Cast<T, uint16_t>(input, output);
      break;
    case DataType::DE_INT32:
      Cast<T, int32_t>(input, output);
      break;
    case DataType::DE_UINT32:
      Cast<T, uint32_t>(input, output);
      break;
    case DataType::DE_INT64:
      Cast<T, int64_t>(input, output);
      break;
    case DataType::DE_UINT64:
      Cast<T, uint64_t>(input, output);
      break;
    case DataType::DE_FLOAT16:
      Cast<T, float16>(input, output);
      break;
    case DataType::DE_FLOAT32:
      Cast<T, float>(input, output);
      break;
    case DataType::DE_FLOAT64:
      Cast<T, double>(input, output);
      break;
    case DataType::DE_UNKNOWN:
    default:
      MS_LOG(ERROR) << "TypeCast: Casting to type " + (*output)->type().ToString() + " is valid, supported datatype: " +
                         "[bool, int8, uint8, int16, uint16, int32, uint32, int64, uint64, float16, float32, float64].";
      break;
  }
}

// Type cast operator
Status TypeCast(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, const DataType &data_type) {
  RETURN_UNEXPECTED_IF_NULL(output);
  switch (input->type().value()) {
    case DataType::DE_BOOL:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<bool>(input, output);
      break;
    case DataType::DE_INT8:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<int8_t>(input, output);
      break;
    case DataType::DE_UINT8:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<uint8_t>(input, output);
      break;
    case DataType::DE_INT16:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<int16_t>(input, output);
      break;
    case DataType::DE_UINT16:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<uint16_t>(input, output);
      break;
    case DataType::DE_INT32:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<int32_t>(input, output);
      break;
    case DataType::DE_UINT32:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<uint32_t>(input, output);
      break;
    case DataType::DE_INT64:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<int64_t>(input, output);
      break;
    case DataType::DE_UINT64:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<uint64_t>(input, output);
      break;
    case DataType::DE_FLOAT16:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<float16>(input, output);
      break;
    case DataType::DE_FLOAT32:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<float>(input, output);
      break;
    case DataType::DE_FLOAT64:
      RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), data_type, output));
      CastFrom<double>(input, output);
      break;
    case DataType::DE_STRING:
      if (data_type == DataType::DE_STRING) {
        *output = input;
        break;
      } else {
        RETURN_STATUS_UNEXPECTED("TypeCast: TypeCast does not support cast from string to " + data_type.ToString());
      }
    case DataType::DE_BYTES:
      if (data_type == DataType::DE_BYTES) {
        *output = input;
        break;
      } else {
        RETURN_STATUS_UNEXPECTED("TypeCast: TypeCast does not support cast from bytes to " + data_type.ToString());
      }
    case DataType::DE_UNKNOWN:
    default:
      // sanity check, unreachable code.
      RETURN_STATUS_UNEXPECTED(
        "TypeCast: Typecast does not support input with type " + input->type().ToString() + ", supported datatype: " +
        "[bool, int8, uint8, int16, uint16, int32, uint32, int64, uint64, float16, float32, float64].");
  }
  return Status::OK();
}

Status ToFloat16(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output) {
  // initiate new tensor for type cast
  DataType new_type = DataType("float16");
  RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), new_type, output));

  auto in_itr = input->begin<float>();
  auto in_end = input->end<float>();
  auto out_itr = (*output)->begin<float16>();
  auto out_end = (*output)->end<float16>();

  for (; (in_itr != in_end) && (out_itr != out_end); ++in_itr, ++out_itr) {
    float element = *in_itr;
    float float16_max = static_cast<float>(std::numeric_limits<float16>::max());
    float float16_min = static_cast<float>(std::numeric_limits<float16>::lowest());
    if (element > float16_max || element < float16_min) {
      RETURN_STATUS_UNEXPECTED("ToFloat16: value " + std::to_string(element) +
                               "in input data is outside of valid float16 range [" + std::to_string(float16_max) +
                               ", " + std::to_string(float16_min) + "].");
    }

    *out_itr = float16(*in_itr);
  }

  return Status::OK();
}

Status PadEnd(const std::shared_ptr<Tensor> &src, std::shared_ptr<Tensor> *dst, const std::vector<dsize_t> &pad_shape,
              const std::shared_ptr<Tensor> &pad_val) {
  if (pad_val == nullptr) {
    if (src->type().IsNumeric()) {
      return PadEndNumeric(src, dst, pad_shape, 0.0);
    } else {
      return PadEndString(src, dst, pad_shape, "");
    }
  }
  CHECK_FAIL_RETURN_UNEXPECTED(src->type().IsNumeric() == pad_val->type().IsNumeric(),
                               "PadEnd: can not pad numeric and string tensors together, but got: " +
                                 pad_val->type().ToString() + " and " + src->type().ToString() + ".");
  if (pad_val->type().IsNumeric()) {
    std::shared_ptr<Tensor> float_pad_value;
    RETURN_IF_NOT_OK(TypeCast(pad_val, &float_pad_value, DataType(DataType::DE_FLOAT32)));
    float val = 0.;
    RETURN_IF_NOT_OK(float_pad_value->GetItemAt<float>(&val, {}));
    return PadEndNumeric(src, dst, pad_shape, val);
  } else {
    CHECK_FAIL_RETURN_UNEXPECTED(src->type() == pad_val->type(),
                                 "PadEnd: can not pad string and byte tensors together, but got: " +
                                   pad_val->type().ToString() + " and " + src->type().ToString() + ".");
    std::string_view val;
    RETURN_IF_NOT_OK(pad_val->GetItemAt(&val, {}));
    return PadEndString(src, dst, pad_shape, std::string(val));
  }
}

Status PadEndNumeric(const std::shared_ptr<Tensor> &src, std::shared_ptr<Tensor> *dst,
                     const std::vector<dsize_t> &pad_shape, float pad_val) {
  CHECK_FAIL_RETURN_UNEXPECTED(src != nullptr && dst != nullptr, "PadEnd: input or output can't be nullptr");
  if (src->Rank() == 0 || src->shape().AsVector() == pad_shape) {
    (*dst) = src;  // if no padding, copy the pointer
  } else {
    CHECK_FAIL_RETURN_UNEXPECTED(src->Rank() == pad_shape.size(),
                                 "PadEnd: invalid pad shape, as rank of input is: " + std::to_string(src->Rank()) +
                                   ", and rank of pad value: " + std::to_string(pad_shape.size()));
    RETURN_IF_NOT_OK(Tensor::CreateEmpty(TensorShape(pad_shape), src->type(), dst));
    auto tensor_type = src->type().value();
    if (std::fabs(pad_val) <= std::numeric_limits<float>::epsilon()) {  // if pad with zero, don't care what type it is
      RETURN_IF_NOT_OK((*dst)->Zero());
    } else if (tensor_type == DataType::DE_INT8) {
      RETURN_IF_NOT_OK((*dst)->Fill<int8_t>(static_cast<int8_t>(pad_val)));
    } else if (tensor_type == DataType::DE_BOOL) {
      RETURN_IF_NOT_OK((*dst)->Fill<bool>(static_cast<bool>(pad_val)));
    } else if (tensor_type == DataType::DE_UINT8) {
      RETURN_IF_NOT_OK((*dst)->Fill<uint8_t>(static_cast<uint8_t>(pad_val)));
    } else if (tensor_type == DataType::DE_INT16) {
      RETURN_IF_NOT_OK((*dst)->Fill<int16_t>(static_cast<int16_t>(pad_val)));
    } else if (tensor_type == DataType::DE_FLOAT16) {
      RETURN_IF_NOT_OK((*dst)->Fill<float16>(static_cast<float16>(pad_val)));
    } else if (tensor_type == DataType::DE_UINT16) {
      RETURN_IF_NOT_OK((*dst)->Fill<uint16_t>(static_cast<uint16_t>(pad_val)));
    } else if (tensor_type == DataType::DE_INT32) {
      RETURN_IF_NOT_OK((*dst)->Fill<int32_t>(static_cast<int32_t>(pad_val)));
    } else if (tensor_type == DataType::DE_UINT32) {
      RETURN_IF_NOT_OK((*dst)->Fill<uint32_t>(static_cast<uint32_t>(pad_val)));
    } else if (tensor_type == DataType::DE_INT64) {
      RETURN_IF_NOT_OK((*dst)->Fill<int64_t>(static_cast<int64_t>(pad_val)));
    } else if (tensor_type == DataType::DE_UINT64) {
      RETURN_IF_NOT_OK((*dst)->Fill<uint64_t>(static_cast<uint64_t>(pad_val)));
    } else if (tensor_type == DataType::DE_FLOAT32) {
      RETURN_IF_NOT_OK((*dst)->Fill<float>(static_cast<float>(pad_val)));
    } else if (tensor_type == DataType::DE_FLOAT64) {
      RETURN_IF_NOT_OK((*dst)->Fill<double>(static_cast<double>(pad_val)));
    } else {
      RETURN_STATUS_UNEXPECTED(
        "PadEnd: Incorrect/Unknown datatype, supported datatype is: [bool, int8, uint8, int16, uint16, int32, uint32, "
        "int64, uint64, float16, float32, float64].");
    }
    std::vector<dsize_t> cur_ind(src->Rank(), 0);
    RETURN_IF_NOT_OK(PadEndNumericHelper(src, *dst, cur_ind, 0));
  }
  return Status::OK();
}

Status PadEndNumericHelper(const std::shared_ptr<Tensor> &src, const std::shared_ptr<Tensor> &dst,
                           std::vector<dsize_t> cur_ind, size_t cur_dim) {
  if (cur_dim == src->Rank() - 1) {  // if this is the last dimension, copy the data
    RETURN_IF_NOT_OK(dst->CopyLastDimAt(src, cur_ind));
  } else {  // not the last dimension, keep doing recursion
    dsize_t min_ind = std::min(dst->shape()[cur_dim], src->shape()[cur_dim]);
    for (dsize_t i = 0; i < min_ind; i++) {
      cur_ind[cur_dim] = i;
      RETURN_IF_NOT_OK(PadEndNumericHelper(src, dst, cur_ind, cur_dim + 1));
    }
  }
  return Status::OK();
}

Status PadEndString(const std::shared_ptr<Tensor> &src, std::shared_ptr<Tensor> *dst,
                    const std::vector<dsize_t> &pad_shape, const std::string &pad_val) {
  CHECK_FAIL_RETURN_UNEXPECTED(src != nullptr && dst != nullptr, "tensor can't be nullptr");
  if (src->Rank() == 0 || src->shape().AsVector() == pad_shape) {
    (*dst) = src;  // if no padding, copy the pointer
  } else {
    CHECK_FAIL_RETURN_UNEXPECTED(src->Rank() == pad_shape.size(),
                                 "PadEnd: invalid pad shape, as rank of input is: " + std::to_string(src->Rank()) +
                                   ", and rank of pad value: " + std::to_string(pad_shape.size()));
    std::vector<dsize_t> cur_ind(src->Rank(), 0);
    std::vector<std::string> strings;
    RETURN_IF_NOT_OK(PadEndStringHelper(src, &strings, TensorShape(pad_shape), cur_ind, 0, pad_val));
    RETURN_IF_NOT_OK(Tensor::CreateFromVector(strings, TensorShape(pad_shape), src->type(), dst));
  }
  return Status::OK();
}

Status PadEndStringHelper(const std::shared_ptr<Tensor> &src, std::vector<std::string> *dst,
                          const TensorShape &dst_shape, std::vector<dsize_t> cur_ind, size_t cur_dim,
                          const std::string &pad_value) {
  if (cur_dim == src->Rank() - 1) {  // if this is the last dimension, copy the data
    dsize_t min_ind = std::min(dst_shape[cur_dim], src->shape()[cur_dim]);
    for (dsize_t i = 0; i < min_ind; i++) {
      cur_ind[cur_dim] = i;
      std::string_view item;
      RETURN_IF_NOT_OK(src->GetItemAt(&item, cur_ind));
      (void)dst->emplace_back(item);
    }
    for (dsize_t i = min_ind; i < dst_shape[cur_dim]; i++) {
      (void)dst->emplace_back(pad_value);
    }

  } else {  // not the last dimension, keep doing recursion
    dsize_t min_ind = std::min(dst_shape[cur_dim], src->shape()[cur_dim]);
    for (dsize_t i = 0; i < min_ind; i++) {
      cur_ind[cur_dim] = i;
      RETURN_IF_NOT_OK(PadEndStringHelper(src, dst, dst_shape, cur_ind, cur_dim + 1, pad_value));
    }
    dsize_t count = (dst_shape[cur_dim] - min_ind) * dst_shape.Strides()[cur_dim];
    for (dsize_t i = 0; i < count; i++) {
      (void)dst->emplace_back(pad_value);
    }
  }
  return Status::OK();
}

template <typename T>
Status MaskHelper(const std::shared_ptr<Tensor> &input, const std::shared_ptr<Tensor> &output,
                  const std::shared_ptr<Tensor> &value_tensor, RelationalOp op) {
  T value;
  RETURN_IF_NOT_OK(value_tensor->GetItemAt(&value, {}));
  auto in_itr = input->begin<T>();
  auto out_itr = output->begin<bool>();
  for (; in_itr != input->end<T>(); ++in_itr, ++out_itr) {
    switch (op) {
      case RelationalOp::kEqual:
        *out_itr = (*in_itr == value);
        break;
      case RelationalOp::kNotEqual:
        *out_itr = (*in_itr != value);
        break;
      case RelationalOp::kGreater:
        *out_itr = (*in_itr > value);
        break;
      case RelationalOp::kGreaterEqual:
        *out_itr = (*in_itr >= value);
        break;
      case RelationalOp::kLess:
        *out_itr = (*in_itr < value);
        break;
      case RelationalOp::kLessEqual:
        *out_itr = (*in_itr <= value);
        break;
      default:
        RETURN_STATUS_UNEXPECTED(
          "Mask: unknown relational operator, supported operator is: equal, notEqual, greater, less, lessEqual.");
    }
  }
  return Status::OK();
}

Status Mask(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, const std::shared_ptr<Tensor> &value,
            RelationalOp op) {
  CHECK_FAIL_RETURN_UNEXPECTED(input->type().IsNumeric() == value->type().IsNumeric(),
                               "Mask: input datatype does not match the value datatype, both should be numeric or "
                               "non-numerical in the same time.");
  CHECK_FAIL_RETURN_UNEXPECTED(value->shape() == TensorShape::CreateScalar(),
                               "Mask: value shape should be a scalar, got shape:" + value->shape().ToString());

  RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), DataType(DataType::DE_BOOL), output));

  std::unique_ptr<TypeCastOp> value_cast_op = std::make_unique<TypeCastOp>(input->type());
  std::shared_ptr<Tensor> casted_value;
  if (input->type().IsNumeric()) {
    RETURN_IF_NOT_OK(value_cast_op->Compute(value, &casted_value));
  } else {
    casted_value = value;
  }

  switch (input->type().value()) {
    case DataType::DE_BOOL:
      RETURN_IF_NOT_OK(MaskHelper<bool>(input, *output, casted_value, op));
      break;
    case DataType::DE_INT8:
      RETURN_IF_NOT_OK(MaskHelper<int8_t>(input, *output, casted_value, op));
      break;
    case DataType::DE_UINT8:
      RETURN_IF_NOT_OK(MaskHelper<uint8_t>(input, *output, casted_value, op));
      break;
    case DataType::DE_UINT16:
      RETURN_IF_NOT_OK(MaskHelper<uint16_t>(input, *output, casted_value, op));
      break;
    case DataType::DE_INT16:
      RETURN_IF_NOT_OK(MaskHelper<int16_t>(input, *output, casted_value, op));
      break;
    case DataType::DE_UINT32:
      RETURN_IF_NOT_OK(MaskHelper<uint32_t>(input, *output, casted_value, op));
      break;
    case DataType::DE_INT32:
      RETURN_IF_NOT_OK(MaskHelper<int32_t>(input, *output, casted_value, op));
      break;
    case DataType::DE_UINT64:
      RETURN_IF_NOT_OK(MaskHelper<uint64_t>(input, *output, casted_value, op));
      break;
    case DataType::DE_INT64:
      RETURN_IF_NOT_OK(MaskHelper<int64_t>(input, *output, casted_value, op));
      break;
    case DataType::DE_FLOAT16:
      RETURN_IF_NOT_OK(MaskHelper<float16>(input, *output, casted_value, op));
      break;
    case DataType::DE_FLOAT32:
      RETURN_IF_NOT_OK(MaskHelper<float>(input, *output, casted_value, op));
      break;
    case DataType::DE_FLOAT64:
      RETURN_IF_NOT_OK(MaskHelper<double>(input, *output, casted_value, op));
      break;
    case DataType::DE_STRING:
      RETURN_IF_NOT_OK(MaskHelper<std::string_view>(input, *output, casted_value, op));
      break;
    case DataType::DE_BYTES:
      RETURN_IF_NOT_OK(MaskHelper<std::string_view>(input, *output, casted_value, op));
      break;
    case DataType::DE_UNKNOWN:
    default:
      RETURN_STATUS_UNEXPECTED(
        "Mask: unsupported input datatype, support datatype is:[bool, int8, uint8, int16, uint16, int32, uint32, "
        "int64, uint64, float16, float32, float64, string, bytes].");
      break;
  }
  return Status::OK();
}

Status Concatenate(const TensorRow &input, TensorRow *output, int8_t axis, const std::shared_ptr<Tensor> &prepend,
                   const std::shared_ptr<Tensor> &append) {
  CHECK_FAIL_RETURN_UNEXPECTED(input.size() > 0, "Concatenate: input cannot be empty.");
  axis = Tensor::HandleNeg(axis, input[0]->shape().Rank());
  CHECK_FAIL_RETURN_UNEXPECTED(
    axis == 0, "Concatenate: only 1D input supported, input 'axis' should be 0, but got: " + std::to_string(axis));

  TensorShape t = TensorShape::CreateScalar();

  DataType first_dtype = input[0]->type();

  TensorRow tensor_list;

  if (prepend != nullptr) {
    CHECK_FAIL_RETURN_UNEXPECTED(
      first_dtype == prepend->type(),
      "Concatenate: input datatype does not match the prepend datatype, got input datatype: " + first_dtype.ToString() +
        ", prepend datatype:" + prepend->type().ToString());
    CHECK_FAIL_RETURN_UNEXPECTED(
      prepend->shape().Rank() == 1,
      "Concatenate: only 1D input supported, got rank of prepend: " + std::to_string(prepend->shape().Rank()));
    tensor_list.emplace_back(prepend);
  }

  for (const auto &tensor : input) {
    CHECK_FAIL_RETURN_UNEXPECTED(first_dtype == tensor->type(), "Concatenate: inconsistent datatype of input.");
    CHECK_FAIL_RETURN_UNEXPECTED(
      tensor->shape().Rank() == 1,
      "Concatenate: only 1D input supported, got rank of input: " + std::to_string(tensor->shape().Rank()));
    tensor_list.emplace_back(tensor);
  }

  if (append != nullptr) {
    CHECK_FAIL_RETURN_UNEXPECTED(
      first_dtype == append->type(),
      "Concatenate: input datatype does not match the append datatype, got input datatype: " + first_dtype.ToString() +
        ", append datatype:" + append->type().ToString());
    CHECK_FAIL_RETURN_UNEXPECTED(
      append->shape().Rank() == 1,
      "Concatenate: only 1D append supported, got rank of append:" + std::to_string(append->shape().Rank()));
    tensor_list.emplace_back(append);
  }

  //  create final shape
  for (dsize_t i = 0; i < tensor_list[0]->shape().Rank(); i++) {
    if (i != axis) {
      t = t.AppendDim(tensor_list[0]->shape()[i]);
    } else {
      dsize_t new_shape = 0;
      for (auto &tensor : tensor_list) {
        new_shape = tensor->shape()[i] + new_shape;
      }
      t = t.AppendDim(new_shape);
    }
  }

  std::shared_ptr<Tensor> out;

  if (input[0]->type().IsNumeric()) {
    RETURN_IF_NOT_OK(Tensor::CreateEmpty(t, tensor_list[0]->type(), &out));
    std::vector<dsize_t> index(axis + 1, 0);

    auto n = index.size() - 1;
    for (auto &tensor : tensor_list) {
      RETURN_IF_NOT_OK(out->InsertTensor({index}, tensor, true));
      index[n] = index[n] + tensor->shape()[axis];
    }
  } else {
    std::vector<std::string> strings;

    for (auto &i : tensor_list) {
      auto itr = i->begin<std::string_view>();
      for (; itr != i->end<std::string_view>(); ++itr) {
        (void)strings.emplace_back(*itr);
      }
    }
    RETURN_IF_NOT_OK(Tensor::CreateFromVector(strings, t, input[0]->type(), &out));
  }

  output->push_back(out);

  return Status::OK();
}

#ifndef ENABLE_ANDROID
Status BatchTensorToCVTensorVector(const std::shared_ptr<Tensor> &input,
                                   std::vector<std::shared_ptr<CVTensor>> *output) {
  RETURN_UNEXPECTED_IF_NULL(output);
  std::vector<int64_t> tensor_shape = input->shape().AsVector();
  TensorShape remaining({-1});
  std::vector<int64_t> index(tensor_shape.size(), 0);
  CHECK_FAIL_RETURN_UNEXPECTED(
    tensor_shape.size() > 1,
    "MixUpBatch: input must be at least 2-D in order to unpack, but got rank: " + std::to_string(tensor_shape.size()));
  TensorShape element_shape(std::vector<int64_t>(tensor_shape.begin() + 1, tensor_shape.end()));

  for (; index[0] < tensor_shape[0]; index[0]++) {
    uchar *start_addr_of_index = nullptr;
    std::shared_ptr<Tensor> out;

    RETURN_IF_NOT_OK(input->StartAddrOfIndex(index, &start_addr_of_index, &remaining));
    RETURN_IF_NOT_OK(Tensor::CreateFromMemory(element_shape, input->type(), start_addr_of_index, &out));
    std::shared_ptr<CVTensor> cv_out = CVTensor::AsCVTensor(std::move(out));
    if (!cv_out->mat().data) {
      RETURN_STATUS_UNEXPECTED("[Internal ERROR] MixUpBatch: allocate memory failed.");
    }
    output->push_back(cv_out);
  }
  return Status::OK();
}
#endif

Status BatchTensorToTensorVector(const std::shared_ptr<Tensor> &input, std::vector<std::shared_ptr<Tensor>> *output) {
  RETURN_UNEXPECTED_IF_NULL(output);
  std::vector<int64_t> tensor_shape = input->shape().AsVector();
  TensorShape remaining({-1});
  std::vector<int64_t> index(tensor_shape.size(), 0);
  CHECK_FAIL_RETURN_UNEXPECTED(
    tensor_shape.size() > 1,
    "CutMixBatch: input must be at least 2-D in order to unpack, but got rank:" + std::to_string(tensor_shape.size()));
  TensorShape element_shape(std::vector<int64_t>(tensor_shape.begin() + 1, tensor_shape.end()));

  for (; index[0] < tensor_shape[0]; index[0]++) {
    uchar *start_addr_of_index = nullptr;
    std::shared_ptr<Tensor> out;

    RETURN_IF_NOT_OK(input->StartAddrOfIndex(index, &start_addr_of_index, &remaining));
    RETURN_IF_NOT_OK(Tensor::CreateFromMemory(element_shape, input->type(), start_addr_of_index, &out));
    output->push_back(out);
  }
  return Status::OK();
}

Status TensorVectorToBatchTensor(const std::vector<std::shared_ptr<Tensor>> &input, std::shared_ptr<Tensor> *output) {
  RETURN_UNEXPECTED_IF_NULL(output);
  CHECK_FAIL_RETURN_UNEXPECTED(!input.empty(), "CutMixBatch: the input is empty.");
  std::vector<int64_t> tensor_shape = input.front()->shape().AsVector();
  (void)tensor_shape.insert(tensor_shape.begin(), input.size());
  RETURN_IF_NOT_OK(Tensor::CreateEmpty(TensorShape(tensor_shape), input.at(0)->type(), output));
  for (int i = 0; i < input.size(); i++) {
    RETURN_IF_NOT_OK((*output)->InsertTensor({i}, input[i]));
  }
  return Status::OK();
}

template <typename T>
struct UniqueOpHashMap {
  using map_type = std::unordered_map<T, int32_t>;
};
#ifndef ENABLE_ANDROID
template <>
struct UniqueOpHashMap<float16> {
  using map_type = std::unordered_map<float16, int32_t>;
};

#else
struct gn_hash {
  size_t operator()(const float16 &f) const { return static_cast<std::size_t>(f); }
};

template <>
struct UniqueOpHashMap<float16> {
  using map_type = std::unordered_map<float16, int32_t, gn_hash>;
};
#endif

template <>
struct UniqueOpHashMap<float> {
  using map_type = std::unordered_map<float, int32_t>;
};

template <>
struct UniqueOpHashMap<double> {
  using map_type = std::unordered_map<double, int32_t>;
};

template <typename T>
Status UniqueHelper(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output,
                    std::shared_ptr<Tensor> *output_idx, std::shared_ptr<Tensor> *output_cnt) {
  const dsize_t N = input->Size();
  RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), DataType(DataType::DE_INT32), output_idx));

  typename UniqueOpHashMap<T>::map_type uniq;
  uniq.reserve(2 * N);
  auto in_iter = input->begin<T>();
  auto out_idx_iter = (*output_idx)->begin<int32_t>();
  int32_t i = 0;
  for (; in_iter != input->end<T>(); ++in_iter, ++out_idx_iter) {
    auto it = uniq.emplace(*in_iter, i);
    *out_idx_iter = it.first->second;
    if (it.second) {
      ++i;
    }
  }
  auto uniq_size = uniq.size();
  RETURN_IF_NOT_OK(Tensor::CreateEmpty(TensorShape({static_cast<int32_t>(uniq_size)}), input->type(), output));
  auto out_iter = (*output)->begin<T>();
  for (const auto &item : uniq) {
    *(out_iter + static_cast<ptrdiff_t>(item.second)) = item.first;
  }
  RETURN_IF_NOT_OK(
    Tensor::CreateEmpty(TensorShape({static_cast<int32_t>(uniq_size)}), DataType(DataType::DE_INT32), output_cnt));
  RETURN_IF_NOT_OK((*output_cnt)->Zero());

  auto out_cnt_iter = (*output_cnt)->begin<int32_t>();
  out_idx_iter = (*output_idx)->begin<int32_t>();
  for (int32_t j = 0; j < N; ++j) {
    auto idx = *(out_idx_iter + static_cast<ptrdiff_t>(j));
    ++*(out_cnt_iter + static_cast<ptrdiff_t>(idx));
  }
  return Status::OK();
}

Status Unique(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output,
              std::shared_ptr<Tensor> *output_idx, std::shared_ptr<Tensor> *output_cnt) {
  CHECK_FAIL_RETURN_UNEXPECTED(input->shape().Rank() == 1, "Unique: only 1D input supported, but got rank: " +
                                                             std::to_string(input->shape().Rank()));
  if (input->type() == DataType::DE_INT64) {
    RETURN_IF_NOT_OK(UniqueHelper<int64_t>(input, output, output_idx, output_cnt));
  } else if (input->type() == DataType::DE_INT32) {
    RETURN_IF_NOT_OK(UniqueHelper<int32_t>(input, output, output_idx, output_cnt));
  } else if (input->type() == DataType::DE_INT16) {
    RETURN_IF_NOT_OK(UniqueHelper<int16_t>(input, output, output_idx, output_cnt));
  } else if (input->type() == DataType::DE_INT8) {
    RETURN_IF_NOT_OK(UniqueHelper<int8_t>(input, output, output_idx, output_cnt));
  } else if (input->type() == DataType::DE_UINT64) {
    RETURN_IF_NOT_OK(UniqueHelper<uint64_t>(input, output, output_idx, output_cnt));
  } else if (input->type() == DataType::DE_UINT32) {
    RETURN_IF_NOT_OK(UniqueHelper<uint32_t>(input, output, output_idx, output_cnt));
  } else if (input->type() == DataType::DE_UINT16) {
    RETURN_IF_NOT_OK(UniqueHelper<uint16_t>(input, output, output_idx, output_cnt));
  } else if (input->type() == DataType::DE_UINT8) {
    RETURN_IF_NOT_OK(UniqueHelper<uint8_t>(input, output, output_idx, output_cnt));
  } else if (input->type() == DataType::DE_FLOAT16) {
    RETURN_IF_NOT_OK(UniqueHelper<float16>(input, output, output_idx, output_cnt));
  } else if (input->type() == DataType::DE_FLOAT32) {
    RETURN_IF_NOT_OK(UniqueHelper<float>(input, output, output_idx, output_cnt));
  } else if (input->type() == DataType::DE_FLOAT64) {
    RETURN_IF_NOT_OK(UniqueHelper<double>(input, output, output_idx, output_cnt));
  } else {
    RETURN_STATUS_UNEXPECTED("Unique: Unique op only supports numeric input.");
  }
  return Status::OK();
}
}  // namespace dataset
}  // namespace mindspore
