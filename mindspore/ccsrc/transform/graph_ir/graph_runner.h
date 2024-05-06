/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_GRAPH_RUNNER_H_
#define MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_GRAPH_RUNNER_H_

#include <vector>
#include <memory>
#include <string>

#include "transform/graph_ir/transform_util.h"
#include "transform/graph_ir/df_graph_manager.h"
#include "ir/tensor.h"

namespace mindspore {
namespace transform {
class GraphRunner {
 public:
  explicit GraphRunner(const GraphRunnerOptions &options);
  ~GraphRunner() { sess_ = nullptr; }
  Status AddGraph(const std::string &name);
  Status RunGraph(const RunOptions &options, const std::vector<MeTensorPtr> &inputs, std::vector<MeTensorPtr> *outputs);
  Status RunGraph(const RunOptions &options, const std::vector<GeTensorPtr> &inputs, std::vector<GeTensorPtr> *outputs);
  Status RunGraphAsync(const RunOptions &options, const std::vector<GeTensorPtr> &inputs,
                       std::vector<GeTensorPtr> *outputs);
  Status RunGraphWithStreamAsync(const RunOptions &options, void *stream, const std::vector<GeTensor> &inputs,
                                 std::vector<GeTensor> *outputs);
  Status CompileGraph(const RunOptions &options, ::ge::CompiledGraphSummaryPtr *graph_summary);
  Status SetConstMemory(const RunOptions &options, const void *const memory, size_t size);
  Status UpdateFeatureMemory(const RunOptions &options, const void *const memory, size_t size);
  static std::shared_ptr<::ge::Session> NewSession(const SessionOptions &sess_options);
  Status RegisterExternalAllocator(const void *const stream, GeAllocatorPtr allocator);
  Status UnregisterExternalAllocator(const void *const stream);
  const bool IsAllocatorRegistered() const { return is_allocator_registered; }

 private:
  Status GetWrapper(const std::string &name, DfGraphWrapperPtr *wrapper) const;

  std::shared_ptr<::ge::Session> sess_;
  transform::GraphRunnerOptions options_;
  DfGraphManager &graph_manager_;
  bool is_allocator_registered = false;
};
}  // namespace transform
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_TRANSFORM_GRAPH_IR_GRAPH_RUNNER_H_
