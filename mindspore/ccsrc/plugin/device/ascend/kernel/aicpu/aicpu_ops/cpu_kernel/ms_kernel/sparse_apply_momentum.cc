/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "cpu_kernel/ms_kernel/sparse_apply_momentum.h"

#include <map>
#include <memory>
#include <string>
#include <securec.h>

#include "inc/kernel_log.h"
#include "context/inc/cpu_kernel_utils.h"
#include "cpu_types.h"
#include "frontend/parallel/status.h"
#include "unsupported/Eigen/CXX11/Tensor"
#include "utils/eigen_tensor.h"
#include "utils/kernel_util.h"

namespace {
const int32_t kInputNum = 6;
const int32_t kOutputNum = 1;
const char *kSparseApplyMomentum = "SparseApplyMomentum";
#define DO_COMPUTE_CASE(DTYPE, TYPE, ITYPE, CTX)                                \
  case (DTYPE): {                                                               \
    uint32_t ret = KERNEL_STATUS_OK;                                            \
    if ((ITYPE) == DT_INT32) {                                                  \
      ret = DoCompute<TYPE, int32_t>(CTX);                                      \
    } else {                                                                    \
      ret = DoCompute<TYPE, int64_t>(CTX);                                      \
    }                                                                           \
    if (ret != KERNEL_STATUS_OK) {                                              \
      CUST_KERNEL_LOG_ERROR(ctx, "SparseApplyMomentum kernel compute failed."); \
      return ret;                                                               \
    }                                                                           \
    break;                                                                      \
  }
}  // namespace

