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
#ifndef MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_RANDOM_CROP_AND_RESIZE_OP_H_
#define MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_RANDOM_CROP_AND_RESIZE_OP_H_

#include <memory>
#include <random>
#include <string>
#include <vector>

#include "minddata/dataset/core/tensor.h"
#include "minddata/dataset/kernels/tensor_op.h"
#include "minddata/dataset/util/status.h"

namespace mindspore {
namespace dataset {
class RandomCropAndResizeOp : public RandomTensorOp {
 public:
  RandomCropAndResizeOp(int32_t target_height, int32_t target_width, float scale_lb, float scale_ub, float aspect_lb,
                        float aspect_ub, InterpolationMode interpolation, int32_t max_attempts);

  RandomCropAndResizeOp() = default;

  RandomCropAndResizeOp(const RandomCropAndResizeOp &rhs) = default;

  RandomCropAndResizeOp(RandomCropAndResizeOp &&rhs) = default;

  ~RandomCropAndResizeOp() override = default;

  void Print(std::ostream &out) const override {
    out << "RandomCropAndResize: " << target_height_ << " " << target_width_;
  }

  Status Compute(const TensorRow &input, TensorRow *output) override;

  Status OutputShape(const std::vector<TensorShape> &inputs, std::vector<TensorShape> &outputs) override;

  TensorShape ComputeOutputShape(const TensorShape &input) const;

  Status GetCropBox(int h_in, int w_in, int *x, int *y, int *crop_height, int *crop_width);

  std::string Name() const override { return kRandomCropAndResizeOp; }

  uint32_t NumInput() override { return 1; }

  uint32_t NumOutput() override { return 1; }

 protected:
  int32_t target_height_;
  int32_t target_width_;
  std::uniform_real_distribution<float> rnd_scale_;
  std::uniform_real_distribution<float> rnd_aspect_;
  InterpolationMode interpolation_;
  int32_t max_iter_;
  double aspect_lb_;
  double aspect_ub_;
};
}  // namespace dataset
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IMAGE_RANDOM_CROP_AND_RESIZE_OP_H_
