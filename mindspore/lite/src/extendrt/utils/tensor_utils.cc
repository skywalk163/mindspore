/**
 * This is the C++ adaptation and derivative work of Myia (https://github.com/mila-iqia/myia/).
 *
 * Copyright 2019-2021 Huawei Technologies Co., Ltd
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

#include <memory>
#include <algorithm>
#include <utility>

#include "extendrt/utils/tensor_utils.h"
#include "mindspore/ccsrc/kernel/common_utils.h"
#include "mindspore/ccsrc/kernel/framework_utils.h"
#include "mindspore/ccsrc/kernel/format_utils.h"

namespace mindspore {
TensorRefData::TensorRefData(void *data, size_t bytes_size, size_t data_size, size_t ndim,
                             const std::function<void(uint8_t *)> &deleter)
    : data_(data), elem_count_(bytes_size), data_size_(data_size), ndim_(ndim), deleter_(deleter) {}

TensorRefData::~TensorRefData() {
  if (deleter_ && data_) {
    deleter_(reinterpret_cast<uint8_t *>(data_));
  }
}

ssize_t TensorRefData::size() const { return static_cast<ssize_t>(elem_count_); }

ssize_t TensorRefData::itemsize() const {
  if (elem_count_ == 0) {
    return 0;
  }
  return static_cast<ssize_t>(data_size_ / elem_count_);
}

ssize_t TensorRefData::nbytes() const { return static_cast<ssize_t>(data_size_); }

ssize_t TensorRefData::ndim() const { return static_cast<ssize_t>(ndim_); }

void *TensorRefData::data() { return data_; }

const void *TensorRefData::const_data() const { return data_; }

std::string TensorRefData::ToString(TypeId type, const ShapeVector &shape, bool use_comma) const {
  std::stringstream stream;
  stream << "RefTensor:[";
  for (size_t i = 0; i < shape.size(); i++) {
    stream << shape[i];
    if (i + 1 < shape.size()) {
      stream << ",";
    }
  }
  stream << "]" << type;
  return stream.str();
}

mindspore::Format TensorTensorImpl::Format() const {
  MS_EXCEPTION_IF_NULL(tensor_);
  return kernel::GetFormatFromStrToEnum(tensor_->device_info().format_);
}

void TensorTensorImpl::SetFormat(mindspore::Format format) {
  MS_EXCEPTION_IF_NULL(tensor_);
  auto device_info = tensor_->device_info();
  device_info.format_ = kernel::GetFormatFromEnumToStr(format);
  tensor_->set_device_info(device_info);
}

std::vector<mindspore::tensor::TensorPtr> TensorUtils::MSTensorToTensorPtr(const std::vector<MSTensor> &ms_tensors) {
  std::vector<mindspore::tensor::TensorPtr> tensor_ptrs;

  for (auto ms_tensor : ms_tensors) {
    auto data_type = ms_tensor.DataType();
    auto type_id = static_cast<mindspore::TypeId>(data_type);
    auto shape = ms_tensor.Shape();
    auto data = ms_tensor.MutableData();
    auto data_size = ms_tensor.DataSize();
    auto ref_tensor_data = std::make_shared<TensorRefData>(data, ms_tensor.ElementNum(), data_size, shape.size());
    auto tensor_ptr = std::make_shared<mindspore::tensor::Tensor>(type_id, shape, ref_tensor_data);
    tensor_ptr->set_name(ms_tensor.Name());
    tensor_ptr->set_data_type(type_id);
    tensor_ptrs.push_back(tensor_ptr);
  }
  return tensor_ptrs;
}

std::vector<MSTensor> TensorUtils::TensorPtrToMSTensor(std::vector<mindspore::tensor::TensorPtr> tensor_ptrs,
                                                       const std::vector<std::string> &tensor_names) {
  std::vector<MSTensor> ms_tensors;
  for (size_t i = 0; i < tensor_ptrs.size(); i++) {
    auto graph_tensor = tensor_ptrs[i];
    std::string graph_tensor_name = tensor_names[i];
    graph_tensor->set_name(graph_tensor_name);
    auto tensor_impl = std::make_shared<TensorTensorImpl>(graph_tensor);
    ms_tensors.push_back(MSTensor(tensor_impl));
  }
  return ms_tensors;
}

std::vector<mindspore::tensor::Tensor> TensorUtils::MSTensorToTensor(const std::vector<MSTensor> &ms_tensors) {
  std::vector<mindspore::tensor::Tensor> tensors;
  for (auto ms_tensor : ms_tensors) {
    auto data_type = ms_tensor.DataType();
    auto type_id = static_cast<mindspore::TypeId>(data_type);
    auto shape = ms_tensor.Shape();
    auto data = const_cast<void *>(ms_tensor.Data().get());
    auto data_size = ms_tensor.DataSize();
    auto ref_tensor_data = std::make_shared<TensorRefData>(data, ms_tensor.ElementNum(), data_size, shape.size());
    mindspore::tensor::Tensor tensor(type_id, shape, ref_tensor_data);
    auto device_address = ms_tensor.GetDeviceData();
    if (device_address != nullptr) {
      auto lite_device_address = std::make_shared<LiteDeviceAddress>(device_address, ms_tensor.DataSize());
      tensor.set_device_address(lite_device_address);
      // only use device_id now.
      auto device_info = tensor::DeviceInfo("DefaultFormat", nullptr, "DefaultFormat", ms_tensor.GetDeviceId());
      tensor.set_device_info(device_info);
    }
    tensors.emplace_back(std::move(tensor));
  }
  return tensors;
}

std::vector<MSTensor> TensorUtils::TensorToMSTensor(std::vector<mindspore::tensor::Tensor> tensors,
                                                    const std::vector<std::string> &tensor_names) {
  std::vector<MSTensor> ms_tensors;
  for (size_t i = 0; i < tensors.size(); i++) {
    auto &graph_tensor = tensors[i];
    std::string graph_tensor_name = tensor_names[i];
    graph_tensor.set_name(graph_tensor_name);
    auto tensor_impl = std::make_shared<TensorTensorImpl>(graph_tensor);
    ms_tensors.emplace_back(MSTensor(tensor_impl));
  }
  return ms_tensors;
}

std::vector<mindspore::tensor::TensorPtr> TensorUtils::TensorToTensorPtr(
  const std::vector<mindspore::tensor::Tensor> &tensors) {
  std::vector<mindspore::tensor::TensorPtr> tensor_ptrs;
  for (auto &tensor : tensors) {
    auto type_id = static_cast<TypeId>(tensor.data_type_c());
    auto shape = tensor.shape_c();
    auto data = tensor.data_c();
    auto data_size = tensor.Size();
    auto tensor_ptr = std::make_shared<mindspore::tensor::Tensor>(type_id, shape, data, data_size);
    tensor_ptrs.push_back(tensor_ptr);
  }
  return tensor_ptrs;
}

std::vector<mindspore::tensor::Tensor> TensorUtils::TensorPtrToTensor(
  const std::vector<mindspore::tensor::TensorPtr> &tensor_ptrs) {
  std::vector<mindspore::tensor::Tensor> tensors;
  std::transform(tensor_ptrs.begin(), tensor_ptrs.end(), std::back_inserter(tensors),
                 [](mindspore::tensor::TensorPtr tensor_ptr) { return mindspore::tensor::Tensor(*tensor_ptr); });
  return tensors;
}

kernel::AddressPtr CloudTensorUtils::LiteTensorToAddressPtr(const lite::Tensor *lite_tensor) {
  kernel::AddressPtr address_ptr = std::make_shared<kernel::Address>(lite_tensor->data(), lite_tensor->Size());
  return address_ptr;
}

std::vector<mindspore::kernel::AddressPtr> CloudTensorUtils::LiteTensorToAddressPtrVec(
  const std::vector<lite::Tensor *> &lite_tensors) {
  kernel::AddressPtrList address_list;

  for (auto lite_tensor : lite_tensors) {
    kernel::AddressPtr address = LiteTensorToAddressPtr(lite_tensor);
    address_list.push_back(address);
  }

  return address_list;
}

kernel::KernelTensor *CloudTensorUtils::LiteTensorToKernelTensorPtr(const lite::Tensor *lite_tensor) {
  kernel::AddressPtr address = LiteTensorToAddressPtr(lite_tensor);
  kernel::KernelTensor *kernel_tensor_ptr = new (std::nothrow) kernel::KernelTensor();
  if (kernel_tensor_ptr == nullptr) {
    return kernel_tensor_ptr;
  }
  kernel_tensor_ptr->SetData(address);
  kernel_tensor_ptr->set_format(lite_tensor->format());
  kernel_tensor_ptr->SetType(std::make_shared<TensorType>(TypeIdToType(lite_tensor->data_type())));

  auto lite_shape = lite_tensor->shape();
  std::vector<int64_t> shape;
  for (size_t i = 0; i < lite_shape.size(); i++) {
    shape.push_back(lite_shape[i]);
  }
  kernel_tensor_ptr->SetShape(std::make_shared<abstract::TensorShape>(std::move(shape)));
  return kernel_tensor_ptr;
}

std::vector<kernel::KernelTensor *> CloudTensorUtils::LiteTensorToKernelTensorPtrVec(
  const std::vector<lite::Tensor *> &lite_tensors) {
  std::vector<kernel::KernelTensor *> kernel_tensor_list;

  for (auto lite_tensor : lite_tensors) {
    if (lite_tensor == nullptr) {
      continue;
    }
    auto kernel_tensor_ptr = LiteTensorToKernelTensorPtr(lite_tensor);
    kernel_tensor_list.push_back(kernel_tensor_ptr);
  }

  return kernel_tensor_list;
}

std::vector<std::vector<int64_t>> AbstractTensorUtils::GetTensorListShapes(
  const std::vector<infer::abstract::Tensor *> &tensors) {
  std::vector<std::vector<int64_t>> original_dims;
  std::transform(tensors.begin(), tensors.end(), std::back_inserter(original_dims),
                 [](infer::abstract::Tensor *tensor) {
                   std::vector<int64_t> shape64;
                   if (tensor != nullptr) {
                     auto shape32 = tensor->shape();
                     std::transform(shape32.begin(), shape32.end(), std::back_inserter(shape64),
                                    [](int dim) { return static_cast<int64_t>(dim); });
                   }
                   return shape64;
                 });
  return original_dims;
}

bool AbstractTensorUtils::SetTensorListShapse(const std::vector<infer::abstract::Tensor *> &tensors,
                                              const std::vector<std::vector<int64_t>> &shapes) {
  for (size_t i = 0; i < tensors.size(); i++) {
    auto tensor = tensors.at(i);
    if (tensor == nullptr) {
      continue;
    }
    auto shape64 = shapes.at(i);
    std::vector<int> shape32;
    std::transform(shape64.begin(), shape64.end(), std::back_inserter(shape32),
                   [](int64_t dim) { return static_cast<int>(dim); });
    tensor->set_shape(shape32);
  }
  return true;
}
}  // namespace mindspore
