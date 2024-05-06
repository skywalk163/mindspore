/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_EIGEN_GAMMA_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_EIGEN_GAMMA_CPU_KERNEL_H_
#define EIGEN_USE_THREADS
#define EIGEN_USE_SIMPLE_THREAD_POOL

#include <vector>
#include <string>
#include <map>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "mindspore/ccsrc/plugin/device/cpu/kernel/random_util.h"
#include "kernel/philox_random.h"
#include "mindspore/core/ops/random_gamma.h"

namespace mindspore {
namespace kernel {
class GammaCpuKernelMod : public NativeCpuKernelMod {
 public:
  GammaCpuKernelMod() = default;
  ~GammaCpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  void Generate(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);
  template <typename T>
  void InferShape(const std::vector<KernelTensor *> &inputs);
  int64_t seed_{0};
  int64_t seed2_{0};

  ShapeVector output_shape_;
  ShapeVector shape_shape_;
  ShapeVector alpha_shape_;
  TypeId shape_dtype_{kTypeUnknown};
  TypeId alpha_dtype_{kTypeUnknown};

  random::GuardedPhiloxRandom rng_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_EIGEN_GAMMA_CPU_KERNEL_H_
