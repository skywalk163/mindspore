/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2022. All rights reserved.
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

#ifndef AICPU_SPARSETENSOR_H
#define AICPU_SPARSETENSOR_H

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "cpu_tensor.h"
#include "cpu_kernel/utils/eigen_tensor.h"
#include "utils/kernel_util.h"
#include "context/inc/cpu_kernel_utils.h"
#include "inc/kernel_log.h"
#include "cpu_kernel/utils/sparse_group.h"
#include "context/common/status.h"

namespace aicpu {
template <typename T>
const T SubtleMustCopy(const T &x) {
  auto *to_x = reinterpret_cast<const volatile T *>(&x);
  return *to_x;
}
}  // namespace aicpu

namespace aicpu {
class DimComparator {
 public:
  DimComparator(const TTypes<int64_t>::Matrix &ix, const std::vector<int64_t> &order, const std::vector<int64_t> &shape)
      : ix_(ix), order_(order), dims_(shape.size()), ix_order_(nullptr) {}

  inline bool operator()(const int64_t i, const int64_t j) const {
    for (int di = 0; di < dims_; ++di) {
      const int64_t d = order_[di];
      if (ix_(i, d) < ix_(j, d)) return true;
      if (ix_(i, d) > ix_(j, d)) return false;
    }
    return false;
  }

  // Compares two indices taken from corresponding index matrices, using the
  // standard, row-major (or lexicographic) order.  Useful for cases that need
  // to distinguish between all three orderings (<, ==, >).
  inline static int cmp(const TTypes<int64_t>::ConstMatrix &a_idx, const TTypes<int64_t>::ConstMatrix &b_idx,
                        const int64_t a_row, const int64_t b_row, const int dims) {
    for (int d = 0; d < dims; ++d) {
      const int64_t a = a_idx(a_row, d);
      const int64_t b = b_idx(b_row, d);
      if (a < b) {
        return -1;
      } else if (a > b) {
        return 1;
      }
    }
    return 0;
  }

 protected:
  const TTypes<int64_t>::Matrix ix_;
  const std::vector<int64_t> order_;
  const int64_t dims_;
  const std::vector<int64_t> *ix_order_;
};

template <int ORDER_DIM>
class FixedDimComparator : DimComparator {
 public:
  FixedDimComparator(const TTypes<int64_t>::Matrix &ix, const std::vector<int64_t> &order,
                     const std::vector<int64_t> &shape)
      : DimComparator(ix, order, shape) {}
  inline bool operator()(const int64_t i, const int64_t j) const {
    bool value = false;
    for (int di = 0; di < ORDER_DIM; ++di) {
      const int64_t d = order_[di];
      if (ix_(i, d) < ix_(j, d)) {
        value = true;
        break;
      }
      if (ix_(i, d) > ix_(j, d)) break;
    }
    return value;
  }
};
}  // namespace aicpu

namespace aicpu {
class SparseTensor {
 public:
  SparseTensor() : dims_(0) {}
  ~SparseTensor() = default;

  /*
   * create sparse tensor
   * @param ix: index tensor
   * @param tensorvals: tensorvals tensor
   * @param shape: shape vec
   * @param order: order vec
   * @return uint32_t: 0->success other->failed
   */
  uint32_t CreateSparseTensor(CpuKernelContext &ctx, Tensor *ix, Tensor *tensorvals, std::vector<int64_t> shape,
                              std::vector<int64_t> order);

  /*
   * sparse indices valid
   * @return uint32_t: 0->success other->failed
   */
  uint32_t IndicesValid(CpuKernelContext &ctx);

  template <typename T>
  void Reorder(CpuKernelContext &ctx, const std::vector<int64_t> &order) {
    int32_t order_size = static_cast<int32_t>(order.size());
    if (order_size != dims_) {
      CUST_KERNEL_LOG_ERROR(ctx, "Order length must be SparseTensor rank");
    }
    auto ix_t = ix_->matrix<int64_t>();
    auto vals_t = vals_->vec<T>();
    std::vector<int64_t> reorder(dims_);
    std::iota(reorder.begin(), reorder.end(), 0);
    // Sort to get order of indices
    switch (order.size()) {
#define CASE_SORT(ORDER_SIZE)                                     \
  case (ORDER_SIZE): {                                            \
    FixedDimComparator<(ORDER_SIZE)> sorter(ix_t, order, shape_); \
    std::sort(reorder.begin(), reorder.end(), sorter);            \
    break;                                                        \
  }
      CASE_SORT(0);
      CASE_SORT(1);
      CASE_SORT(2);
      CASE_SORT(3);
      CASE_SORT(4);
      CASE_SORT(5);
#undef CASE_SORT
      default: {
        DimComparator sorter(ix_t, order, shape_);
        std::sort(reorder.begin(), reorder.end(), sorter);
      }
    }
    // We have a forward reordering, but what we'll need is a
    // permutation (the inverse).  This can be calculated with O(1)
    // additional
    // and O(n) time (INVPERM) but we just do the simple thing here.
    std::vector<size_t> permutation(reorder.size());
    for (std::size_t n = 0; n < reorder.size(); ++n) {
      permutation[reorder[n]] = n;
    }
    // Update indices & values by converting the permutations to
    // a product of transpositions.  Iterate over the cycles in the
    // permutation, and convert each of those into a product of
    // transpositions (swaps):
    //   https://en.wikipedia.org/wiki/Cyclic_permutation
    // This is N swaps, 2*N comparisons.
    for (std::size_t n = 0; n + 1 < permutation.size(); ++n) {
      while (n != permutation[n]) {
        std::size_t r = permutation[n];
        std::swap_ranges(&(ix_t(n, 0)), &(ix_t(n + 1, 0)), &(ix_t(r, 0)));
        std::swap(vals_t(n), vals_t(r));
        std::swap(permutation[n], permutation[r]);
      }
    }
    order_.assign(order.begin(), order.end());
  }

