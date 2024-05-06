/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#include "pybind_api/ir/tensor_index_py.h"
#include <pybind11/stl.h>
#include <memory>
#include <string>
#include <algorithm>
#include <utility>
#include <vector>
#include <functional>
#include "pybind11/pytypes.h"
#include "pipeline/jit/ps/parse/parse_base.h"
#include "utils/hash_set.h"
#include "utils/log_adapter.h"
#include "pipeline/pynative/pynative_execute.h"
#include "mindspore/core/ops/array_ops.h"

namespace mindspore::tensor {
using tensor::TensorPy;
py::handle TensorIndex::py_index_handle_ = py::none();
py::handle TensorIndex::py_value_handle_ = py::none();
bool TensorIndex::is_ascend_ = false;
IndexOpType TensorIndex::index_op_type_ = IndexOpType::GetItem;
py::module TensorIndex::np_module_ = py::module();
static const std::vector<TypeId> kIntTypes{kNumberTypeInt8, kNumberTypeInt16, kNumberTypeInt32, kNumberTypeInt64};
// ***********************************************utils*******************************************
std::ostream &operator<<(std::ostream &stream, const TensorIndex &tensor_index) {
  TensorIndexType tensor_index_type = tensor_index.type();
  switch (tensor_index_type) {
    case TensorIndexType::None: {
      stream << "None";
      break;
    }
    case TensorIndexType::Integer: {
      stream << tensor_index.integer();
      break;
    }
    case TensorIndexType::Ellipsis: {
      stream << "...";
      break;
    }
    case TensorIndexType::Boolean: {
      stream << std::boolalpha << tensor_index.boolean();
      break;
    }
    case TensorIndexType::Slice: {
      stream << tensor_index.slice();
      break;
    }
    case TensorIndexType::Tensor: {
      MS_EXCEPTION_IF_NULL(tensor_index.tensor());
      stream << tensor_index.tensor()->ToString();
      break;
    }
    case TensorIndexType::List: {
      stream << tensor_index.list();
      break;
    }
    case TensorIndexType::Tuple: {
      stream << tensor_index.tuple();
      break;
    }
    case TensorIndexType::Array: {
      stream << tensor_index.array();
      break;
    }
    case TensorIndexType::Float: {
      stream << tensor_index.floating_point();
      break;
    }
  }
  return stream;
}

std::ostream &operator<<(std::ostream &stream, const std::vector<TensorIndex> &tensor_indices) {
  stream << "(";
  for (size_t i = 0; i < tensor_indices.size(); i++) {
    stream << tensor_indices[i];
    if (i < tensor_indices.size() - 1) {
      stream << ", ";
    }
  }
  stream << ")";
  return stream;
}

void TensorIndex::CheckGetItemIndex(const TensorIndexType &index_data_type) {
  bool valid = CheckTypeIsInstance<TensorIndexType>(
    index_data_type,
    {TensorIndexType::Tensor, TensorIndexType::List, TensorIndexType::Boolean, TensorIndexType::Slice,
     TensorIndexType::Integer, TensorIndexType::Tuple, TensorIndexType::Ellipsis, TensorIndexType::None});
  if (!valid) {
    MS_EXCEPTION(IndexError)
      << "Only support integers, slices(`:`), ellipsis(`...`), None, bool, tensor, int, list and "
         "tuple as index, but got "
      << TensorIndex::py_index_handle_ << " with type " << TensorIndex::py_index_handle_.get_type();
  }
}

void TensorIndex::CheckSetItemIndex(const TensorIndexType &index_data_type, const TensorIndexType &value_data_type) {
  CheckGetItemIndex(index_data_type);
  bool valid = CheckTypeIsInstance<TensorIndexType>(
    value_data_type, {TensorIndexType::Integer, TensorIndexType::Float, TensorIndexType::Boolean,
                      TensorIndexType::Tensor, TensorIndexType::List, TensorIndexType::Tuple});
  if (!valid) {
    MS_EXCEPTION(TypeError) << "only support numbers, Tensor, tuple, list as value, but got "
                            << TensorIndex::py_value_handle_ << " with type "
                            << TensorIndex::py_value_handle_.get_type();
  }
}

ShapeVector TensorIndex::BroadCastShape(const ShapeVector &x_shape, const ShapeVector &y_shape) {
  if (x_shape == y_shape) {
    return x_shape;
  }
  const size_t x_len = x_shape.size();
  const size_t y_len = y_shape.size();
  const size_t min_length = std::min(x_len, y_len);
  ShapeVector broadcast_shape_back;

  for (size_t i = 0; i < min_length; i++) {
    size_t x_shape_index = x_len - min_length + i;
    size_t y_shape_index = y_len - min_length + i;
    if (x_shape[x_shape_index] == 1) {
      (void)broadcast_shape_back.emplace_back(y_shape[y_shape_index]);
    } else if (y_shape[y_shape_index] == 1 || x_shape[x_shape_index] == y_shape[y_shape_index]) {
      (void)broadcast_shape_back.emplace_back(x_shape[x_shape_index]);
    } else {
      string index_op_type = index_op_type_ == IndexOpType::GetItem ? "tensor getitem" : "tensor setitem";
      MS_EXCEPTION(ValueError) << "For '" << index_op_type
                               << "', x.shape and y.shape need to broadcast. The value of x.shape["
                               << std::to_string(x_shape_index) << "] or y.shape[" << std::to_string(y_shape_index)
                               << "] must be 1 or -1 when they are not the same, but got x.shape = " << x_shape
                               << " and y.shape = " << y_shape;
    }
  }
  ShapeVector broadcast_shape_front;
  if (min_length == x_len) {
    (void)broadcast_shape_front.insert(
      broadcast_shape_front.end(), y_shape.begin(),
      y_shape.begin() + static_cast<int64_t>(y_len) - static_cast<int64_t>(min_length));
  } else {
    (void)broadcast_shape_front.insert(
      broadcast_shape_front.end(), x_shape.begin(),
      x_shape.begin() + static_cast<int64_t>(x_len) - static_cast<int64_t>(min_length));
  }
  (void)broadcast_shape_front.insert(broadcast_shape_front.end(), broadcast_shape_back.begin(),
                                     broadcast_shape_back.end());
  return broadcast_shape_front;
}

template <typename T>
TensorIndex TensorIndex::SequenceToTensor(const T &sequence, int64_t dim_size) {
  if (sequence.empty()) {
    return TensorIndex(py::bool_(false));
  }
  if (std::all_of(sequence.begin(), sequence.end(), [](auto &x) { return py::isinstance<py::bool_>(x); })) {
    int64_t seq_size = SizeToLong(sequence.size());
    if (seq_size != dim_size) {
      MS_EXCEPTION(IndexError) << "dimension is " << dim_size << " but corresponding boolean dimension is " << seq_size;
    }
    py::list new_range_dim_size;
    for (size_t i = 0; i < sequence.size(); i++) {
      if (py::cast<bool>(sequence[i]) == true) {
        new_range_dim_size.append(py::int_(i));
      }
    }
    if (new_range_dim_size.empty()) {
      return TensorIndex(py::bool_(false));
    }
    return TensorIndex(TensorPy::MakeTensor(MakeNdArray(new_range_dim_size, dim_size)));
  }
  py::array output = MakeNdArray(sequence, dim_size);
  if (output.dtype() == pybind11::dtype("object")) {
    MS_LOG(EXCEPTION) << "Sequence as indices must have the same size across all dimensions and elements must be "
                         "integer (or boolean) type";
  }
  return TensorIndex(TensorPy::MakeTensor(output));
}

py::object TensorIndex::Unpack(const py::object &x) {
  if (py::isinstance<py::tuple>(x)) {
    auto new_x = x.cast<py::tuple>();
    if (new_x.size() == 1) {
      return Unpack(new_x[0]);
    }
  }
  if (py::isinstance<py::list>(x)) {
    auto new_x = x.cast<py::list>();
    if (new_x.size() == 1) {
      return Unpack(new_x[0]);
    }
  }
  return x;
}

template <typename T>
TensorIndex TensorIndex::UnpackTuple(const T &sequence) {
  py::tuple res(sequence.size());
  for (size_t i = 0; i < sequence.size(); i++) {
    if (py::isinstance<py::list>(sequence[i]) || py::isinstance<py::tuple>(sequence[i])) {
      res[i] = Unpack(sequence[i]);
    } else {
      res[i] = sequence[i];
    }
  }
  return TensorIndex(res);
}

py::object TensorIndex::DeepList(const py::object &array_like, int64_t dim_size) {
  py::object new_array_like = CheckRange(array_like, dim_size);
  if (py::isinstance<py::list>(array_like) || py::isinstance<py::tuple>(array_like)) {
    auto list_array_like = array_like.cast<py::list>();
    for (size_t i = 0; i < list_array_like.size(); i++) {
      list_array_like[i] = DeepList(list_array_like[i], dim_size);
    }
    return list_array_like;
  }
  return new_array_like;
}

py::object TensorIndex::DeepTensorToNdArray(const py::object &array_like) {
  if (py::isinstance<tensor::Tensor>(array_like) || IsStubTensor(array_like)) {
    auto tensor_index = IsStubTensor(array_like) ? ConvertStubTensor(array_like) : py::cast<TensorPtr>(array_like);
    MS_EXCEPTION_IF_NULL(tensor_index);
    return TensorPy::AsNumpy(*tensor_index);
  }
  if (py::isinstance<py::list>(array_like)) {
    auto new_array_like_vector = array_like.cast<py::list>();
    for (size_t i = 0; i < new_array_like_vector.size(); i++) {
      new_array_like_vector[i] = DeepTensorToNdArray(new_array_like_vector[i]);
    }
    return new_array_like_vector;
  }
  return array_like;
}

py::array TensorIndex::MakeNdArray(const py::object &a, int64_t dim_size) {
  if (!py::isinstance<py::list>(a) && !py::isinstance<py::tuple>(a) && !py::isinstance<py::int_>(a) &&
      !py::isinstance<py::float_>(a) && !py::isinstance<py::bool_>(a)) {
    MS_EXCEPTION(TypeError) << "Input data must be `int`, `float`, `bool`, `list` or `tuple` but got " << a.get_type();
  }
  py::object new_array = CheckRange(a, dim_size);
  if (py::isinstance<py::list>(new_array) || py::isinstance<py::tuple>(new_array)) {
    new_array = DeepList(new_array, dim_size);
    new_array = DeepTensorToNdArray(new_array);
  }
  return new_array;
}

namespace Convert {
string ConvertTypeToString(const TensorIndex &index) {
  if (index.IsNone())
    return "None";
  else if (index.IsEllipsis())
    return "Ellipsis";
  else if (index.IsInteger())
    return "Integer";
  else if (index.IsBoolean())
    return "Boolean";
  else if (index.IsSlice())
    return "Slice";
  else if (index.IsTensor())
    return "Tensor";
  else if (index.IsList())
    return "List";
  else if (index.IsTuple())
    return "Tuple";
  else if (index.IsArray())
    return "Array";
  else if (index.IsFloat())
    return "Float";
  return "Unknown";
}
}  // namespace Convert

std::vector<TensorIndex> TensorIndex::TransformEllipsisToSlice(const ShapeVector &data_shape,
                                                               const std::vector<TensorIndex> &indices) {
  // Check if the tuple index len is longer than the data's dims and transform ellipsis in the indices
  // to several slice.
  int64_t ellipsis_occupy_dims = SizeToLong(data_shape.size());
  int64_t ellipsis_positions = 0;
  int64_t ellipsis_cnt = 0;
  for (size_t i = 0; i < indices.size(); i++) {
    bool valid = (CheckTypeIsInstance<TensorIndexType>(
      indices[i].type(),
      {TensorIndexType::List, TensorIndexType::Ellipsis, TensorIndexType::Tuple, TensorIndexType::None,
       TensorIndexType::Integer, TensorIndexType::Tensor, TensorIndexType::Slice, TensorIndexType::Boolean}));
    if (!valid) {
      MS_EXCEPTION(TypeError) << "For tuple index, the types only support 'Slice', 'Ellipsis', 'None', 'Tensor', "
                                 "'int', 'List', 'Tuple', 'bool', but got type '"
                              << Convert::ConvertTypeToString(indices[i]) << "', value: " << indices[i];
    }
    if (indices[i].IsSlice() || indices[i].IsInteger() || indices[i].IsTensor() || indices[i].IsSequence()) {
      ellipsis_occupy_dims -= 1;
    } else if (indices[i].IsEllipsis()) {
      if (ellipsis_cnt >= 1) {
        MS_EXCEPTION(IndexError) << "An index can only have a single ellipsis('...')";
      }
      ellipsis_cnt += 1;
      ellipsis_positions = static_cast<int64_t>(i);
    }
  }
  if (ellipsis_occupy_dims < 0) {
    MS_EXCEPTION(IndexError) << "Tuple index " << indices << " out rang of tensor shape " << data_shape;
  }

  if (ellipsis_cnt == 0) {
    return indices;
  }

  std::vector<TensorIndex> empty_slice(ellipsis_occupy_dims, TensorIndex(Slice()));
  std::vector<TensorIndex> new_indices(indices.begin(), indices.end());
  MS_EXCEPTION_IF_CHECK_FAIL(ellipsis_positions <= SizeToLong(new_indices.size()), "Index out of vector size.");
  (void)new_indices.insert(new_indices.erase(new_indices.begin() + ellipsis_positions), empty_slice.begin(),
                           empty_slice.end());
  return new_indices;
}

std::tuple<ShapeVector, ShapeVector, ShapeVector, int64_t> TensorIndex::GenerateIndexInfoFromTupleOfMixedTensors(
  const std::vector<int64_t> &tensor_positions, const std::vector<ShapeVector> &tensor_indexes_shapes,
  const ShapeVector &slice_shapes, const TensorIndex &py_fancy_position) {
  bool tensor_index_continue_tag = true;
  if (tensor_positions.empty()) {
    tensor_index_continue_tag = false;
  }
  for (size_t i = 1; i < tensor_positions.size(); i++) {
    if (tensor_positions[i] != tensor_positions[i - 1] + 1) {
      tensor_index_continue_tag = false;
      break;
    }
  }
  int64_t fancy_position = 0;
  if (py_fancy_position.IsNone()) {
    fancy_position = tensor_index_continue_tag ? tensor_positions[0] : 0;
  } else {
    fancy_position = py_fancy_position.integer();
  }

  ShapeVector broadcast_shape = BroadCastShape(tensor_indexes_shapes);

  fancy_position = std::min(fancy_position, SizeToLong(slice_shapes.size()));
  ShapeVector final_shape = slice_shapes;
  (void)final_shape.insert(final_shape.begin() + fancy_position, broadcast_shape.begin(), broadcast_shape.end());

  ShapeVector index_tensor_new_shape(slice_shapes.size(), 1);
  fancy_position = std::min(fancy_position, SizeToLong(index_tensor_new_shape.size()));

  (void)index_tensor_new_shape.insert(index_tensor_new_shape.begin() + fancy_position, broadcast_shape.begin(),
                                      broadcast_shape.end());

  return std::make_tuple(broadcast_shape, index_tensor_new_shape, final_shape, fancy_position);
}

TensorIndex TensorIndex::SliceToArray(const TensorIndex &tensor_index, const ShapeVector &shape) {
  MS_EXCEPTION_IF_CHECK_FAIL(!shape.empty(), "DataShape of Tensor can not be empty when sed item");
  Slice slice_info = Slice(tensor_index.slice(), shape[0]);
  int64_t start = slice_info.start();
  int64_t stop = slice_info.stop();
  int64_t step = slice_info.step();
  if ((start - stop) * step >= 0) {
    return TensorIndex(py::bool_(false));
  }
  int64_t n_dim = SizeToLong(shape.size());
  py::tuple grids(n_dim);
  grids[0] = TensorIndex::np_module_.attr("arange")(py::int_(start), py::int_(stop), py::int_(step));
  for (size_t i = 1; i < shape.size(); i++) {
    grids[i] = TensorIndex::np_module_.attr("arange")(0, py::int_(shape[i]), 1, TensorIndex::np_module_.attr("int32"));
  }

  py::object mesh = TensorIndex::np_module_.attr("ix_")(*grids);
  py::tuple broadcast_mesh = TensorIndex::np_module_.attr("broadcast_arrays")(*mesh);
  return TensorIndex(TensorIndex::np_module_.attr("stack")(broadcast_mesh, -1));
}

TensorIndex TensorIndex::SliceToArray(const TensorPtr &index, const ShapeVector &final_shape, size_t slice_cnt,
                                      const ShapeVector &broadcast_shape, const ShapeVector &slice_shape,
                                      int64_t fancy_position) {
  ShapeVector shape = ComputeSliceShape(slice_shape, broadcast_shape.size(), slice_cnt, fancy_position);

  py::object array = TensorPy::SyncAsNumpy(*index);
  array = TensorIndex::np_module_.attr("ndarray").attr("astype")(array, TensorIndex::np_module_.attr("int32"));
  array = TensorIndex::np_module_.attr("reshape")(array, py::cast(shape));
  array = BroadCastTo(final_shape, array);
  return TensorIndex(array);
}

py::object TensorIndex::BroadCastTo(const ShapeVector &broadcast_shape, const py::object &item) {
  return TensorIndex::np_module_.attr("broadcast_to")(item, py::cast(broadcast_shape));
}

TensorIndex TensorIndex::BroadCastTensor(const ShapeVector &broadcast_shape, const ShapeVector &final_shape,
                                         const ShapeVector &new_shape, const TensorPtr &item) {
  py::array py_item = TensorPy::SyncAsNumpy(*item);
  py_item = TensorIndex::np_module_.attr("ndarray").attr("astype")(py_item, TensorIndex::np_module_.attr("int32"));
  py_item = BroadCastTo(broadcast_shape, py_item);
  return TensorIndex(BroadCastTo(final_shape, TensorIndex::np_module_.attr("reshape")(py_item, py::cast(new_shape))));
}

std::tuple<int64_t, py::object, ShapeVector> TensorIndex::GetValueTransferType(const TensorIndexType &py_value_type,
                                                                               int64_t op_type,
                                                                               const TypePtr &data_type, bool is_view) {
  ValueTransferType value_transfer_type = ValueTransferType::kByPass;
  py::object value_transfer_arg = py::none();
  ShapeVector value_shape = {};
  if (py_value_type == TensorIndexType::Tensor) {
    if (is_view) {
      return std::make_tuple(static_cast<int>(value_transfer_type), value_transfer_arg, value_shape);
    }
    value_transfer_arg = py::none();
    if (IsStubTensor(TensorIndex::py_value_handle_)) {
      value_shape = GetStubTensorInfo(TensorIndex::py_value_handle_).first;
    } else {
      auto value_ptr = TensorIndex::py_value_handle_.cast<TensorPtr>();
      MS_EXCEPTION_IF_NULL(value_ptr);
      value_shape = value_ptr->shape();
    }
  } else if (CheckTypeIsInstance(py_value_type,
                                 {TensorIndexType::Float, TensorIndexType::Integer, TensorIndexType::Boolean})) {
    value_transfer_type = ValueTransferType::kNumberToTensor;
    value_transfer_arg = py::none();
  } else if (py_value_type == TensorIndexType::List || py_value_type == TensorIndexType::Tuple) {
    value_transfer_type = ValueTransferType::kHandleSequenceValue;
    auto py_value_list = TensorIndex::py_value_handle_.cast<py::list>();
    if (!py_value_list.empty()) {
      (void)value_shape.emplace_back(SizeToLong(py_value_list.size()));
      const py::object &first_py_ele = py_value_list[0];
      TensorPtr ele;
      if (py::isinstance<Tensor>(first_py_ele) || IsStubTensor(first_py_ele)) {
        ele = IsStubTensor(first_py_ele) ? ConvertStubTensor(first_py_ele) : py::cast<TensorPtr>(first_py_ele);
      } else {
        ele = TensorPy::MakeTensor(py_value_list[0], data_type);
      }
      MS_EXCEPTION_IF_NULL(ele);
      (void)value_shape.insert(value_shape.end(), ele->shape().begin(), ele->shape().end());
    }
    value_transfer_arg = py::make_tuple(py::int_(op_type), TensorIndex::py_index_handle_);
  }
  return std::make_tuple(static_cast<int>(value_transfer_type), value_transfer_arg, value_shape);
}

static py::array CastToInt(const py::array &input) {
  return TensorIndex::np_module_.attr("ndarray").attr("astype")(input, TensorIndex::np_module_.attr("int32"));
}

static bool CheckLargeTensor(const ShapeVector &data_shape) {
  constexpr int64_t max_dim = 1024 * 32;
  int64_t data_shape_dim = std::accumulate(data_shape.begin(), data_shape.end(), 1, std::multiplies<>());
  return data_shape_dim > max_dim;
}

// ***********************************************for get_item*******************************************
py::tuple TensorIndex::GenerateNonZeroIndex(const ShapeVector &data_shape, const TensorPtr &tensor_index,
                                            bool check_align) {
  if (!check_align) {
    py::array index_array = TensorPy::SyncAsNumpy(*tensor_index);
    return TensorIndex::np_module_.attr("nonzero")(index_array);
  }
  const int64_t data_dim = SizeToLong(data_shape.size());
  const int64_t index_dims = tensor_index->DataDim();
  if (data_dim < index_dims) {
    MS_EXCEPTION(IndexError) << "The dim of index cannot be greater than indexed data, but got dim of index:"
                             << index_dims << ", dim of data:" << data_dim;
  }
  for (size_t i = 0; i < static_cast<size_t>(index_dims); i++) {
    if (data_shape[i] != tensor_index->shape()[i]) {
      MS_EXCEPTION(ValueError) << "The shape of index " << tensor_index->shape()
                               << "does not match the shape of the indexed data " << data_shape << " at dim index" << i;
    }
  }
  py::array index_array = TensorPy::SyncAsNumpy(*tensor_index);
  return TensorIndex::np_module_.attr("nonzero")(index_array);
}

std::vector<TensorPtr> TensorIndex::GenerateNonZeroIndexTensorList(const ShapeVector &data_shape,
                                                                   const TensorPtr &tensor_index, bool check_align) {
  py::tuple nonzero_indices = GenerateNonZeroIndex(data_shape, tensor_index, check_align);
  MS_EXCEPTION_IF_CHECK_FAIL(!nonzero_indices.empty(), "Output size of nonzero should not be empty");
  int64_t nonzero_indices_nums = SizeToLong(len(py::array(nonzero_indices[0])));
  if (nonzero_indices_nums == 0) {
    return {};
  }
  std::vector<TensorPtr> nonzero_indices_tensor_list;
  (void)std::transform(nonzero_indices.begin(), nonzero_indices.end(), std::back_inserter(nonzero_indices_tensor_list),
                       [](const py::handle &nonzero_index) {
                         return TensorPy::MakeTensor(TensorIndex::np_module_.attr("array")(nonzero_index));
                       });
  return nonzero_indices_tensor_list;
}

bool TensorIndex::TensorGetitemByTupleParseTensorIndex(const ShapeVector &data_shape, const TensorPtr &tensor_index,
                                                       std::vector<TensorPtr> *tuple_index_new,
                                                       std::vector<TensorPtr> *tensor_indexes,
                                                       std::vector<int64_t> *tensor_positions, bool check_align) {
  //  parse index of tensor type
  MS_EXCEPTION_IF_NULL(tensor_index);
  if (CheckTypeIsInstance<TypeId>(tensor_index->data_type(), kIntTypes)) {
    tensor_positions->emplace_back(tuple_index_new->size());
    tuple_index_new->emplace_back(tensor_index);
    tensor_indexes->emplace_back(tensor_index);
  } else if (tensor_index->data_type() == kNumberTypeBool) {
    std::vector<TensorPtr> nonzero_indices_tensors =
      GenerateNonZeroIndexTensorList(data_shape, tensor_index, check_align);
    if (nonzero_indices_tensors.empty()) {
      return false;
    }
    int64_t nonzero_indices_position = SizeToLong(tuple_index_new->size());
    (void)std::transform(nonzero_indices_tensors.begin(), nonzero_indices_tensors.end(),
                         std::back_inserter(*tensor_positions),
                         [&nonzero_indices_position](auto &) { return nonzero_indices_position++; });
    tuple_index_new->insert(tuple_index_new->end(), nonzero_indices_tensors.begin(), nonzero_indices_tensors.end());
    tensor_indexes->insert(tensor_indexes->end(), nonzero_indices_tensors.begin(), nonzero_indices_tensors.end());
  } else {
    MS_EXCEPTION(IndexError) << "The tensor element in tuple index must be int or bool type, but got "
                             << TypeIdToString(tensor_index->data_type(), false);
  }
  return true;
}

std::tuple<std::vector<std::vector<int64_t>>, std::vector<int64_t>> TensorIndex::GetStrideInfoFromTuple(
  const ShapeVector &data_shape, const std::vector<TensorIndex> &tuple_index) {
  const size_t data_dim = data_shape.size();
  const size_t tuple_index_len = tuple_index.size();
  const size_t stride_slice_info_size = std::min(tuple_index_len, data_dim);
  std::vector<int64_t> begin_info(stride_slice_info_size);
  std::vector<int64_t> end_info(stride_slice_info_size);
  std::vector<int64_t> step_info(stride_slice_info_size);

  size_t index_count = 0;
  int64_t shrink_axis = 0;
  int64_t ellipsis_count = 0;

  for (size_t i = 0; i < stride_slice_info_size; i++) {
    const TensorIndex &index = tuple_index[i];

    int64_t dim_size = data_shape[i];
    if (index.IsSlice()) {
      Slice slice_info = Slice(index.slice(), dim_size);
      begin_info[i] = slice_info.start();
      end_info[i] = slice_info.stop();
      step_info[i] = slice_info.step();
      index_count += 1;
    } else if (index.IsInteger()) {
      const auto mask_bit = 1 << index_count;
      begin_info[i] = index.integer();
      end_info[i] = index.integer() + 1;
      step_info[i] = 1;
      shrink_axis += mask_bit;
      index_count += 1;
    } else if (index.IsEllipsis()) {
      ellipsis_count = ellipsis_count + 1;
      if (ellipsis_count > 1) {
        MS_EXCEPTION(ValueError) << "An Tensor index can have only one ellipsis (...) ";
      }
      auto ellipsis_range_size = data_dim - tuple_index_len + 1;
      for (size_t j = 0; j < ellipsis_range_size; j++) {
        MS_EXCEPTION_IF_CHECK_FAIL(index_count + j < stride_slice_info_size && index_count + j < data_dim,
                                   "Index out of data dims");
        begin_info[index_count + j] = 0;
        end_info[index_count + j] = data_shape[index_count + j];
        step_info[index_count + j] = 1;
      }
      index_count += ellipsis_range_size;
    }
  }

  int64_t begin_mask = 0;
  int64_t end_mask = 0;

  for (size_t i = 0; i < tuple_index_len; i++) {
    if (tuple_index[i].IsSlice()) {
      Slice slice_info = tuple_index[i].slice();
      const auto mask_bit = 1 << i;
      if (slice_info.start_init_by_none()) {
        begin_mask += mask_bit;
      }
      if (slice_info.stop_init_by_none()) {
        end_mask += mask_bit;
      }
    }
  }
  for (size_t i = tuple_index_len; i < data_dim; i++) {
    const auto mask_bit = 1 << i;
    begin_mask += mask_bit;
    end_mask += mask_bit;
  }

  return std::make_tuple(std::vector<std::vector<int64_t>>({begin_info, end_info, step_info}),
                         std::vector<int64_t>({begin_mask, end_mask, shrink_axis}));
}

std::tuple<bool, ShapeVector, std::vector<TensorIndex>> TensorIndex::GetExpandDimsInfo(
  const ShapeVector &data_shape, const std::vector<TensorIndex> &index) {
  bool need_expand_dims = std::any_of(index.begin(), index.end(), [](auto &x) { return x.IsNone() || x.IsBoolean(); });
  if (!need_expand_dims) {
    return std::make_tuple(false, ShapeVector(), std::vector<TensorIndex>());
  }
  std::vector<TensorIndex> new_tuple_index;
  std::vector<int64_t> expand_dims_info;
  for (size_t i = 0; i < index.size(); i++) {
    if (index[i].IsNone()) {
      (void)new_tuple_index.emplace_back(tensor::Slice());
      (void)expand_dims_info.emplace_back(i);
    } else if (index[i].IsBoolean()) {
      if (!index[i].boolean()) {
        MS_EXCEPTION(IndexError) << "Bool element of tuple index must be 'True', but got 'False'.";
      }
      (void)new_tuple_index.emplace_back(std::make_shared<Tensor>(std::vector<int64_t>({0})));
      (void)expand_dims_info.emplace_back(i);
    } else {
      (void)new_tuple_index.emplace_back(index[i]);
    }
  }
  auto reshape_info = data_shape;
  for (auto dim : expand_dims_info) {
    dim = std::min(dim, SizeToLong(reshape_info.size()));
    (void)reshape_info.insert(reshape_info.begin() + dim, 1);
  }

  return std::make_tuple(need_expand_dims, reshape_info, new_tuple_index);
}

py::object TensorIndex::GenerateIndices(const std::vector<TensorPtr> &tuple_index_new,
                                        const std::vector<int64_t> &broadcast_shape,
                                        const std::vector<int64_t> &index_tensor_new_shape,
                                        const std::vector<int64_t> &final_shape,
                                        const std::vector<int64_t> &tensor_positions,
                                        const std::vector<int64_t> &slice_shapes, int64_t fancy_position) {
  py::tuple final_index_tensors(tuple_index_new.size());
  size_t slice_cnt = 0;
  for (size_t i = 0; i < tuple_index_new.size(); i++) {
    if (std::find(tensor_positions.begin(), tensor_positions.end(), i) != tensor_positions.end()) {
      TensorIndex transform_tensor =
        BroadCastTensor(broadcast_shape, final_shape, index_tensor_new_shape, tuple_index_new[i]);
      final_index_tensors[i] = transform_tensor.array();
    } else {
      TensorIndex slice_index_tensor =
        SliceToArray(tuple_index_new[i], final_shape, slice_cnt, broadcast_shape, slice_shapes, fancy_position);

      final_index_tensors[i] = slice_index_tensor.array();
      slice_cnt += 1;
    }
  }
  return TensorIndex::np_module_.attr("array")(TensorIndex::np_module_.attr("stack")(final_index_tensors, -1));
}

py::object TensorIndex::TensorGetitemByTuple(const ShapeVector &data_shape, const std::vector<TensorIndex> &tuple_index,
                                             std::vector<int64_t> *data_transfer_types,
                                             std::vector<py::object> *data_transfer_args) {
  size_t data_dims = data_shape.size();
  std::vector<TensorPtr> tensor_indexes;
  std::vector<TensorPtr> tuple_index_new;
  std::vector<int64_t> slice_shapes;
  std::vector<int64_t> tensor_positions;
  size_t tuple_index_len = tuple_index.size();
  bool empty_mask_tensor = false;
  const size_t min_length = std::min(data_dims, tuple_index_len);
  for (size_t i = 0; i < min_length; i++) {
    int64_t dim_size = data_shape[i];
    const TensorIndex &index = tuple_index[i];

    if (index.IsInteger()) {
      int64_t int_index = index.integer();
      if (int_index >= dim_size || int_index < -dim_size) {
        MS_EXCEPTION(IndexError) << "Index " << int_index << " is out of bounds for dimension with size " << dim_size;
      }
      int_index = CheckRange(int_index, dim_size);
      TensorPtr tensor_index = std::make_shared<Tensor>(int_index);
      (void)tensor_positions.emplace_back(tuple_index_new.size());
      (void)tuple_index_new.emplace_back(tensor_index);
      (void)tensor_indexes.emplace_back(tensor_index);
    } else if (index.IsSequence()) {
      TensorIndex sequence_list = SequenceToTensor(index, data_shape[i]);
      TensorPtr tensor_index = sequence_list.tensor();
      (void)tensor_positions.emplace_back(tuple_index_new.size());
      (void)tuple_index_new.emplace_back(tensor_index);
      (void)tensor_indexes.emplace_back(tensor_index);
    } else if (index.IsTensor()) {
      const TensorPtr &tensor_index = index.tensor();
      if (!TensorGetitemByTupleParseTensorIndex(data_shape, tensor_index, &tuple_index_new, &tensor_indexes,
                                                &tensor_positions, false)) {
        TensorPtr new_tensor_index = std::make_shared<Tensor>(kNumberTypeInt32, ShapeVector({0}));
        for (int j = 0; j < tensor_index->DataDim(); j++) {
          (void)tensor_positions.emplace_back(tuple_index_new.size());
          (void)tuple_index_new.emplace_back(new_tensor_index);
          (void)tensor_indexes.emplace_back(new_tensor_index);
        }
        empty_mask_tensor = true;
      }
    } else if (index.IsSlice()) {
      Slice slice_info = Slice(index.slice(), dim_size);
      int64_t start = slice_info.start();
      int64_t stop = slice_info.stop();
      int64_t step = slice_info.step();

      std::vector<int64_t> slice_ele_list_index;
      for (int64_t j = start; j < stop; j += step) {
        (void)slice_ele_list_index.emplace_back(j);
      }
      (void)slice_shapes.emplace_back(SizeToLong(slice_ele_list_index.size()));
      (void)tuple_index_new.emplace_back(std::make_shared<Tensor>(slice_ele_list_index));
    }
  }
  tuple_index_len = tuple_index.size();
  std::vector<ShapeVector> tensor_indexes_shapes;
  (void)std::transform(
    tensor_indexes.begin(), tensor_indexes.end(), std::back_inserter(tensor_indexes_shapes), [](auto &tensor_index) {
      if (tensor_index == nullptr) {
        MS_EXCEPTION(IndexError) << "IndexError: The sequence element(tuple/list) in tuple index can't be empty.";
      }
      return tensor_index->shape();
    });
  std::tuple<ShapeVector, ShapeVector, ShapeVector, int64_t> index_info = GenerateIndexInfoFromTupleOfMixedTensors(
    tensor_positions, tensor_indexes_shapes, slice_shapes, TensorIndex(py::none()));
  constexpr size_t broadcast_shape_index = 0;
  constexpr size_t index_tensor_new_shape_index = 1;
  constexpr size_t final_shape_index = 2;
  constexpr size_t fancy_position_index = 3;
  ShapeVector broadcast_shape = std::get<broadcast_shape_index>(index_info);
  ShapeVector index_tensor_new_shape = std::get<index_tensor_new_shape_index>(index_info);
  ShapeVector final_shape = std::get<final_shape_index>(index_info);
  int64_t fancy_position = std::get<fancy_position_index>(index_info);
  if (empty_mask_tensor) {
    (void)data_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kEmptyTensor));
    (void)data_transfer_args->emplace_back(VectorToPyTuple(final_shape));
    return py::make_tuple(py::none(), VectorToPyTuple(*data_transfer_types), VectorToPyTuple(*data_transfer_args));
  }
  if (std::find(final_shape.begin(), final_shape.end(), 0) != final_shape.end() ||
      std::find(data_shape.begin(), data_shape.end(), 0) != data_shape.end()) {
    if (tuple_index_len < data_dims) {
      (void)final_shape.insert(final_shape.end(), data_shape.begin() + SizeToLong(tuple_index_len), data_shape.end());
    }
    data_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kEmptyTensor));
    data_transfer_args->emplace_back(VectorToPyTuple(final_shape));
    return py::make_tuple(py::none(), VectorToPyTuple(*data_transfer_types), VectorToPyTuple(*data_transfer_args));
  }

  data_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kGatherND));
  data_transfer_args->emplace_back(py::make_tuple(
    VectorToPyTuple(broadcast_shape), VectorToPyTuple(final_shape), VectorToPyTuple(index_tensor_new_shape),
    VectorToPyTuple(slice_shapes), VectorToPyTuple(tensor_positions), fancy_position));
  if (CheckLargeTensor(data_shape)) {
    return py::make_tuple(tuple_index_new, VectorToPyTuple(*data_transfer_types), VectorToPyTuple(*data_transfer_args));
  }
  py::array new_index = GenerateIndices(tuple_index_new, broadcast_shape, index_tensor_new_shape, final_shape,
                                        tensor_positions, slice_shapes, fancy_position);
  return py::make_tuple(TensorPy::MakeTensor(CastToInt(new_index)), VectorToPyTuple(*data_transfer_types),
                        VectorToPyTuple(*data_transfer_args));
}

