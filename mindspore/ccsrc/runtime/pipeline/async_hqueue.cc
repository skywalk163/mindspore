/**
 * Copyright 2023-2024 Huawei Technologies Co., Ltd
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

#include "runtime/pipeline/async_hqueue.h"

#if !defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__)
#include "include/common/utils/signal_util.h"
#endif
#include "utils/log_adapter.h"
#include "utils/ms_exception.h"
#include "mindrt/include/fork_utils.h"
#include "include/common/profiler.h"

namespace mindspore {
namespace runtime {
#ifndef LIKELY
#ifdef _MSC_VER
#define LIKELY(x) (x)
#else
#define LIKELY(x) __builtin_expect(!!(x), 1)
#endif
#endif

constexpr int32_t kTaskQueueSize = 8192;
constexpr size_t kMaxSpinCount = 300000;
constexpr size_t kThreadNameThreshold = 15;

void AsyncHqueue::SetThreadName() const {
// Set thread name for gdb debug
#if !defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__)
  (void)pthread_setname_np(pthread_self(), name_.substr(0, kThreadNameThreshold).c_str());
#endif
}

AsyncHqueue::~AsyncHqueue() {
  try {
    WorkerJoin();
  } catch (const std::exception &e) {
    MS_LOG(INFO) << "WorkerJoin failed, error msg:" << e.what();
  }
}

void AsyncHqueue::WorkerLoop() {
#if !defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__)
  // cppcheck-suppress unreadVariable
  SignalGuard sig([](int, siginfo_t *, void *) {
    int this_pid = getpid();
    MS_LOG(WARNING) << "Process " << this_pid << " receive KeyboardInterrupt signal.";
    (void)kill(this_pid, SIGTERM);
  });
#endif

  SetThreadName();
  runtime::ProfilerAnalyzer::GetInstance().SetThreadIdToName(std::this_thread::get_id(), name_);

  while (alive_) {
    // cppcheck-suppress unreadVariable
    if (LIKELY(!tasks_hqueque_.Empty())) {
      auto task = tasks_hqueque_.Dequeue();
      // cppcheck-suppress unreadVariable
      if (LIKELY(task != nullptr)) {
        // cppcheck-suppress unreadVariable
        if (LIKELY(!stop_)) {
          try {
            task->Run();
          } catch (const std::exception &e) {
            MS_LOG(INFO) << "Grad queue catch excption: " << e.what();
            e_ptr_ = std::current_exception();
            stop_ = true;
          }
        }
        delete task;
        spin_count_ = 0;
      }
      continue;
    } else {
      if (spin_count_ == 0) {
        status_.store(kThreadIdle);
      }
      ++spin_count_;
    }
    if (spin_count_ == kMaxSpinCount) {
      std::unique_lock<std::mutex> lock(task_mutex_);
      status_.store(kThreadIdle);
      task_cond_var_->wait(lock, [this]() { return !tasks_hqueque_.Empty() || !alive_; });
      spin_count_ = 0;
    } else {
      std::this_thread::yield();
    }
  }
}

void AsyncHqueue::Init() {
  if (task_cond_var_ == nullptr) {
    task_cond_var_ = std::make_unique<std::condition_variable>();
  }
  if (!tasks_hqueque_.Init(kTaskQueueSize)) {
    MS_LOG(EXCEPTION) << "Init task queue failed.";
  }
  worker_ = std::make_unique<std::thread>(&AsyncHqueue::WorkerLoop, this);
}

bool AsyncHqueue::Push(AsyncTask *task) {
  // For process fork will cause thread confusion
  if (!init_) {
    Init();
    init_ = true;
  }
  if (stop_ && e_ptr_ != nullptr) {
    delete task;
    return false;
  }
  while (!tasks_hqueque_.Enqueue(task)) {
  }
  if (status_.load() == kThreadIdle) {
    status_.store(kThreadBusy);
  }
  if (spin_count_ == kMaxSpinCount) {
    // cppcheck-suppress unreadVariable
    std::unique_lock<std::mutex> lock(task_mutex_);
    task_cond_var_->notify_one();
  }
  return true;
}

void AsyncHqueue::Wait() {
  if (worker_ == nullptr) {
    return;
  }
  if (worker_->get_id() == std::this_thread::get_id()) {
    return;
  }
  while (status_.load() == kThreadBusy) {
  }
}

void AsyncHqueue::Clear() {
  if (status_.load() == kThreadIdle) {
    return;
  }
  stop_ = true;
  Wait();
  stop_ = false;
}

bool AsyncHqueue::Empty() { return tasks_hqueque_.Empty(); }

void AsyncHqueue::WorkerJoin() {
  if (worker_ == nullptr) {
    return;
  }
  Wait();
  {
    // cppcheck-suppress unreadVariable
    std::lock_guard<std::mutex> lock(task_mutex_);
    alive_ = false;
  }
  task_cond_var_->notify_one();

  if (worker_->joinable()) {
    worker_->join();
  }
}

void AsyncHqueue::ChildAfterFork() {
  MS_LOG(DEBUG) << "AsyncHqueue reinitialize after fork.";
  if (task_cond_var_ != nullptr) {
    MS_LOG(DEBUG) << "Release and recreate task_cond_var_.";
    (void)task_cond_var_.release();
    task_cond_var_ = std::make_unique<std::condition_variable>();
  }
  if (worker_ != nullptr) {
    MS_LOG(DEBUG) << "Release and recreate worker_.";
    (void)worker_.release();
    worker_ = std::make_unique<std::thread>(&AsyncHqueue::WorkerLoop, this);
  }
  MS_LOG(DEBUG) << "AsyncHqueue reinitialize after fork done.";
}

void AsyncHqueue::CheckException() {
  if (stop_ && e_ptr_ != nullptr) {
    Wait();
    const auto temp_ptr = e_ptr_;
    e_ptr_ = nullptr;
    stop_ = false;
    std::rethrow_exception(temp_ptr);
  }
}
}  // namespace runtime
}  // namespace mindspore
