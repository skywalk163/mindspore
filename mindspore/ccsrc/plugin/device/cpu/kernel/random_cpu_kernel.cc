/**
 * Copyright 2020-2023 Huawei Technologies Co., Ltd
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
#include "plugin/device/cpu/kernel/random_cpu_kernel.h"
#include <thread>
#include <memory>
#include <algorithm>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <wincrypt.h>
#endif
#include "mindspore/core/ops/random_ops.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "kernel/philox_random.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kUniformIntInputsNum = 3;
constexpr size_t kUniformRealInputsNum = 1;
constexpr size_t kUniformIntOutputsNum = 1;
constexpr size_t kUniformRealOutputsNum = 1;
constexpr size_t kStandardNormalOutputsNum = 1;
constexpr char kKernelName[] = "Random";
}  // namespace

void LaunchStandardNormal(std::default_random_engine *rng, const std::vector<KernelTensor *> &outputs) {
  // Init output address.
  auto output = reinterpret_cast<float *>(outputs[0]->device_ptr());

  // Init sample number.
  size_t num_sample = outputs[0]->size() / sizeof(float);

  // Init random normal generator.
  std::normal_distribution<float> distribution;

  // Generate random normal values.
  for (size_t i = 0; i < num_sample; ++i) {
    output[i] = distribution(*rng);
  }
}

void LaunchUniformInt(std::mt19937 *rng, const std::vector<KernelTensor *> &inputs,
                      const std::vector<KernelTensor *> &outputs) {
  // Init min/max values.
  int min_val = reinterpret_cast<int *>(inputs[1]->device_ptr())[0];
  int max_val = reinterpret_cast<int *>(inputs[2]->device_ptr())[0];
  if (max_val <= min_val) {
    MS_LOG(EXCEPTION) << "For '" << kKernelName << "', invalid min/max values: (" << min_val << "/" << max_val << ")";
  }

  // Init output address.
  auto output = reinterpret_cast<int *>(outputs[0]->device_ptr());

  // Init sample number.
  size_t num_sample = outputs[0]->size() / sizeof(int);

  // Init random int generator.
  std::uniform_int_distribution<> distrib(min_val, max_val - 1);

  // Generate random int values.
  for (size_t i = 0; i < num_sample; ++i) {
    output[i] = distrib(*rng);
  }
}

void LaunchUniformReal(std::mt19937 *rng, const std::vector<KernelTensor *> &outputs) {
  // Init output address.
  auto output = reinterpret_cast<float *>(outputs[0]->device_ptr());

  // Init sample number.
  size_t num_sample = outputs[0]->size() / sizeof(int);

  // Init random real generator.
  std::uniform_real_distribution<> distrib(0.0, 1.0);

  // Generate random real values.
  for (size_t i = 0; i < num_sample; ++i) {
    output[i] = static_cast<float>(distrib(*rng));
  }
}

bool RandomCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  auto iter = kRandomOpTypeMap.find(kernel_type_);
  if (iter == kRandomOpTypeMap.end()) {
    MS_LOG(EXCEPTION) << "For '" << kernel_type_
                      << ", only support these types: StandardNormal, UniformInt or UniformReal currently, but got "
                      << kernel_type_;
  } else {
    random_op_type_ = iter->second;
  }
  uint64_t seed = static_cast<uint64_t>(GetValue<int64_t>(primitive_->GetAttr("seed")));
  uint64_t seed2 = static_cast<uint64_t>(GetValue<int64_t>(primitive_->GetAttr("seed2")));
  uint64_t init_seed = random::GetSeed(seed, seed2);
  mtrng_.seed(init_seed);
  dfrng_.seed(init_seed);
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto res = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!res.first) {
    MS_LOG(ERROR) << "For '" << kernel_type_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }
  return true;
}

int RandomCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  return KRET_OK;
}

bool RandomCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                const std::vector<kernel::KernelTensor *> &workspace,
                                const std::vector<kernel::KernelTensor *> &outputs) {
  if (random_op_type_ == RANDOM_OP_NORMAL) {
    CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kStandardNormalOutputsNum, kernel_type_);
    LaunchStandardNormal(&dfrng_, outputs);
  } else if (random_op_type_ == RANDOM_OP_UNIFORM_INT) {
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), kUniformIntInputsNum, kernel_type_);
    CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kUniformIntOutputsNum, kernel_type_);
    LaunchUniformInt(&mtrng_, inputs, outputs);
  } else if (random_op_type_ == RANDOM_OP_UNIFORM_REAL) {
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), kUniformRealInputsNum, kernel_type_);
    CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kUniformRealOutputsNum, kernel_type_);
    LaunchUniformReal(&mtrng_, outputs);
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_type_
                      << ", only support these types: StandardNormal, UniformInt or UniformReal currently, but got "
                      << random_op_type_;
  }
  return true;
}

std::vector<KernelAttr> RandomCpuKernelMod::GetOpSupport() {
  static std::map<std::string, std::vector<KernelAttr>> support_list_map = {
    {kStandardNormal,
     {KernelAttr().AddInputAttr(kObjectTypeTuple, kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32),
      KernelAttr().AddInputAttr(kObjectTypeTuple, kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32)}},
    {kUniformInt,
     {KernelAttr()
        .AddInputAttr(kNumberTypeInt32)
        .AddInputAttr(kNumberTypeInt32)
        .AddInputAttr(kNumberTypeInt32)
        .AddOutputAttr(kNumberTypeInt32),
      KernelAttr()
        .AddInputAttr(kNumberTypeInt64)
        .AddInputAttr(kNumberTypeInt32)
        .AddInputAttr(kNumberTypeInt32)
        .AddOutputAttr(kNumberTypeInt32),
      KernelAttr()
        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt32)
        .AddInputAttr(kNumberTypeInt32)
        .AddInputAttr(kNumberTypeInt32)
        .AddOutputAttr(kNumberTypeInt32),
      KernelAttr()
        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
        .AddInputAttr(kNumberTypeInt32)
        .AddInputAttr(kNumberTypeInt32)
        .AddOutputAttr(kNumberTypeInt32)}},
    {kUniformReal,
     {
       KernelAttr().AddInputAttr(kObjectTypeTuple, kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32),
       KernelAttr().AddInputAttr(kObjectTypeTuple, kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
       KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32),
       KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
     }}};
  auto iter = support_list_map.find(kernel_type_);
  if (iter == support_list_map.end()) {
    MS_LOG(EXCEPTION) << "Does not support " << kernel_type_ << "!";
  }
  return iter->second;
}
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, StandardNormal,
                                 []() { return std::make_shared<RandomCpuKernelMod>(kStandardNormal); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, UniformInt,
                                 []() { return std::make_shared<RandomCpuKernelMod>(kUniformInt); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, UniformReal,
                                 []() { return std::make_shared<RandomCpuKernelMod>(kUniformReal); });
}  // namespace kernel
}  // namespace mindspore
