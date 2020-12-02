/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_MINDDATA_DATASET_ENGINE_IR_DATASETOPS_SOURCE_CSV_NODE_H_
#define MINDSPORE_CCSRC_MINDDATA_DATASET_ENGINE_IR_DATASETOPS_SOURCE_CSV_NODE_H_

#include <memory>
#include <string>
#include <vector>

#include "minddata/dataset/engine/ir/datasetops/dataset_node.h"

namespace mindspore {
namespace dataset {
/// \brief Record type for CSV
enum CsvType : uint8_t { INT = 0, FLOAT, STRING };

/// \brief Base class of CSV Record
class CsvBase {
 public:
  CsvBase() = default;
  explicit CsvBase(CsvType t) : type(t) {}
  virtual ~CsvBase() {}
  CsvType type;
};

/// \brief CSV Record that can represent integer, float and string.
template <typename T>
class CsvRecord : public CsvBase {
 public:
  CsvRecord() = default;
  CsvRecord(CsvType t, T v) : CsvBase(t), value(v) {}
  ~CsvRecord() {}
  T value;
};

class CSVNode : public NonMappableSourceNode {
 public:
  /// \brief Constructor
  CSVNode(const std::vector<std::string> &dataset_files, char field_delim,
          const std::vector<std::shared_ptr<CsvBase>> &column_defaults, const std::vector<std::string> &column_names,
          int64_t num_samples, ShuffleMode shuffle, int32_t num_shards, int32_t shard_id,
          std::shared_ptr<DatasetCache> cache);

  /// \brief Destructor
  ~CSVNode() = default;

  /// \brief Node name getter
  /// \return Name of the current node
  std::string Name() const override { return kCSVNode; }

  /// \brief Print the description
  /// \param out - The output stream to write output to
  void Print(std::ostream &out) const override;

  /// \brief Copy the node to a new object
  /// \return A shared pointer to the new copy
  std::shared_ptr<DatasetNode> Copy() override;

  /// \brief a base class override function to create the required runtime dataset op objects for this class
  /// \return shared pointer to the list of newly created DatasetOps
  std::vector<std::shared_ptr<DatasetOp>> Build() override;

  /// \brief Parameters validation
  /// \return Status Status::OK() if all the parameters are valid
  Status ValidateParams() override;

  /// \brief Get the shard id of node
  /// \return Status Status::OK() if get shard id successfully
  Status GetShardId(int32_t *shard_id) override;

  /// \brief Base-class override for GetDatasetSize
  /// \param[in] size_getter Shared pointer to DatasetSizeGetter
  /// \param[in] estimate This is only supported by some of the ops and it's used to speed up the process of getting
  ///     dataset size at the expense of accuracy.
  /// \param[out] dataset_size the size of the dataset
  /// \return Status of the function
  Status GetDatasetSize(const std::shared_ptr<DatasetSizeGetter> &size_getter, bool estimate,
                        int64_t *dataset_size) override;

 private:
  std::vector<std::string> dataset_files_;
  char field_delim_;
  std::vector<std::shared_ptr<CsvBase>> column_defaults_;
  std::vector<std::string> column_names_;
  int64_t num_samples_;
  ShuffleMode shuffle_;
  int32_t num_shards_;
  int32_t shard_id_;
};

}  // namespace dataset
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_MINDDATA_DATASET_ENGINE_IR_DATASETOPS_SOURCE_CSV_NODE_H_
