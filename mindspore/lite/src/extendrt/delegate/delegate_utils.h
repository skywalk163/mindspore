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
#ifndef MINDSPORE_LITE_SRC_RUNTIME_DELEGATE_DELEGATE_UTILS_H_
#define MINDSPORE_LITE_SRC_RUNTIME_DELEGATE_DELEGATE_UTILS_H_
#include <vector>
#include <map>
#include <set>
#include "src/common/log_adapter.h"
#include "include/errorcode.h"
#include "core/base/base.h"
#include "src/extendrt/delegate/tensorrt/tensor_info.h"

namespace mindspore::lite {
bool IsSubGraphInputTensor(const std::vector<TensorInfo> &inputs, const TensorInfo &input);

template <typename T>
std::vector<T *> FindPreOps(T *cur_op, std::vector<T *> all_ops) {
  std::vector<T *> in_ops;
  for (auto in_tensor : cur_op->inputs()) {
    for (auto op : all_ops) {
      if (std::find(op->outputs().begin(), op->outputs().end(), in_tensor) != op->outputs().end()) {
        in_ops.push_back(op);
      }
    }
  }
  return in_ops;
}

template <typename T>
std::vector<T *> FindNextOps(T *cur_op, std::vector<T *> all_ops) {
  std::vector<T *> out_ops;
  for (auto out_tensor : cur_op->outputs()) {
    for (auto op : all_ops) {
      if (std::find(op->inputs().begin(), op->inputs().end(), out_tensor) != op->inputs().end()) {
        out_ops.push_back(op);
      }
    }
  }
  return out_ops;
}

template <typename T>
void FindPreNextOps(std::vector<T *> all_ops) {
  std::map<TensorInfo, std::set<T *>> in_tensor_op;
  std::map<TensorInfo, std::set<T *>> out_tensor_op;
  for (auto op : all_ops) {
    for (auto in_tensor : op->inputs()) {
      in_tensor_op[in_tensor].insert(op);
    }
    for (auto out_tensor : op->outputs()) {
      out_tensor_op[out_tensor].insert(op);
    }
  }
  for (auto op : all_ops) {
    std::set<T *> in_ops_set;
    for (auto in_tensor : op->inputs()) {
      auto in_ops = out_tensor_op[in_tensor];
      in_ops_set.insert(in_ops.begin(), in_ops.end());
    }
    std::vector<T *> in_ops_vec;
    in_ops_vec.assign(in_ops_set.begin(), in_ops_set.end());
    op->set_in_ops(in_ops_vec);

    std::set<T *> out_ops_set;
    for (auto out_tensor : op->outputs()) {
      auto out_ops = in_tensor_op[out_tensor];
      out_ops_set.insert(out_ops.begin(), out_ops.end());
    }
    std::vector<T *> out_ops_vec;
    out_ops_vec.assign(out_ops_set.begin(), out_ops_set.end());
    op->set_out_ops(out_ops_vec);
  }
}

template <typename T>
int GetGraphInOutOps(const std::vector<TensorInfo> &inputs, const std::vector<TensorInfo> &outputs,
                     std::vector<T *> *in_ops, std::vector<T *> *out_ops, const std::vector<T *> &all_ops) {
  for (auto in_tensor : inputs) {
    for (auto op : all_ops) {
      if (std::find(op->inputs().begin(), op->inputs().end(), in_tensor) != op->inputs().end() &&
          std::find(in_ops->begin(), in_ops->end(), op) == in_ops->end()) {
        in_ops->push_back(op);
      }
    }
  }
  if (in_ops->empty()) {
    MS_LOG(ERROR) << "Can't find the input ops for npu sub graph.";
    return RET_ERROR;
  }

  for (auto out_tensor : outputs) {
    for (auto op : all_ops) {
      if (std::find(op->outputs().begin(), op->outputs().end(), out_tensor) != op->outputs().end() &&
          std::find(out_ops->begin(), out_ops->end(), op) == out_ops->end()) {
        out_ops->push_back(op);
      }
    }
  }
  if (out_ops->empty()) {
    MS_LOG(ERROR) << "Can't find the output ops for npu sub graph.";
    return RET_ERROR;
  }
  return RET_OK;
}
}  // namespace mindspore::lite

#endif  // MINDSPORE_LITE_SRC_RUNTIME_DELEGATE_DELEGATE_UTILS_H_
