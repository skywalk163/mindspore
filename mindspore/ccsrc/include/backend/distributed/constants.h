/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_DISTRIBUTED_CONSTANTS_H_
#define MINDSPORE_CCSRC_DISTRIBUTED_CONSTANTS_H_

#include <set>
#include <map>
#include <chrono>
#include <string>
#include <vector>
#include <utility>
#include <functional>

#include "actor/log.h"
#include "actor/msg.h"
#include "utils/ms_utils.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace distributed {
// The detailed reason of failing to run 'mindspore.communication.init()' with ClusterContext.
constexpr char kDetailedFailureReason[] =
  "Maybe you are trying to call 'mindspore.communication.init()' without using 'mpirun', which will make MindSpore "
  "load several environment variables and check their validation. Please use 'mpirun' to launch this process to fix "
  "this issue, or refer to this link if you want to run distributed training without using 'mpirun': "
  "https://www.mindspore.cn/tutorials/experts/zh-CN/master/parallel/dynamic_cluster.html";

constexpr char kWorkerProcessNotEnoughError[] = "Spawned worker process number is not as expected.";
constexpr char kSchedPortOccupiedError[] = "Configured scheduler port MS_SCHED_PORT is occupied by other processes.";
constexpr char kSchedWorkerAddrNotConsistentError[] =
  "Scheduler and worker's configured MS_SCHED_HOST or MS_SCHED_PORT is not consistent with each other.";

constexpr char kEnvServerNum[] = "MS_SERVER_NUM";
constexpr char kEnvWorkerNum[] = "MS_WORKER_NUM";
constexpr char kEnvSchedulerHost[] = "MS_SCHED_HOST";
constexpr char kEnvSchedulerPort[] = "MS_SCHED_PORT";

constexpr char kEnvRole[] = "MS_ROLE";
constexpr char kEnvRoleOfServer[] = "MS_SERVER";
constexpr char kEnvRoleOfPServer[] = "MS_PSERVER";
constexpr char kEnvRoleOfWorker[] = "MS_WORKER";
constexpr char kEnvRoleOfScheduler[] = "MS_SCHED";
const std::set<std::string> kValidRoleName = {kEnvRoleOfServer, kEnvRoleOfPServer, kEnvRoleOfWorker,
                                              kEnvRoleOfScheduler};

// Denote which ip address is used for cluster building.
constexpr char kEnvWorkerIp[] = "MS_WORKER_IP";

// Used in parameter server embedding cache scenarios to identify the same Parameter between Worker and Server.
constexpr char kParameterKey[] = "parameter_key";
// Embedding cache lookup operation.
constexpr char kLookupEmbeddingCache[] = "LookupEmbeddingCache";
// Embedding cache update operation.
constexpr char kUpdateEmbeddingCache[] = "UpdateEmbeddingCache";
const std::vector<std::string> kEmbeddingCacheOps = {kLookupEmbeddingCache, kUpdateEmbeddingCache};
// Message header of finalize mux recv actor.
constexpr char kFinalizeMuxRecvActor[] = "FINALIZE_MUX_RECV_ACTOR";

// The distributed execution mode enum.
// For each execution mode, different graph optimization, splitting strategy, device location, etc are applied. For
// details please refer to class DistributedExecutionMode and its subclasses.

// kGeneralMode: Simply split a training graph into multiple devices without other extra features.

// kParallelMode: MindSpore's existing auto-parallel feature along with distributed graph splitting feature are
// combined. This is much more complicated than other mode. It is always applied in MoE scenarios.

// kPSMode: Applied when running Parameter Server training.

// kEmbeddingCacheMode: Applied when embedding cache is enabled. Normally used for training models with large embedding
// layer.
enum class DistExecutionMode { kGeneralMode = 0, kParallelMode, kPSMode, kEmbeddingCacheMode, kInvalidMode };

// The operator's label in distributed execution.
constexpr char kOpLabelRankId[] = "rank_id";
constexpr char kOpLabelRole[] = "ms_role";

constexpr char kLocalHost[] = "127.0.0.1";
constexpr int MAX_HOSTNAME_LEN = 1024;
const uint16_t kDefaultSchedPort = 6667;
const uint16_t kMaxPort = 65535;
constexpr uint32_t kDefaultFinishTimeout = 30;

// For each computing graph node, there is a range for rpc server's port number.
// Each node has range number 2048, and the port started from 8118.
constexpr uint32_t kStartPort = 8118;
constexpr uint32_t kNodePortRangeNum = 4096;
constexpr char kNodePortRange[] = "node_port_range";
using ServerPortRange = std::pair<uint32_t, uint32_t>;

constexpr char kDataSyncSrcOpName[] = "DataSyncSrc";
constexpr char kDataSyncDstOpName[] = "DataSyncDst";
constexpr char kControlSrcOpName[] = "ControlSrc";
constexpr char kControlDstOpName[] = "ControlDst";

static const char URL_PROTOCOL_IP_SEPARATOR[] = "://";
static const char URL_IP_PORT_SEPARATOR[] = ":";

constexpr char kEnableRDMA[] = "enable_rdma";
constexpr char kRDMADevName[] = "rdma_dev";
constexpr char kRDMAIP[] = "rdma_ip";

constexpr char kDefaultIP[] = "1.1.8.203";
constexpr char kDefaultIfName[] = "hrn0_2";
constexpr uint16_t kDefaultPort = 10969;

// The interval of retrying connecting for rpc clients.
constexpr uint32_t kRetryConnectInterval = 2;

// Time of retrying with increasing port number.
constexpr uint32_t kMaxRetryPortNum = 10;

// The remote function id which will be increased progressively.
inline uint32_t kRemoteFuncId = 0;

// Rank list vector, could be [m, n] or [m, m+1, ..., m+n].
using RankList = std::vector<uint32_t>;

// This macro the current timestamp in milliseconds.
#define CURRENT_TIMESTAMP_MILLI \
  (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()))

using MessageHandler = std::function<MessageBase *const(MessageBase *const)>;

/**
 * @description: The callback function type for allocating memory after receiving data for the peer.
 * @param {size_t} size: Size of the memory to be allocated.
 * @return {void *}: A pointer to the newly allocated memory.
 */
using MemAllocateCallback = std::function<void *(size_t size)>;

/**
 * @description: The callback function for releasing memory after sending it to the peer.
 * @param {void} *data: The memory to be released, which should be allocated on heap.
 * @return {bool}: Whether the memory is successfully released.
 */
using MemFreeCallback = std::function<bool(void *data)>;
}  // namespace distributed
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_DISTRIBUTED_CONSTANTS_H_
