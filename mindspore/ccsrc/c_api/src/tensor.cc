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

#include "include/c_api/ms/tensor.h"
#include <memory>
#include "c_api/src/helper.h"
#include "c_api/src/common.h"
#include "ir/tensor.h"
#include "ir/dtype.h"

template <typename T>
std::vector<T> GetDataByFile(const char *path, size_t *elem_size) {
  std::string fileName(path);
  MS_LOG(INFO) << "Reading File: " << fileName << std::endl;
  std::ifstream fin(fileName, std::ios::in);
  std::vector<T> data;
  if (!fin.is_open()) {
    MS_LOG(ERROR) << "Open file failed, File path: %s  " << fileName << std::endl;
    return data;
  }
  T t;
  while (fin >> t) {
    data.push_back(t);
  }
  fin.close();
  *elem_size = data.size() * sizeof(T);
  return data;
}

TensorHandle MSNewTensor(ResMgrHandle res_mgr, void *data, DataTypeC type, const int64_t shape[], size_t shape_size,
                         size_t data_len) {
  if (res_mgr == nullptr || data == nullptr || shape == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] or [data] or [shape] is nullptr.";
    return nullptr;
  }
  TensorPtr tensor = nullptr;
  ShapeVector shape_vec(shape, shape + shape_size);
  try {
    tensor = std::make_shared<TensorImpl>(mindspore::TypeId(type), shape_vec, data, data_len);
  } catch (const std::exception &e) {
    MS_LOG(ERROR) << "New Tensor failed. Error info: " << e.what();
    return nullptr;
  }
  return GetRawPtr(res_mgr, tensor);
}

TensorHandle MSNewTensorFromFile(ResMgrHandle res_mgr, DataTypeC type, const int64_t shape[], size_t shape_size,
                                 const char *path) {
  if (res_mgr == nullptr || shape == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] or [shape] is nullptr.";
    return nullptr;
  }
  TensorPtr tensor = nullptr;
  ShapeVector shape_vec(shape, shape + shape_size);
  try {
    size_t data_len;
    switch (type) {
      case MS_INT32: {
        std::vector<int32_t> data = GetDataByFile<int32_t>(path, &data_len);
        tensor = std::make_shared<TensorImpl>(mindspore::TypeId(type), shape_vec, data.data(), data_len);
        break;
      }
      case MS_INT64: {
        std::vector<int64_t> data = GetDataByFile<int64_t>(path, &data_len);
        tensor = std::make_shared<TensorImpl>(mindspore::TypeId(type), shape_vec, data.data(), data_len);
        break;
      }
      case MS_FLOAT32: {
        std::vector<float> data = GetDataByFile<float>(path, &data_len);
        tensor = std::make_shared<TensorImpl>(mindspore::TypeId(type), shape_vec, data.data(), data_len);
        break;
      }
      default:
        MS_LOG(ERROR) << "Unrecognized datatype w/ DataTypeC ID: " << type << std::endl;
        return nullptr;
    }
  } catch (const std::exception &e) {
    MS_LOG(ERROR) << "New Tensor failed. Error info: " << e.what();
    return nullptr;
  }
  return GetRawPtr(res_mgr, tensor);
}

TensorHandle MSNewTensorWithSrcType(ResMgrHandle res_mgr, void *data, const int64_t shape[], size_t shape_size,
                                    DataTypeC tensor_type, DataTypeC src_type) {
  if (res_mgr == nullptr || data == nullptr || shape == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] or [data] or [shape] is nullptr.";
    return nullptr;
  }
  TensorPtr tensor = nullptr;
  ShapeVector shape_vec(shape, shape + shape_size);
  try {
    tensor = std::make_shared<TensorImpl>(mindspore::TypeId(tensor_type), shape_vec, data, mindspore::TypeId(src_type));
  } catch (const std::exception &e) {
    MS_LOG(ERROR) << "New Tensor failed. Error info: " << e.what();
    return nullptr;
  }
  return GetRawPtr(res_mgr, tensor);
}

TensorHandle MSNewTensorScalarFloat32(ResMgrHandle res_mgr, float value) {
  if (res_mgr == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] is nullptr.";
    return nullptr;
  }
  TensorPtr tensor = nullptr;
  try {
    auto type_ptr = mindspore::TypeIdToType(mindspore::kNumberTypeFloat32);
    MS_EXCEPTION_IF_NULL(type_ptr);
    tensor = std::make_shared<TensorImpl>(value, type_ptr);
  } catch (const std::exception &e) {
    MS_LOG(ERROR) << "New Float32 Scalar Tensor failed. Error info: " << e.what();
    return nullptr;
  }
  return GetRawPtr(res_mgr, tensor);
}

TensorHandle MSNewTensorScalarInt32(ResMgrHandle res_mgr, int value) {
  if (res_mgr == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] is nullptr.";
    return nullptr;
  }
  TensorPtr tensor = nullptr;
  try {
    auto type_ptr = mindspore::TypeIdToType(mindspore::kNumberTypeInt32);
    MS_EXCEPTION_IF_NULL(type_ptr);
    tensor = std::make_shared<TensorImpl>(value, type_ptr);
  } catch (const std::exception &e) {
    MS_LOG(ERROR) << "New Int32 Scalar Tensor failed. Error info: " << e.what();
    return nullptr;
  }
  return GetRawPtr(res_mgr, tensor);
}

void *MSTensorGetData(ResMgrHandle res_mgr, ConstTensorHandle tensor) {
  if (res_mgr == nullptr || tensor == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] or [tensor] is nullptr.";
    return nullptr;
  }
  auto src_tensor = GetSrcPtr<TensorPtr>(res_mgr, tensor);
  if (src_tensor == nullptr) {
    MS_LOG(ERROR) << "Get source pointer failed.";
    return nullptr;
  }
  return src_tensor->data_c();
}