// ***********************************************for set_item*******************************************
TensorIndex TensorIndex::FormatList(const TensorIndex &tensor_index, int64_t length) {
  bool transform_to_array = std::all_of(tensor_index.list_.begin(), tensor_index.list_.end(), [](auto &x) {
    return py::isinstance<py::int_>(x) || py::isinstance<py::bool_>(x);
  });
  if (transform_to_array) {
    return SequenceToTensor<py::list>(tensor_index.list_, length);
  }
  return TensorIndex(DeepList(tensor_index.list_, length).cast<py::tuple>());
}

TensorPtr TensorIndex::IntToTensor(int64_t int_index, const ShapeVector &shape) {
  int64_t dim_size = shape[0];
  auto out_i = static_cast<int32_t>(CheckRange(int_index, dim_size));
  if (shape.size() == 1) {
    return std::make_shared<Tensor>(kNumberTypeInt32, ShapeVector({1, 1}), &out_i, int32_bytes_number);
  }

  ShapeVector index_shape(shape.begin() + 1, shape.end());
  int64_t grids_size = SizeToLong(shape.size()) - 1;
  py::tuple grids(grids_size);
  for (size_t i = 1; i < shape.size(); i++) {
    grids[i - 1] =
      TensorIndex::np_module_.attr("arange")(0, py::int_(shape[i]), 1, TensorIndex::np_module_.attr("int32"));
  }
  py::object mesh = TensorIndex::np_module_.attr("ix_")(*grids);
  py::tuple index(SizeToLong(shape.size()));
  index[0] =
    TensorIndex::np_module_.attr("full")(py::cast(index_shape), py::int_(out_i), TensorIndex::np_module_.attr("int32"));
  py::tuple broadcast_mesh = TensorIndex::np_module_.attr("broadcast_arrays")(*mesh);
  for (size_t i = 1; i < shape.size(); i++) {
    index[i] = broadcast_mesh[i - 1];
  }
  py::object output_index = TensorIndex::np_module_.attr("stack")(index, -1);
  return TensorPy::MakeTensor(TensorIndex::np_module_.attr("array")(output_index));
}

