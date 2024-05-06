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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_HAL_DEVICE_DUMP_ASCEND_DUMP_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_HAL_DEVICE_DUMP_ASCEND_DUMP_H_
#include <vector>
#include <memory>
#include <string>
#include <map>
#include "include/backend/anf_runtime_algorithm.h"
#include "backend/common/session/session_basic.h"
#include "include/backend/debug/data_dump/dump_utils.h"
#include "include/backend/debug/debugger/debugger.h"
#include "include/backend/debug/data_dump/dump_json_parser.h"
#include "include/common/debug/anf_dump_utils.h"
#include "include/common/utils/config_manager.h"
#include "include/common/utils/anfalgo.h"
#include "kernel/kernel.h"
#include "plugin/device/ascend/hal/device/dump/dump_data_builder.h"
#include "runtime/hardware/device_context.h"
#include "acl/acl_dump.h"
#include "proto/dump_data.pb.h"

namespace mindspore {
using DumpChunk = acldumpChunk;
namespace ascend {
struct dump_data_t {
  std::string dump_file_path;
  char *data_ptr;
  mindspore::TypeId data_type;
  std::string format;
  ShapeVector device_shape;
  ShapeVector host_shape;
  size_t data_size;
  int32_t sub_format;
  std::string in_out_str;
  uint32_t slot;
  std::shared_ptr<tensor::Tensor> trans_buf{nullptr};
};

class AscendAsyncDumpManager {
 public:
  static AscendAsyncDumpManager &GetInstance();

  std::shared_ptr<DumpDataBuilder> LoadDumpDataBuilder(const std::string &node_name);
  void ClearDumpDataBuilder(const std::string &node_name);
  void WaitForWriteFileFinished() const;

 private:
  AscendAsyncDumpManager() = default;
  ~AscendAsyncDumpManager() = default;
  // to construct kernel data for async dump, key is the dump path to the node
  std::map<std::string, std::shared_ptr<DumpDataBuilder>> dump_data_construct_map_;
};

class AscendAsyncDump {
 public:
  AscendAsyncDump() = default;
  ~AscendAsyncDump() = default;
  static void DumpTensorToFile(const std::string &dump_path, const toolkit::dumpdata::DumpData &dump_data,
                               char *data_ptr);

  static void DumpOpDebugToFile(const std::string &dump_path, const toolkit::dumpdata::DumpData &dump_data,
                                const char *data_ptr);

 private:
  static nlohmann::json ParseOverflowInfo(const char *data_ptr);

  static bool ConvertFormatForOneTensor(dump_data_t *dump_tensor_info);

  static void ConvertFormatForTensors(std::vector<dump_data_t> *dump_tensor_vec, size_t start_idx, size_t end_idx);

  static bool DumpTensorStatsIfNeeded(const dump_data_t &dump_tensor_info);

  static bool DumpTensorDataIfNeeded(const dump_data_t &dump_tensor_info);
};

// Callback function to dump ascend async mode
int32_t DumpDataCallBack(const DumpChunk *dump_chunk, int32_t size);
}  // namespace ascend
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_ASCEND_HAL_DEVICE_DUMP_ASCEND_DUMP_H_
