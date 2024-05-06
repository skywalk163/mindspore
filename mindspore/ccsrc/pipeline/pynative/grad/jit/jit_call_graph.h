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

#ifndef MINDSPORE_MINDSPORE_CCSRC_PIPELINE_PYNATIVE_GRAD_JIT_JIT_CALL_GRAPH_H_
#define MINDSPORE_MINDSPORE_CCSRC_PIPELINE_PYNATIVE_GRAD_JIT_JIT_CALL_GRAPH_H_

#include <functional>
#include <utility>
#include "runtime/pipeline/task/task.h"
#include "pipeline/pynative/base.h"

namespace mindspore {
namespace pynative {
class BACKEND_EXPORT JitCallGraph {
 public:
  explicit JitCallGraph(std::function<VectorRef(const VectorRef &arg_list)> call_back_func)
      : call_back_func_(std::move(call_back_func)) {}
  ~JitCallGraph() = default;
  VectorRef Run(const VectorRef &arg_list) { return call_back_func_(arg_list); }

  // Key for user data.
  constexpr static char key[] = "JitCallGraph";

 private:
  std::function<VectorRef(const VectorRef &arg_list)> call_back_func_;
};
}  // namespace pynative
}  // namespace mindspore
#endif  // MINDSPORE_MINDSPORE_CCSRC_PIPELINE_PYNATIVE_GRAD_JIT_JIT_CALL_GRAPH_H_