py::object TensorIndex::GenerateIndicesFromTupleOfTensor(const ShapeVector &data_shape,
                                                         const std::vector<TensorIndex> &tuple_index,
                                                         ShapeVector *output_index_shape,
                                                         py::object *data_transfer_arg) {
  std::vector<ShapeVector> tensor_index_shape;
  std::vector<TensorPtr> tuple_index_vector;
  for (const auto &index : tuple_index) {
    TensorPtr index_tensor = index.tensor();
    MS_EXCEPTION_IF_NULL(index_tensor);
    (void)tuple_index_vector.emplace_back(index_tensor);
    if (!CheckTypeIsInstance<TypeId>(index_tensor->data_type(), kIntTypes)) {
      string index_op_type = index_op_type_ == IndexOpType::GetItem ? "tensor getitem" : "tensor setitem";
      MS_EXCEPTION(IndexError) << "For '" << index_op_type << "', the index tensor data type '"
                               << index_tensor->data_type() << "' is not supported.";
    }
  }
  (void)std::transform(tuple_index_vector.begin(), tuple_index_vector.end(), std::back_inserter(tensor_index_shape),
                       [](const TensorPtr &x) { return x->shape(); });
  ShapeVector broadcast_shape = BroadCastShape(tensor_index_shape);

  constexpr int64_t min_broadcast_shape_size = 2;
  if (SizeToLong(broadcast_shape.size()) < min_broadcast_shape_size) {
    (void)broadcast_shape.insert(broadcast_shape.begin(), 1);
  }

  *output_index_shape = broadcast_shape;
  output_index_shape->emplace_back(tuple_index.size());
  if (CheckLargeTensor(data_shape)) {
    *data_transfer_arg = py::make_tuple(VectorToPyTuple(broadcast_shape));
    return VectorToPyTuple(tuple_index_vector);
  }

  std::vector<py::array> broadcast_tensors;
  (void)std::transform(tuple_index.begin(), tuple_index.end(), std::back_inserter(broadcast_tensors),
                       [&broadcast_shape](auto &index) {
                         return TensorIndex::np_module_.attr("broadcast_to")(
                           CastToInt(TensorPy::SyncAsNumpy(*index.tensor())), broadcast_shape);
                       });
  py::array output_index = TensorIndex::np_module_.attr("stack")(py::cast(broadcast_tensors), -1);
  return py::cast(TensorPy::MakeTensor(TensorIndex::np_module_.attr("array")(output_index)));
}

void TensorIndex::RemNotExpandedDims(int64_t *idx_advanced, bool expand_true, int64_t tensor_index_ndim,
                                     int64_t rem_ndim, std::vector<bool> *not_expanded_dim) {
  if (*idx_advanced != -1) {
    std::vector<bool> tensor_dims(tensor_index_ndim, true);
    if (expand_true) {
      tensor_dims = {false};
    }
    *idx_advanced = std::min(*idx_advanced, SizeToLong(not_expanded_dim->size()));
    not_expanded_dim->insert(not_expanded_dim->begin() + *idx_advanced, tensor_dims.begin(), tensor_dims.end());
  }
  std::vector<bool> rem_ndim_vector(rem_ndim, true);
  not_expanded_dim->insert(not_expanded_dim->end(), rem_ndim_vector.begin(), rem_ndim_vector.end());
  size_t count_leading_false = 0;
  while (count_leading_false < not_expanded_dim->size() && !((*not_expanded_dim)[count_leading_false])) {
    count_leading_false += 1;
  }
  *idx_advanced = std::max(static_cast<int64_t>(0), *idx_advanced - SizeToLong(count_leading_false));
}

TensorIndex TensorIndex::FormatIndex(const TensorIndex &idx, const ShapeVector &data_shape, size_t cur_dim,
                                     bool *need_format) {
  if (!CheckTypeIsInstance<TensorIndexType>(idx.type(), {TensorIndexType::List, TensorIndexType::Tuple,
                                                         TensorIndexType::Integer, TensorIndexType::Tensor})) {
    return idx;
  }
  MS_EXCEPTION_IF_CHECK_FAIL(cur_dim < data_shape.size(), "Index" + std::to_string(cur_dim) + "out of data dims" +
                                                            std::to_string(data_shape.size()));
  int64_t dims_size = data_shape[cur_dim];
  if (idx.IsSequence()) {
    return SequenceToTensor(idx, dims_size);
  } else if (idx.IsInteger()) {
    return TensorIndex(std::make_shared<Tensor>(CheckRange(idx.integer(), dims_size)));
  }
  const TensorPtr &tensor_idx = idx.tensor();
  MS_EXCEPTION_IF_NULL(tensor_idx);
  if (CheckTypeIsInstance<TypeId>(tensor_idx->data_type(), kIntTypes)) {
    if (CheckLargeTensor(data_shape)) {
      *need_format = true;
      return idx;
    }
    py::array new_idx = TensorPy::SyncAsNumpy(*tensor_idx);
    if (tensor_idx->DataDim() == 0) {
      auto new_int_idx = new_idx.cast<int64_t>();
      new_int_idx = new_int_idx < 0 ? new_int_idx + dims_size : new_int_idx;
      return TensorIndex(std::make_shared<Tensor>(new_int_idx));
    }
    // numpy op select is very slow for one dim array
    new_idx = TensorIndex::np_module_.attr("expand_dims")(new_idx, 0);
    new_idx = TensorIndex::np_module_.attr("select")(TensorIndex::np_module_.attr("less")(new_idx, 0),
                                                     TensorIndex::np_module_.attr("add")(new_idx, py::int_(dims_size)),
                                                     new_idx);
    new_idx = TensorIndex::np_module_.attr("squeeze")(new_idx, 0);
    return TensorIndex(TensorPy::MakeTensor(CastToInt(new_idx)));
  } else if (tensor_idx->data_type() != kNumberTypeBool) {
    string index_op_type = index_op_type_ == IndexOpType::GetItem ? "tensor getitem" : "tensor setitem";
    MS_EXCEPTION(IndexError) << "For '" << index_op_type << "', the index tensor data type '"
                             << TypeIdToString(tensor_idx->data_type(), false) << "' is not supported.";
  }
  return idx;
}

