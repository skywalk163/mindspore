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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_NORMALIZE_TUPLE_INDEX_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_NORMALIZE_TUPLE_INDEX_CPU_KERNEL_H_
#include <vector>
#include <map>
#include <utility>
#include <string>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class NormalizeTupleIndexCpuKernelMod : public NativeCpuKernelMod {
 public:
  NormalizeTupleIndexCpuKernelMod() = default;
  ~NormalizeTupleIndexCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs);

 protected:
  std::vector<KernelAttr> GetOpSupport() override;
  bool IsNeedUpdateOutputShapeAndSize() override { return true; }
  void UpdateOutputShapeAndSize(const std::vector<KernelTensor *> &inputs,
                                const std::vector<KernelTensor *> &outputs) override {
    if (output_sizes_.empty()) {
      // scalar
      outputs[0]->SetShapeVector({});
      outputs[0]->set_size(UnitSizeInBytes(outputs[0]->dtype_id()));
    } else {
      outputs[0]->SetShapeVector({SizeToLong(output_sizes_[0])});
      outputs[0]->set_size(output_sizes_[0] * UnitSizeInBytes(outputs[0]->dtype_id()));
    }
  }

  using NormalizeTupleIndexFunc =
    std::function<bool(NormalizeTupleIndexCpuKernelMod *, const std::vector<KernelTensor *> &,
                       const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &)>;

  static std::vector<std::pair<KernelAttr, NormalizeTupleIndexFunc>> func_list_;
  NormalizeTupleIndexFunc kernel_func_;

 private:
  template <typename T>
  void NormalizeIntIndex(const ShapeVector &data_shape, int64_t *output_addr, const T *index_val_addr,
                         size_t dim_index);
  template <typename T>
  void NormalizeSequenceIndex(const ShapeVector &data_shape, int64_t *output_addr, const T *index_val_addr,
                              size_t seq_size, size_t dim_index);
  template <typename T>
  void NormalizeBoolSequenceIndex(const ShapeVector &data_shape, int64_t *output_addr, const T *index_val_addr,
                                  size_t seq_size, size_t dim_index);

  void NormalizeNoneIndex(int64_t *output_addr, const ShapeVector &data_shape, size_t dim_index);
  void NormalizeEllipsisIndex(int64_t *output_addr, const ShapeVector &data_shape, size_t dim_index);
  string index_types_;
  std::vector<size_t> output_sizes_;
  std::vector<int64_t> tuple_index_types_;
  std::vector<std::vector<int64_t>> data_shapes_;
  size_t dim_index_ = 0;
  size_t expand_dims_mask_ = 0;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_NORMALIZE_TUPLE_INDEX_CPU_KERNEL_H_
