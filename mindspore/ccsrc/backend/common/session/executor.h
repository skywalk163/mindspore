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
#ifndef MINDSPORE_CCSRC_BACKEND_SESSION_EXECUTOR_H
#define MINDSPORE_CCSRC_BACKEND_SESSION_EXECUTOR_H

#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <utility>
#include "backend/common/session/session_basic.h"
#include "ir/anf.h"
#include "ir/tensor.h"
#include "utils/any.h"
#include "include/common/utils/comm_manager.h"
#include "include/common/utils/contract.h"
#include "include/backend/visible.h"

namespace mindspore::session {
enum TaskType {
  kUnKnown,
  kExit,
  kCompileNodes,
  kCompileGraph,
  kBuildGraph,
  kRunGraph,
  kRunOp,
  kCreateCommGroup,
  kDestroyCommGroup,
  kRunOpsInGraph
};

class Task {
 public:
  Task() = default;
  virtual ~Task() = default;
  SessionPtr session_{nullptr};
  TaskType type_{kUnKnown};
  bool sync_run_{false};
  virtual void Run() {}
};

class CompileNodesTask : public Task {
 public:
  CompileNodesTask() { type_ = kCompileNodes; }
  ~CompileNodesTask() override = default;
  void Run() override;
  GraphSegmentPtr segment_;
  AnfNodePtrList output_nodes_;
  GraphId graph_id_{0};
};

class CompileGraphTask : public Task {
 public:
  CompileGraphTask() { type_ = kCompileGraph; }
  ~CompileGraphTask() override = default;
  void Run() override;
  FuncGraphPtr func_graph_{nullptr};
  GraphId graph_id_{0};
};

class BuildGraphTask : public Task {
 public:
  BuildGraphTask() { type_ = kBuildGraph; }
  ~BuildGraphTask() override = default;
  void Run() override;
  GraphId graph_id_{0};
};

class RunGraphTask : public Task {
 public:
  RunGraphTask() { type_ = kRunGraph; }
  ~RunGraphTask() override = default;
  void Run() override;
  std::vector<tensor::TensorPtr> input_tensors_;
  std::vector<tensor::TensorPtr> input_need_wait_tensors_;
  std::vector<tensor::TensorPtr> input_need_lock_tensors_;
  VectorRef outputs_;
  GraphId graph_id_{0};
  std::map<tensor::TensorPtr, session::KernelWithIndex> tensor_to_node_;
  KernelMapTensor node_to_tensor_;
};

class CreateCommGroupTask : public Task {
 public:
  CreateCommGroupTask() { type_ = kCreateCommGroup; }
  ~CreateCommGroupTask() override = default;
  void Run() override;
  std::string group_name_;
  std::vector<uint32_t> ranks_;
  bool result_{false};
};

class DestroyCommGroupTask : public Task {
 public:
  DestroyCommGroupTask() { type_ = kDestroyCommGroup; }
  ~DestroyCommGroupTask() override = default;
  void Run() override;
  std::string group_name_;
  bool result_{false};
};

class ExitTask : public Task {
 public:
  ExitTask() { type_ = kExit; }
  ~ExitTask() override = default;
};

enum class ExecutorEvent { kClear, kRunGraphFinished, kException };

class BACKEND_EXPORT Executor {
 public:
  Executor(std::string device_name, uint32_t device_id) : device_name_(std::move(device_name)), device_id_(device_id) {
    worker_ = std::make_shared<std::thread>(&Executor::WorkerLoop, this);
  }
  ~Executor();
  void WorkerLoop();
  void WorkerJoin();
  GraphId CompileGraph(const SessionPtr &session, const GraphSegmentPtr &segment, const AnfNodePtrList &outputs);
  GraphId CompileGraph(const SessionPtr &session, NotNull<FuncGraphPtr> func_graph);
  void BuildGraph(const SessionPtr &session, GraphId graphId);
  void RunGraph(const SessionPtr &session, const GraphId &graph_id, const std::vector<tensor::TensorPtr> &inputs,
                VectorRef *outputs);
  void RunGraphAsync(const SessionPtr &session, const GraphId &graph_id, const std::vector<tensor::TensorPtr> &inputs,
                     VectorRef *outputs);
  bool CreateCommGroup(const std::string &group_name, const std::vector<uint32_t> &ranks);
  bool DestroyCommGroup(const std::string &group_name);
  void OnEvent(const ExecutorEvent &event);
  void ClearDoneTasks();

 private:
  void RunTask(const std::shared_ptr<Task> &task, bool sync, bool long_run = false);
  std::vector<std::shared_ptr<RunGraphTask>> GetReadyTasksFromPendingList();
  void OnWorkerExit();
  void OnClear();
  void OnRunGraphFinished();
  void OnException();

  std::string device_name_;
  uint32_t device_id_;
  std::mutex task_mutex_;
  std::mutex done_task_mutex_;
  std::mutex pending_task_mutex_;
  std::mutex reenter_mutex_;
  std::condition_variable task_cond_var_;
  std::condition_variable sync_cond_var_;
  std::condition_variable reenter_cond_var_;
  std::queue<std::shared_ptr<Task>> ready_tasks_;
  std::list<std::shared_ptr<RunGraphTask>> pending_tasks_;
  std::vector<std::shared_ptr<Task>> done_tasks_;
  std::shared_ptr<std::thread> worker_;
  bool sync_run_task_finished_{false};
};
}  // namespace mindspore::session
#endif  // MINDSPORE_CCSRC_BACKEND_SESSION_EXECUTOR_H