bool TensorIndex::RemoveExpandedDimsParseTensorIndex(const ShapeVector &data_shape, const TensorPtr &index_out,
                                                     std::vector<TensorIndex> *indices_out,
                                                     std::vector<ShapeVector> *shapes, bool *has_sequence,
                                                     size_t *cur_dim, bool check_align) {
  // Parse tensor_index
  MS_EXCEPTION_IF_NULL(index_out);
  if (index_out->data_type() == kNumberTypeBool) {
    std::vector<TensorPtr> nonzero_indices_tensors = GenerateNonZeroIndexTensorList(data_shape, index_out, check_align);
    if (nonzero_indices_tensors.empty()) {
      return false;
    }
    std::vector<TensorIndex> true_index_tensors;
    (void)std::transform(nonzero_indices_tensors.begin(), nonzero_indices_tensors.end(),
                         std::back_inserter(true_index_tensors),
                         [](const TensorPtr &true_index) { return TensorIndex(true_index); });
    size_t true_index_nums = nonzero_indices_tensors.size();
    indices_out->insert(indices_out->end(), true_index_tensors.begin(), true_index_tensors.end());
    MS_EXCEPTION_IF_NULL(nonzero_indices_tensors[0]);
    std::vector<ShapeVector> true_index_shapes(true_index_nums, {nonzero_indices_tensors[0]->shape()});
    shapes->insert(shapes->end(), true_index_shapes.begin(), true_index_shapes.end());
    *cur_dim += true_index_nums;
  } else {
    if (index_out->DataDim() > 0) {
      *has_sequence = true;
    }
    indices_out->emplace_back(index_out);
    shapes->emplace_back(index_out->shape());
    *cur_dim += 1;
  }
  return true;
}

std::pair<std::vector<TensorIndex>, ShapeVector> TensorIndex::RemoveExpandedDims(
  const std::vector<TensorIndex> &indices, const ShapeVector &data_shape, const ShapeVector &value_shape,
  std::vector<int64_t> *value_transfer_types, std::vector<py::object> *value_transfer_args, int64_t *idx_advanced,
  bool *by_pass, std::vector<size_t> *format_index, std::vector<int64_t> *format_dim) {
  // Removes expanded dimensions in tuple_index and value.
  size_t cur_dim = 0;
  bool has_true = false;
  bool has_false = false;
  bool has_sequence = false;
  int64_t idx_tensor = -1;
  std::vector<bool> not_expanded_dim;
  std::vector<TensorIndex> indices_out;
  std::vector<ShapeVector> shapes;

  for (size_t i = 0; i < indices.size(); i++) {
    const TensorIndex &v = indices[i];
    bool need_format = false;
    TensorIndex index_out = TensorIndex::FormatIndex(v, data_shape, cur_dim, &need_format);
    if (need_format) {
      (void)format_index->emplace_back(cur_dim);
      (void)format_dim->emplace_back(data_shape[cur_dim]);
    }
    if (index_out.IsNone()) {
      (void)not_expanded_dim.emplace_back(false);
    } else if (index_out.IsSlice()) {
      (void)indices_out.emplace_back(index_out);
      (void)not_expanded_dim.emplace_back(true);
      Slice slice_info = Slice(v.slice(), data_shape[cur_dim]);

      int64_t start = slice_info.start();
      int64_t stop = slice_info.stop();
      int64_t step = slice_info.step();
      has_false = ((start - stop) * step > 0) || has_false;
      cur_dim += 1;
    } else if (index_out.IsBoolean() || index_out.IsTensor()) {
      if (*idx_advanced == -1) {
        *idx_advanced = SizeToLong(not_expanded_dim.size());
      } else if (static_cast<int64_t>(i) - idx_tensor > 1) {
        *idx_advanced = 0;
      }
      idx_tensor = static_cast<int64_t>(i);
      if (index_out.IsTensor()) {
        const TensorPtr &index_out_tensor = index_out.tensor();
        if (!RemoveExpandedDimsParseTensorIndex(data_shape, index_out_tensor, &indices_out, &shapes, &has_sequence,
                                                &cur_dim, false)) {
          *by_pass = true;
          *idx_advanced = 0;
          return {std::vector<TensorIndex>(), ShapeVector()};
        }
      } else {
        bool bool_index_out = index_out.boolean();
        has_true = bool_index_out || has_true;
        has_false = !bool_index_out || has_false;
      }
    } else {
      MS_EXCEPTION(IndexError) << "Invalid index type, index: " << TensorIndex::py_index_handle_;
    }
  }

  ShapeVector broadcast_shape = BroadCastShape(shapes);
  if (has_false) {
    if (std::accumulate(broadcast_shape.begin(), broadcast_shape.end(), 1, std::multiplies<>()) != 1) {
      MS_EXCEPTION(IndexError) << "Unable to broadcast indices " << broadcast_shape;
    }
    *by_pass = true;
    return std::make_pair(std::vector<TensorIndex>(), ShapeVector());
  }

  bool expand_true = has_true && !(has_false || has_sequence);
  int64_t tensor_index_ndim = SizeToLong(broadcast_shape.size());
  int64_t rem_ndim = SizeToLong(data_shape.size()) - SizeToLong(cur_dim);
  RemNotExpandedDims(idx_advanced, expand_true, tensor_index_ndim, rem_ndim, &not_expanded_dim);
  if (indices_out.empty()) {
    indices_out = {TensorIndex(py::bool_(true))};
  }
  value_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kReshape));
  ShapeVector reshape_info = FilterExpandedDims(value_shape, not_expanded_dim);
  value_transfer_args->emplace_back(py::cast(reshape_info));
  *by_pass = false;
  return std::make_pair(indices_out, reshape_info);
}

py::object TensorIndex::GenerateIndicesFromTuple(const ShapeVector &data_shape,
                                                 const std::vector<TensorIndex> &tuple_index, int64_t py_fancy_position,
                                                 bool *by_pass, ShapeVector *output_index_shape,
                                                 py::object *data_transfer_arg) {
  std::vector<TensorPtr> tensor_indexes;
  std::vector<TensorPtr> tuple_index_new;
  std::vector<int64_t> slice_shapes;
  std::vector<int64_t> tensor_positions;
  std::vector<ShapeVector> tensor_indexes_shapes;
  const size_t min_length = std::min(data_shape.size(), tuple_index.size());
  for (size_t i = 0; i < min_length; i++) {
    const TensorIndex &index = tuple_index[i];
    int64_t dim_size = data_shape[i];

    if (index.IsInteger()) {
      int64_t int_index = index.integer();
      if (int_index >= dim_size || int_index < -dim_size) {
        MS_EXCEPTION(IndexError) << "Index " << int_index << " is out of bounds for dimension with size " << dim_size;
      }
      int_index = CheckRange(int_index, dim_size);
      TensorPtr tensor_index = std::make_shared<Tensor>(int_index);
      MS_EXCEPTION_IF_NULL(tensor_index);
      (void)tuple_index_new.emplace_back(tensor_index);
      (void)tensor_indexes.emplace_back(tensor_index);
      (void)tensor_positions.emplace_back(i);
      (void)tensor_indexes_shapes.emplace_back(tensor_index->shape());
    } else if (index.IsSequence()) {
      TensorIndex sequence_list = SequenceToTensor(index, data_shape[i]);
      TensorPtr tensor_index = sequence_list.tensor();
      (void)tuple_index_new.emplace_back(tensor_index);
      (void)tensor_indexes.emplace_back(tensor_index);
      (void)tensor_positions.emplace_back(i);
      MS_EXCEPTION_IF_NULL(tensor_index);
      (void)tensor_indexes_shapes.emplace_back(tensor_index->shape());
    } else if (index.IsTensor()) {
      TensorPtr tensor_index = index.tensor();
      if (!CheckTypeIsInstance<TypeId>(tensor_index->data_type(), kIntTypes)) {
        MS_EXCEPTION(TypeError) << "The tensor element in tuple index must be int type, but got "
                                << tensor_index->data_type();
      }
      (void)tuple_index_new.emplace_back(tensor_index);
      (void)tensor_indexes.emplace_back(tensor_index);
      (void)tensor_positions.emplace_back(i);
      (void)tensor_indexes_shapes.emplace_back(tensor_index->shape());
    } else if (index.IsSlice()) {
      Slice slice_info = Slice(index.slice(), dim_size);
      int64_t start = slice_info.start();
      int64_t stop = slice_info.stop();
      int64_t step = slice_info.step();
      if ((start - stop) * step >= 0) {
        *by_pass = true;
        return py::none();
      }
      std::vector<int64_t> slice_ele_list_index = SliceToVector(start, stop, step);
      (void)slice_shapes.emplace_back(SizeToLong(slice_ele_list_index.size()));
      (void)tuple_index_new.emplace_back(std::make_shared<Tensor>(slice_ele_list_index));
    }
  }

  std::tuple<ShapeVector, ShapeVector, ShapeVector, int64_t> index_info = GenerateIndexInfoFromTupleOfMixedTensors(
    tensor_positions, tensor_indexes_shapes, slice_shapes, TensorIndex(py_fancy_position));
  constexpr size_t k_broadcast_shape_index = 0;
  constexpr size_t index_tensor_new_shape_index = 1;
  constexpr size_t final_shape_index = 2;
  constexpr size_t fancy_position_index = 3;
  ShapeVector broadcast_shape = std::get<k_broadcast_shape_index>(index_info);
  ShapeVector index_tensor_new_shape = std::get<index_tensor_new_shape_index>(index_info);
  ShapeVector final_shape = std::get<final_shape_index>(index_info);
  *output_index_shape = final_shape;
  output_index_shape->emplace_back(tuple_index_new.size());
  int64_t fancy_position = std::get<fancy_position_index>(index_info);
  if (CheckLargeTensor(data_shape)) {
    *data_transfer_arg = py::make_tuple(VectorToPyTuple(broadcast_shape), VectorToPyTuple(final_shape),
                                        VectorToPyTuple(index_tensor_new_shape), VectorToPyTuple(slice_shapes),
                                        VectorToPyTuple(tensor_positions), fancy_position);
    return VectorToPyTuple(tuple_index_new);
  }
  py::array output_index = GenerateIndices(tuple_index_new, broadcast_shape, index_tensor_new_shape, final_shape,
                                           tensor_positions, slice_shapes, fancy_position);
  return py::cast(TensorPy::MakeTensor(TensorIndex::np_module_.attr("array")(output_index)));
}

py::object TensorIndex::ReSetitemByTensor(const std::vector<TensorIndex> &new_tuple_index,
                                          const std::vector<int64_t> &value_transfer_types,
                                          const std::vector<py::object> &value_transfer_args) {
  py::object output_py_index;
  if (new_tuple_index[0].IsSlice()) {
    Slice slice_info = new_tuple_index[0].slice();
    output_py_index = py::slice(slice_info.start(), slice_info.stop(), slice_info.step());
  } else if (new_tuple_index[0].IsTensor()) {
    output_py_index = py::cast(new_tuple_index[0].tensor());
  } else {
    output_py_index = py::cast(new_tuple_index[0].boolean());
  }
  return py::make_tuple(
    output_py_index, VectorToPyTuple<int64_t>(value_transfer_types), VectorToPyTuple<py::object>(value_transfer_args),
    py::make_tuple(static_cast<int>(ValueTransferType::kReSetItemByIndex)), py::make_tuple(py::none()));
}

py::object TensorIndex::SetitemByTupleWithTensor(const ShapeVector &data_shape, const std::vector<TensorIndex> &indices,
                                                 const ShapeVector &value_shape,
                                                 std::vector<int64_t> *value_transfer_types,
                                                 std::vector<py::object> *value_transfer_args) {
  std::vector<TensorIndex> new_indices = TransformEllipsisToSlice(data_shape, indices);
  ValueTransferType tensor_update_type = ValueTransferType::kTensorScatterUpdate;
  if (UseCopySlice(new_indices, SizeToLong(data_shape.size())) && !TensorIndex::is_ascend_) {
    Slice slice_info = Slice(new_indices[1].slice(), data_shape[1]);
    int64_t dim1_start = slice_info.start();
    int64_t dim1_stop = slice_info.stop();
    if (dim1_stop - dim1_start <= 0) {
      tensor_update_type = ValueTransferType::kByPass;
      return py::make_tuple(py::none(), VectorToPyTuple<int64_t>(*value_transfer_types),
                            VectorToPyTuple<py::object>(*value_transfer_args),
                            py::make_tuple(static_cast<int>(tensor_update_type)), py::make_tuple(py::none()));
    }
    if (data_shape.empty()) {
      MS_EXCEPTION(TypeError) << "Cannot iterate over a scalar tensor.";
    }
    int64_t dim0_start =
      new_indices[0].integer() >= 0 ? new_indices[0].integer() : new_indices[0].integer() + data_shape[0];
    py::tuple start = py::make_tuple(dim0_start, dim1_start);
    py::tuple stop = py::make_tuple(dim0_start + 1, dim1_stop);
    py::tuple step = py::make_tuple(1, 1);

    ShapeVector new_value_shape = {dim1_stop - dim1_start};
    constexpr int64_t start_position_of_data_shape = 2;
    (void)new_value_shape.insert(new_value_shape.end(), data_shape.begin() + start_position_of_data_shape,
                                 data_shape.end());
    value_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kBroadCast));
    value_transfer_args->emplace_back(VectorToPyTuple(new_value_shape));
    value_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kCast));
    value_transfer_args->emplace_back(py::none());
    tensor_update_type = ValueTransferType::kCopySlice;
    return py::make_tuple(
      py::none(), VectorToPyTuple<int64_t>(*value_transfer_types), VectorToPyTuple<py::object>(*value_transfer_args),
      py::make_tuple(static_cast<int>(tensor_update_type)), py::make_tuple(py::make_tuple(start, stop, step)));
  }
  int64_t idx_advanced = -1;
  bool by_pass = false;
  std::vector<size_t> format_index;
  std::vector<int64_t> format_dim;
  std::pair<std::vector<TensorIndex>, ShapeVector> tuple_index_info =
    RemoveExpandedDims(new_indices, data_shape, value_shape, value_transfer_types, value_transfer_args, &idx_advanced,
                       &by_pass, &format_index, &format_dim);
  if (by_pass) {
    tensor_update_type = ValueTransferType::kByPass;
    return py::make_tuple(py::none(), VectorToPyTuple<int64_t>(*value_transfer_types),
                          VectorToPyTuple<py::object>(*value_transfer_args),
                          py::make_tuple(static_cast<int>(tensor_update_type)), py::make_tuple(py::none()));
  }

  MS_LOG(DEBUG) << "After remove expand dims: " << tuple_index_info.first;

  std::vector<TensorIndex> new_tuple_index = tuple_index_info.first;
  ShapeVector new_value_shape = tuple_index_info.second;

  if (new_tuple_index.size() == 1) {
    return ReSetitemByTensor(new_tuple_index, *value_transfer_types, *value_transfer_args);
  }
  py::object output_index;
  ShapeVector output_index_shape;
  py::object data_transfer_args = py::none();
  if (std::all_of(new_tuple_index.begin(), new_tuple_index.end(), [](const TensorIndex &x) { return x.IsTensor(); })) {
    output_index =
      GenerateIndicesFromTupleOfTensor(data_shape, new_tuple_index, &output_index_shape, &data_transfer_args);
  } else {
    by_pass = false;
    output_index = GenerateIndicesFromTuple(data_shape, new_tuple_index, idx_advanced, &by_pass, &output_index_shape,
                                            &data_transfer_args);
    if (by_pass) {
      tensor_update_type = ValueTransferType::kByPass;
      return py::make_tuple(py::none(), VectorToPyTuple<int64_t>(*value_transfer_types),
                            VectorToPyTuple<py::object>(*value_transfer_args),
                            py::make_tuple(static_cast<int>(tensor_update_type)), py::make_tuple(py::none()));
    }
  }

  value_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kCast));
  value_transfer_args->emplace_back(py::make_tuple());
  ShapeVector updates_shape(output_index_shape.begin(), output_index_shape.end() - 1);

  if (output_index_shape.back() < SizeToLong(data_shape.size())) {
    (void)updates_shape.insert(updates_shape.end(), data_shape.begin() + output_index_shape.back(), data_shape.end());
  }

  if (updates_shape != new_value_shape) {
    value_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kBroadCast));
    value_transfer_args->emplace_back(VectorToPyTuple(updates_shape));
  }
  std::vector<int> tensor_update_types{static_cast<int>(tensor_update_type)};
  std::vector<py::object> tensor_update_args{data_transfer_args};
  if (!format_index.empty()) {
    (void)tensor_update_types.insert(tensor_update_types.begin(),
                                     static_cast<int>(ValueTransferType::kFormatIndexTensor));
    (void)tensor_update_args.insert(tensor_update_args.begin(), py::make_tuple(VectorToPyTuple<size_t>(format_index),
                                                                               VectorToPyTuple<int64_t>(format_dim)));
  }
  if (py::isinstance<py::tuple>(output_index)) {
    return py::make_tuple(py::cast<py::list>(output_index), VectorToPyTuple<int64_t>(*value_transfer_types),
                          VectorToPyTuple<py::object>(*value_transfer_args), VectorToPyTuple<int>(tensor_update_types),
                          VectorToPyTuple<py::object>(tensor_update_args));
  }
  return py::make_tuple(py::cast<TensorPtr>(output_index), VectorToPyTuple<int64_t>(*value_transfer_types),
                        VectorToPyTuple<py::object>(*value_transfer_args), VectorToPyTuple<int>(tensor_update_types),
                        VectorToPyTuple<py::object>(tensor_update_args));
}

