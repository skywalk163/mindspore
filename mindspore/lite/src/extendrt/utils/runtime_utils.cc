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

#include <functional>
#include <vector>
#include <string>
#include <memory>

#include "extendrt/utils/runtime_utils.h"

#include "src/extendrt/infer_device_address.h"
#include "include/common/utils/anfalgo.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "mindspore/lite/src/common/common.h"

namespace mindspore {
namespace {
const size_t tensor_max_size_utils = 0x1000000;
}  // namespace

void *RuntimeUtils::GetAddressPtr(device::DeviceAddressPtr address_ptr) {
  MS_EXCEPTION_IF_NULL(address_ptr);
  return address_ptr->GetDevicePtr();
}

void RuntimeUtils::SetAddressPtr(device::DeviceAddressPtr address_ptr, void *ptr) {
  MS_EXCEPTION_IF_NULL(address_ptr);
  address_ptr->SetDevicePtr(ptr);
}

void RuntimeUtils::AllocAddressPtr(device::DeviceAddressPtr address_ptr) {
  MS_EXCEPTION_IF_NULL(address_ptr);
  if (address_ptr->GetDevicePtr() == nullptr) {
    address_ptr->SetDevicePtr(malloc(address_ptr->GetSize()));
  }
}

kernel::AddressPtr RuntimeUtils::GetAddressFromDevice(device::DeviceAddressPtr device_address) {
  MS_EXCEPTION_IF_NULL(device_address);
  kernel::AddressPtr kernel_address = std::make_shared<kernel::Address>();
  MS_EXCEPTION_IF_NULL(kernel_address);
  if (device_address->GetDevicePtr() == nullptr) {
    device_address->SetDevicePtr(malloc(device_address->GetSize()));
  }
  MS_EXCEPTION_IF_NULL(device_address->GetDevicePtr());
  kernel_address->addr = device_address->GetDevicePtr();
  kernel_address->size = device_address->GetSize();
  return kernel_address;
}

device::DeviceAddressPtr RuntimeUtils::CreateDeviceAddress(void *device_ptr, size_t device_size, const string &format,
                                                           TypeId type_id) {
  return std::make_shared<InferDeviceAddress>(device_ptr, device_size, format, type_id);
}

void RuntimeUtils::UpdateKernelNodeOutputInfo(const AnfNodePtr &kernel_node,
                                              const std::vector<kernel::AddressPtr> &output_addrs) {
  std::string kernel_name = common::AnfAlgo::GetCNodeName(kernel_node);
  if (kernel_name == lite::kNameCustomAscend) {
    size_t output_num = AnfUtils::GetOutputTensorNum(kernel_node);
    if (output_addrs.size() != output_num) {
      MS_LOG(ERROR) << "Output addr size[" << output_addrs.size() << "] is not equal to node outputs size["
                    << output_num << "]";
      return;
    }
    // update output addr
    bool is_update_shape = false;
    for (size_t i = 0; i < output_num; ++i) {
      auto device_address = AnfAlgo::GetMutableOutputAddr(kernel_node, i);
      MS_EXCEPTION_IF_NULL(device_address);
      MS_EXCEPTION_IF_NULL(output_addrs[i]);
      auto addr_ptr = device_address->GetMutablePtr();
      if (addr_ptr != nullptr && output_addrs[i]->addr != addr_ptr) {
        free(addr_ptr);
        device_address->set_ptr(output_addrs[i]->addr);
        device_address->SetSize(output_addrs[i]->size);
        is_update_shape = true;
      }
    }
    if (!is_update_shape) {
      MS_LOG(DEBUG) << "There is no need to update output shape.";
      return;
    }

    auto output_tensors = AnfAlgo::GetOrCreateAllOutputKernelTensors(kernel_node);
    auto input_tensors = AnfAlgo::GetOrCreateAllInputKernelTensors(kernel_node);
    auto kernel_mod = AnfAlgo::GetKernelMod(kernel_node);
    kernel_mod->UpdateOutputShapeAndSize(input_tensors, output_tensors);
    if (output_tensors.empty()) {
      MS_LOG(ERROR) << "The output shape size of custom ascend is empty.";
      return;
    }
    auto abstract = kernel_node->abstract();
    MS_EXCEPTION_IF_NULL(abstract);
    if (utils::isa<abstract::AbstractTuplePtr>(abstract)) {
      auto abstract_tuple = abstract->cast<abstract::AbstractTuplePtr>();
      MS_EXCEPTION_IF_NULL(abstract_tuple);
      if (abstract_tuple->elements().size() != output_tensors.size()) {
        MS_LOG(ERROR) << "Abstract size[" << abstract_tuple->elements().size() << "] is not equal to output shape size["
                      << output_tensors.size() << "]";
        return;
      }
      for (size_t i = 0; i < abstract_tuple->elements().size(); ++i) {
        auto tmp_abstract = abstract_tuple->elements()[i];
        MS_EXCEPTION_IF_NULL(tmp_abstract);
        MS_EXCEPTION_IF_NULL(output_tensors[i]);
        tmp_abstract->set_shape(std::make_shared<abstract::Shape>(output_tensors[i]->GetShapeVector()));
      }
    } else {
      MS_EXCEPTION_IF_NULL(output_tensors[0]);
      abstract->set_shape(std::make_shared<abstract::Shape>(output_tensors[0]->GetShapeVector()));
    }
  }
}
}  // namespace mindspore
