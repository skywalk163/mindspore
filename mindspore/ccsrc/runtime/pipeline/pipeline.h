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
#ifndef MINDSPORE_MINDSPORE_CCSRC_RUNTIME_PIPELINE_PIPELINE_H_
#define MINDSPORE_MINDSPORE_CCSRC_RUNTIME_PIPELINE_PIPELINE_H_

#include "utils/ms_utils.h"
#include "include/backend/visible.h"
#include "runtime/pipeline/async_rqueue.h"

namespace mindspore {
namespace runtime {
class BACKEND_EXPORT Pipeline {
 public:
  static Pipeline &Get();

  const AsyncRQueuePtr &frontend_stage() const { return frontend_stage_; }
  const AsyncRQueuePtr &backend_stage() const { return backend_stage_; }

 private:
  Pipeline();
  ~Pipeline() = default;
  DISABLE_COPY_AND_ASSIGN(Pipeline);

  // Infer and create output tensor.
  AsyncRQueuePtr frontend_stage_;
  // Malloc and launch kernels.
  AsyncRQueuePtr backend_stage_;
};
}  // namespace runtime
}  // namespace mindspore
#endif  // MINDSPORE_MINDSPORE_CCSRC_RUNTIME_PIPELINE_PIPELINE_H_
