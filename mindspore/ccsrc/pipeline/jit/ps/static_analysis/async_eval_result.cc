/**
 * Copyright 2021-2023 Huawei Technologies Co., Ltd
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

#include "pipeline/jit/ps/static_analysis/async_eval_result.h"
#include "pipeline/jit/ps/debug/trace.h"
#include "utils/symbolic.h"
#include "utils/compile_config.h"
#include "include/common/debug/common.h"
#include "pipeline/jit/ps/base.h"
#include "include/common/utils/utils.h"

namespace mindspore {
namespace abstract {
namespace {
constexpr auto kStateStop = "Stop";
}  // namespace
thread_local std::string AnalysisSchedule::thread_id_ = "m";

void AnalysisSchedule::Schedule() {
  const auto checkPeriod = std::chrono::seconds(3);
  while (run_ || infer_thread_count_.load() > 0) {
    std::unique_lock<std::mutex> lock(activate_thread_lock_);
    auto ok = activate_thread_cv_.wait_for(lock, checkPeriod,
                                           [this] { return activate_threads_.empty() && !schedule_list_.empty(); });
    if (ok) {
      SetNextReady();
    }
  }
  MS_LOG(DEBUG) << "Success to exit.";
}

void AnalysisSchedule::YieldTask(AsyncInferTask *async_infer_task) {
  MS_EXCEPTION_IF_NULL(async_infer_task);
  {
    std::lock_guard<std::mutex> activeLock(activate_thread_lock_);
    if (async_infer_task->ready() == 0) {
      MS_LOG(DEBUG) << " The active thread count: " << activate_threads_.size() << " thread id: " << thread_id()
                    << " async_infer_task thread id:" << async_infer_task->thread_id();
      (void)activate_threads_.erase(thread_id());
    }
  }
  activate_thread_cv_.notify_one();
}

void AnalysisSchedule::HandleException(const std::exception &ex) {
  // Just record the first exception information.
  if (!StaticAnalysisException::Instance().HasException()) {
    StaticAnalysisException::Instance().SetException();

    // If python Exception, record the eval stack.
    if (dynamic_cast<const py::error_already_set *>(&ex) != nullptr) {
      try {
        MS_LOG(DEBUG) << "Python exception happened, check the information as below.";
        std::ostringstream exceptionStream;
        exceptionStream << ex.what() << std::endl;
        trace::GetTraceStackInfo(exceptionStream);
        if (!trace::GetCNodeDebugStack().empty()) {
          MS_LOG(ERROR) << "Exception happened, check the information as below.\n" << exceptionStream.str();
        }
      } catch (const std::exception &e) {
        // Ignored.
      }
    }
  }
  // Free all the locks. Let all the threads continue to run.
  std::lock_guard<std::mutex> lock(activate_thread_lock_);
  for (auto &item : schedule_list_) {
    MS_EXCEPTION_IF_NULL(item);
    item->SetException();
  }
  schedule_list_.clear();
  // The global primitive evaluate cache should be cleared,
  // Since it may contains invalid results when exception raised.
  auto &prim_eval_cache = AnalysisResultCacheMgr::GetInstance().prim_eval_cache();
  if (prim_eval_cache != nullptr) {
    prim_eval_cache->Clear();
  }
}

void AnalysisSchedule::Stop() {
  AsyncInferTaskPtr stop_task = AsyncInferTask::MakeShared(std::make_shared<AsyncAbstract>(), kStateStop);
  Add2Schedule(stop_task);
  if (dispatcher_ != nullptr && dispatcher_->joinable()) {
    try {
      dispatcher_->join();
    } catch (const std::exception &e) {
      MS_LOG(WARNING) << " Analysis schedule thread join exception:" << e.what();
    }
  }
  MS_LOG(DEBUG) << "Set analysis schedule to stop";
}

void AnalysisSchedule::Wait() {
  EnterWaiting();
  if (infer_thread_count_.load() > 0) {
    py::gil_scoped_release infer_gil_release;
    MS_LOG(DEBUG) << AnalysisSchedule::thread_id() << " waiting.";
    std::unique_lock<std::mutex> lock(infer_thread_lock_);
    infer_thread_cv_.wait(lock, [this] { return infer_thread_count_.load() <= 0; });
  }
  MS_LOG(DEBUG) << AnalysisSchedule::thread_id() << " active.";
  if (infer_thread_count_.load() < 0) {
    MS_LOG(ERROR) << "There is something wrong. thread count: " << infer_thread_count_;
  }
  MS_LOG(DEBUG) << "Infer finished.";
  StaticAnalysisException::Instance().CheckException();
}

void AnalysisSchedule::WaitForRun() const {
  // Control the order to run.
  AsyncAbstractPtr control_run_order = std::make_shared<AsyncAbstract>();
  control_run_order->set_result(std::make_shared<AbstractScalar>(1));
  AsyncInferTaskPtr async_task = AsyncInferTask::MakeShared(control_run_order);
  AnalysisSchedule::GetInstance().Add2Schedule(async_task);
  (void)async_task->GetResult();
}

void AnalysisSchedule::Add2Schedule(const AsyncInferTaskPtr &async_infer_task_ptr) {
  std::lock_guard<std::mutex> lock(activate_thread_lock_);
  MS_EXCEPTION_IF_NULL(async_infer_task_ptr);
  schedule_list_.push_back(async_infer_task_ptr);
  activate_thread_cv_.notify_one();
  MS_LOG(DEBUG) << " async: " << async_infer_task_ptr->thread_id() << " address: " << async_infer_task_ptr.get()
                << " The active thread count: " << activate_threads_.size()
                << " The infer_thread_count: " << infer_thread_count_
                << " schedule list size: " << schedule_list_.size();
}

void AnalysisSchedule::SetNextReady() {
  if (schedule_list_.empty()) {
    return;
  }
  // Exit Flag
  if (schedule_list_.front() != nullptr && schedule_list_.front()->thread_id() == kStateStop) {
    run_ = false;
    schedule_list_.pop_front();
    return;
  }
  // Check if enter endless loop
  auto it = std::find_if(schedule_list_.cbegin(), schedule_list_.cend(), [](const auto &item) {
    MS_EXCEPTION_IF_NULL(item);
    return item->HasResult();
  });
  while (it == schedule_list_.end()) {
    if (IntToSize(infer_thread_count_.load()) > schedule_list_.size()) {
      MS_LOG(DEBUG) << "There is some task to be added. Please wait. "
                    << " infer_count: " << infer_thread_count_.load() << " schedule: " << schedule_list_.size();
      return;
    }

    (void)std::for_each(schedule_list_.begin(), schedule_list_.end(),
                        [](const auto &item) { MS_LOG(DEBUG) << "Leave infer thread: " << item->thread_id(); });
    if (enable_waiting_branch_eval()) {
      // Try to set one of possible result in the case of ignore value.
      auto possible_it = std::find_if(schedule_list_.cbegin(), schedule_list_.cend(), [](const auto &item) {
        MS_EXCEPTION_IF_NULL(item);
        return item->SetPossibleResult(true);
      });
      if (possible_it != schedule_list_.end()) {
        MS_EXCEPTION_IF_NULL(*possible_it);
        MS_LOG(DEBUG) << "Try to set one branch result from the other branch, ignore value: true, infer thread: "
                      << (*possible_it)->thread_id() << " , result: " << (*possible_it)->HasResult();
        it = possible_it;
        break;
      }
      // Try to set one of possible result.
      possible_it = std::find_if(schedule_list_.cbegin(), schedule_list_.cend(), [](const auto &item) {
        MS_EXCEPTION_IF_NULL(item);
        return item->SetPossibleResult(false);
      });
      if (possible_it != schedule_list_.end()) {
        MS_EXCEPTION_IF_NULL(*possible_it);
        MS_LOG(DEBUG) << "Try to set one branch result from the other branch, ignore value: false, infer thread: "
                      << (*possible_it)->thread_id() << " , result: " << (*possible_it)->HasResult();
        it = possible_it;
        break;
      }
    }
    MS_EXCEPTION_IF_NULL(schedule_list_.front());
    // Enter endless loop if there is not ready result.
    (void)activate_threads_.insert(schedule_list_.front()->thread_id());
    // Let the first thread to trigger endless loop exception.
    MS_LOG(DEBUG) << "Enter endless loop if there is not ready result.Set the async to trigger exception:"
                  << schedule_list_.front().get() << " The active thread count: " << activate_threads_.size();
    schedule_list_.front()->SetEndLessLoopException();
    schedule_list_.pop_front();
    return;
  }

  auto async_task = *it;
  MS_EXCEPTION_IF_NULL(async_task);
  (void)activate_threads_.insert(async_task->thread_id());
  async_task->SetReady();
  (void)schedule_list_.erase(it);
  MS_LOG(DEBUG) << " Success to SetReady. The active thread count: " << activate_threads_.size()
                << " The infer_thread_count: " << infer_thread_count_
                << " schedule list size: " << schedule_list_.size() << " async: " << async_task->thread_id()
                << "  address: " << async_task.get();
}

AbstractBasePtr AsyncAbstract::GetResult() {
  ClearPossibleResult();
  auto async_task = AsyncInferTask::MakeShared(shared_from_this());
  MS_LOG(DEBUG) << GetInferThread() << " is waiting for async: " << async_task.get();
  AnalysisSchedule::GetInstance().Add2Schedule(async_task);
  auto ret = async_task->GetResult();
  MS_LOG(DEBUG) << GetInferThread() << " success to get async result: " << async_task.get() << " " << ret->ToString();
  return ret;
}
void AsyncAbstract::ClearPossibleResult() {
  std::lock_guard<std::mutex> lock(lock_);
  if (result_ != nullptr && result_->isa<AsyncAbstractFuncAtom>()) {
    result_ = nullptr;
  }
}

bool AsyncAbstract::SetPossibleResult(bool first) {
  std::lock_guard<std::mutex> lock(lock_);
  bool condition = not_copy_from_other_ && switchAbstract_ != nullptr && switchAbstract_->HasResult();
  if (first && condition) {
    condition = switchAbstract_->ignore_value_;
  }
  if (condition) {
    result_ = switchAbstract_->TryGetResult();
    // Set the result with the other branches abstract
    // when there are not available branches to infer.
    // Just copy the type otherwise the two branches would be optimized to a const value.
    MS_EXCEPTION_IF_NULL(result_->BuildValue());
    if (!result_->BuildValue()->isa<ValueAny>()) {
      result_ = AbstractBroaden(result_);
    }
    if (NeedWaitForBranches(result_)) {
      result_ = AsyncAbstractFuncAtom::MakeShared(shared_from_this(), std::vector<size_t>{0});
    }
    not_copy_from_other_ = false;
    return true;
  }
  return false;
}

namespace {
AbstractFunctionPtr GetAbstractFuncRecursively(const AbstractBasePtr &abs, const std::vector<std::size_t> &index,
                                               const std::size_t offset) {
  MS_EXCEPTION_IF_NULL(abs);
  if (abs->isa<AbstractFuncAtom>()) {
    return abs->cast<AbstractFuncAtomPtr>();
  }
  if (abs->isa<AbstractSequence>()) {
    auto abs_seq = abs->cast_ptr<AbstractSequence>();
    MS_EXCEPTION_IF_NULL(abs_seq);
    const auto &elements = abs_seq->elements();
    if (offset >= index.size()) {
      MS_LOG(INTERNAL_EXCEPTION) << "Offset " << offset << " is greater than or equal to vector size: " << index.size();
    }
    if (index[offset] >= elements.size()) {
      MS_LOG(INTERNAL_EXCEPTION) << "At offset" << offset
                                 << ", elements size of AsyncAbstract result: " << abs->ToString()
                                 << " is less than or equal to index: " << index[offset];
    }
    auto resolved = GetAbstractFuncRecursively(elements[index[offset]], index, offset + 1);
    MS_EXCEPTION_IF_NULL(resolved);
    if (!resolved->isa<AbstractFuncAtom>()) {
      MS_LOG(INTERNAL_EXCEPTION) << "AsyncAbstract result cannot be resolved to AbstractFuncAtom, but: "
                                 << resolved->ToString();
    }
    MS_LOG(DEBUG) << "Return abstract: " << resolved->ToString();
    return resolved;
  }
  MS_LOG(INTERNAL_EXCEPTION) << "AsyncAbstract cannot resolved to AbstractFuncAtom or AbstractSeqeunce, but: "
                             << abs->ToString();
}
}  // namespace
bool NeedWaitForBranches(const AbstractBasePtr &abstract) {
  MS_EXCEPTION_IF_NULL(abstract);
  if (abstract->isa<AbstractFunction>()) {
    return true;
  }
  if (abstract->isa<AbstractSequence>()) {
    auto seq = abstract->cast_ptr<AbstractSequence>();
    MS_EXCEPTION_IF_NULL(seq);
    auto elements = seq->elements();
    if (std::any_of(elements.begin(), elements.end(),
                    [](const AbstractBasePtr &item) { return NeedWaitForBranches(item); })) {
      return true;
    }
  }
  return false;
}

AbstractFunctionPtr AsyncAbstractFuncAtom::GetUnique() {
  if (resolved_ != nullptr) {
    return resolved_;
  }
  // Release GIL for C++;
  py::gil_scoped_release infer_gil_release;

  MS_LOG(DEBUG) << "Try to GetResult from async_abstract: " << async_abstract_->ToString();
  const auto &result = async_abstract_->GetResult();
  resolved_ = GetAbstractFuncRecursively(result, index_, 0);
  return resolved_;
}

std::string AsyncAbstractFuncAtom::ToString() const {
  if (resolved_ == nullptr) {
    return "AsyncAbstractFuncAtom(Not Resolved)";
  }

  std::ostringstream buffer;
  buffer << "AsyncAbstractFuncAtom(";
  buffer << resolved_->ToString();
  buffer << ")";

  return buffer.str();
}

void AnalysisResultCacheMgr::Clear() {
  prim_eval_cache_->Clear();
  std::lock_guard<std::mutex> lock(lock_);
  cache_.clear();
  switch_cache_.clear();
  switch_cache_for_check_.clear();
}

void AnalysisResultCacheMgr::InitSwitchValue(const AnfNodeConfigPtr &conf) {
  std::lock_guard<std::mutex> lock(lock_);
  AsyncAbstractPtr async_eval_result = switch_cache_.get(conf);
  if (async_eval_result == nullptr) {
    async_eval_result = std::make_shared<AsyncAbstract>();
    switch_cache_.set(conf, async_eval_result);
  }
}

AbstractBasePtr AnalysisResultCacheMgr::GetSwitchValue(const AnfNodeConfigPtr &conf) {
  // don't call lock_.lock(). switch_cache is protected. and it waits for result.
  AsyncAbstractPtr async_eval_result = switch_cache_.get(conf);
  if (async_eval_result == nullptr) {
    return nullptr;
  }
  return async_eval_result->GetResult();
}

void AnalysisResultCacheMgr::SetCacheValue(const AnfNodeConfigPtr &conf, const AbstractBasePtr &current_abs,
                                           AnalysisConfigAsyncResultCache *cache) {
  MS_EXCEPTION_IF_NULL(conf);
  MS_EXCEPTION_IF_NULL(cache);
  if (current_abs == nullptr) {
    MS_LOG(EXCEPTION) << conf->ToString() << " value is nullptr";
  }
  std::lock_guard<std::mutex> lock(lock_);
  AsyncAbstractPtr async_eval_result = cache->get(conf);
  if (async_eval_result == nullptr) {
    async_eval_result = std::make_shared<AsyncAbstract>();
    async_eval_result->set_result(current_abs);
    cache->set(conf, async_eval_result);
  } else {
    auto previous_abs = async_eval_result->TryGetResult();
    AbstractBasePtrList abstract_list;
    if (previous_abs != nullptr) {
      abstract_list.push_back(previous_abs);
      abstract_list.push_back(current_abs);
      // Join two branches's result
      MS_EXCEPTION_IF_NULL(conf->node());
      MS_LOG(DEBUG) << "Join node: " << conf->node()->DebugString() << ", previous_abs: " << previous_abs->ToString()
                    << ", and current_abs: " << current_abs->ToString();
      auto joined_result = AnalysisEngine::ProcessEvalResults(abstract_list, conf->node());
      async_eval_result->set_result(joined_result->abstract());
    } else {
      async_eval_result->set_result(current_abs);
    }
  }
}

void AnalysisResultCacheMgr::CheckSwitchValueJoinable(const AnfNodeConfigPtr &conf, const AbstractBasePtr &arg) {
  SetCacheValue(conf, arg, &switch_cache_for_check_);
}

void AnalysisResultCacheMgr::SetSwitchValue(const AnfNodeConfigPtr &conf, const AbstractBasePtr &arg) {
  SetCacheValue(conf, arg, &switch_cache_);
}

std::string ArgsToString(const AbstractBasePtrList &args_abs_list) {
  std::ostringstream buffer;
  for (const auto &item : args_abs_list) {
    MS_EXCEPTION_IF_NULL(item);
    MS_EXCEPTION_IF_NULL(item->BuildType());
    MS_EXCEPTION_IF_NULL(item->BuildShape());
    MS_EXCEPTION_IF_NULL(item->BuildValue());
    buffer << " # " << item->BuildType()->ToString() << ", " << item->BuildShape()->ToString() << ", "
           << item->BuildValue()->ToString() << "\n";
  }
  return buffer.str();
}
bool enable_waiting_branch_eval() {
  static const bool enable_waiting_branch_eval = common::GetCompileConfig("NOT_WAIT_BRANCH_EVAL") != "1";
  return enable_waiting_branch_eval;
}
}  // namespace abstract
}  // namespace mindspore