ValuePtr GetStubTensorValue(const py::handle &obj) {
  auto py_stub = py::getattr(obj, stub::PY_ATTR_STUB);
  ValuePtr stub = py_stub.cast<stub::StubNodePtr>();
  if (stub == nullptr) {
    auto tensor_ptr = py::getattr(obj, stub::PY_ATTR_TENSOR).cast<tensor::TensorPtr>();
    MS_EXCEPTION_IF_NULL(tensor_ptr);
    stub = tensor_ptr;
  }
  return stub;
}

ValuePtr SqueezeRDataValue(const TensorPtr &tensor, const py::handle &py_value, const ValuePtr &rdata_value) {
  auto rdata_shape = tensor->shape();
  if (rdata_shape.size() >= 1 && (rdata_shape.at(0) > 1 || rdata_shape.size() > 1)) {
    MS_EXCEPTION(ValueError)
      << "For SetItem, the shape of right value must be () or (1, ) when shape of left value is 0, but got"
      << rdata_shape;
  } else if (rdata_shape.size() == 1 && rdata_shape.at(0) == 1) {
    auto new_value = py::cast<py::list>(py_value);
    auto first_value = new_value[0];
    ValuePtr result =
      IsStubTensor(first_value) ? GetStubTensorValue(first_value) : first_value.cast<tensor::TensorPtr>();
    return result;
  }
  return rdata_value;
}

static inline py::object SetitemCopyView(std::vector<pynative::SliceOpInfoPtr> *slice_op_infos,
                                         const ValuePtr data_value, const std::vector<int64_t> &new_data_shape,
                                         const TypePtr &data_type, const py::handle &py_value) {
  auto cast_op_info = std::make_shared<pynative::SliceOpInfo>();
  cast_op_info->slice_op_name = prim::kPrimCast->name();
  (void)cast_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(data_type->type_id()));
  cast_op_info->data_indexs = {1};
  (void)slice_op_infos->emplace_back(cast_op_info);

  auto broadcastto_op_info = std::make_shared<pynative::SliceOpInfo>();
  broadcastto_op_info->slice_op_name = prim::kPrimBroadcastTo->name();
  (void)broadcastto_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(new_data_shape));
  broadcastto_op_info->data_indexs = {1};
  (void)slice_op_infos->emplace_back(broadcastto_op_info);

  auto copy_op_info = std::make_shared<pynative::SliceOpInfo>();
  copy_op_info->slice_op_name = kCopyWithSliceOpName;
  copy_op_info->data_indexs = {0, 1};
  (void)slice_op_infos->emplace_back(copy_op_info);
  ValuePtr rdata_value;
  if (IsStubTensor(py_value)) {
    rdata_value = GetStubTensorValue(py_value);
    if (new_data_shape.size() == 0) {
      auto tensor = ConvertStubTensor(py_value);
      rdata_value = SqueezeRDataValue(tensor, py_value, rdata_value);
    }
  } else if (py::isinstance<Tensor>(py_value)) {
    auto tensor = py_value.cast<TensorPtr>();
    MS_EXCEPTION_IF_NULL(tensor);
    rdata_value = tensor;
    if (new_data_shape.size() == 0) {
      rdata_value = SqueezeRDataValue(tensor, py_value, rdata_value);
    }
  } else if (py::isinstance<py::int_>(py_value)) {
    rdata_value = MakeValue(py::cast<int64_t>(py_value));
  } else if (py::isinstance<py::float_>(py_value)) {
    rdata_value = MakeValue(py::cast<float>(py_value));
  } else if (py::isinstance<py::bool_>(py_value)) {
    rdata_value = MakeValue(py::cast<bool>(py_value));
  } else {
    return py::none();
  }
  return pynative::PyNativeExecutor::GetInstance()->RunSliceOpStub({data_value, rdata_value}, *slice_op_infos);
}

py::object TensorIndex::SetitemBySliceWithTensor(const ShapeVector &data_shape, const TensorIndex &slice_index,
                                                 std::vector<int64_t> *value_transfer_types,
                                                 std::vector<py::object> *value_transfer_args,
                                                 const ValuePtr &data_value, const TypePtr &data_type) {
  ValueTransferType tensor_update_type = ValueTransferType::kTensorScatterUpdate;
  Slice slice_info = Slice(slice_index.slice(), data_shape[0]);
  int64_t start = slice_info.start();
  int64_t stop = slice_info.stop();
  int64_t step = slice_info.step();
  if (step >= 0 && data_value != nullptr) {
    std::vector<int64_t> data_transfer_types;
    std::vector<py::object> data_transfer_args;
    std::vector<int64_t> begin_info(data_shape.size(), 0);
    std::vector<int64_t> end_info(data_shape);
    std::vector<int64_t> step_info(data_shape.size(), 1);
    std::vector<pynative::SliceOpInfoPtr> slice_op_infos;
    if (start >= stop) {
      (void)data_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kJustReturn));
      return py::make_tuple(py::str("view"), py::tuple(), py::tuple(), VectorToPyTuple(data_transfer_types),
                            py::tuple());
    }
    if (slice_info.start() != 0 || slice_info.step() != 1 || slice_info.stop() != end_info[0]) {
      begin_info[0] = slice_info.start();
      end_info[0] = slice_info.stop();
      step_info[0] = slice_info.step();
      auto slice_op_info = std::make_shared<pynative::SliceOpInfo>();
      slice_op_info->slice_op_name = prim::kPrimStridedSlice->name();
      (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(begin_info));
      (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(end_info));
      (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(step_info));
      (void)slice_op_info->data_indexs.emplace_back(0);
      (void)slice_op_infos.emplace_back(slice_op_info);
    }
    auto new_data_shape = data_shape;
    if (step != 0) {
      auto new_shape_zero = (stop - start) / step;
      new_data_shape[0] = (new_shape_zero < 0 ? 0 : (stop + step - 1 - start) / step);
    }
    auto slice_output = SetitemCopyView(&slice_op_infos, data_value, new_data_shape, data_type, py_value_handle_);
    if (slice_output != py::none()) {
      data_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kJustReturn));
      data_transfer_args.emplace_back(slice_output);
      return py::make_tuple(py::str("view"), py::tuple(), py::tuple(), VectorToPyTuple(data_transfer_types),
                            VectorToPyTuple(data_transfer_args));
    }
    (void)data_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kStrideSlice));
    (void)data_transfer_args.emplace_back(py::make_tuple(
      py::make_tuple(slice_info.start()), py::make_tuple(slice_info.stop()), py::make_tuple(slice_info.step())));
    (void)data_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kCopyView));
    (void)data_transfer_args.emplace_back(py::none());
    return py::make_tuple(py::str("view"), VectorToPyTuple<int64_t>(*value_transfer_types),
                          VectorToPyTuple<py::object>(*value_transfer_args), VectorToPyTuple(data_transfer_types),
                          VectorToPyTuple(data_transfer_args));
  }
  if (slice_index.slice().step() == 1 && !TensorIndex::is_ascend_) {
    if (data_shape.empty()) {
      MS_EXCEPTION(TypeError) << "Cannot iterate over a scalar tensor.";
    }
    int64_t dim0_size = stop - start;
    if (dim0_size <= 0) {
      tensor_update_type = ValueTransferType::kByPass;
      return py::make_tuple(py::none(), VectorToPyTuple<int64_t>(*value_transfer_types),
                            VectorToPyTuple<py::object>(*value_transfer_args),
                            py::make_tuple(static_cast<int>(tensor_update_type)), py::make_tuple(py::none()));
    }
    ShapeVector value_shape = {dim0_size};
    (void)value_shape.insert(value_shape.end(), data_shape.begin() + 1, data_shape.end());
    value_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kBroadCast));
    value_transfer_args->emplace_back(VectorToPyTuple(value_shape));
    value_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kCast));
    value_transfer_args->emplace_back(py::none());
    tensor_update_type = ValueTransferType::kCopySlice;
    return py::make_tuple(
      py::none(), VectorToPyTuple<int64_t>(*value_transfer_types), VectorToPyTuple<py::object>(*value_transfer_args),
      py::make_tuple(static_cast<int>(tensor_update_type)),
      py::make_tuple(py::make_tuple(py::make_tuple(start), py::make_tuple(stop), py::make_tuple(step))));
  }
  TensorIndex indices = SliceToArray(slice_index, data_shape);
  if (indices.IsBoolean()) {
    tensor_update_type = ValueTransferType::kByPass;
    return py::make_tuple(indices.boolean(), VectorToPyTuple<int64_t>(*value_transfer_types),
                          VectorToPyTuple<py::object>(*value_transfer_args),
                          py::make_tuple(static_cast<int>(tensor_update_type)), py::make_tuple(py::none()));
  }
  value_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kBroadCast));
  TensorPtr indices_tensor = TensorPy::MakeTensor(TensorIndex::np_module_.attr("array")(indices.array()));
  MS_EXCEPTION_IF_NULL(indices_tensor);
  ShapeVector broad_cast_shape(indices_tensor->shape().begin(), indices_tensor->shape().end() - 1);
  value_transfer_args->emplace_back(VectorToPyTuple(broad_cast_shape));
  value_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kCast));
  value_transfer_args->emplace_back(py::none());
  return py::make_tuple(indices_tensor, VectorToPyTuple<int64_t>(*value_transfer_types),
                        VectorToPyTuple<py::object>(*value_transfer_args),
                        py::make_tuple(static_cast<int>(tensor_update_type)), py::make_tuple(py::none()));
}

py::array TensorIndex::SetItemByTensorByBool(const ShapeVector &data_shape, const TensorPtr &index, int64_t data_dims,
                                             std::vector<int64_t> *value_transfer_types,
                                             std::vector<py::object> *value_transfer_args,
                                             ValueTransferType *tensor_update_type) {
  ShapeVector index_shape = GeneratePaddingShape(index->shape(), data_dims);
  py::array np_index = TensorPy::SyncAsNumpy(*index);
  py::array output_np_index = TensorIndex::np_module_.attr("broadcast_to")(
    TensorIndex::np_module_.attr("reshape")(np_index, VectorToPyTuple(index_shape)), VectorToPyTuple(data_shape));
  value_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kCast));
  value_transfer_args->emplace_back(py::none());
  value_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kBroadCast));
  value_transfer_args->emplace_back(VectorToPyTuple(data_shape));
  *tensor_update_type = ValueTransferType::kSelect;
  return output_np_index;
}

// ***********************************************get get_item info*******************************************
py::object TensorIndex::GetItemByTensor(const ShapeVector &data_shape, const TensorPtr &index) {
  MS_EXCEPTION_IF_NULL(index);
  MS_LOG(DEBUG) << "In branch get item by tensor, data_shape: " << data_shape
                << " tensor_indexes: " << index->ToString();
  constexpr int min_data_dim = 1;
  constexpr int max_data_dim = 7;
  const int64_t data_dim = SizeToLong(data_shape.size());
  JudgeDataDim(data_dim, min_data_dim, max_data_dim);
  py::object output = py::none();
  if (CheckTypeIsInstance<TypeId>(index->data_type(), kIntTypes)) {
    output =
      py::make_tuple(index, py::make_tuple(static_cast<int>(ValueTransferType::kGather)), py::make_tuple(py::none()));
  } else if (index->data_type() == kNumberTypeBool) {
    py::tuple nonzero_indices = GenerateNonZeroIndex(data_shape, index, true);
    MS_EXCEPTION_IF_CHECK_FAIL(!nonzero_indices.empty(), "Output size of nonzero should not be empty");
    int64_t nonzero_indices_nums = SizeToLong(len(py::array(nonzero_indices[0])));
    if (nonzero_indices_nums == 0) {
      ShapeVector empty_tensor_shape(data_shape.begin() + index->DataDim(), data_shape.end());
      (void)empty_tensor_shape.insert(empty_tensor_shape.begin(), 0);

      return py::make_tuple(py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kEmptyTensor)),
                            py::make_tuple(VectorToPyTuple(empty_tensor_shape)));
    }
    output = py::make_tuple(index, py::make_tuple(static_cast<int>(ValueTransferType::kGetitemByBoolTensor)),
                            py::make_tuple(py::none()));
  } else {
    MS_EXCEPTION(IndexError) << "The tensor index must be int or bool type, but got " << TensorIndex::py_index_handle_;
  }
  return output;
}

