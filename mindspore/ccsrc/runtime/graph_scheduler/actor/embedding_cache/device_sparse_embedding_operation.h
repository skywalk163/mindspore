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

#ifndef MINDSPORE_CCSRC_RUNTIME_GRAPH_SCHEDULER_ACTOR_EMBEDDING_CACHE_DEVICE_SPARSE_EMBEDDING_OPERATION_H_
#define MINDSPORE_CCSRC_RUNTIME_GRAPH_SCHEDULER_ACTOR_EMBEDDING_CACHE_DEVICE_SPARSE_EMBEDDING_OPERATION_H_

#include <memory>
#include <vector>
#include <utility>
#include "runtime/graph_scheduler/actor/embedding_cache/device_embedding_operation.h"
#include "include/backend/device_address.h"

namespace mindspore {
namespace runtime {
using device::DeviceAddress;

class DeviceSparseEmbeddingOperation : public DeviceEmbeddingOperation {
 public:
  DeviceSparseEmbeddingOperation(EmbeddingCachePrefetchActor *actor, device::DeviceContext *device_context,
                                 const std::pair<int, int> &local_embedding_slice_bounds,
                                 const std::pair<int, int> &local_device_cache_bounds,
                                 EmbeddingCacheStatisticsInfo *statistics_info, const size_t &stream_id)
      : DeviceEmbeddingOperation(actor, device_context, local_embedding_slice_bounds, local_device_cache_bounds,
                                 statistics_info, stream_id) {}

  ~DeviceSparseEmbeddingOperation() override = default;

  bool Initialize() override;

  // Push non-hotspot embeddings on the device cache to the local host cache.
  bool PushCacheFromDeviceToLocalHost(const HashTableInfo &hash_info, const CacheAnalysis *cache_analysis) override;

  // Pull missing embeddings on the device cache from the local host.
  bool PullCacheFromLocalHostToDevice(const HashTableInfo &hash_info, const CacheAnalysis *cache_analysis) override;

  // Get the id range of each server's embedding table slice.
  void GetRemoteEmbeddingSliceBound(size_t vocab_size, size_t server_num,
                                    std::vector<std::pair<size_t, size_t>> *remote_embedding_slice_bounds) override;

 protected:
  // Build a CNode of embedding cache look up kernel(operator name: 'MapTensorGet'), which is used to look up local
  // device embedding cache.
  void BuildEmbeddingCacheLookupKernel() override;
  // Build a CNode of embedding cache update kernel(operator name: 'MapTensorPut'), which is used to update local
  // device embedding cache.
  void BuildEmbeddingCacheUpdateKernel() override;

 private:
  // Build a CNode of embedding cache erase kernel(operator name: 'MapTensorErase'), which is used to erase local
  // device embedding cache.
  void BuildEmbeddingCacheEraseKernel();

  static ParameterPtr NewMapParameter(const KernelGraphPtr &graph, TypeId key_type, TypeId value_type,
                                      const ShapeVector &value_shape);

  // Look up feature weights on Device Embedding Cache.
  bool LookupDeviceCache(const DeviceAddress *embed_device_address, void *ids, void *embedding_cache, size_t ids_num,
                         size_t embedding_size, void *outputs) {
    MS_LOG(EXCEPTION) << "Not implemented function.";
  }

  // Update feature weights on Device Embedding Cache.
  bool UpdateDeviceCache(void *ids, void *update_value, size_t indices_num, size_t embedding_size,
                         void *embedding_cache, const DeviceAddress *embed_device_address) {
    MS_LOG(EXCEPTION) << "Not implemented function.";
  }

  // Erase feature embeddings on device embedding cache.
  bool EraseDeviceCache(void *ids, size_t ids_num, void *embedding_cache, const DeviceAddress *embed_device_address) {
    MS_LOG(EXCEPTION) << "Not implemented function.";
  }

  // The embedding cache erase kernel node(operator name: 'MapTensorErase').
  CNodePtr embedding_cache_erase_node_{nullptr};

  DISABLE_COPY_AND_ASSIGN(DeviceSparseEmbeddingOperation);
};
}  // namespace runtime
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_RUNTIME_GRAPH_SCHEDULER_ACTOR_EMBEDDING_CACHE_DEVICE_SPARSE_EMBEDDING_OPERATION_H_
