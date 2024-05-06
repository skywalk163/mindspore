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
#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_HAL_DEVICE_CPU_HASH_TABLE_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_HAL_DEVICE_CPU_HASH_TABLE_H_

#include <shared_mutex>
#include <unordered_map>
#include <random>
#include <vector>
#include <string>
#include <utility>

#include "runtime/device/hash_table.h"
#include "plugin/device/cpu/hal/hardware/cpu_memory_pool.h"
#include "include/common/random.h"

namespace mindspore {
namespace device {
namespace cpu {
constexpr static size_t kImportTensorNum = 3;
constexpr static char kNormalDistribution[] = "normal";
constexpr static char kZerosDistribution[] = "zeros";
constexpr static char kOnesDistribution[] = "ones";

using DataType = float;
using Generator = random::Philox;
using NormalDistribution = random::NormalDistribution<double>;

// A hash table base on the host side cpu.
template <typename Key, typename Value>
class CPUHashTable : public HashTable<Key, Value> {
 public:
  using Status = HashTableElementStatus;
  using ValueStatusPair = std::pair<Value *, Status>;

  CPUHashTable(size_t value_dim, const std::string &initializer);
  CPUHashTable(size_t value_dim, const Value &default_value);
  ~CPUHashTable() override;

  // Initialize the resources (e.g. device context) needed by this hash table.
  bool Initialize();

  // Release all the resources (e.g. the host side memory) used by this hash table.
  bool Finalize();

  // The last parameter `stream` is meaningless for the cpu hash table version.
  bool Find(const Key *keys, size_t key_num, bool insert_default_value, Value *outputs, void *) override;

  bool Insert(const Key *keys, size_t key_num, const Value *values, void *) override;

  bool Insert(const Key *keys, size_t key_num, const Value *values, Status *statuses, void *) override;

  bool Erase(const Key *keys, size_t key_num, void *) override;

  bool Reserve(size_t new_capacity, void *) override;

  bool GetKeysAndValues(Key *keys, Value *values, void *) override;

  bool Import(const DataLenPair &input_data) override;

  HashTableExportData Export(bool incremental) override;

  // Export a slice from the hash table, the size is specified by the parameter 'slice_size_in_mega_bytes' in MB.
  HashTableExportData ExportSlice(bool incremental, bool *last_slice, size_t slice_size_in_mega_bytes) override;

  size_t capacity() const override;

  size_t size() const override;

  bool is_dirty() const override;

  bool Clear() override;

 private:
  // Export all keys, values and status of the hash table in the iterator interval [begin, end).
  HashTableExportData ExportSliceFully(size_t begin, size_t end);

  // Export the keys, values and status in the iterator interval [begin, end) which are modified or erased since last
  // import or export.
  HashTableExportData ExportSliceIncrementally(size_t begin, size_t end);

  // Allocate host memory from dynamic memory pool.
  void *AllocateMemory(size_t size) const;

  // Free host memory to dynamic memory pool.
  void FreeMemory(void *ptr) const;

  // The key-value style elements stored in this hash table.
  std::unordered_map<Key, ValueStatusPair> values_;

  // This mutex is to guarantee the thread-safe of the `values_` above.
  // The perforcemence of this thread-safe mechanism is poor, so a more efficient way could be used later.
  mutable std::shared_mutex mutex_;

  // The value dimension and byte size for each key.
  size_t value_dim_;
  size_t value_size_;

  // These two augments are set for padding the missing keys.
  std::string initializer_;
  Value default_value_;
  // The flag records whether the elements of the hash table have changed since the last export, true means that there
  // has been a change.
  bool is_dirty_{true};

  // Record the position of slice export, the elements in the iterator interval [begin_, end_) of hash table will be
  // exported.
  size_t begin_{0};
  size_t end_{0};
};
}  // namespace cpu
}  // namespace device
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_HAL_DEVICE_CPU_HASH_TABLE_H_