namespace aicpu {
uint32_t SparseApplyMomentumCpuKernel::ValidParam(CpuKernelContext &ctx) {
  Tensor *var_tensor = ctx.Input(0);
  Tensor *accum_tensor = ctx.Input(1);
  Tensor *lr_tensor = ctx.Input(2);
  Tensor *grad_tensor = ctx.Input(3);
  Tensor *indices_tensor = ctx.Input(4);
  Tensor *momentum_tensor = ctx.Input(5);
  Tensor *output_tensor = ctx.Output(0);

  auto var_shape = var_tensor->GetTensorShape();
  auto accum_shape = accum_tensor->GetTensorShape();
  auto lr_shape = lr_tensor->GetTensorShape();
  auto grad_shape = grad_tensor->GetTensorShape();
  auto indices_shape = indices_tensor->GetTensorShape();
  auto momentum_shape = momentum_tensor->GetTensorShape();
  auto output_shape = output_tensor->GetTensorShape();

  std::map<std::string, Tensor *> tensor_types;
  tensor_types.insert({"lr", lr_tensor});
  tensor_types.insert({"grad", grad_tensor});
  tensor_types.insert({"momentum", momentum_tensor});
  tensor_types.insert({"output var", output_tensor});
  for (auto iter = tensor_types.begin(); iter != tensor_types.end(); iter++) {
    CUST_KERNEL_CHECK_FALSE(ctx, var_tensor->GetDataType() == iter->second->GetDataType(), KERNEL_STATUS_PARAM_INVALID,
                            "The data type of %s [%s] need be same with var [%s].", iter->first.c_str(),
                            DTypeStr(iter->second->GetDataType()).c_str(), DTypeStr(var_tensor->GetDataType()).c_str());
  }

  std::map<std::string, std::shared_ptr<TensorShape>> tensor_shapes;
  tensor_shapes.insert({"accum", accum_shape});
  tensor_shapes.insert({"grad", grad_shape});
  tensor_shapes.insert({"output var", output_shape});
  for (auto iter = tensor_shapes.begin(); iter != tensor_shapes.end(); iter++) {
    CUST_KERNEL_CHECK_FALSE(ctx, var_shape->GetDimSizes() == iter->second->GetDimSizes(), KERNEL_STATUS_PARAM_INVALID,
                            "The %s shape size should be same as the input var shape size.", iter->first.c_str());
  }

  std::map<std::string, std::shared_ptr<TensorShape>> scalar_shapes;
  scalar_shapes.insert({"lr", lr_shape});
  scalar_shapes.insert({"momentum", momentum_shape});
  for (auto iter = scalar_shapes.begin(); iter != scalar_shapes.end(); iter++) {
    CUST_KERNEL_CHECK_FALSE(ctx, iter->second->GetDims() <= 1, KERNEL_STATUS_PARAM_INVALID,
                            "The input %s should be a scalar, got dim size [%d].", iter->first.c_str(),
                            iter->second->GetDims());
  }

  CUST_KERNEL_CHECK_FALSE(ctx, var_shape->GetDims() >= 1, KERNEL_STATUS_PARAM_INVALID,
                          "The input var must be at least 1 dimensional, got dims [%d].", var_shape->GetDims());

  CUST_KERNEL_CHECK_FALSE(ctx, indices_shape->GetDims() == 1, KERNEL_STATUS_PARAM_INVALID,
                          "The input indices must be one-dimensional, but got dims [%d].", indices_shape->GetDims());

  CUST_KERNEL_CHECK_FALSE(ctx, grad_shape->GetDimSize(0) == indices_shape->GetDimSize(0), KERNEL_STATUS_PARAM_INVALID,
                          "The input grad must be the same size as indices in the first dimension.");

  AttrValue *use_nesterov = ctx.GetAttr("use_nesterov");
  use_nesterov_ = (use_nesterov == nullptr) ? false : (use_nesterov->GetBool());

  return KERNEL_STATUS_OK;
}

uint32_t SparseApplyMomentumCpuKernel::Compute(CpuKernelContext &ctx) {
  CUST_KERNEL_HANDLE_ERROR(ctx, NormalCheck(ctx, kInputNum, kOutputNum),
                           "SparseApplyMomentum check input or output is failed.");
  CUST_KERNEL_HANDLE_ERROR(ctx, ValidParam(ctx), "[%s] check params failed.", kSparseApplyMomentum);
  auto data_type = ctx.Input(0)->GetDataType();
  auto data_type_indices = ctx.Input(4)->GetDataType();
  CUST_KERNEL_CHECK_FALSE(ctx, (data_type_indices == DT_INT32 || data_type_indices == DT_INT64),
                          KERNEL_STATUS_PARAM_INVALID, "indices data type[%s] is unsupported",
                          DTypeStr(data_type_indices).c_str());
  switch (data_type) {
    DO_COMPUTE_CASE(DT_FLOAT16, Eigen::half, data_type_indices, ctx);
    DO_COMPUTE_CASE(DT_FLOAT, float, data_type_indices, ctx);
    DO_COMPUTE_CASE(DT_DOUBLE, double, data_type_indices, ctx);
    DO_COMPUTE_CASE(DT_INT8, int8_t, data_type_indices, ctx);
    DO_COMPUTE_CASE(DT_INT16, int16_t, data_type_indices, ctx);
    DO_COMPUTE_CASE(DT_INT32, int32_t, data_type_indices, ctx);
    DO_COMPUTE_CASE(DT_INT64, int64_t, data_type_indices, ctx);
    DO_COMPUTE_CASE(DT_UINT8, uint8_t, data_type_indices, ctx);
    DO_COMPUTE_CASE(DT_UINT16, uint16_t, data_type_indices, ctx);
    DO_COMPUTE_CASE(DT_UINT32, uint32_t, data_type_indices, ctx);
    DO_COMPUTE_CASE(DT_UINT64, uint64_t, data_type_indices, ctx);
    default:
      CUST_KERNEL_LOG_ERROR(ctx, "SparseApplyMomentum kernel data type[%s] not support.", DTypeStr(data_type).c_str());
      return KERNEL_STATUS_PARAM_INVALID;
  }
  return KERNEL_STATUS_OK;
}

template <typename T, typename TI>
uint32_t SparseApplyMomentumCpuKernel::DoCompute(CpuKernelContext &ctx) {
  Tensor *var = ctx.Input(0);
  auto var_shape = var->GetTensorShape();

  Tensor *accum = ctx.Input(1);
  auto accum_shape = accum->GetTensorShape();

  Tensor *lr = ctx.Input(2);

  Tensor *grad = ctx.Input(3);
  auto grad_shape = grad->GetTensorShape();

  Tensor *indices_tensor = ctx.Input(4);
  EigenTensor indices(indices_tensor, indices_tensor->GetData());
  TI N = indices_tensor->GetTensorShape()->GetDimSize(0);

  Tensor *momentum = ctx.Input(5);

  if (N > 0) {
    TI first_dim_size = var->GetTensorShape()->GetDimSize(0);
    auto indices_vec = indices.flat<TI>();

    Eigen::TensorMap<Eigen::Tensor<T, 2, Eigen::RowMajor>> var_flat(
      reinterpret_cast<T *>(var->GetData()), var_shape->GetDimSize(0),
      var_shape->NumElements() / var_shape->GetDimSize(0));
    Eigen::TensorMap<Eigen::Tensor<T, 2, Eigen::RowMajor>> accum_flat(
      reinterpret_cast<T *>(accum->GetData()), accum_shape->GetDimSize(0),
      accum_shape->NumElements() / accum_shape->GetDimSize(0));
    Eigen::TensorMap<Eigen::Tensor<T, 2, Eigen::RowMajor>> grad_flat(
      reinterpret_cast<T *>(grad->GetData()), grad_shape->GetDimSize(0),
      grad_shape->NumElements() / grad_shape->GetDimSize(0));

    T lr_scalar = *(reinterpret_cast<const T *>(lr->GetData()));
    T momentum_scalar = *(reinterpret_cast<const T *>(momentum->GetData()));

    for (TI i = 0; i < N; i++) {
      const TI index = SubtleMustCopy(indices_vec(i));
      CUST_KERNEL_CHECK_FALSE(ctx, index < first_dim_size, KERNEL_STATUS_PARAM_INVALID,
                              "Index [%d] at offset [%d] in indices is out of range[%d].", index, i, first_dim_size);
      auto a = accum_flat.template chip<0>(index);
      auto g = grad_flat.template chip<0>(i);
      auto v = var_flat.template chip<0>(index);
      a = a * a.constant(momentum_scalar) + g;
      if (use_nesterov_) {
        v -= g.constant(lr_scalar) * g + a.constant(lr_scalar) * a.constant(momentum_scalar) * a;
      } else {
        v -= a.constant(lr_scalar) * a;
      }
    }
  }
  auto var_data = var->GetData();
  auto output_data = ctx.Output(0)->GetData();
  auto copy_size = var->GetDataSize();
  auto mem_ret = memcpy_s(output_data, copy_size, var_data, copy_size);
  CUST_KERNEL_CHECK_FALSE(ctx, mem_ret == EOK, KERNEL_STATUS_INNER_ERROR,
                          "Memcpy size[%zu] from input var to output var failed.", copy_size);
  return KERNEL_STATUS_OK;
}
REGISTER_MS_CPU_KERNEL(kSparseApplyMomentum, SparseApplyMomentumCpuKernel);
}  // namespace aicpu
