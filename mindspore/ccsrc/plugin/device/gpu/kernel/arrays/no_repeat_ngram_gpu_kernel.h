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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_ARRAYS_NO_REPEAT_NGRAM_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_ARRAYS_NO_REPEAT_NGRAM_GPU_KERNEL_H_

#include <vector>
#include <string>
#include <utility>
#include <map>
#include "mindspore/core/ops/no_repeat_ngram.h"
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class NoRepeatNGramGpuKernelMode : public NativeGpuKernelMod {
 public:
  NoRepeatNGramGpuKernelMode() {}
  ~NoRepeatNGramGpuKernelMode() override = default;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
              const std::vector<KernelTensor *> &outputs, void *cuda_stream) override {
    cuda_stream_ = cuda_stream;
    return kernel_func_(this, inputs, outputs);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename StateType, typename LogProbType>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);
  using NoRepeatNGramFunc =
    std::function<bool(NoRepeatNGramGpuKernelMode *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &)>;

 private:
  int64_t ngram_{1};
  size_t batch_size_{1};
  size_t beam_size_{1};
  size_t seq_len_{1};
  size_t state_size_{1};
  size_t vocab_size_{1};
  size_t logit_size_{1};
  std::vector<int64_t> state_seq_shape_;
  std::vector<int64_t> log_probs_shape_;
  void *cuda_stream_{nullptr};
  NoRepeatNGramFunc kernel_func_{};
  static std::vector<std::pair<KernelAttr, NoRepeatNGramFunc>> func_list_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_ARRAYS_NO_REPEAT_NGRAM_GPU_KERNEL_H_
