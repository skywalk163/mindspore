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

#include "backend/common/optimizer/dynamic_shape/dynamic_shape_helper.h"
#include "ops/nn_op_name.h"
#include "ops/array_op_name.h"
#include "runtime/device/device_address_utils.h"
#include "runtime/graph_scheduler/actor/embedding_cache/device_dense_embedding_operation.h"
namespace mindspore {
namespace runtime {
using distributed::kInvalidIndexValue;
using kernel::Address;
using kernel::AddressPtrList;

bool DeviceDenseEmbeddingOperation::AnalyseCache(int *batch_ids, const size_t batch_ids_num, size_t data_step,
                                                 const std::atomic_ulong *graph_running_step,
                                                 bool *device_cache_need_wait_graph, bool *host_cache_need_wait_graph,
                                                 int *indices, EmbeddingDeviceCache *embedding_device_cache,
                                                 EmbeddingHostCache *embedding_host_cache,
                                                 EmbeddingCacheStatisticsInfo *statistics_info) {
  MS_ERROR_IF_NULL(batch_ids);
  MS_ERROR_IF_NULL(graph_running_step);
  MS_ERROR_IF_NULL(embedding_device_cache);
  MS_ERROR_IF_NULL(statistics_info);

  statistics_info_->batch_id_count_ = batch_ids_num;
  std::unique_ptr<bool[]> out_range = std::make_unique<bool[]>(batch_ids_num);
  auto ret = memset_s(out_range.get(), batch_ids_num * sizeof(bool), 0, batch_ids_num * sizeof(bool));
  if (ret != EOK) {
    MS_LOG(ERROR) << "Memset failed, errno[" << ret << "]";
    return false;
  }

  // 1. Analyze the hit/miss info of the local host cache and device cache.
  RETURN_IF_FALSE_WITH_LOG(CheckCacheHitOrOutRange(batch_ids, batch_ids_num, indices, out_range.get(), data_step),
                           "Check cache hit or out range failed.");
  RETURN_IF_FALSE_WITH_LOG(actor_->ResetEmbeddingHashMap(), "Reset embedding hash map failed.");

  size_t cur_graph_running_step = graph_running_step->load();
  // 2.calculate the swapping and mapping(feature id to cache index) information of the missing feature id that needs to
  // be inserted into the cache.
  for (size_t i = 0; i < batch_ids_num; i++) {
    if (out_range[i]) {
      continue;
    }
    (void)modified_ids_.insert(batch_ids[i]);
    bool need_swap_host_to_device = true;
    bool need_swap_device_to_host = true;
    int index = kInvalidIndexValue;
    RETURN_IF_FALSE_WITH_LOG(ParseDeviceData(batch_ids[i], &need_swap_device_to_host, &need_swap_host_to_device, &index,
                                             data_step, &cur_graph_running_step, graph_running_step,
                                             device_cache_need_wait_graph, embedding_device_cache, statistics_info),
                             "Parse device cache data failed.");
    indices[i] = index + local_device_cache_bounds_.first;
    if (need_swap_host_to_device) {
      RETURN_IF_FALSE_WITH_LOG(
        ParseHostDataHostToDevice(batch_ids[i], data_step, &cur_graph_running_step, graph_running_step,
                                  host_cache_need_wait_graph, embedding_host_cache, statistics_info),
        "Parse local host cache data(swap local host cache to device) failed.");
    }
    if (need_swap_device_to_host) {
      RETURN_IF_FALSE_WITH_LOG(
        ParseHostDataDeviceToHost(data_step, &cur_graph_running_step, graph_running_step, host_cache_need_wait_graph,
                                  embedding_device_cache, embedding_host_cache, statistics_info),
        "Parse local host cache data(swap device cache to local host) failed.");
    }
  }
  return true;
}

bool DeviceDenseEmbeddingOperation::PushCacheFromDeviceToLocalHost(const HashTableInfo &hash_info,
                                                                   const CacheAnalysis *cache_analysis) {
  MS_ERROR_IF_NULL(cache_analysis);
  auto statistics_info = cache_analysis->statistics_info_;
  auto embedding_device_cache = cache_analysis->embedding_device_cache_;
  auto embedding_host_cache = cache_analysis->embedding_host_cache_;
  MS_ERROR_IF_NULL(statistics_info);
  MS_ERROR_IF_NULL(embedding_device_cache);
  MS_ERROR_IF_NULL(embedding_host_cache);

  auto swap_indices_size = statistics_info->device_to_host_size_;
  if (swap_indices_size == 0) {
    return true;
  }

  auto device_cache_device_to_host_index = embedding_device_cache->device_to_host_index.get();
  auto host_cache_device_to_host_index = embedding_host_cache->device_to_host_index.get();
  MS_ERROR_IF_NULL(device_cache_device_to_host_index);
  MS_ERROR_IF_NULL(host_cache_device_to_host_index);
  auto hash_table_addr = reinterpret_cast<float *>(hash_info.address.addr);
  auto cache_vocab_size = hash_info.cache_vocab_size;
  auto host_hash_table_addr = hash_info.host_address;
  auto embedding_size = hash_info.embedding_size;
  auto swap_out_data = std::make_unique<float[]>(swap_indices_size * embedding_size);
  if (swap_indices_size >
      embedding_cache_table_manager.batch_ids_num_ * embedding_cache_table_manager.multi_batch_threshold_) {
    MS_LOG(EXCEPTION) << "The swap size is greater than the size of batch id buffer.";
  }
  RETURN_IF_FALSE_WITH_LOG(
    MemcpyHostToDeviceAsync(embedding_cache_table_manager.hash_swap_index_addr_, device_cache_device_to_host_index,
                            swap_indices_size * sizeof(int), device_context_, stream_id_),
    "Memcpy host to device asynchronously failed.");

  RETURN_IF_FALSE_WITH_LOG(
    LookupDeviceCache(embedding_cache_table_manager.hash_swap_index_addr_, hash_table_addr, swap_indices_size,
                      cache_vocab_size, embedding_size, embedding_cache_table_manager.hash_swap_value_addr_),
    "Lookup device cache failed.");

  RETURN_IF_FALSE_WITH_LOG(
    MemcpyDeviceToHostAsync(swap_out_data.get(), embedding_cache_table_manager.hash_swap_value_addr_,
                            swap_indices_size * embedding_size * sizeof(float), device_context_, stream_id_),
    "Memcpy device to host asynchronously failed.");

  MS_ERROR_IF_NULL(device_context_);
  MS_ERROR_IF_NULL(device_context_->device_res_manager_);
  RETURN_IF_FALSE_WITH_LOG(device_context_->device_res_manager_->SyncStream(stream_id_), "Synchronize stream failed.");
  RETURN_IF_FALSE_WITH_LOG(
    actor_->InsertLocalHostCache(embedding_size, IntToSize(swap_indices_size), host_cache_device_to_host_index,
                                 swap_out_data.get(), host_hash_table_addr),
    "Insert local host cache failed.");
  return true;
}

bool DeviceDenseEmbeddingOperation::PullCacheFromLocalHostToDevice(const HashTableInfo &hash_info,
                                                                   const CacheAnalysis *cache_analysis) {
  MS_ERROR_IF_NULL(cache_analysis);
  auto statistics_info = cache_analysis->statistics_info_;
  auto embedding_device_cache = cache_analysis->embedding_device_cache_;
  auto embedding_host_cache = cache_analysis->embedding_host_cache_;
  MS_ERROR_IF_NULL(statistics_info);
  MS_ERROR_IF_NULL(embedding_device_cache);
  MS_ERROR_IF_NULL(embedding_host_cache);

  auto swap_indices_size = statistics_info->host_to_device_size_;
  if (swap_indices_size == 0) {
    return true;
  }

  auto host_cache_host_to_device_index = embedding_host_cache->host_to_device_index.get();
  auto device_cache_host_to_device_index = embedding_device_cache->host_to_device_index.get();
  MS_ERROR_IF_NULL(host_cache_host_to_device_index);
  MS_ERROR_IF_NULL(device_cache_host_to_device_index);

  auto embedding_size = hash_info.embedding_size;
  MS_ERROR_IF_NULL(hash_info.address.addr);
  auto hash_table_addr = reinterpret_cast<float *>(hash_info.address.addr);
  auto cache_vocab_size = hash_info.cache_vocab_size;
  auto host_hash_table_addr = hash_info.host_address;
  MS_ERROR_IF_NULL(host_hash_table_addr);
  auto swap_out_data = std::make_unique<float[]>(swap_indices_size * embedding_size);
  RETURN_IF_FALSE_WITH_LOG(actor_->LookupLocalHostCache(embedding_size, swap_indices_size, host_hash_table_addr,
                                                        host_cache_host_to_device_index, swap_out_data.get()),
                           "Lookup local host cache failed.");

  if (swap_indices_size >
      embedding_cache_table_manager.batch_ids_num_ * embedding_cache_table_manager.multi_batch_threshold_) {
    MS_LOG(EXCEPTION) << "The swap size is greater than the size of batch value buffer.";
  }
  RETURN_IF_FALSE_WITH_LOG(
    MemcpyHostToDeviceAsync(embedding_cache_table_manager.hash_swap_value_addr_, swap_out_data.get(),
                            swap_indices_size * embedding_size * sizeof(float), device_context_, stream_id_),
    "Memcpy host to device asynchronously failed.");
  RETURN_IF_FALSE_WITH_LOG(
    MemcpyHostToDeviceAsync(embedding_cache_table_manager.hash_swap_index_addr_, device_cache_host_to_device_index,
                            swap_indices_size * sizeof(int), device_context_, stream_id_),
    "Memcpy host to device asynchronously failed.");

  RETURN_IF_FALSE_WITH_LOG(UpdateDeviceCache(embedding_cache_table_manager.hash_swap_index_addr_,
                                             embedding_cache_table_manager.hash_swap_value_addr_, swap_indices_size,
                                             cache_vocab_size, embedding_size, hash_table_addr),
                           "Update device embedding cache failed.");
  MS_ERROR_IF_NULL(device_context_);
  MS_ERROR_IF_NULL(device_context_->device_res_manager_);
  RETURN_IF_FALSE_WITH_LOG(device_context_->device_res_manager_->SyncStream(stream_id_), "Synchronize stream failed.");
  return true;
}

void DeviceDenseEmbeddingOperation::GetRemoteEmbeddingSliceBound(
  size_t vocab_size, size_t server_num, std::vector<std::pair<size_t, size_t>> *remote_embedding_slice_bounds) {
  if (server_num == 0) {
    MS_LOG(EXCEPTION) << "The number of servers is at least 1, but get 0";
  }
  size_t average_slice_size = vocab_size / server_num;
  std::vector<size_t> remote_embedding_slice_sizes = std::vector<size_t>(server_num, average_slice_size);
  size_t rest_vocab_size = vocab_size % server_num;
  for (size_t i = 0; i < rest_vocab_size; i++) {
    remote_embedding_slice_sizes[i] += 1;
  }

  size_t begin;
  size_t end;
  for (size_t i = 0; i < server_num; i++) {
    if (i == 0) {
      begin = 0;
      end = remote_embedding_slice_sizes[0] - 1;
    } else {
      MS_EXCEPTION_IF_NULL(remote_embedding_slice_bounds);
      begin = remote_embedding_slice_bounds->at(i - 1).second + 1;
      end = begin + remote_embedding_slice_sizes[i] - 1;
    }
    (void)remote_embedding_slice_bounds->emplace_back(begin, end);
  }
}

void DeviceDenseEmbeddingOperation::BuildEmbeddingCacheLookupKernel() {
  auto graph = std::make_shared<KernelGraph>();
  MS_EXCEPTION_IF_NULL(graph);
  graph->set_graph_id((std::numeric_limits<uint32_t>::max)());
  embedding_cache_graphs_.push_back(graph);

  // 1. Create parameter nodes which are inputs of embedding cache look up kernel(operator name: 'EmbeddingLookup').
  ParameterPtr input_param = NewParameter(graph, kFloat32, kTwoDimensionalShape);
  ParameterPtr input_indices = NewParameter(graph, kInt32, kOneDimensionalShape);
  ValueNodePtr offset_value_node = NewValueNode(0, device_context_, stream_id_);

  // 2. Create a CNode for operator EmbeddingLookup.
  PrimitivePtr emb_lookup_primitive = std::make_shared<Primitive>(kEmbeddingLookupOpName);
  emb_lookup_primitive->set_attr(kAttrInputIsDynamicShape, MakeValue(true));
  emb_lookup_primitive->set_attr(kAttrOutputIsDynamicShape, MakeValue(true));

  std::vector<AnfNodePtr> emb_lookup_input_nodes{mindspore::NewValueNode(emb_lookup_primitive), input_param,
                                                 input_indices, offset_value_node};
  embedding_cache_lookup_node_ = graph->NewCNode(emb_lookup_input_nodes);
  MS_EXCEPTION_IF_NULL(embedding_cache_lookup_node_);
  auto abstract = std::make_shared<abstract::AbstractTensor>(kFloat32, kTwoDimensionalShape);
  embedding_cache_lookup_node_->set_abstract(abstract);

  // 3. Kernel build process.
  MS_EXCEPTION_IF_NULL(device_context_);
  MS_EXCEPTION_IF_NULL(device_context_->GetKernelExecutor(false));
  device_context_->GetKernelExecutor(false)->CreateKernel({embedding_cache_lookup_node_});
  AnfAlgo::SetStreamId(stream_id_, embedding_cache_lookup_node_.get());

  DeviceAddressUtils::CreateParameterDeviceAddress(device_context_, graph);
  DeviceAddressUtils::CreateKernelOutputDeviceAddress(device_context_, graph, false);
  DeviceAddressUtils::CreateKernelWorkspaceDeviceAddress(device_context_, graph);
}

void DeviceDenseEmbeddingOperation::BuildEmbeddingCacheUpdateKernel() {
  auto graph = std::make_shared<KernelGraph>();
  MS_EXCEPTION_IF_NULL(graph);
  graph->set_graph_id((std::numeric_limits<uint32_t>::max)());
  embedding_cache_graphs_.push_back(graph);

  // 1. Create parameter nodes which are inputs of embedding cache update kernel(operator name: 'ScatterUpdate').
  ParameterPtr input_param = NewParameter(graph, kFloat32, kTwoDimensionalShape);
  ParameterPtr input_indices = NewParameter(graph, kInt32, kOneDimensionalShape);
  ParameterPtr update_values = NewParameter(graph, kFloat32, kTwoDimensionalShape);

  // 2. Create a CNode for operator ScatterUpdate.
  PrimitivePtr embedding_cache_update_primitive = std::make_shared<Primitive>(kScatterUpdateOpName);
  embedding_cache_update_primitive->set_attr(kAttrInputIsDynamicShape, MakeValue(true));

  std::vector<AnfNodePtr> embedding_cache_update_input_nodes{mindspore::NewValueNode(embedding_cache_update_primitive),
                                                             input_param, input_indices, update_values};
  embedding_cache_update_node_ = graph->NewCNode(embedding_cache_update_input_nodes);
  MS_EXCEPTION_IF_NULL(embedding_cache_update_node_);
  auto abstract = std::make_shared<abstract::AbstractTensor>(kFloat32, kTwoDimensionalShape);
  embedding_cache_update_node_->set_abstract(abstract);

  // 3. Kernel build process.
  MS_EXCEPTION_IF_NULL(device_context_);
  MS_EXCEPTION_IF_NULL(device_context_->GetKernelExecutor(false));
  device_context_->GetKernelExecutor(false)->CreateKernel({embedding_cache_update_node_});
  AnfAlgo::SetStreamId(stream_id_, embedding_cache_update_node_.get());

  DeviceAddressUtils::CreateParameterDeviceAddress(device_context_, graph);
  DeviceAddressUtils::CreateKernelOutputDeviceAddress(device_context_, graph, false);
  DeviceAddressUtils::CreateKernelWorkspaceDeviceAddress(device_context_, graph);
}

bool DeviceDenseEmbeddingOperation::LookupDeviceCache(void *indices, void *embedding_cache, size_t indices_num,
                                                      size_t cache_size, size_t embedding_size, void *outputs) {
  MS_ERROR_IF_NULL(indices);
  MS_ERROR_IF_NULL(embedding_cache);
  MS_ERROR_IF_NULL(outputs);
  MS_ERROR_IF_NULL(embedding_cache_lookup_node_);

  // 1. Get input and output kernel tensors.
  std::vector<kernel::KernelTensor *> input_kernel_tensors =
    AnfAlgo::GetOrCreateAllInputKernelTensors(embedding_cache_lookup_node_);
  std::vector<kernel::KernelTensor *> output_kernel_tensors =
    AnfAlgo::GetOrCreateAllOutputKernelTensors(embedding_cache_lookup_node_);
  MS_EXCEPTION_IF_NULL(input_kernel_tensors[kIndex0]);
  MS_EXCEPTION_IF_NULL(input_kernel_tensors[kIndex1]);
  MS_EXCEPTION_IF_NULL(input_kernel_tensors[kIndex2]);

  MS_EXCEPTION_IF_CHECK_FAIL((input_kernel_tensors.size() == kCacheOpInputNum),
                             "For op: " + embedding_cache_lookup_node_->fullname_with_scope() +
                               " need 3 inputs, but got " + std::to_string(input_kernel_tensors.size()));
  MS_EXCEPTION_IF_CHECK_FAIL((output_kernel_tensors.size() == kCacheOpOutputNum),
                             "For op: " + embedding_cache_lookup_node_->fullname_with_scope() +
                               " has 1 output, but got " + std::to_string(output_kernel_tensors.size()));

  std::vector<abstract::AbstractBasePtr> input_kernel_tensors_for_infer(kCacheOpInputNum, nullptr);
  for (size_t i = 0; i < kCacheOpInputNum; i++) {
    const auto &kernel_tensor = AnfAlgo::GetPrevNodeOutputKernelTensor(embedding_cache_lookup_node_, i);
    MS_EXCEPTION_IF_NULL(kernel_tensor);
    input_kernel_tensors_for_infer[i] = kernel_tensor;
  }

  // 2. Update input shape.
  ShapeVector input_param_shape = {SizeToLong(cache_size), SizeToLong(embedding_size)};
  input_kernel_tensors[kIndex0]->SetShape(std::make_shared<abstract::TensorShape>(std::move(input_param_shape)));
  ShapeVector input_indices_shape = {SizeToLong(indices_num)};
  input_kernel_tensors[kIndex1]->SetShape(std::make_shared<abstract::TensorShape>(std::move(input_indices_shape)));

  // 3. Infer shape for embedding cache look up kernel(operator name: 'EmbeddingLookup') which is dynamic shape kernel.
  if (!InferOpShape(embedding_cache_lookup_node_, input_kernel_tensors, output_kernel_tensors,
                    input_kernel_tensors_for_infer)) {
    MS_LOG(ERROR) << "Infer operator shape failed, op name: " << embedding_cache_lookup_node_->fullname_with_scope();
    return false;
  }

  // 4. Do embedding cache look up on device.
  input_kernel_tensors[kIndex0]->set_device_ptr(embedding_cache);
  input_kernel_tensors[kIndex1]->set_device_ptr(indices);
  output_kernel_tensors[kIndex1]->set_device_ptr(outputs);

  MS_ERROR_IF_NULL(device_context_);
  MS_ERROR_IF_NULL(device_context_->GetKernelExecutor(false));
  auto kernel_mod = AnfAlgo::GetKernelMod(embedding_cache_lookup_node_);
  auto stream = device_context_->device_res_manager_->GetStream(stream_id_);
  auto ret = device_context_->GetKernelExecutor(false)->LaunchKernel(embedding_cache_lookup_node_, input_kernel_tensors,
                                                                     {}, output_kernel_tensors, kernel_mod, stream);
  if (!ret) {
    MS_LOG(ERROR) << "Launch kernel: " << embedding_cache_lookup_node_->fullname_with_scope() << " failed.";
    return false;
  }
  return true;
}

bool DeviceDenseEmbeddingOperation::UpdateDeviceCache(void *indices, void *update_value, size_t indices_num,
                                                      size_t cache_size, size_t embedding_size, void *embedding_cache) {
  MS_ERROR_IF_NULL(indices);
  MS_ERROR_IF_NULL(update_value);
  MS_ERROR_IF_NULL(embedding_cache);
  MS_ERROR_IF_NULL(embedding_cache_update_node_);

  // 1. Get input and output kernel tensors.
  std::vector<kernel::KernelTensor *> input_kernel_tensors =
    AnfAlgo::GetOrCreateAllInputKernelTensors(embedding_cache_update_node_);
  std::vector<kernel::KernelTensor *> output_kernel_tensors =
    AnfAlgo::GetOrCreateAllOutputKernelTensors(embedding_cache_update_node_);
  MS_EXCEPTION_IF_NULL(input_kernel_tensors[kIndex0]);
  MS_EXCEPTION_IF_NULL(input_kernel_tensors[kIndex1]);
  MS_EXCEPTION_IF_NULL(input_kernel_tensors[kIndex2]);

  MS_EXCEPTION_IF_CHECK_FAIL((input_kernel_tensors.size() == kCacheOpInputNum),
                             "For op: " + embedding_cache_update_node_->fullname_with_scope() +
                               " need 3 inputs, but got " + std::to_string(input_kernel_tensors.size()));
  MS_EXCEPTION_IF_CHECK_FAIL((output_kernel_tensors.size() == kCacheOpOutputNum),
                             "For op: " + embedding_cache_update_node_->fullname_with_scope() +
                               " has 1 output, but got " + std::to_string(output_kernel_tensors.size()));

  std::vector<abstract::AbstractBasePtr> input_kernel_tensors_for_infer(kCacheOpInputNum, nullptr);
  for (size_t i = 0; i < kCacheOpInputNum; i++) {
    const auto &kernel_tensor = AnfAlgo::GetPrevNodeOutputKernelTensor(embedding_cache_update_node_, i);
    MS_EXCEPTION_IF_NULL(kernel_tensor);
    input_kernel_tensors_for_infer[i] = kernel_tensor;
  }

  // 2. Update input shape.
  ShapeVector input_param_shape = {SizeToLong(cache_size), SizeToLong(embedding_size)};
  input_kernel_tensors[kIndex0]->SetShape(std::make_shared<abstract::TensorShape>(std::move(input_param_shape)));
  const ShapeVector input_indices_shape = {SizeToLong(indices_num)};
  input_kernel_tensors[kIndex1]->SetShape(std::make_shared<abstract::TensorShape>(std::move(input_indices_shape)));
  const ShapeVector update_values_shape = {SizeToLong(indices_num), SizeToLong(embedding_size)};
  input_kernel_tensors[kIndex2]->SetShape(std::make_shared<abstract::TensorShape>(std::move(update_values_shape)));

  // 3. Infer shape for embedding cache update kernel(operator name: 'ScatterUpdate') which is dynamic shape kernel.
  if (!InferOpShape(embedding_cache_update_node_, input_kernel_tensors, output_kernel_tensors,
                    input_kernel_tensors_for_infer)) {
    MS_LOG(ERROR) << "Infer operator shape failed, op name: " << embedding_cache_update_node_->fullname_with_scope();
    return false;
  }

  // 4. Do update cache on device.
  input_kernel_tensors[kIndex0]->set_device_ptr(embedding_cache);
  input_kernel_tensors[kIndex1]->set_device_ptr(indices);
  input_kernel_tensors[kIndex2]->set_device_ptr(update_value);
  output_kernel_tensors[kIndex0]->set_device_ptr(embedding_cache);

  MS_ERROR_IF_NULL(device_context_);
  MS_ERROR_IF_NULL(device_context_->GetKernelExecutor(false));
  auto kernel_mod = AnfAlgo::GetKernelMod(embedding_cache_update_node_);
  auto stream = device_context_->device_res_manager_->GetStream(stream_id_);
  auto ret = device_context_->GetKernelExecutor(false)->LaunchKernel(embedding_cache_update_node_, input_kernel_tensors,
                                                                     {}, output_kernel_tensors, kernel_mod, stream);
  if (!ret) {
    MS_LOG(ERROR) << "Launch kernel: " << embedding_cache_update_node_->fullname_with_scope() << " failed.";
    return false;
  }
  return true;
}

bool DeviceDenseEmbeddingOperation::CheckCacheHitOrOutRange(const int *batch_ids, const size_t batch_ids_num,
                                                            int *hash_index, bool *out_range, size_t data_step) {
  MS_ERROR_IF_NULL(batch_ids);
  MS_ERROR_IF_NULL(hash_index);
  MS_ERROR_IF_NULL(out_range);

  size_t thread_num = batch_ids_num / kMaxIdsPerThread + 1;
  thread_num = thread_num > kMaxThreadNum ? kMaxThreadNum : thread_num;
  std::thread threads[kMaxThreadNum];
  size_t hash_hit_count[kMaxThreadNum] = {0};
  size_t i = 0;
  size_t offset = 0;

  for (; i < thread_num; ++i) {
    if (offset >= batch_ids_num) {
      break;
    }
    size_t proc_len = batch_ids_num / thread_num + (i < (batch_ids_num % thread_num) ? 1 : 0);
    threads[i] = std::thread(&DeviceDenseEmbeddingOperation::CheckCacheHitOrOutRangeFunc, this, batch_ids + offset,
                             proc_len, hash_index + offset, out_range + offset, hash_hit_count + i, data_step);
    offset += proc_len;
  }
  if (offset != batch_ids_num) {
    MS_LOG(WARNING) << "Check id in device inadequate, total:" << batch_ids_num << " checked:" << offset;
  }

  for (size_t j = 0; j < i; j++) {
    threads[j].join();
  }
  for (size_t j = 0; j < i; j++) {
    statistics_info_->hash_hit_count_ += hash_hit_count[j];
  }
  return true;
}

bool DeviceDenseEmbeddingOperation::CheckCacheHitOrOutRangeFunc(const int *batch_ids, const size_t batch_ids_num,
                                                                int *hash_index, bool *out_range,
                                                                size_t *hash_hit_count, size_t data_step) {
  MS_ERROR_IF_NULL(batch_ids);
  MS_ERROR_IF_NULL(hash_index);
  MS_ERROR_IF_NULL(out_range);
  MS_ERROR_IF_NULL(hash_hit_count);
  auto &device_hash_map = embedding_cache_table_manager.device_hash_map_;
  MS_ERROR_IF_NULL(device_hash_map);

  for (size_t i = 0; i < batch_ids_num; ++i) {
    if (batch_ids[i] < local_embedding_slice_bounds_.first) {
      hash_index[i] = batch_ids[i] - local_embedding_slice_bounds_.first + local_device_cache_bounds_.first;
      out_range[i] = true;
      continue;
    }
    if (batch_ids[i] >= local_embedding_slice_bounds_.second) {
      hash_index[i] = batch_ids[i] + local_device_cache_bounds_.second;
      out_range[i] = true;
      continue;
    }
  }
  return true;
}

bool DeviceDenseEmbeddingOperation::ParseDeviceData(int id, bool *need_swap_device_to_host,
                                                    bool *need_swap_host_to_device, int *hash_index, size_t data_step,
                                                    size_t *cur_graph_running_step,
                                                    const std::atomic_ulong *latest_graph_running_step,
                                                    bool *device_cache_need_wait_graph,
                                                    EmbeddingDeviceCache *embedding_device_cache,
                                                    EmbeddingCacheStatisticsInfo *statistics_info) {
  MS_ERROR_IF_NULL(need_swap_device_to_host);
  MS_ERROR_IF_NULL(need_swap_host_to_device);
  MS_ERROR_IF_NULL(hash_index);
  MS_ERROR_IF_NULL(cur_graph_running_step);
  MS_ERROR_IF_NULL(latest_graph_running_step);
  MS_ERROR_IF_NULL(embedding_device_cache);
  MS_ERROR_IF_NULL(statistics_info);
  auto &device_hash_map = embedding_cache_table_manager.device_hash_map_;
  MS_ERROR_IF_NULL(device_hash_map);

  int index = kInvalidIndexValue;
  if (device_hash_map->GetIndex(id, &index)) {
    *need_swap_device_to_host = false;
    *need_swap_host_to_device = false;
    if (device_hash_map->hash_step(index) != data_step) {
      statistics_info->hash_hit_count_++;
      device_hash_map->set_hash_step(index, data_step);
    }
  } else {
    int *device_to_host_index = embedding_device_cache->device_to_host_index.get();
    int *device_to_host_ids = embedding_device_cache->device_to_host_ids.get();
    int *host_to_device_index = embedding_device_cache->host_to_device_index.get();
    int *host_to_device_ids = embedding_device_cache->host_to_device_ids.get();
    MS_ERROR_IF_NULL(host_to_device_index);
    MS_ERROR_IF_NULL(host_to_device_ids);
    auto tmp_device_to_host_size = statistics_info->device_to_host_size_;
    size_t retry_count = 0;
    while (true) {
      // Calculate the mapping of id to index.
      index =
        device_hash_map->ParseData(id, device_to_host_index, device_to_host_ids, data_step, *cur_graph_running_step,
                                   &statistics_info->device_to_host_size_, device_cache_need_wait_graph);
      if (index == kInvalidIndexValue) {
        const int64_t wait_interval = 10000;
        *cur_graph_running_step = latest_graph_running_step->load();
        std::this_thread::sleep_for(std::chrono::microseconds(wait_interval));
        ++retry_count;
        if (retry_count > kMaxRetryNum) {
          MS_LOG(ERROR) << "Prefetch embedding cache timeout, please enlarge the vocab cache size.";
          return false;
        }
        MS_LOG(DEBUG) << "There is no space in device cache, wait and retrying, current graph running step: "
                      << *cur_graph_running_step << ", data step: " << data_step;
        continue;
      }
      host_to_device_index[statistics_info->host_to_device_size_] = index;
      host_to_device_ids[statistics_info->host_to_device_size_] = id;
      statistics_info->host_to_device_size_++;
      *need_swap_device_to_host = statistics_info->device_to_host_size_ > tmp_device_to_host_size;
      break;
    }
  }

  *hash_index = index;
  return true;
}
}  // namespace runtime
}  // namespace mindspore