py::object TensorIndex::GetItemByList(const ShapeVector &data_shape, const TensorIndex &tensor_index) {
  MS_LOG(DEBUG) << "In branch get item by List, data_shape: " << data_shape << " tensor_index: " << tensor_index;
  constexpr int min_data_dim = 1;
  constexpr int max_data_dim = 8;
  int64_t data_dim = SizeToLong(data_shape.size());
  JudgeDataDim(data_dim, min_data_dim, max_data_dim);
  bool use_gather = std::all_of(tensor_index.list().begin(), tensor_index.list().end(),
                                [](auto &x) { return py::isinstance<py::int_>(x) || py::isinstance<py::bool_>(x); });
  if (use_gather) {
    if (data_shape.empty()) {
      MS_EXCEPTION(TypeError) << "Cannot iterate over a scalar tensor.";
    }
    TensorIndex tuple_index = SequenceToTensor(tensor_index, data_shape[0]);
    if (tuple_index.IsBoolean() && !tuple_index.boolean()) {
      MS_EXCEPTION(IndexError) << "When tensor is indexed by list, the list can't be empty.";
    }
    return py::make_tuple(tuple_index.tensor(), py::make_tuple(static_cast<int>(ValueTransferType::kGather)),
                          py::make_tuple(py::none()));
  }
  return GetItemByTuple(data_shape, tensor_index.ExpandToVector());
}

static void JudgeTupleIndexDim(int64_t data_dim, const std::vector<TensorIndex> &new_tuple_indexes) {
  int64_t index_dims = 0;
  for (const TensorIndex &index : new_tuple_indexes) {
    if (index.IsTensor() && index.tensor() != nullptr && index.tensor()->data_type() == kNumberTypeBool) {
      index_dims += index.tensor()->DataDim();
    } else {
      index_dims += 1;
    }
  }
  if (index_dims > data_dim) {
    MS_EXCEPTION(IndexError) << "The dim of index cannot be greater than indexed data, but got dim of index:"
                             << index_dims << ", dim of data:" << data_dim;
  }
}

size_t GetSpecifiedDimensions(const py::tuple &new_tuple_index, size_t data_dims) {
  size_t specified_dimensions = std::count_if(new_tuple_index.begin(), new_tuple_index.end(), [](auto const &obj) {
    return (obj != Py_None && obj != Py_Ellipsis && obj != Py_True && obj != Py_False);
  });
  constexpr size_t max_data_dim = 8;
  if (data_dims > max_data_dim) {
    MS_EXCEPTION(ValueError) << "The input data's dim must in the range of [0, " << max_data_dim << "], but got '"
                             << data_dims << "'.";
  }
  if (specified_dimensions > data_dims) {
    MS_EXCEPTION(IndexError) << "too many indices for tensor of dimension" << data_dims;
  }
  return specified_dimensions;
}

namespace {
void CheckDataDim(const ShapeVector &data_shape) {
  constexpr size_t max_data_dim = 8;
  if (data_shape.size() > max_data_dim) {
    MS_EXCEPTION(ValueError) << "The input data's dim must in the range of [1, " << max_data_dim << "], but got '"
                             << data_shape.size() << "'.";
  }
}

void CheckNumberOfEllipsis(const size_t counter) {
  if (counter > 0) {
    MS_EXCEPTION(IndexError) << "An index can only have a single ellipsis('...')";
  }
}
}  // namespace

bool TensorIndex::GetItemByTupleWithView(const ValuePtr &data_value, const ShapeVector &data_shape,
                                         const py::object &py_index, std::vector<int64_t> *data_transfer_types,
                                         std::vector<py::object> *data_transfer_args, const TypePtr &data_type) {
  if (data_value == nullptr) {
    return false;
  }
  MS_LOG(DEBUG) << "In branch get item by tuple with view, data_shape: " << data_shape
                << " tensor_indexes: " << py_index;
  size_t data_dims = data_shape.size();
  auto new_tuple_index = py_index.cast<py::tuple>();
  size_t specified_dimensions = GetSpecifiedDimensions(new_tuple_index, data_dims);
  bool empty_strided_slice_result = false;
  auto new_data_shape = data_shape;
  size_t dim = 0;
  std::vector<pynative::SliceOpInfoPtr> slice_op_infos;
  size_t ellipsis_count = 0;
  for (auto const &obj : new_tuple_index) {
    if (py::isinstance<py::int_>(obj) && !py::isinstance<py::bool_>(obj)) {
      auto index = py::cast<int64_t>(obj);
      if (index >= new_data_shape[dim] || index < -new_data_shape[dim]) {
        // Raise exception in python, because python iterator need raise IndexError to stop for loop.
        data_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kRaiseIndexError));
        data_transfer_args->emplace_back(py::make_tuple(index, new_data_shape[dim]));
        return true;
      }
      int64_t transformed_number = CheckRange(index, new_data_shape[dim]);
      auto slice_op_info = std::make_shared<pynative::SliceOpInfo>();
      slice_op_info->slice_op_name = prim::kPrimSelectView->name();
      (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(transformed_number));
      (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(dim));
      (void)slice_op_info->data_indexs.emplace_back(0);
      (void)slice_op_infos.emplace_back(slice_op_info);
      (void)new_data_shape.erase(new_data_shape.begin() + dim);
    } else if (py::isinstance<py::slice>(obj)) {
      auto slice_info = Slice(TensorIndex(obj).slice(), new_data_shape[dim]);
      std::vector<int64_t> begin_info(new_data_shape.size(), 0);
      std::vector<int64_t> end_info(new_data_shape);
      std::vector<int64_t> step_info(new_data_shape.size(), 1);
      if (slice_info.step() < 0) {
        data_transfer_types->clear();
        data_transfer_args->clear();
        return false;
      }
      if (slice_info.start() == 0 && slice_info.step() == 1 && slice_info.stop() == end_info[dim]) {
        dim++;
        continue;
      }
      empty_strided_slice_result = (slice_info.start() >= slice_info.stop());
      begin_info[dim] = slice_info.start();
      end_info[dim] = slice_info.stop();
      step_info[dim] = slice_info.step();
      auto slice_op_info = std::make_shared<pynative::SliceOpInfo>();
      slice_op_info->slice_op_name = prim::kPrimStridedSlice->name();
      (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(begin_info));
      (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(end_info));
      (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(step_info));
      (void)slice_op_info->data_indexs.emplace_back(0);
      (void)slice_op_infos.emplace_back(slice_op_info);
      new_data_shape[dim] = (slice_info.stop() + slice_info.step() - 1 - slice_info.start()) / slice_info.step();
      dim++;
    } else if (py::isinstance<py::ellipsis>(obj)) {
      CheckNumberOfEllipsis(ellipsis_count);
      dim += data_shape.size() - specified_dimensions;
      ellipsis_count += 1;
    } else if (py::isinstance<py::none>(obj)) {
      auto slice_op_info = std::make_shared<pynative::SliceOpInfo>();
      slice_op_info->slice_op_name = prim::kPrimExpandDims->name();
      (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(dim));
      (void)slice_op_info->data_indexs.emplace_back(0);
      (void)slice_op_infos.emplace_back(slice_op_info);
      new_data_shape.insert(new_data_shape.begin() + dim, 1);
      dim++;
    } else {
      data_transfer_types->clear();
      data_transfer_args->clear();
      return false;
    }
  }
  CheckDataDim(new_data_shape);
  py::object slice_output;
  if (data_type != nullptr) {
    if (empty_strided_slice_result) {
      data_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kByPass));
      data_transfer_args->emplace_back(py::none());
      return true;
    }
    slice_output = SetitemCopyView(&slice_op_infos, data_value, new_data_shape, data_type, py_value_handle_);
    if (slice_output == py::none()) {
      return false;
    }
  } else {
    if (slice_op_infos.empty()) {
      data_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kByPass));
      data_transfer_args->emplace_back(py::none());
      return true;
    }
    slice_output = pynative::PyNativeExecutor::GetInstance()->RunSliceOpStub({data_value}, slice_op_infos);
  }
  data_transfer_types->emplace_back(static_cast<int>(ValueTransferType::kJustReturn));
  data_transfer_args->emplace_back(slice_output);
  return true;
}

py::object TensorIndex::GetItemByTuple(const ShapeVector &data_shape, const std::vector<TensorIndex> &tensor_indexes) {
  MS_LOG(DEBUG) << "In branch get item by tuple, data_shape: " << data_shape << " tensor_indexes: " << tensor_indexes;
  std::vector<int64_t> data_transfer_types;
  std::vector<py::object> data_transfer_args;
  ShapeVector new_data_shape = data_shape;
  if (tensor_indexes.empty()) {
    return py::make_tuple(py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kByPass)),
                          py::make_tuple(py::none()));
  }
  std::vector<TensorIndex> new_tuple_indexes = TransformEllipsisToSlice(new_data_shape, tensor_indexes);
  std::tuple expand_dim_info = GetExpandDimsInfo(new_data_shape, new_tuple_indexes);
  constexpr size_t expand_dim_info_index = 0;
  constexpr size_t new_data_shape_index = 1;
  constexpr size_t new_tuple_indexes_index = 2;
  bool need_expand_dim = std::get<expand_dim_info_index>(expand_dim_info);
  if (need_expand_dim) {
    (void)data_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kReshape));
    new_data_shape = std::get<new_data_shape_index>(expand_dim_info);
    (void)data_transfer_args.emplace_back(VectorToPyTuple(new_data_shape));
    new_tuple_indexes = std::get<new_tuple_indexes_index>(expand_dim_info);  // NOLINT
  }
  constexpr int min_data_dim = 1;
  constexpr int max_data_dim = 8;
  int64_t data_dim = SizeToLong(new_data_shape.size());
  JudgeDataDim(data_dim, min_data_dim, max_data_dim);
  JudgeTupleIndexDim(data_dim, new_tuple_indexes);
  bool normal_tuple = std::all_of(new_tuple_indexes.begin(), new_tuple_indexes.end(), [](auto &index_e) {
    return index_e.IsEllipsis() || index_e.IsInteger() || index_e.IsSlice();
  });
  if (normal_tuple) {
    std::tuple stride_slice_info = GetStrideInfoFromTuple(new_data_shape, new_tuple_indexes);
    (void)data_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kStrideSliceWithMask));
    std::vector<std::vector<int64_t>> stride_info = std::get<0>(stride_slice_info);
    std::vector<py::tuple> py_stride_info;
    (void)std::transform(stride_info.begin(), stride_info.end(), std::back_inserter(py_stride_info),
                         [](auto &stride_info_i) { return VectorToPyTuple(stride_info_i); });
    std::vector<int64_t> mask_info = std::get<1>(stride_slice_info);
    (void)data_transfer_args.emplace_back(py::make_tuple(VectorToPyTuple(py_stride_info), VectorToPyTuple(mask_info)));
    return py::make_tuple(py::none(), VectorToPyTuple(data_transfer_types), VectorToPyTuple(data_transfer_args));
  }
  return TensorGetitemByTuple(new_data_shape, new_tuple_indexes, &data_transfer_types, &data_transfer_args);
}

py::object TensorIndex::GetItemByBool(const ValuePtr &data_value, const ShapeVector &data_shape, bool index) {
  MS_LOG(INFO) << "(View) In branch get item by bool, data_shape: " << data_shape << " tensor_indexes: " << index;
  constexpr int min_data_dim = 0;
  constexpr int max_data_dim = 7;
  int64_t data_dim = SizeToLong(data_shape.size());
  JudgeDataDim(data_dim, min_data_dim, max_data_dim);
  if (!index) {
    MS_EXCEPTION(IndexError) << "When tensor is indexed by a bool object, the value only support 'True'.";
  }
  auto transfer_type = (data_value == nullptr ? ValueTransferType::kExpandDims : ValueTransferType::kUnsqueeze);
  return py::make_tuple(py::none(), py::make_tuple(static_cast<int>(transfer_type)), py::make_tuple(py::int_(0)));
}

py::object TensorIndex::GetItemByNumber(const ShapeVector &data_shape, int64_t index) {
  MS_LOG(DEBUG) << "In branch get item by number, data_shape: " << data_shape << " tensor_indexes: " << index;
  if (data_shape.empty()) {
    MS_EXCEPTION(TypeError) << "Cannot iterate over a scalar tensor.";
  }
  constexpr int min_data_dim = 1;
  constexpr int max_data_dim = 8;
  int64_t data_dim = SizeToLong(data_shape.size());
  JudgeDataDim(data_dim, min_data_dim, max_data_dim);
  if (index >= data_shape[0] || index < -data_shape[0]) {
    // Raise exception in python, because python iterator need raise IndexError to stop for loop.
    return py::make_tuple(py::make_tuple(py::none()),
                          py::make_tuple(static_cast<int>(ValueTransferType::kRaiseIndexError)),
                          py::make_tuple(py::make_tuple(index, data_shape[0])));
  }
  int64_t transformed_number = CheckRange(index, data_shape[0]);
  if (!TensorIndex::is_ascend_) {
    return py::make_tuple(std::make_shared<Tensor>(transformed_number),
                          py::make_tuple(static_cast<int>(ValueTransferType::kGather)), py::make_tuple(py::none()));
  }
  std::vector<int64_t> begin_strides = {transformed_number};
  std::vector<int64_t> end_strides = {transformed_number + 1};
  std::vector<int64_t> step_strides = {1};
  for (size_t i = 1; i < data_shape.size(); i++) {
    (void)begin_strides.emplace_back(0);
    (void)end_strides.emplace_back(data_shape[i]);
    (void)step_strides.emplace_back(1);
  }
  int64_t shrink_axis_mask = 1;
  int64_t begin_mask = 0;
  int64_t end_mask = 0;
  constexpr size_t begin_mask_begin_bit = 2;
  constexpr size_t begin_mask_end_bit = 8;
  for (size_t i = begin_mask_begin_bit; i < begin_mask_end_bit; i++) {
    const auto mask_bit = 1 << i;
    begin_mask += mask_bit;
    end_mask += mask_bit;
  }

  py::tuple stride_info =
    py::make_tuple(VectorToPyTuple(begin_strides), VectorToPyTuple(end_strides), VectorToPyTuple(step_strides));
  py::tuple mask_info = py::make_tuple(begin_mask, end_mask, shrink_axis_mask);
  return py::make_tuple(py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kStrideSliceWithMask)),
                        py::make_tuple(py::make_tuple(stride_info, mask_info)));
}

