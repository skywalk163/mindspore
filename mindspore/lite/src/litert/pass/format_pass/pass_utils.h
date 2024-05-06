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
#ifndef MINDSPORE_LITE_SRC_RUNTIME_PASS_FORMAT_PASS_UTILS_H_
#define MINDSPORE_LITE_SRC_RUNTIME_PASS_FORMAT_PASS_UTILS_H_

#include <string>
#include <vector>
#include <functional>
#include "src/executor/kernel_exec.h"
#include "src/executor/sub_graph_kernel.h"

namespace mindspore::lite::pass {
static const std::vector<int> nh2nc_perm = {0, 3, 1, 2};
static const std::vector<int> nc2nh_perm = {0, 2, 3, 1};
struct TransInfoPair {
  mindspore::Format src_format_;
  mindspore::Format dst_format_;
  TransInfoPair() : src_format_(DEFAULT_FORMAT), dst_format_(DEFAULT_FORMAT) {}
  TransInfoPair(Format src, Format dst) : src_format_(src), dst_format_(dst) {}
};

using CreateFormatTransposeFunc = std::function<kernel::KernelExec *(
  InferTensor *input, InferTensor *output, const TransInfoPair &trans_info, const std::string &name,
  const lite::InnerContext *ctx, const kernel::KernelKey &desc)>;

inline bool IsNCHWFormat(Format format) { return format == NCHW || format == NC4HW4 || format == NC8HW8; }

bool IsNoneTranspose(const TransInfoPair &trans);

bool IsSameTranspose(const TransInfoPair &trans0, const TransInfoPair &trans1);

bool IsOppositiveTranspose(const TransInfoPair &trans0, const TransInfoPair &trans1);

template <typename ShapeDT>
std::vector<ShapeDT> TransShape(const std::vector<ShapeDT> &shape, const TransInfoPair &trans, bool *ret) {
  *ret = true;
  if (shape.size() != DIMENSION_4D) {
    return shape;
  }
  if (trans.src_format_ == trans.dst_format_ || (IsNCHWFormat(trans.src_format_) && IsNCHWFormat(trans.dst_format_))) {
    return shape;
  }
  if (IsNCHWFormat(trans.src_format_) && trans.dst_format_ == NHWC) {
    return {shape[0], shape[2], shape[3], shape[1]};
  } else if (trans.src_format_ == NHWC && IsNCHWFormat(trans.dst_format_)) {
    return {shape[0], shape[3], shape[1], shape[2]};
  } else {
    MS_LOG(WARNING) << "Unsupported transpose perm, from " << FormatEnumToString(trans.src_format_) << " to "
                    << FormatEnumToString(trans.dst_format_);
    *ret = false;
    return {};
  }
}

bool TransTensorShapeAndFormat(Tensor *tensor, Format dst_format);
bool SetShape(const Tensor *src_tensor, Tensor *dst_tensor);
bool SetShape4D(const Tensor *src_tensor, Tensor *dst_tensor);

int InsertPreTranspose(kernel::SubGraphKernel *subgraph, kernel::KernelExec *kernel, std::vector<Tensor *> *all_tensors,
                       const TransInfoPair &trans_info, const size_t &index, const CreateFormatTransposeFunc &func);

int InsertPostTranspose(kernel::SubGraphKernel *subgraph, kernel::KernelExec *kernel,
                        std::vector<Tensor *> *all_tensors, const TransInfoPair &trans_info, const size_t &index,
                        const CreateFormatTransposeFunc &func);

int GetTransposeInfo(const kernel::KernelExec *kernel, TransInfoPair *trans_info);
}  // namespace mindspore::lite::pass
#endif  // MINDSPORE_LITE_SRC_RUNTIME_PASS_FORMAT_PASS_UTILS_H_
