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

#ifndef MINDSPORE_LITE_SRC_EXTENDRT_EXECUTION_PLAN_H_
#define MINDSPORE_LITE_SRC_EXTENDRT_EXECUTION_PLAN_H_

#include <memory>
#include <utility>
#include <vector>
#include <unordered_map>

#include "infer/execution_plan.h"
#include "src/executor/sub_graph_kernel.h"

namespace mindspore::infer {
/**
 * ExecutionPlan: Execution plan for cloud infer
 */
class ExecutionPlan : public abstract::ExecutionPlan {
 public:
  ExecutionPlan() = default;
  ~ExecutionPlan() override;

  std::vector<abstract::Kernel *> GetKernels() override { return kernels_; }

  void SetKernels(std::vector<abstract::Kernel *> kernels) { this->kernels_ = std::move(kernels); }

  void AddKernel(abstract::Kernel *kernel) override { this->kernels_.emplace_back(kernel); }

  FuncGraphPtr GetFuncGraph() override { return func_graph_; }

  void SetFuncGraph(FuncGraphPtr func_graph) override { func_graph_ = func_graph; }

  std::vector<abstract::Tensor *> GetInputs() override { return inputs_; }

  void SetInputs(const std::vector<abstract::Tensor *> &inputs) override { inputs_ = inputs; }

  std::vector<abstract::Tensor *> GetOutputs() override { return outputs_; }

  void SetOutputs(const std::vector<abstract::Tensor *> &outputs) override { outputs_ = outputs; }

  std::shared_ptr<abstract::Context> GetContext() override { return context_; }

  void SetContext(std::shared_ptr<abstract::Context> context) override { context_ = context; }

  const abstract::KernelCallBack &GetKernelBeforeCallBack() override { return before_; }

  void SetKernelBeforeCallBack(const abstract::KernelCallBack &callback) override { before_ = callback; }

  const abstract::KernelCallBack &GetKernelAfterCallBack() override { return after_; }

  void SetKernelAfterCallBack(const abstract::KernelCallBack &callback) override { after_ = callback; }

  void SetInputsMap(std::unordered_map<abstract::Tensor *, abstract::Tensor *> *input_isolate_map) {
    delete input_isolate_map_;
    input_isolate_map_ = input_isolate_map;
  }

  std::unordered_map<abstract::Tensor *, abstract::Tensor *> *GetInputsMap() { return input_isolate_map_; }

  void SetOutputsMap(std::unordered_map<abstract::Tensor *, abstract::Tensor *> *output_isolate_map) {
    delete output_isolate_map_;
    output_isolate_map_ = output_isolate_map;
  }

  std::unordered_map<abstract::Tensor *, abstract::Tensor *> *GetOutputsMap() { return output_isolate_map_; }

  std::vector<abstract::Kernel *> ToKernelList() override;

  bool PrepareKernels();

 private:
  bool CalcTensorRefCount(abstract::Kernel *kernel);

 private:
  // compiled result kernels
  std::vector<abstract::Kernel *> kernels_;

  // runtime kernel for execution
  std::vector<abstract::Kernel *> kernel_list_;

  FuncGraphPtr func_graph_;
  std::vector<abstract::Tensor *> inputs_;
  std::vector<abstract::Tensor *> outputs_;
  std::shared_ptr<abstract::Context> context_;

  // kernel callback pointers
  abstract::KernelCallBack before_;
  abstract::KernelCallBack after_;

  // multi graph tensor mapping, used for sub graph parallelize execution
  std::unordered_map<abstract::Tensor *, abstract::Tensor *> *input_isolate_map_ = nullptr;
  std::unordered_map<abstract::Tensor *, abstract::Tensor *> *output_isolate_map_ = nullptr;
};
}  // namespace mindspore::infer

#endif  // MINDSPORE_LITE_SRC_EXTENDRT_EXECUTION_PLAN_H_