py::object TensorIndex::GetItemByNumberWithView(const ValuePtr &data_value, const ShapeVector &data_shape,
                                                int64_t index) {
  MS_LOG(INFO) << "(View) In branch get item by number, data_shape: " << data_shape << " tensor_indexes: " << index;
  if (data_shape.empty()) {
    MS_EXCEPTION(TypeError) << "Cannot iterate over a scalar tensor.";
  }
  constexpr int min_data_dim = 1;
  constexpr int max_data_dim = 8;
  int64_t data_dim = SizeToLong(data_shape.size());
  JudgeDataDim(data_dim, min_data_dim, max_data_dim);
  if (index >= data_shape[0] || index < -data_shape[0]) {
    // Raise exception in python, because python iterator need raise IndexError to stop for loop.
    return py::make_tuple(py::make_tuple(py::none()),
                          py::make_tuple(static_cast<int>(ValueTransferType::kRaiseIndexError)),
                          py::make_tuple(py::make_tuple(index, data_shape[0])));
  }
  int64_t transformed_number = CheckRange(index, data_shape[0]);
  // return py::make_tuple(py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kSelectView)),
  //                       py::make_tuple(py::make_tuple(py::int_(transformed_number), py::int_(0))));
  int64_t dim = 0;
  auto slice_op_info = std::make_shared<pynative::SliceOpInfo>();

  slice_op_info->slice_op_name = prim::kPrimSelectView->name();
  (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(transformed_number));
  (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(dim));
  (void)slice_op_info->data_indexs.emplace_back(0);

  auto slice_output = pynative::PyNativeExecutor::GetInstance()->RunSliceOpStub({data_value}, {slice_op_info});
  return py::make_tuple(py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kJustReturn)),
                        py::make_tuple(slice_output));
}

py::object TensorIndex::GetItemBySlice(const ValuePtr &data_value, const ShapeVector &data_shape,
                                       const TensorIndex &py_index) {
  MS_LOG(INFO) << "(View) In branch get item by slice, data_shape: " << data_shape << " tensor_indexes: " << py_index;
  constexpr int min_data_dim = 1;
  constexpr int max_data_dim = 8;
  size_t data_dim = data_shape.size();
  JudgeDataDim(SizeToLong(data_dim), min_data_dim, max_data_dim);
  if (data_shape.empty()) {
    MS_EXCEPTION(TypeError) << "Cannot iterate over a scalar tensor.";
  }
  Slice slice_info = Slice(py_index.slice(), data_shape[0]);
  if (slice_info.step() >= 0 && data_value != nullptr) {
    std::vector<int64_t> begin_info(data_dim, 0);
    std::vector<int64_t> end_info(data_shape);
    std::vector<int64_t> step_info(data_dim, 1);
    begin_info[0] = slice_info.start();
    end_info[0] = slice_info.stop();
    step_info[0] = slice_info.step();
    return py::make_tuple(py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kStrideSlice)),
                          py::make_tuple(py::make_tuple(VectorToPyTuple(begin_info), VectorToPyTuple(end_info),
                                                        VectorToPyTuple(step_info))));
  }
  int64_t begin_mask = slice_info.start_init_by_none() ? 1 : 0;
  int64_t end_mask = slice_info.stop_init_by_none() ? 1 : 0;
  for (size_t i = 1; i < data_dim; i++) {
    const auto mask_bit = 1 << i;
    begin_mask += mask_bit;
    end_mask += mask_bit;
  }
  if (begin_mask != 0 || end_mask != 0) {
    py::tuple stride_info = py::make_tuple(py::make_tuple(slice_info.start()), py::make_tuple(slice_info.stop()),
                                           py::make_tuple(slice_info.step()));
    py::tuple mask_info = py::make_tuple(begin_mask, end_mask, 0);
    return py::make_tuple(py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kStrideSliceWithMask)),
                          py::make_tuple(py::make_tuple(stride_info, mask_info)));
  }
  return py::make_tuple(
    py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kStrideSlice)),
    py::make_tuple(py::make_tuple(py::make_tuple(slice_info.start()), py::make_tuple(slice_info.stop()),
                                  py::make_tuple(slice_info.step()))));
}

py::object TensorIndex::GetItemIndexSimpleIndex(const py::object &py_index, const ValuePtr &data_value,
                                                const ShapeVector &data_shape) {
  if (py::isinstance<py::bool_>(py_index)) {
    return TensorIndex::GetItemByBool(data_value, data_shape, TensorIndex(py_index).boolean());
  }
  if (data_value != nullptr && py::isinstance<py::int_>(py_index)) {
    return TensorIndex::GetItemByNumberWithView(data_value, data_shape, TensorIndex(py_index).integer());
  }
  if (py::isinstance<py::slice>(py_index) || TensorIndex(py_index).slice().step() == -1) {
    return TensorIndex::GetItemBySlice(data_value, data_shape, TensorIndex(py_index));
  }
  if (py::isinstance<py::none>(py_index)) {
    return TensorIndex::GetItemByBool(data_value, data_shape, 1);
  }
  return py::none();
}

TypeId GetStubAbsTypeId(const AbstractBasePtr &abs) {
  MS_EXCEPTION_IF_NULL(abs);

  if (abs->isa<abstract::AbstractTensor>()) {
    auto tensor_abs = abs->cast<abstract::AbstractTensorPtr>();
    MS_EXCEPTION_IF_NULL(tensor_abs);
    MS_EXCEPTION_IF_NULL(tensor_abs->element());
    MS_EXCEPTION_IF_NULL(tensor_abs->element()->BuildType());
    return tensor_abs->element()->BuildType()->type_id();
  } else {
    MS_EXCEPTION_IF_NULL(abs->BuildType());
    return abs->BuildType()->type_id();
  }
}

bool EnableView(bool is_setitem = false) {
  if (pynative::PyNativeExecutor::GetInstance()->grad_executor()->is_high_order_top_cell()) {
    // 1. pack node will slice failed with view.
    // 2. SelectView and CopyWithSlice has no kernel, can not enable view in high order cell.
    return false;
  }

  // For setitem, the grad of CopyWithSlice is erroneous. If we are in setitem and requires grad, disable view.
  if (is_setitem && pynative::PyNativeExecutor::GetInstance()->grad_executor()->RequiresGrad()) return false;

  return true;
}

py::object TensorIndex::GetItemIndexInfo(const py::object &py_data, const py::object &py_index,
                                         const py::bool_ &is_ascend) {
  ShapeVector data_shape;
  ValuePtr data_value;
  if (IsStubTensor(py_data)) {
    auto value = GetStubTensorValue(py_data);
    MS_EXCEPTION_IF_NULL(value);
    auto abs = value->ToAbstract();
    MS_EXCEPTION_IF_NULL(abs);
    data_shape = dyn_cast<abstract::Shape>(abs->BuildShape())->shape();

    if (EnableView()) {
      data_value = value;
    }
  } else if (py::isinstance<Tensor>(py_data)) {
    auto tensor = py_data.cast<TensorPtr>();
    MS_EXCEPTION_IF_NULL(tensor);
    if (EnableView()) {
      data_value = tensor;
    }
    data_shape = tensor->shape();
  } else {
    MS_EXCEPTION(TypeError) << "First input of Tensor index must be tensor but got " << py_data;
  }

  const auto &simple_index_output = GetItemIndexSimpleIndex(py_index, data_value, data_shape);
  if (simple_index_output != py::none()) {
    return simple_index_output;
  }

  std::vector<int64_t> data_transfer_types;
  std::vector<py::object> data_transfer_args;
  if (py::isinstance<py::tuple>(py_index) &&
      GetItemByTupleWithView(data_value, data_shape, py_index, &data_transfer_types, &data_transfer_args, nullptr)) {
    MS_LOG(INFO) << "(View) In branch get item by tuple with view, data_shape: " << data_shape
                 << " tensor_indexes: " << py_index;
    return py::make_tuple(py::none(), VectorToPyTuple(data_transfer_types), VectorToPyTuple(data_transfer_args));
  }
  MS_LOG(INFO) << "(Tensor) Get item datashape is: " << data_shape << ", index is: " << py_index;
  py::object new_py_index = IsStubTensor(py_index) ? py::cast(ConvertStubTensor(py_index)) : py_index;
  TensorIndex::py_index_handle_ = new_py_index;
  TensorIndex::is_ascend_ = is_ascend;
  TensorIndex::np_module_ = py::module::import("numpy");
  TensorIndex::index_op_type_ = IndexOpType::GetItem;
  TensorIndex index(new_py_index);
  CheckGetItemIndex(index.type());
  py::object output = py::none();
  switch (index.type()) {
    case TensorIndexType::Tensor: {
      output = GetItemByTensor(data_shape, index.tensor());
      break;
    }
    case TensorIndexType::List: {
      output = GetItemByList(data_shape, index);
      break;
    }
    case TensorIndexType::Tuple: {
      output = GetItemByTuple(data_shape, index.ExpandToVector());
      break;
    }
    case TensorIndexType::Boolean: {
      output = GetItemByBool(data_value, data_shape, index.boolean());
      break;
    }
    case TensorIndexType::Ellipsis: {
      output = py::make_tuple(py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kByPass)),
                              py::make_tuple(py::none()));
      break;
    }
    case TensorIndexType::Integer: {
      output = GetItemByNumber(data_shape, index.integer());
      break;
    }
    default: {
      MS_EXCEPTION(TypeError)
        << "Only support integers, slices(`:`), ellipsis(`...`), None, bool, tensor, int, list and "
           "tuple as index, but got "
        << TensorIndex::py_index_handle_ << " with type " << TensorIndex::py_index_handle_.get_type();
    }
  }
  return output;
}

// ***********************************************get set_item info*******************************************
py::object TensorIndex::SetItemByNumber(const ShapeVector &data_shape, const TypePtr &data_type, bool is_parameter,
                                        const TensorIndex &tensor_index, const TensorIndexType &py_value_type) {
  // If tensor is small, we use method in IntToTensor for faster
  MS_LOG(DEBUG) << "In branch Set item by number, data_shape: " << data_shape << " tensor_indexes: " << tensor_index
                << "value: " << TensorIndex::py_value_handle_;

  std::tuple<int64_t, py::object, ShapeVector> value_transfer =
    GetValueTransferType(py_value_type, set_item_by_non_tensor, data_type, false);
  std::vector<int64_t> value_transfer_types = {std::get<0>(value_transfer)};
  std::vector<py::object> value_transfer_args = {std::get<1>(value_transfer)};
  if (data_shape.empty()) {
    MS_EXCEPTION(TypeError) << "Cannot iterate over a scalar tensor.";
  }
  int64_t dim_size = data_shape[0];
  int64_t index = tensor_index.integer();
  if (index < -dim_size || index >= dim_size) {
    MS_EXCEPTION(IndexError) << "Index " << index << " is out of bounds for axis 0 with size " << dim_size;
  }
  TensorPtr new_index = std::make_shared<Tensor>();
  if (!CheckLargeTensor(data_shape)) {
    new_index = IntToTensor(index, data_shape);
    (void)value_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kBroadCast));
    MS_EXCEPTION_IF_NULL(new_index);
    ShapeVector value_shape(new_index->shape().begin(), new_index->shape().end() - 1);
    value_transfer_args.push_back(VectorToPyTuple<int64_t>(value_shape));
  } else {
    auto out_i = static_cast<int32_t>(CheckRange(index, dim_size));
    new_index = std::make_shared<Tensor>(kNumberTypeInt32, ShapeVector({1, 1}), &out_i, int32_bytes_number);
    ShapeVector updates_shape = {1};
    (void)updates_shape.insert(updates_shape.end(), data_shape.begin() + 1, data_shape.end());
    (void)value_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kBroadCast));
    (void)value_transfer_args.emplace_back(VectorToPyTuple(updates_shape));
  }
  ValueTransferType data_transfer_type =
    is_parameter ? ValueTransferType::kScatterNdUpdate : ValueTransferType::kTensorScatterUpdate;
  return py::make_tuple(new_index, VectorToPyTuple<int64_t>(value_transfer_types),
                        VectorToPyTuple<py::object>(value_transfer_args),
                        py::make_tuple(static_cast<int>(data_transfer_type)), py::make_tuple(py::none()));
}

py::object TensorIndex::SetItemByNumberWithView(const ShapeVector &data_shape, const TypePtr &data_type,
                                                bool is_parameter, const TensorIndex &tensor_index,
                                                const TensorIndexType &py_value_type, const ValuePtr &data_value) {
  // If tensor is small, we use method in IntToTensor for faster
  MS_LOG(INFO) << "(View) In branch set item by number, data_shape: " << data_shape
               << " tensor_indexes: " << tensor_index << "value: " << TensorIndex::py_value_handle_;

  std::tuple<int64_t, py::object, ShapeVector> value_transfer =
    GetValueTransferType(py_value_type, set_item_by_non_tensor, data_type, true);
  std::vector<int64_t> value_transfer_types = {std::get<0>(value_transfer)};
  std::vector<py::object> value_transfer_args = {std::get<1>(value_transfer)};
  if (data_shape.empty()) {
    MS_EXCEPTION(TypeError) << "Cannot iterate over a scalar tensor.";
  }
  int64_t dim_size = data_shape[0];
  int64_t index = tensor_index.integer();
  if (index < -dim_size || index >= dim_size) {
    MS_EXCEPTION(IndexError) << "Index " << index << " is out of bounds for axis 0 with size " << dim_size;
  }
  ShapeVector updates_shape = {1};
  (void)updates_shape.insert(updates_shape.end(), data_shape.begin() + 1, data_shape.end());
  std::vector<int64_t> data_transfer_types;
  std::vector<py::object> data_transfer_args;
  int64_t transformed_number = CheckRange(index, data_shape.at(0));

  std::vector<pynative::SliceOpInfoPtr> slice_op_infos;
  std::vector<int64_t> new_data_shape(data_shape.begin() + 1, data_shape.end());
  auto slice_op_info = std::make_shared<pynative::SliceOpInfo>();
  slice_op_info->slice_op_name = prim::kPrimSelectView->name();
  (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(transformed_number));
  (void)slice_op_info->slice_index_inputs.emplace_back(std::make_shared<pynative::FastValue>(0));
  (void)slice_op_info->data_indexs.emplace_back(0);
  (void)slice_op_infos.emplace_back(slice_op_info);
  auto slice_output = SetitemCopyView(&slice_op_infos, data_value, new_data_shape, data_type, py_value_handle_);
  if (slice_output != py::none()) {
    data_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kJustReturn));
    data_transfer_args.emplace_back(slice_output);
    return py::make_tuple(py::str("view"), py::tuple(), py::tuple(), VectorToPyTuple(data_transfer_types),
                          VectorToPyTuple(data_transfer_args));
  }

  (void)data_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kSelectView));
  (void)data_transfer_args.emplace_back(py::make_tuple(py::int_(transformed_number), py::int_(0)));
  (void)data_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kCopyView));
  (void)data_transfer_args.emplace_back(py::none());
  return py::make_tuple(py::str("view"), VectorToPyTuple<int64_t>(value_transfer_types),
                        VectorToPyTuple<py::object>(value_transfer_args), VectorToPyTuple(data_transfer_types),
                        VectorToPyTuple(data_transfer_args));
}