STATUS MSTensorSetDataType(ResMgrHandle res_mgr, TensorHandle tensor, DataTypeC type) {
  if (res_mgr == nullptr || tensor == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] or [tensor] is nullptr.";
    return RET_ERROR;
  }
  auto src_tensor = GetSrcPtr<TensorPtr>(res_mgr, tensor);
  if (src_tensor == nullptr) {
    MS_LOG(ERROR) << "Get source pointer failed.";
    return RET_ERROR;
  }
  (void)src_tensor->set_data_type(mindspore::TypeId(type));
  return RET_OK;
}

DataTypeC MSTensorGetDataType(ResMgrHandle res_mgr, ConstTensorHandle tensor, STATUS *error) {
  if (error == nullptr) {
    MS_LOG(ERROR) << "Input status flag [error] is nullptr.";
    return MS_INVALID_TYPE;
  }
  if (res_mgr == nullptr || tensor == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] or [tensor] is nullptr.";
    *error = RET_NULL_PTR;
    return MS_INVALID_TYPE;
  }
  auto src_tensor = GetSrcPtr<TensorPtr>(res_mgr, tensor);
  if (src_tensor == nullptr) {
    MS_LOG(ERROR) << "Get source pointer failed.";
    *error = RET_NULL_PTR;
    return MS_INVALID_TYPE;
  }
  *error = RET_OK;
  return (enum DataTypeC)(src_tensor->data_type_c());
}

size_t MSTensorGetDataSize(ResMgrHandle res_mgr, ConstTensorHandle tensor, STATUS *error) {
  if (error == nullptr) {
    MS_LOG(ERROR) << "Input status flag [error] is nullptr.";
    return 0;
  }
  if (res_mgr == nullptr || tensor == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] or [tensor] is nullptr.";
    *error = RET_NULL_PTR;
    return 0;
  }
  auto src_tensor = GetSrcPtr<TensorPtr>(res_mgr, tensor);
  if (src_tensor == nullptr) {
    MS_LOG(ERROR) << "Get source pointer failed.";
    *error = RET_NULL_PTR;
    return 0;
  }
  auto size = src_tensor->Size();
  *error = RET_OK;
  return size;
}

size_t MSTensorGetElementNum(ResMgrHandle res_mgr, ConstTensorHandle tensor, STATUS *error) {
  if (error == nullptr) {
    MS_LOG(ERROR) << "Input status flag [error] is nullptr.";
    return 0;
  }
  if (res_mgr == nullptr || tensor == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] or [tensor] is nullptr.";
    *error = RET_NULL_PTR;
    return 0;
  }
  auto src_tensor = GetSrcPtr<TensorPtr>(res_mgr, tensor);
  if (src_tensor == nullptr) {
    MS_LOG(ERROR) << "Get source pointer failed.";
    *error = RET_NULL_PTR;
    return 0;
  }
  auto ele_num = src_tensor->DataSize();
  *error = RET_OK;
  return ele_num;
}

size_t MSTensorGetDimension(ResMgrHandle res_mgr, ConstTensorHandle tensor, STATUS *error) {
  if (error == nullptr) {
    MS_LOG(ERROR) << "Input status flag [error] is nullptr.";
    return 0;
  }
  if (res_mgr == nullptr || tensor == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] or [tensor] is nullptr.";
    *error = RET_NULL_PTR;
    return 0;
  }
  auto src_tensor = GetSrcPtr<TensorPtr>(res_mgr, tensor);
  if (src_tensor == nullptr) {
    MS_LOG(ERROR) << "Get source pointer failed.";
    *error = RET_NULL_PTR;
    return 0;
  }
  auto dim = src_tensor->shape().size();
  *error = RET_OK;
  return dim;
}

STATUS MSTensorSetShape(ResMgrHandle res_mgr, TensorHandle tensor, const int64_t shape[], size_t dim) {
  if (res_mgr == nullptr || tensor == nullptr || shape == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] or [tensor] or [shape] is nullptr.";
    return RET_NULL_PTR;
  }
  auto src_tensor = GetSrcPtr<TensorPtr>(res_mgr, tensor);
  if (src_tensor == nullptr) {
    MS_LOG(ERROR) << "Get source pointer failed.";
    return RET_NULL_PTR;
  }
  auto dimension = src_tensor->shape().size();
  if (dimension != dim) {
    MS_LOG(ERROR) << "Invalid input shape array length, it should be: " << dimension << ", but got: " << dim;
    return RET_ERROR;
  }
  ShapeVector shape_vec(shape, shape + dim);
  (void)src_tensor->set_shape(shape_vec);
  return RET_OK;
}

STATUS MSTensorGetShape(ResMgrHandle res_mgr, ConstTensorHandle tensor, int64_t shape[], size_t dim) {
  if (res_mgr == nullptr || tensor == nullptr || shape == nullptr) {
    MS_LOG(ERROR) << "Input Handle [res_mgr] or [tensor] or [shape] is nullptr.";
    return RET_NULL_PTR;
  }
  auto src_tensor = GetSrcPtr<TensorPtr>(res_mgr, tensor);
  if (src_tensor == nullptr) {
    MS_LOG(ERROR) << "Get source pointer failed.";
    return RET_NULL_PTR;
  }
  auto dimension = src_tensor->shape().size();
  if (dimension != dim) {
    MS_LOG(ERROR) << "Invalid input shape array length, it should be: " << dimension << ", but got: " << dim;
    return RET_ERROR;
  }
  for (size_t i = 0; i < dim; i++) {
    shape[i] = src_tensor->shape()[i];
  }
  return RET_OK;
}
