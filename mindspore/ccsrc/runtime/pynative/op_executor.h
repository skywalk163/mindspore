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

#ifndef MINDSPORE_MINDSPORE_CCSRC_RUNTIME_PYNATIVE_OP_EXECUTOR_H_
#define MINDSPORE_MINDSPORE_CCSRC_RUNTIME_PYNATIVE_OP_EXECUTOR_H_

#include <vector>
#include <memory>
#include <queue>
#include <map>
#include <string>
#include <set>
#include <utility>
#include "include/backend/kernel_graph.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "runtime/hardware/device_context.h"
#include "runtime/graph_scheduler/graph_scheduler.h"
#include "include/backend/visible.h"
#include "runtime/pipeline/task/device_task.h"
#include "runtime/pipeline/async_rqueue.h"

namespace mindspore::runtime {
class BACKEND_EXPORT OpExecutor {
 public:
  static OpExecutor &GetInstance();

  void RegisterForwardCallback(const std::function<void()> &callback);

  void PushOpRunTask(const std::shared_ptr<DeviceOpRunTask> &op_run_task);

  void PushOpRunTask(const std::shared_ptr<PyBoostDeviceTask> &op_run_task);

  void PushSimpleOpRunTask(const std::shared_ptr<AsyncTask> &op_run_task);

  bool RunQueueEmpty();

  // When an exception occurs, the state needs to be reset.
  // Because we may sometimes have to ignore the exception and continue to run other tasks
  void Reset();

  // Wait for all OpRunTasks to finish executing.
  void Wait();

  void WaitAll();

  // Thread join before the process exit.
  void WorkerJoin();

  // Child process reinitialize resource after fork.
  void ChildAfterFork();

  static bool NeedSync();

 private:
  OpExecutor();
  ~OpExecutor();
  DISABLE_COPY_AND_ASSIGN(OpExecutor);

  void WaitForRun();
  std::function<void()> forward_callback_{nullptr};
};
}  // namespace mindspore::runtime
#endif  // MINDSPORE_MINDSPORE_CCSRC_RUNTIME_PYNATIVE_OP_EXECUTOR_H_