  /*
   * group sparse tensor
   * @return GroupIterable
   */
  GroupIterable group(CpuKernelContext &ctx, const std::vector<int64_t> &group_ix) const;
  /*
   * sparse eigen tensor indices valid
   * @return uint32_t: 0->success other->failed
   */
  int dims() const { return dims_; }

  std::shared_ptr<EigenTensor> indices() const { return ix_; }

  std::shared_ptr<EigenTensor> values() const { return vals_; }

  std::vector<int64_t> shape() const { return shape_; }

  template <typename T>
  uint32_t EigenTensorIndicesValidCheck(CpuKernelContext &ctx, int64_t dims_size) const {
    const auto ix_t = ix_->matrix<T>();
    for (int64_t n = 1; n < dims_size; ++n) {
      bool valid = true;
      bool different = false;
      bool increasing = true;
      for (int32_t di = 0; di < dims_; ++di) {
        if (ix_t(n, di) < 0 || ix_t(n, di) >= shape_[di]) {
          valid = false;
        }
        int64_t diff = ix_t(n, order_[di]) - ix_t(n - 1, order_[di]);
        if (diff > 0) {
          different = true;
        }
        if (!different && diff < 0) {
          increasing = false;
        }
      }
      if (!valid) {
        CUST_KERNEL_LOG_ERROR(ctx, "Indices is out of bounds, index=%lld.", n);
        return static_cast<uint32_t>(KERNEL_STATUS_PARAM_INVALID);
      }
      if (!increasing) {
        CUST_KERNEL_LOG_ERROR(ctx, "indices is out of order, index=%lld.", n);
        return static_cast<uint32_t>(KERNEL_STATUS_PARAM_INVALID);
      }
      if (!different) {
        CUST_KERNEL_LOG_ERROR(ctx, "indices is repeated, index=%lld.", n);
        return static_cast<uint32_t>(KERNEL_STATUS_PARAM_INVALID);
      }
    }
    return static_cast<uint32_t>(KERNEL_STATUS_OK);
  }
  /*
   * sparse eigen tensor indices valid
   * @return uint32_t: 0->success other->failed
   */
  template <typename T>
  uint32_t EigenTensorIndicesValidParaCheck(CpuKernelContext &ctx, int64_t dims_size) {
    uint32_t min_core_num = 1;
    int64_t max_core_num = std::max(min_core_num, aicpu::CpuKernelUtils::GetCPUNum(ctx) - kResvCpuNum);
    uint32_t result = static_cast<uint32_t>(KERNEL_STATUS_OK);
    (void)aicpu::CpuKernelUtils::ParallelFor(
      ctx, dims_size, dims_size / max_core_num, [&](std::int64_t begin, std::int64_t end) {
        int64_t start = begin;
        if (begin == 0) {
          start = begin + 1;
        }
        const auto ix_t = ix_->matrix<T>();
        for (int64_t n = start; n < end; ++n) {
          bool valid = true;
          bool different = false;
          bool increasing = true;
          for (int32_t di = 0; di < dims_; ++di) {
            if (ix_t(n, di) < 0 || ix_t(n, di) >= shape_[di]) {
              valid = false;
            }
            int64_t diff = ix_t(n, order_[di]) - ix_t(n - 1, order_[di]);
            if (diff > 0) {
              different = true;
            }
            if (!different && diff < 0) {
              increasing = false;
            }
          }
          if (!valid) {
            CUST_KERNEL_LOG_ERROR(ctx, "Indices is out of bounds, index=%lld.", n);
            result = static_cast<uint32_t>(KERNEL_STATUS_PARAM_INVALID);
            return;
          }
          if (!increasing) {
            CUST_KERNEL_LOG_ERROR(ctx, "indices is out of order, index=%lld.", n);
            result = static_cast<uint32_t>(KERNEL_STATUS_PARAM_INVALID);
            return;
          }
          if (!different) {
            CUST_KERNEL_LOG_ERROR(ctx, "indices is repeated, index=%lld.", n);
            result = static_cast<uint32_t>(KERNEL_STATUS_PARAM_INVALID);
            return;
          }
        }
      });
    return result;
  }
  /*
   * sparse eigen tensor indices valid
   * @return uint32_t: 0->success other->failed
   */
  template <typename T>
  uint32_t EigenTensorIndicesValid(CpuKernelContext &ctx) {
    const auto ix_t = ix_->matrix<T>();
    int64_t dims_size =
      (ix_->GetTensor()->GetTensorShape()->GetDims() == 0) ? 1 : ix_->GetTensor()->GetTensorShape()->GetDimSize(0);
    if (dims_size > 0) {
      for (int32_t di = 0; di < dims_; ++di) {
        if ((ix_t(0, di) < 0) || (ix_t(0, di) >= shape_[di])) {
          CUST_KERNEL_LOG_ERROR(ctx, "Indices is out of bounds, index=0.");
          return KERNEL_STATUS_PARAM_INVALID;
        }
      }
    }
    const int64_t paralled_data_size = 16 * 1024;
    if (dims_size < paralled_data_size) {
      return EigenTensorIndicesValidCheck<T>(ctx, dims_size);
    } else {
      return EigenTensorIndicesValidParaCheck<T>(ctx, dims_size);
    }
  }

