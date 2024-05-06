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

#include "pipeline/pynative/grad/ir/bprop_tensor_replace.h"
#include <memory>
#include "pipeline/pynative/pynative_utils.h"
#include "include/backend/device_address.h"

namespace mindspore {
namespace pynative {
namespace {
void SaveForwardTensorForReplace(const ValuePtr &value, const TensorIdWithOpInfo &id_with_op_info,
                                 bool need_save_tensor_info, OpInfoWithTensorObject *op_info_with_tensor_object) {
  MS_EXCEPTION_IF_NULL(value);
  if (value->isa<tensor::Tensor>()) {
    auto tensor = value->cast<tensor::TensorPtr>();
    const auto it = id_with_op_info.find(tensor->id());
    if (it != id_with_op_info.end() && tensor->device_address() != nullptr) {
      // For release memory
      tensor->set_is_forward_output(true);
      if (!need_save_tensor_info) {
        return;
      }
      MS_EXCEPTION_IF_NULL(op_info_with_tensor_object);
      (void)(*op_info_with_tensor_object)[it->second.first].emplace_back(std::make_pair(it->second.second, tensor));
      MS_LOG(DEBUG) << "Save forward tensor " << tensor.get() << " id " << tensor->id()
                    << " device address: " << tensor->device_address() << " shape and dtype "
                    << tensor->GetShapeAndDataTypeInfo();
    }
  } else if (value->isa<ValueSequence>()) {
    const auto &value_seq = value->cast<ValueSequencePtr>();
    for (const auto &v : value_seq->value()) {
      SaveForwardTensorForReplace(v, id_with_op_info, need_save_tensor_info, op_info_with_tensor_object);
    }
  }
}

tensor::TensorPtr GetTensorFromOutValue(size_t index, const ValuePtr &v) {
  MS_EXCEPTION_IF_NULL(v);
  // Only one outpout
  if (index == kIndex0) {
    return v->cast<tensor::TensorPtr>();
  }
  // Multi output
  const auto &v_seq = v->cast<ValueSequencePtr>();
  MS_EXCEPTION_IF_NULL(v_seq);
  if (v_seq->size() < index) {
    MS_LOG(EXCEPTION) << "Get wrong index " << index << " with multi output size " << v_seq->size();
  }
  return v_seq->value()[index - kIndex1]->cast<tensor::TensorPtr>();
}

void UpdatePreTensorInfo(const tensor::TensorPtr &new_tensor, const tensor::TensorPtr &old_tensor) {
  MS_EXCEPTION_IF_NULL(new_tensor);
  MS_EXCEPTION_IF_NULL(old_tensor);
  MS_LOG(DEBUG) << "Replace old tensor id " << old_tensor->id() << " device_address: " << old_tensor->device_address()
                << " shape and type " << old_tensor->GetShapeAndDataTypeInfo() << " with new tensor id "
                << new_tensor->id() << " device_address " << new_tensor->device_address() << " shape and dtype "
                << new_tensor->GetShapeAndDataTypeInfo();
  (void)old_tensor->set_shape(new_tensor->shape());
  (void)old_tensor->set_data_type(new_tensor->data_type());
  auto device_address = std::dynamic_pointer_cast<device::DeviceAddress>(new_tensor->device_address());
  // Like cell CellBackwardHook is first op, its input is input param have but no device address
  if (device_address == nullptr) {
    return;
  }

  auto kernel_tensor = device_address->kernel_tensor();
  MS_EXCEPTION_IF_NULL(kernel_tensor);
  if (!kernel_tensor->host_info_exist()) {
    // The tensor from PyBoost output.
    kernel_tensor->SetHostInfo(std::make_shared<abstract::TensorShape>(new_tensor->shape()),
                               std::make_shared<TensorType>(new_tensor->Dtype()), nullptr);
  }

  auto forward = PyNativeAlgo::Common::GetPyNativeExecutor()->forward_executor();
  if (forward->device_target() != kCPUDevice && device_address->GetDeviceType() != device::DeviceType::kCPU) {
    old_tensor->set_device_address(device_address);
    return;
  }
  for (const auto &item : forward->mindrt_backend()) {
    MS_EXCEPTION_IF_NULL(item.second);
    item.second->WaitTaskFinish();
  }
  // Replace data in device address when run in CPU device.
  if (old_tensor->device_address() != nullptr) {
    // If tensor is dynamic shape, Just replace device address.
    if (PyNativeAlgo::Common::ValueHasDynamicShape(old_tensor)) {
      old_tensor->set_device_address(device_address);
      return;
    }
    auto old_device_address = std::dynamic_pointer_cast<device::DeviceAddress>(old_tensor->device_address());
    MS_EXCEPTION_IF_NULL(old_device_address);

    // CPU host tensor data_c is different from device address if the address is from mem_pool.
    if (device_address->from_mem_pool()) {
      old_tensor->set_device_address(device_address);
      return;
    }

    auto old_ptr = old_device_address->GetMutablePtr();
    MS_EXCEPTION_IF_NULL(old_ptr);
    auto new_ptr = device_address->GetPtr();
    MS_EXCEPTION_IF_NULL(new_ptr);
    MS_EXCEPTION_IF_CHECK_FAIL(old_device_address->GetSize() == device_address->GetSize(), "Size not equal");
    if (old_device_address->GetSize() < SECUREC_MEM_MAX_LEN) {
      auto ret_code = memcpy_s(old_ptr, old_device_address->GetSize(), new_ptr, device_address->GetSize());
      MS_EXCEPTION_IF_CHECK_FAIL(ret_code == EOK, "Memory copy failed, ret code: " + std::to_string(ret_code));
    } else {
      auto ret_code = std::memcpy(old_ptr, new_ptr, old_device_address->GetSize());
      MS_EXCEPTION_IF_CHECK_FAIL(ret_code == old_ptr, "Memory copy failed");
    }
  } else {
    old_tensor->set_device_address(device_address);
    old_tensor->data_sync();
    old_tensor->set_device_address(nullptr);
    old_tensor->set_sync_status(kNeedSyncHostToDevice);
  }
}
}  // namespace

void SetIdWithOpInfo(const ValuePtr &v, const std::string &op_info, size_t out_index,
                     TensorIdWithOpInfo *id_with_op_info) {
  MS_EXCEPTION_IF_NULL(v);
  MS_EXCEPTION_IF_NULL(id_with_op_info);
  if (v->isa<tensor::Tensor>()) {
    // Only one output, index will be 0
    const auto t = v->cast<tensor::TensorPtr>();
    (*id_with_op_info)[t->id()] = std::make_pair(op_info, out_index);
  } else if (v->isa<ValueSequence>()) {
    const auto &v_seq = v->cast<ValueSequencePtr>();
    // Multi output, index will increase from 1
    for (const auto &item : v_seq->value()) {
      SetIdWithOpInfo(item, op_info, ++out_index, id_with_op_info);
    }
  }
}

void UpdateForwardOutputTensorInfo(const std::string &op_info, const ValuePtr &v, const TensorReplaceInfo &replace_info,
                                   const size_t &stream_id) {
  const auto it = replace_info.op_info_with_tensor_object.find(op_info);
  if (it == replace_info.op_info_with_tensor_object.end()) {
    return;
  }
  for (const auto &elem : it->second) {
    const auto &new_tensor = GetTensorFromOutValue(elem.first, v);
    UpdatePreTensorInfo(new_tensor, elem.second);
  }
}

void SaveForwardOutputTensorInfo(const FuncGraphPtr &func_graph, bool need_save_tensor_info,
                                 TensorReplaceInfo *replace_info) {
  // Get all tensors obj in value node of bprop graph
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(replace_info);
  const auto &value_node_list = func_graph->value_nodes();
  for (const auto &elem : value_node_list) {
    auto value_node = elem.first->cast<ValueNodePtr>();
    MS_EXCEPTION_IF_NULL(value_node);
    SaveForwardTensorForReplace(value_node->value(), replace_info->id_with_op_info, need_save_tensor_info,
                                &(replace_info->op_info_with_tensor_object));
  }
}
}  // namespace pynative
}  // namespace mindspore
