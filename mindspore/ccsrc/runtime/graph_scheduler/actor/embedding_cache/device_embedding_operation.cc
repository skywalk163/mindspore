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

#include "runtime/graph_scheduler/actor/embedding_cache/device_embedding_operation.h"
#include <string>
#include "kernel/framework_utils.h"
#include "include/backend/optimizer/helper.h"
#include "backend/common/optimizer/dynamic_shape/dynamic_shape_helper.h"
#include "runtime/graph_scheduler/actor/embedding_cache/embedding_cache_prefetch_actor.h"

namespace mindspore {
namespace runtime {
bool DeviceEmbeddingOperation::Initialize() {
  BuildEmbeddingCacheLookupKernel();
  BuildEmbeddingCacheUpdateKernel();
  return true;
}

bool DeviceEmbeddingOperation::ParseHostDataHostToDevice(int id, size_t data_step, size_t *cur_graph_running_step,
                                                         const std::atomic_ulong *latest_graph_running_step,
                                                         bool *host_cache_need_wait_graph,
                                                         EmbeddingHostCache *embedding_host_cache,
                                                         EmbeddingCacheStatisticsInfo *statistics_info) {
  MS_ERROR_IF_NULL(cur_graph_running_step);
  MS_ERROR_IF_NULL(latest_graph_running_step);
  MS_ERROR_IF_NULL(embedding_host_cache);
  MS_ERROR_IF_NULL(statistics_info);
  int *host_to_device_index = embedding_host_cache->host_to_device_index.get();
  MS_ERROR_IF_NULL(host_to_device_index);
  auto &host_hash_map = embedding_cache_table_manager.host_hash_map_;
  MS_ERROR_IF_NULL(host_hash_map);

  int index;
  if (host_hash_map->GetIndex(id, &index)) {
    if (host_hash_map->hash_step(index) != data_step) {
      host_hash_map->set_hash_step(index, data_step);
    }
    host_to_device_index[statistics_info->host_to_device_size_ - 1] = index;
  } else {
    int *host_to_server_index = embedding_host_cache->host_to_server_index.get();
    int *host_to_server_ids = embedding_host_cache->host_to_server_ids.get();
    auto tmp_host_to_server_size = statistics_info_->host_to_server_size_;
    size_t retry_count = 0;
    while (true) {
      // Calculate the mapping of id to index.
      index = host_hash_map->ParseData(id, host_to_server_index, host_to_server_ids, data_step, *cur_graph_running_step,
                                       &(statistics_info->host_to_server_size_), host_cache_need_wait_graph);
      if (index == kInvalidIndexValue) {
        const int64_t wait_interval = 10000;
        *cur_graph_running_step = latest_graph_running_step->load();
        std::this_thread::sleep_for(std::chrono::microseconds(wait_interval));
        ++retry_count;
        if (retry_count > kMaxRetryNum) {
          MS_LOG(ERROR) << "Prefetch embedding cache timeout, please enlarge the vocab cache size.";
          return false;
        }
        MS_LOG(DEBUG) << "There is no space in local host cache, wait and retrying, current graph running step: "
                      << *cur_graph_running_step << ", data step: " << data_step;
        continue;
      }

      // The embedding vector of id which is never used before need not be evicted to remote.
      if (tmp_host_to_server_size < statistics_info_->host_to_server_size_) {
        if (modified_ids_.find(host_to_server_ids[tmp_host_to_server_size]) == modified_ids_.end()) {
          statistics_info_->host_to_server_size_ = tmp_host_to_server_size;
        }
      }

      host_to_device_index[statistics_info->host_to_device_size_ - 1] = index;

      // This feature id has never been seen before, so it's value is initialized using the local random generator.
      // Initialize with random value when checkpoint has not been loaded.
      if (!embedding_cache_table_manager.checkpoint_load_status() &&
          initialized_ids_.find(id) == initialized_ids_.end()) {
        int *new_id_index = embedding_host_cache->new_id_index.get();
        MS_ERROR_IF_NULL(new_id_index);
        new_id_index[statistics_info->new_id_size_++] = index;
        (void)initialized_ids_.insert(id);
        // This feature id has been initialized already, so it's latest value has been kept in the remote server.
      } else {
        int *server_to_host_index = embedding_host_cache->server_to_host_index.get();
        int *server_to_host_ids = embedding_host_cache->server_to_host_ids.get();
        MS_ERROR_IF_NULL(server_to_host_index);
        MS_ERROR_IF_NULL(server_to_host_ids);
        server_to_host_index[statistics_info->server_to_host_size_] = index;
        server_to_host_ids[statistics_info->server_to_host_size_++] = id;
      }
      break;
    }
  }

  return true;
}

bool DeviceEmbeddingOperation::ParseHostDataDeviceToHost(size_t data_step, size_t *cur_graph_running_step,
                                                         const std::atomic_ulong *latest_graph_running_step,
                                                         bool *host_cache_need_wait_graph,
                                                         EmbeddingDeviceCache *embedding_device_cache,
                                                         EmbeddingHostCache *embedding_host_cache,
                                                         EmbeddingCacheStatisticsInfo *statistics_info) {
  MS_ERROR_IF_NULL(cur_graph_running_step);
  MS_ERROR_IF_NULL(latest_graph_running_step);
  MS_ERROR_IF_NULL(embedding_device_cache);
  MS_ERROR_IF_NULL(embedding_host_cache);
  MS_ERROR_IF_NULL(statistics_info);

  int *device_to_host_ids = embedding_device_cache->device_to_host_ids.get();
  int *device_to_host_index = embedding_host_cache->device_to_host_index.get();
  MS_ERROR_IF_NULL(device_to_host_ids);
  MS_ERROR_IF_NULL(device_to_host_index);

  auto &host_hash_map = embedding_cache_table_manager.host_hash_map_;
  MS_ERROR_IF_NULL(host_hash_map);
  int swap_device_to_host_id = device_to_host_ids[statistics_info->device_to_host_size_ - 1];
  int index;
  if (host_hash_map->GetIndex(swap_device_to_host_id, &index)) {
    if (host_hash_map->hash_step(index) != data_step) {
      host_hash_map->set_hash_step(index, data_step);
    }
    device_to_host_index[statistics_info->device_to_host_size_ - 1] = index;
  } else {
    int *host_to_server_index = embedding_host_cache->host_to_server_index.get();
    int *host_to_server_ids = embedding_host_cache->host_to_server_ids.get();
    auto tmp_host_to_server_size = statistics_info_->host_to_server_size_;
    size_t retry_count = 0;
    while (true) {
      // Calculate the mapping of id to index.
      index = host_hash_map->ParseData(swap_device_to_host_id, host_to_server_index, host_to_server_ids, data_step,
                                       *cur_graph_running_step, &statistics_info->host_to_server_size_,
                                       host_cache_need_wait_graph);
      if (index == kInvalidIndexValue) {
        const int64_t wait_interval = 10000;
        *cur_graph_running_step = latest_graph_running_step->load();
        std::this_thread::sleep_for(std::chrono::microseconds(wait_interval));
        ++retry_count;
        if (retry_count > kMaxRetryNum) {
          MS_LOG(ERROR) << "Prefetch embedding cache timeout, please enlarge the vocab cache size.";
          return false;
        }
        MS_LOG(DEBUG) << "There is no space in local host cache, wait and retrying, current graph running step: "
                      << *cur_graph_running_step << ", data step: " << data_step;
        continue;
      }

      // The embedding vector of id which is never used before need not be evicted to remote.
      if (tmp_host_to_server_size < statistics_info_->host_to_server_size_) {
        if (modified_ids_.find(host_to_server_ids[tmp_host_to_server_size]) == modified_ids_.end()) {
          statistics_info_->host_to_server_size_ = tmp_host_to_server_size;
        }
      }

      device_to_host_index[statistics_info->device_to_host_size_ - 1] = index;
      break;
    }
  }

  return true;
}

bool DeviceEmbeddingOperation::MemcpyHostToDeviceAsync(void *dst, const void *src, size_t size,
                                                       const DeviceContext *device_context, size_t stream_id) {
  MS_ERROR_IF_NULL(dst);
  MS_ERROR_IF_NULL(src);
  MS_ERROR_IF_NULL(device_context);
  MS_ERROR_IF_NULL(device_context->device_res_manager_);

  void *device_ptr = dst;
  const void *host_ptr = src;

  auto kernel_tensor = std::make_shared<kernel::KernelTensor>(
    device_ptr, size, Format::DEFAULT_FORMAT, kTypeUnknown, ShapeVector(),
    device_context->device_context_key().device_name_, device_context->device_context_key().device_id_);
  kernel_tensor->set_stream_id(stream_id);
  auto device_address = device_context->device_res_manager_->CreateDeviceAddress(kernel_tensor);
  MS_ERROR_IF_NULL(device_address);
  RETURN_IF_FALSE_WITH_LOG(device_address->AsyncHostToDevice({}, size, kTypeUnknown, host_ptr, stream_id),
                           "Async memcpy host to device failed.");

  return true;
}

bool DeviceEmbeddingOperation::MemcpyDeviceToHostAsync(void *dst, const void *src, size_t size,
                                                       const DeviceContext *device_context, size_t stream_id) {
  MS_ERROR_IF_NULL(dst);
  MS_ERROR_IF_NULL(src);
  MS_ERROR_IF_NULL(device_context);
  MS_ERROR_IF_NULL(device_context->device_res_manager_);

  void *device_ptr = const_cast<void *>(src);
  void *host_ptr = dst;

  auto kernel_tensor = std::make_shared<kernel::KernelTensor>(
    device_ptr, size, Format::DEFAULT_FORMAT, kTypeUnknown, ShapeVector(),
    device_context->device_context_key().device_name_, device_context->device_context_key().device_id_);
  kernel_tensor->set_stream_id(stream_id);
  auto device_address = device_context->device_res_manager_->CreateDeviceAddress(kernel_tensor);
  MS_ERROR_IF_NULL(device_address);
  RETURN_IF_FALSE_WITH_LOG(device_address->AsyncDeviceToHost({}, size, kTypeUnknown, host_ptr, stream_id),
                           "Async memcpy device to host failed.");

  return true;
}

ParameterPtr DeviceEmbeddingOperation::NewParameter(const KernelGraphPtr &graph, TypePtr type,
                                                    const ShapeVector &shape) {
  MS_EXCEPTION_IF_NULL(graph);
  MS_EXCEPTION_IF_NULL(type);

  auto param = graph->NewParameter();
  MS_EXCEPTION_IF_NULL(param);
  auto abstract = std::make_shared<abstract::AbstractTensor>(type, shape);
  param->set_abstract(abstract);

  auto kernel_build_info_builder = std::make_shared<kernel::KernelBuildInfo::KernelBuildInfoBuilder>();
  std::vector<std::string> formats = {kOpFormat_DEFAULT};
  std::vector<TypeId> types = {type->type_id()};
  kernel_build_info_builder->SetOutputsFormat(formats);
  kernel_build_info_builder->SetOutputsDeviceType(types);
  AnfAlgo::SetSelectKernelBuildInfo(kernel_build_info_builder->Build(), param.get());

  auto mutable_inputs = graph->MutableInputs();
  MS_EXCEPTION_IF_NULL(mutable_inputs);
  mutable_inputs->push_back(param);

  return param;
}

ValueNodePtr DeviceEmbeddingOperation::NewValueNode(int64_t value, const DeviceContext *device_context,
                                                    size_t stream_id) {
  MS_EXCEPTION_IF_NULL(device_context);

  auto tensor = std::make_shared<tensor::Tensor>(static_cast<int64_t>(value), kInt32);
  auto value_node = mindspore::NewValueNode(tensor);
  value_node->set_abstract(tensor->ToAbstract());

  // Create kernel build info.
  auto kernel_build_info_builder = std::make_shared<kernel::KernelBuildInfo::KernelBuildInfoBuilder>();
  std::vector<std::string> formats = {kOpFormat_DEFAULT};
  std::vector<TypeId> types = {kInt32->type_id()};
  kernel_build_info_builder->SetOutputsFormat(formats);
  kernel_build_info_builder->SetOutputsDeviceType(types);

  auto kernel_info = std::make_shared<device::KernelInfo>();
  MS_EXCEPTION_IF_NULL(kernel_info);
  value_node->set_kernel_info(kernel_info);
  AnfAlgo::SetSelectKernelBuildInfo(kernel_build_info_builder->Build(), value_node.get());

  // Create device address.
  size_t output_idx = 0;
  size_t tensor_size = AnfAlgo::GetOutputTensorMemSize(value_node, output_idx);
  TypeId output_type_id = AnfAlgo::GetOutputDeviceDataType(value_node, output_idx);
  std::string output_format = AnfAlgo::GetOutputFormat(value_node, output_idx);

  MS_EXCEPTION_IF_NULL(device_context->device_res_manager_);
  auto value_addr = device_context->device_res_manager_->AllocateMemory(tensor_size);
  MS_EXCEPTION_IF_NULL(value_addr);

  const auto &kernel_tensor = AnfAlgo::CreateOutputKernelTensorWithDeviceInfo(
    {value_node, output_idx}, value_addr, tensor_size, output_format, output_type_id,
    trans::GetRuntimePaddingShape(value_node, output_idx), device_context->device_context_key().device_name_,
    device_context->device_context_key().device_id_);
  kernel_tensor->set_stream_id(stream_id);
  auto address = device_context->device_res_manager_->CreateDeviceAddress(kernel_tensor);
  MS_EXCEPTION_IF_NULL(address);

  // Sync tensor value.
  MS_EXCEPTION_IF_CHECK_FAIL(address->AsyncHostToDevice({}, tensor_size, output_type_id, tensor->data_c(), stream_id),
                             "Async memcpy host to device failed.");
  MS_EXCEPTION_IF_CHECK_FAIL(device_context->device_res_manager_->SyncStream(stream_id), "Synchronize stream failed.");

  address->set_from_persistent_mem(true);
  AnfAlgo::SetOutputAddr(address, output_idx, value_node.get());

  return value_node;
}

bool DeviceEmbeddingOperation::InferOpShape(
  const CNodePtr &kernel, const std::vector<kernel::KernelTensor *> &input_kernel_tensors,
  const std::vector<kernel::KernelTensor *> &output_kernel_tensors,
  const std::vector<abstract::AbstractBasePtr> &input_kernel_tensors_for_infer) {
  MS_ERROR_IF_NULL(kernel);
  auto kernel_mod = AnfAlgo::GetKernelMod(kernel);
  MS_ERROR_IF_NULL(kernel_mod);
  // 1. Infer operator's output's Shape.
  auto base_shape = opt::dynamic_shape::InferShape(kernel_mod->primitive(), input_kernel_tensors_for_infer);
  MS_ERROR_IF_NULL(base_shape);
  MS_LOG(DEBUG) << "End InferShape for kernel: " << kernel->fullname_with_scope()
                << ", shape: " << base_shape->ToString();

  // 2. Update shape of output kernel tensor.
  opt::dynamic_shape::UpdateKernelTensorShape(base_shape, output_kernel_tensors);

  // 3. Resize kernel mod.
  MS_LOG(DEBUG) << "Begin Resize kernel mod for kernel: " << kernel->fullname_with_scope();
  int ret = kernel_mod->Resize(input_kernel_tensors, output_kernel_tensors);
  MS_LOG(DEBUG) << "End Resize kernel mod for kernel: " << kernel->fullname_with_scope()
                << ", the output size list: " << kernel_mod->GetOutputSizeList();
  if (ret != kernel::KRET_OK) {
    MS_LOG(ERROR) << "Resize failed for kernel: " << kernel->fullname_with_scope();
    return false;
  }
  return true;
}
}  // namespace runtime
}  // namespace mindspore
