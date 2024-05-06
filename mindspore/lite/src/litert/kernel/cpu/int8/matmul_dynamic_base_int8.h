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

#ifndef MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_INT8_MATMUL_DYNAMIC_BASE_INT8_H_
#define MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_INT8_MATMUL_DYNAMIC_BASE_INT8_H_

#include <vector>
#include <algorithm>
#include "include/errorcode.h"
#include "src/litert/lite_kernel.h"
#include "nnacl/matmul_parameter.h"
#include "nnacl/common_func.h"
#include "nnacl/int8/quantize.h"
#include "nnacl/int8/common_func_int8.h"
#include "src/common/common.h"

namespace mindspore::kernel {
class MatmulDynamicBaseInt8CPUKernel : public LiteKernel {
 public:
  MatmulDynamicBaseInt8CPUKernel(OpParameter *parameter, const std::vector<lite::Tensor *> &inputs,
                                 const std::vector<lite::Tensor *> &outputs, const lite::InnerContext *ctx)
      : LiteKernel(parameter, inputs, outputs, ctx) {
    param_ = reinterpret_cast<MatMulParameter *>(op_parameter_);
    param_->matmul_type_ = MatmulType::kNotImplemented;
  }
  ~MatmulDynamicBaseInt8CPUKernel() override;
  int Prepare() override;
  int ReSize() override;
  static int InitBroadcastParams(const std::vector<int> &a_shape_const, const std::vector<int> &b_shape_const,
                                 MatMulParameter *params, std::vector<int> *a_offsets, std::vector<int> *b_offsets);

  const int8_t *GetPackBPtr() const { return pack_b_ptr_; }
  const int *GetWeightSums() const { return weight_sums_; }
  const int GetBBatch() const { return b_batch_; }
  int PreparePackedWeight(const lite::Tensor *tensor) override;

 private:
  void ResizeMatrixBParameter();
  int CopyBias();
  int InitMatrixBBuffer();
  int MallocQuantParam();

 protected:
  int a_batch_ = 1;
  int b_batch_ = 1;
  std::vector<int> a_offset_;
  std::vector<int> b_offset_;
  int a_quant_offset_ = 0;
  int b_quant_offset_ = 0;
  typedef void (*PackFunc)(const int8_t *src, int8_t *dst, int row, int col);
  virtual void InitParameter() = 0;
  int TransferA();
  int InitInputQuantParam(std::vector<float> *scales, std::vector<int32_t> *zp);
  int InitFilterQuantParam();
  int TransferB();
  void FreeTmpBuffer();
  void FreeQuantParam();
  int InitMatrixABuffer();
  void FreeMatrixABuffer();

  MatMulParameter *param_ = nullptr;
  MatmulDynamicQuantParameter *quant_param_ = nullptr;
  int8_t *pack_a_ptr_ = nullptr;
  int8_t *pack_b_ptr_ = nullptr;

  bool input_per_channel_ = false;
  bool input_per_batch_channel_ = false;
  bool filter_per_channel_ = false;
  bool filter_per_batch_channel_ = false;
  int8_t *batch_input_ptr_ = nullptr;
  int8_t *batch_weight_ptr_ = nullptr;
  int8_t *batch_a_ptr_ = nullptr;
  int8_t *batch_b_ptr_ = nullptr;
  void *bias_ptr_ = nullptr;
  void *batch_c_ptr_ = nullptr;
  int *input_sums_ = nullptr;
  int *weight_sums_ = nullptr;
  int row_tile_ = C4NUM;
  int col_tile_ = C4NUM;
  int deep_tile_ = C16NUM;
  int thread_stride_ = 0;
  bool enable_fp16_ = false;
  PackFunc b_pack_func_ = nullptr;
  bool weight_is_packed_ = false;
  const lite::Tensor *weight_sums_tensor_ = nullptr;
};
}  // namespace mindspore::kernel

#endif  // MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_INT8_MATMUL_DYNAMIC_BASE_INT8_H_
