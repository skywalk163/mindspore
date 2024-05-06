/**
 * Copyright 2024 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_RUNTIME_HARDWARE_DUMMY_ASCEND_COLLECTIVE_COMM_LIB_H_
#define MINDSPORE_CCSRC_RUNTIME_HARDWARE_DUMMY_ASCEND_COLLECTIVE_COMM_LIB_H_

#include <string>
#include <vector>

#include "runtime/collective/communication_group.h"
#include "runtime/collective/dummy_collective_communication_lib.h"

namespace mindspore {
namespace device {
///
/// \brief DummyAscendCollectiveCommLib to maintain collective communication relationship with ascend device
/// communication.
///
class DummyAscendCollectiveCommLib : public DummyCollectiveCommunicationLib {
 public:
  static DummyAscendCollectiveCommLib &GetInstance() {
    static DummyAscendCollectiveCommLib instance;
    return instance;
  }
  DummyAscendCollectiveCommLib();

  ~DummyAscendCollectiveCommLib() override = default;

  bool Initialize(uint32_t global_rank, uint32_t global_rank_size, uint32_t local_rank_id) override;

  bool CreateCommunicationGroup(const std::string &group_name, const std::vector<uint32_t> &group_ranks,
                                uint32_t local_group_rank, uint32_t local_group_size) override;

  std::string HcclInnerCommName(const std::string &group_name);

  bool DestroyDeviceCommunicationGroup(const std::string &group_name) override;

  bool DestroyHcclComm();
};
}  // namespace device
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_RUNTIME_HARDWARE_DUMMY_ASCEND_COLLECTIVE_COMM_LIB_H_