  /*
   * validate sparse to dense
   * @param output: output tensor
   * @return bool: true->success false->failed
   */
  bool ValidateToDense(CpuKernelContext &ctx, const Tensor *out) const;

  /*
   * sparse tensor to dense tensor
   * @param output: output tensor
   * @return uint32_t: 0->success other->failed
   */
  template <typename IndiceT, typename ValueT>
  uint32_t ToDenseParallel(CpuKernelContext &ctx, Tensor *output) {
    EigenTensor outputET(output, output->GetData());
    auto output_t = outputET.flat<ValueT>();
    auto ix_t = ix_->matrix<IndiceT>();
    std::vector<int64_t> strides(dims_);
    const auto &out_shape = output->GetTensorShape();
    if (dims_ > 0) {
      strides[dims_ - 1] = 1;
    }
    for (int32_t d = dims_ - 2; d >= 0; --d) {
      strides[d] = strides[d + 1] * out_shape->GetDimSize(d + 1);
    }
    auto vals_t = vals_->vec<ValueT>();
    int64_t vals_size = vals_t.dimension(0);
    uint32_t min_core_num = 1;
    int64_t max_core_num = std::max(min_core_num, aicpu::CpuKernelUtils::GetCPUNum(ctx) - kResvCpuNum);
    uint32_t result = static_cast<uint32_t>(KERNEL_STATUS_OK);
    auto parallel_proc = [&](std::int64_t begin, std::int64_t end) {
      for (int64_t n = begin; n < end; ++n) {
        bool invalid_dims = false;
        int64_t ix = 0;
        for (int d = 0; d < dims_; ++d) {
          const int64_t ix_n_d = ix_t(n, d);
          if (ix_n_d > out_shape->GetDimSize(d)) {
            invalid_dims = true;
          }
          ix += strides[d] * ix_n_d;
        }
        if (invalid_dims) {
          result = static_cast<uint32_t>(KERNEL_STATUS_INNER_ERROR);
          CUST_KERNEL_LOG_ERROR(ctx, "Sparse to dense got invalid dims.");
          return;
        }
        output_t(ix) = vals_t(n);
      }
      return;
    };
    CUST_KERNEL_HANDLE_ERROR(
      ctx, aicpu::CpuKernelUtils::ParallelFor(ctx, vals_size, vals_size / max_core_num, parallel_proc),
      "SparseToDense Compute failed.");
    return result;
  }

