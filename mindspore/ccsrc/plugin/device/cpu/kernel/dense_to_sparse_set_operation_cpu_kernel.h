/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_DENSE_TO_SPARSE_SET_OPERATION_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_DENSE_TO_SPARSE_SET_OPERATION_H_

#include <vector>
#include <memory>
#include <set>
#include <map>
#include <string>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "utils/ms_utils.h"

namespace mindspore {
namespace kernel {
enum SetOperation { A_MINUS_B = 0, B_MINUS_A = 1, INTERSECTION = 2, UNION = 3 };
class DenseToSparseSetOperationCpuKernelMod : public NativeCpuKernelMod {
 public:
  DenseToSparseSetOperationCpuKernelMod() = default;
  ~DenseToSparseSetOperationCpuKernelMod() = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override;
  bool IsNeedUpdateOutputShapeAndSize() override { return true; }
  void UpdateOutputShapeAndSize(const std::vector<KernelTensor *> &inputs,
                                const std::vector<KernelTensor *> &outputs) override;

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &outputs);
  template <typename T>
  bool OutputSparseTensor(const std::vector<kernel::KernelTensor *> &inputs,
                          const std::vector<kernel::KernelTensor *> &outputs, ShapeVector *output_shape,
                          const int64_t num_values, const std::map<ShapeVector, std::set<T>> &sets);
  template <typename T>
  void ApplySetOperation(const std::set<T> &set1, const std::set<T> &set2, std::set<T> *result);

  SetOperation set_operation_ = A_MINUS_B;
  bool validate_indices_ = true;
  CNodePtr node_ptr;
  ShapeVector shape1_;
  int64_t set2_nums_;
  int64_t set2_dim_;
  std::vector<ShapeVector> infer_shape_ = {};
  TypeId data_type_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_DENSE_TO_SPARSE_SET_OPERATION_H_
