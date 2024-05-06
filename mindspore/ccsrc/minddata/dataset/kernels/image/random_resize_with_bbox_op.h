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

#ifndef MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_RANDOM_RESIZE_WITH_BBOX_OP_H_
#define MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_RANDOM_RESIZE_WITH_BBOX_OP_H_

#include <memory>
#include <random>
#include <string>

#include "minddata/dataset/core/tensor.h"
#include "minddata/dataset/kernels/image/resize_op.h"
#include "minddata/dataset/kernels/image/resize_with_bbox_op.h"
#include "minddata/dataset/kernels/tensor_op.h"
#include "minddata/dataset/util/random.h"
#include "minddata/dataset/util/status.h"

namespace mindspore {
namespace dataset {
class RandomResizeWithBBoxOp : public RandomTensorOp {
 public:
  RandomResizeWithBBoxOp(int32_t size_1, int32_t size_2) : size1_(size_1), size2_(size_2) {}

  ~RandomResizeWithBBoxOp() override = default;

  // Description: A function that prints info about the node
  void Print(std::ostream &out) const override { out << Name() << ": " << size1_ << " " << size2_; }

  Status Compute(const TensorRow &input, TensorRow *output) override;

  std::string Name() const override { return kRandomResizeWithBBoxOp; }

 private:
  int32_t size1_;
  int32_t size2_;
  std::uniform_int_distribution<int> distribution_{0, 3};
};
}  // namespace dataset
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_RANDOM_RESIZE_WITH_BBOX_OP_H_