py::object TensorIndex::SetItemByTensor(const ShapeVector &data_shape, bool is_parameter,
                                        const TensorIndex &tensor_index, const TensorIndexType &py_value_type) {
  MS_LOG(DEBUG) << "In branch Set item by tensor, data_shape: " << data_shape << " tensor_indexes: " << tensor_index
                << "value: " << TensorIndex::py_value_handle_;
  std::vector<int64_t> value_transfer_types;
  std::vector<py::object> value_transfer_args;
  const TensorPtr &index = tensor_index.tensor();
  int64_t data_dims = SizeToLong(data_shape.size());
  MS_EXCEPTION_IF_NULL(index);
  bool format_index_tensor = false;
  ValueTransferType tensor_update_type = ValueTransferType::kTensorScatterUpdate;
  py::array np_index;
  if (CheckTypeIsInstance(py_value_type, {TensorIndexType::Float, TensorIndexType::Integer, TensorIndexType::Boolean,
                                          TensorIndexType::Tensor})) {
    if (!CheckTypeIsInstance<TypeId>(index->data_type(), {kNumberTypeInt8, kNumberTypeInt16, kNumberTypeInt32,
                                                          kNumberTypeInt64, kNumberTypeBool})) {
      MS_EXCEPTION(IndexError) << "For tensor set item, the index tensor data type" << index->data_type()
                               << " is not supported.";
    }
    if (index->data_type() == kNumberTypeBool) {
      if (CheckScalarValue(TensorIndex::py_value_handle_)) {
        np_index = SetItemByTensorByBool(data_shape, index, data_dims, &value_transfer_types, &value_transfer_args,
                                         &tensor_update_type);
      } else {
        return py::make_tuple(index, py::make_tuple(), py::make_tuple(),
                              py::make_tuple(static_cast<int>(ValueTransferType::kSetitemByBoolTensor)),
                              py::make_tuple(py::none()));
      }
    } else {
      ShapeVector index_shape = index->shape();
      np_index = TensorPy::SyncAsNumpy(*index);
      if (index_shape.empty()) {
        np_index = TensorIndex::np_module_.attr("expand_dims")(np_index, -1);
        (void)index_shape.emplace_back(1);
      }
      ShapeVector updates_shape = index_shape;
      (void)updates_shape.insert(updates_shape.end(), data_shape.begin() + 1, data_shape.end());
      if (py_value_type != TensorIndexType::Tensor) {
        (void)value_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kNumberToTensor));
      } else {
        (void)value_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kCast));
      }
      (void)value_transfer_args.emplace_back(py::none());
      (void)value_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kBroadCast));
      (void)value_transfer_args.emplace_back(VectorToPyTuple(updates_shape));
      if (data_shape.empty()) {
        MS_EXCEPTION(TypeError) << "Cannot iterate over a scalar tensor.";
      }
      int64_t index_shape_dim = std::accumulate(index_shape.begin(), index_shape.end(), 1, std::multiplies<>());
      if (index_shape_dim <= 1) {
        int64_t first_val = data_shape[0];
        np_index = TensorIndex::np_module_.attr("select")(
          TensorIndex::np_module_.attr("less")(np_index, 0),
          TensorIndex::np_module_.attr("add")(np_index, py::int_(first_val)), np_index);
      } else {
        format_index_tensor = true;
      }
      np_index = TensorIndex::np_module_.attr("expand_dims")(np_index, -1);
      (void)index_shape.emplace_back(1);
      constexpr int64_t min_index_shape_size = 2;
      if (index_shape.size() < min_index_shape_size) {
        auto np_expand_dims_method = TensorIndex::np_module_.attr("expand_dims");
        np_index = np_expand_dims_method(np_index, 0);
        (void)value_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kExpandDims));
        (void)value_transfer_args.emplace_back(py::int_(0));
      }
      tensor_update_type = is_parameter ? ValueTransferType::kScatterNdUpdate : ValueTransferType::kTensorScatterUpdate;
    }
  } else if (py_value_type == TensorIndexType::Tuple || py_value_type == TensorIndexType::List) {
    (void)value_transfer_types.emplace_back(static_cast<int>(ValueTransferType::kHandleSequenceValue));
    (void)value_transfer_args.emplace_back(py::make_tuple(py::int_(set_item_by_one_tensor), index));
    if (CheckTypeIsInstance<TypeId>(index->data_type(), kIntTypes)) {
      np_index = TensorPy::SyncAsNumpy(*index);
      np_index = CastToInt(TensorIndex::np_module_.attr("expand_dims")(np_index, -1));
      tensor_update_type = ValueTransferType::kTensorScatterUpdate;
    } else if (index->data_type() == kNumberTypeBool) {
      return py::make_tuple(
        index, VectorToPyTuple<int64_t>(value_transfer_types), VectorToPyTuple<py::object>(value_transfer_args),
        py::make_tuple(static_cast<int>(ValueTransferType::kSetitemByBoolTensor)), py::make_tuple(py::none()));
    } else {
      MS_EXCEPTION(TypeError) << "The tensor index must be int or bool type, but got " << tensor_index;
    }
  }
  std::vector<int> tensor_update_types{static_cast<int>(tensor_update_type)};
  std::vector<py::object> tensor_update_args{py::none()};
  if (format_index_tensor) {
    (void)tensor_update_types.insert(tensor_update_types.begin(),
                                     static_cast<int>(ValueTransferType::kFormatIndexTensor));
    (void)tensor_update_args.insert(tensor_update_args.begin(), py::make_tuple(0, data_shape[0]));
  }
  return py::make_tuple(TensorPy::MakeTensor(TensorIndex::np_module_.attr("array")(np_index)),
                        VectorToPyTuple<int64_t>(value_transfer_types),
                        VectorToPyTuple<py::object>(value_transfer_args), VectorToPyTuple<int>(tensor_update_types),
                        VectorToPyTuple<py::object>(tensor_update_args));
}

py::object TensorIndex::SetItemByTuple(const ShapeVector &data_shape, const TypePtr &data_type,
                                       const TensorIndex &py_index, const TensorIndexType &py_value_type) {
  MS_LOG(DEBUG) << "In branch Set item by tuple, data_shape: " << data_shape << " tensor_indexes: " << py_index
                << "value: " << TensorIndex::py_value_handle_;
  if (!CheckTypeIsInstance<TensorIndexType>(py_value_type,
                                            {TensorIndexType::Integer, TensorIndexType::Float, TensorIndexType::Boolean,
                                             TensorIndexType::Tensor, TensorIndexType::List, TensorIndexType::Tuple})) {
    MS_EXCEPTION(TypeError) << "Only support int, float, bool, Tensor, list, tuple as value, but got "
                            << TensorIndex::py_value_handle_.get_type();
  }

  std::tuple<int64_t, py::object, ShapeVector> value_transfer =
    GetValueTransferType(py_value_type, set_item_by_non_tensor, data_type, false);
  constexpr size_t value_transfer_types_index = 0;
  constexpr size_t value_transfer_args_index = 1;
  constexpr size_t value_transfer_shapes_index = 2;
  std::vector<int64_t> value_transfer_types = {std::get<value_transfer_types_index>(value_transfer)};
  std::vector<py::object> value_transfer_args = {std::get<value_transfer_args_index>(value_transfer)};
  ShapeVector value_transfer_shape = {std::get<value_transfer_shapes_index>(value_transfer)};

  if (CheckTypeIsInstance<TensorIndexType>(
        py_value_type, {TensorIndexType::Boolean, TensorIndexType::Float, TensorIndexType::Integer})) {
    TensorIndex index = TensorIndex::UnpackTuple(py_index);
    std::vector<TensorIndex> index_list = index.ExpandToVector();
    return SetitemByTupleWithTensor(data_shape, index_list, value_transfer_shape, &value_transfer_types,
                                    &value_transfer_args);
  }
  std::vector<TensorIndex> index_list = py_index.ExpandToVector();
  return SetitemByTupleWithTensor(data_shape, index_list, value_transfer_shape, &value_transfer_types,
                                  &value_transfer_args);
}

py::object TensorIndex::SetItemBySlice(const ShapeVector &data_shape, const TypePtr &data_type,
                                       const TensorIndex &tensor_index, const TensorIndexType &py_value_type,
                                       const ValuePtr &data_value) {
  MS_LOG(INFO) << "(View) In branch set item by slice, data_shape: " << data_shape
               << " tensor_indexes: " << tensor_index << "value: " << TensorIndex::py_value_handle_;
  if (!CheckTypeIsInstance<TensorIndexType>(py_value_type,
                                            {TensorIndexType::Integer, TensorIndexType::Float, TensorIndexType::Boolean,
                                             TensorIndexType::Tensor, TensorIndexType::List, TensorIndexType::Tuple})) {
    MS_EXCEPTION(TypeError) << "Only support int, float, bool, Tensor, list, tuple as value, but got "
                            << TensorIndex::py_value_handle_.get_type();
  }
  Slice slice_info = Slice(tensor_index.slice(), data_shape[0]);
  std::tuple<int64_t, py::object, ShapeVector> value_transfer =
    GetValueTransferType(py_value_type, set_item_by_non_tensor, data_type, slice_info.step() >= 0);
  std::vector<int64_t> value_transfer_types = {std::get<0>(value_transfer)};
  std::vector<py::object> value_transfer_args = {std::get<1>(value_transfer)};
  return SetitemBySliceWithTensor(data_shape, tensor_index, &value_transfer_types, &value_transfer_args, data_value,
                                  data_type);
}

py::object TensorIndex::SetItemIndexInfo(const py::object &py_data, const py::object &py_index,
                                         const py::object &py_value, const py::bool_ &is_ascend) {
  if (!py::isinstance<Tensor>(py_data) && !IsStubTensor(py_data)) {
    MS_EXCEPTION(TypeError) << "First input of Tensor index must be tensor but got " << py_data;
  }
  ShapeVector data_shape;
  TypePtr data_type;
  bool is_parameter = false;
  ValuePtr data_value;
  if (IsStubTensor(py_data)) {  // PackTensor have not real Tensor.
    auto value = GetStubTensorValue(py_data);
    MS_EXCEPTION_IF_NULL(value);
    auto abs = value->ToAbstract();
    MS_EXCEPTION_IF_NULL(abs);
    data_shape = dyn_cast<abstract::Shape>(abs->BuildShape())->shape();
    data_type = abs->BuildType();
    MS_EXCEPTION_IF_NULL(data_type);
    if (EnableView()) {
      data_value = value;
    }
  } else {
    TensorPtr data = py_data.cast<TensorPtr>();
    MS_EXCEPTION_IF_NULL(data);
    if (EnableView(true)) {
      data_value = data;
    }
    data_shape = data->shape();
    data_type = data->Dtype();
    is_parameter = data->is_parameter();
  }
  TensorIndex::py_value_handle_ = py_value;
  TensorIndex::np_module_ = py::module::import("numpy");
  TensorIndex::py_index_handle_ = py_index;
  TensorIndex::is_ascend_ = is_ascend;
  TensorIndex::index_op_type_ = IndexOpType::SetItem;
  const TensorIndexType value_type = IsStubTensor(py_value) ? TensorIndexType::Tensor : TensorIndex(py_value).type();
  bool valid = CheckTypeIsInstance<TensorIndexType>(
    value_type, {TensorIndexType::Integer, TensorIndexType::Float, TensorIndexType::Boolean, TensorIndexType::Tensor,
                 TensorIndexType::List, TensorIndexType::Tuple});
  if (!valid) {
    MS_EXCEPTION(TypeError) << "only support numbers, Tensor, tuple, list as value, but got "
                            << TensorIndex::py_value_handle_ << " with type "
                            << TensorIndex::py_value_handle_.get_type();
  }
  if (py::isinstance<py::int_>(py_index) && !py::isinstance<py::bool_>(py_index) && data_value != nullptr) {
    return SetItemByNumberWithView(data_shape, data_type, is_parameter, TensorIndex(py_index), value_type, data_value);
  }
  if (py::isinstance<py::slice>(py_index)) {
    return TensorIndex::SetItemBySlice(data_shape, data_type, TensorIndex(py_index), value_type, data_value);
  }
  if (data_value != nullptr && (py::isinstance<py::none>(py_index) || py::isinstance<py::ellipsis>(py_index))) {
    auto output = py::make_tuple(
      py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kByPass)), py::make_tuple(py::none()),
      py::make_tuple(static_cast<int>(ValueTransferType::kSetItemByEllipsis)), py::make_tuple(py::none()));
    return output;
  }
  std::vector<int64_t> data_transfer_types;
  std::vector<py::object> data_transfer_args;
  if (py::isinstance<py::tuple>(py_index) &&
      GetItemByTupleWithView(data_value, data_shape, py_index, &data_transfer_types, &data_transfer_args, data_type)) {
    MS_LOG(INFO) << "(View) In branch set item by tuple with view, data_shape: " << data_shape
                 << " tensor_indexes: " << py_index;
    return py::make_tuple(py::str("view"), py::tuple(), py::tuple(), VectorToPyTuple(data_transfer_types),
                          VectorToPyTuple(data_transfer_args));
  }
  MS_LOG(INFO) << "(Tensor) Set item data shape is: " << data_shape << ", index is: " << py_index
               << ", value is: " << py_value;
  TensorIndex index = TensorIndex(py_index);

  CheckSetItemIndex(index.type(), value_type);
  if (index.IsList()) {
    if (data_shape.empty()) {
      MS_EXCEPTION(TypeError) << "Cannot iterate over a scalar tensor.";
    }
    index = TensorIndex::FormatList(index, data_shape[0]);
  }

  return SetItemIndexByIndexType(index, py_index, data_shape, data_type, value_type, is_parameter);
}

py::object TensorIndex::SetItemIndexByIndexType(const TensorIndex &index, const py::object &py_index,
                                                const ShapeVector &data_shape, const TypePtr &data_type,
                                                const TensorIndexType &value_type, bool is_parameter) {
  py::object output =
    py::make_tuple(py::none(), py::none(), py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kUnknown)),
                   py::make_tuple(py::none()));
  switch (index.type()) {
    case TensorIndexType::Integer: {
      output = SetItemByNumber(data_shape, data_type, is_parameter, index, value_type);
      break;
    }
    case TensorIndexType::Tensor: {
      output = SetItemByTensor(data_shape, is_parameter, index, value_type);
      break;
    }
    case TensorIndexType::Tuple: {
      output = SetItemByTuple(data_shape, data_type, index, value_type);
      break;
    }
    case TensorIndexType::Ellipsis:
    case TensorIndexType::None: {
      output = py::make_tuple(
        py::none(), py::make_tuple(static_cast<int>(ValueTransferType::kByPass)), py::make_tuple(py::none()),
        py::make_tuple(static_cast<int>(ValueTransferType::kSetItemByEllipsis)), py::make_tuple(py::none()));
      break;
    }
    case TensorIndexType::Boolean: {
      output = py::make_tuple(
        py_index, py::make_tuple(static_cast<int>(ValueTransferType::kByPass)), py::make_tuple(py::none()),
        py::make_tuple(static_cast<int>(ValueTransferType::kSetItemByBool)), py::make_tuple(py::none()));
      break;
    }
    default: {
      MS_EXCEPTION(TypeError)
        << "Only support integers, slices(`:`), ellipsis(`...`), None, bool, tensor, int, list and "
           "tuple as index, but got "
        << TensorIndex::py_index_handle_ << "with type " << TensorIndex::py_index_handle_.get_type();
    }
  }

  return output;
}

}  // namespace mindspore::tensor