  /*
   * sparse tensor to dense tensor
   * @param output: output tensor
   * @return uint32_t: 0->success other->failed
   */
  template <typename IndiceT, typename ValueT>
  uint32_t ToDense(CpuKernelContext &ctx, Tensor *output) {
    CUST_KERNEL_LOG_INFO(ctx, "Start to execute ToDense.");
    if (output == nullptr || output->GetData() == nullptr) {
      CUST_KERNEL_LOG_ERROR(ctx, "Output tensor is nullptr.");
      return KERNEL_STATUS_INNER_ERROR;
    }
    if (!ValidateToDense(ctx, output)) {
      CUST_KERNEL_LOG_ERROR(ctx, "Validate to dense param failed.");
      return KERNEL_STATUS_INNER_ERROR;
    }
    auto vals_t = vals_->vec<ValueT>();
    int64_t vals_size = vals_t.dimension(0);
    const int64_t paralled_data_size = 16 * 1024;
    if (vals_size >= paralled_data_size) {
      return ToDenseParallel<IndiceT, ValueT>(ctx, output);
    }
    EigenTensor outputET(output, output->GetData());
    auto output_t = outputET.flat<ValueT>();
    auto ix_t = ix_->matrix<IndiceT>();
    std::vector<int64_t> strides(dims_);
    const auto &out_shape = output->GetTensorShape();
    if (dims_ > 0) {
      strides[dims_ - 1] = 1;
    }
    for (int32_t d = dims_ - 2; d >= 0; --d) {
      strides[d] = strides[d + 1] * out_shape->GetDimSize(d + 1);
    }
    for (int64_t n = 0; n < vals_size; ++n) {
      bool invalid_dims = false;
      int64_t ix = 0;
      for (int d = 0; d < dims_; ++d) {
        const int64_t ix_n_d = ix_t(n, d);
        if (ix_n_d > out_shape->GetDimSize(d)) {
          invalid_dims = true;
        }
        ix += strides[d] * ix_n_d;
      }
      if (invalid_dims) {
        CUST_KERNEL_LOG_ERROR(ctx, "Sparse to dense got invalid dims.");
        return KERNEL_STATUS_INNER_ERROR;
      }
      output_t(ix) = vals_t(n);
    }
    return KERNEL_STATUS_OK;
  }

  template <typename IndiceT, typename ValueT>
  uint32_t GetIndicesAndValues(CpuKernelContext &ctx, Tensor *y_indices, Tensor *y_values) {
    auto dims = y_indices->GetTensorShape()->GetDimSize(0);
    auto elements = order_.size();
    auto ix_t = ix_->matrix<IndiceT>();
    auto vals_t = vals_->vec<ValueT>();
    auto indices = reinterpret_cast<IndiceT *>(y_indices->GetData());
    auto values = reinterpret_cast<ValueT *>(y_values->GetData());
    for (int64_t n = 0; n < dims; ++n) {
      *(values + n) = vals_t(n);
      for (std::size_t di = 0; di < elements; ++di) {
        if (ix_t(n, di) < 0 || ix_t(n, di) >= shape_[di]) {
          CUST_KERNEL_LOG_ERROR(ctx, "indices is out of bounds, index=%lld.", n);
          return KERNEL_STATUS_PARAM_INVALID;
        }
        *(indices + n * elements + di) = ix_t(n, di);
      }
    }
    return KERNEL_STATUS_OK;
  }

  inline std::vector<int64_t> Shape() { return shape_; }

  inline std::vector<int64_t> Order() { return order_; }

  template <typename IndiceT, typename ValueT>
  inline uint32_t Reorder() {
    auto ix_t = ix_->matrix<IndiceT>();
    auto vals_t = vals_->vec<ValueT>();
    int64_t dim_size =
      (ix_->GetTensor()->GetTensorShape()->GetDims() == 0) ? 1 : ix_->GetTensor()->GetTensorShape()->GetDimSize(0);
    std::vector<int64_t> reorder(dim_size);
    std::iota(reorder.begin(), reorder.end(), 0);

    // Sort to get order of indices
    switch (order_.size()) {
#define CASE_SORT(ORDER_SIZE)                                      \
  case ORDER_SIZE: {                                               \
    FixedDimComparator<(ORDER_SIZE)> sorter(ix_t, order_, shape_); \
    std::sort(reorder.begin(), reorder.end(), sorter);             \
    break;                                                         \
  };
      CASE_SORT(0);
      CASE_SORT(1);
      CASE_SORT(2);
      CASE_SORT(3);
      CASE_SORT(4);
      CASE_SORT(5);
#undef CASE_SORT
      default: {
        DimComparator sorter(ix_t, order_, shape_);
        std::sort(reorder.begin(), reorder.end(), sorter);
      }
    }
    std::vector<size_t> permutation(reorder.size());
    for (std::size_t n = 0; n < reorder.size(); ++n) {
      permutation[reorder[n]] = n;
    }
    for (std::size_t n = 0; n + 1 < permutation.size(); ++n) {
      while (n != permutation[n]) {
        std::size_t r = permutation[n];
        std::swap_ranges(&(ix_t(n, 0)), &(ix_t(n + 1, 0)), &(ix_t(r, 0)));
        std::swap(vals_t(n), vals_t(r));
        std::swap(permutation[n], permutation[r]);
      }
    }
    return KERNEL_STATUS_OK;
  }

 private:
  std::shared_ptr<EigenTensor> ix_;
  std::shared_ptr<EigenTensor> vals_;
  std::vector<int64_t> shape_;
  std::vector<int64_t> order_;
  int32_t dims_;
};
}  // namespace aicpu

#endif  // AICPU_SPARSETENSOR_H
