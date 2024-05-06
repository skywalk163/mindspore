/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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
#include "extendrt/graph_executor/mindrt_graph_executor.h"

#include <algorithm>

#include "extendrt/graph_executor/factory.h"
#include "litert/mindrt_executor.h"
#include "extendrt/execution_plan.h"

namespace mindspore {
MindRTGraphExecutor::MindRTGraphExecutor() {
  name_ = "MindRTGraphExecutor";
  execution_plan_ = nullptr;
}

MindRTGraphExecutor::MindRTGraphExecutor(const std::string &name,
                                         std::shared_ptr<infer::abstract::ExecutionPlan> execution_plan) {
  name_ = name;
  execution_plan_ = execution_plan;
}

bool MindRTGraphExecutor::Init() {
  auto infer_execution_plan = std::dynamic_pointer_cast<infer::ExecutionPlan>(execution_plan_);
  if (infer_execution_plan == nullptr) {
    MS_LOG(ERROR) << "Not Supported execution plan is passed";
    return false;
  }
  if (!infer_execution_plan->PrepareKernels()) {
    MS_LOG(ERROR) << "Prepare kernels failed";
    return false;
  }

  mindrt_executor_ = std::make_shared<mindspore::lite::MindrtExecutor>(infer_execution_plan->GetOutputsMap(),
                                                                       infer_execution_plan->GetInputsMap());
  if (mindrt_executor_ == nullptr) {
    MS_LOG(ERROR) << "Create mindrt executor failed";
    return false;
  }

  inited_ = true;

  return true;
}

Status MindRTGraphExecutor::Prepare() {
  if (!Init()) {
    MS_LOG(ERROR) << "Init executor failed";
    return kLiteError;
  }

  MS_ASSERT(inited_ == true);
  MS_ASSERT(mindrt_executor_ != nullptr);
  MS_ASSERT(execution_plan_ != nullptr);

  auto ret = mindrt_executor_->Prepare(execution_plan_->ToKernelList(), execution_plan_->GetInputs(),
                                       execution_plan_->GetOutputs(), execution_plan_->GetContext().get());
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Prepare execution plan failed with code " << ret;
    return kLiteError;
  }
  return kSuccess;
}

Status MindRTGraphExecutor::Execute() {
  if (!inited_) {
    MS_LOG(ERROR) << "Executor is not inited correctly";
    return kLiteError;
  }
  MS_ASSERT(mindrt_executor_ != nullptr);
  MS_ASSERT(execution_plan_ != nullptr);

  auto ret =
    mindrt_executor_->Run(execution_plan_->GetInputs(), execution_plan_->GetOutputs(), execution_plan_->ToKernelList(),
                          execution_plan_->GetKernelBeforeCallBack(), execution_plan_->GetKernelAfterCallBack());
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Run execution plan failed with code " << ret;
    return kLiteError;
  }
  return kSuccess;
}

int MindRTGraphExecutor::Resize(const std::vector<infer::abstract::Tensor *> &inputs,
                                const std::vector<std::vector<int64_t>> &dims) {
  if (!inited_) {
    MS_LOG(ERROR) << "Executor is not inited correctly";
    return kLiteError;
  }
  MS_ASSERT(mindrt_executor_ != nullptr);

  std::vector<std::vector<int>> dims32;
  std::transform(dims.begin(), dims.end(), std::back_inserter(dims32), [](std::vector<int64_t> shape) {
    std::vector<int> shape32;
    std::transform(shape.begin(), shape.end(), std::back_inserter(shape32),
                   [](int64_t dim) { return static_cast<int>(dim); });
    return shape32;
  });
  return mindrt_executor_->Resize(inputs, dims32);
}

static std::shared_ptr<infer::abstract::Executor> MindRTGraphExecutorCreator(
  const std::string &name, std::shared_ptr<infer::abstract::ExecutionPlan> execution_plan) {
  auto graph_executor = std::make_shared<MindRTGraphExecutor>(name, execution_plan);
  return graph_executor;
}
REG_GRAPH_EXECUTOR(kMindRTExecutor, MindRTGraphExecutorCreator);
}  // namespace mindspore
