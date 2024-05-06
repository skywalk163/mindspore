/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IR_VISION_ASCEND_VISION_IR_H_
#define MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IR_VISION_ASCEND_VISION_IR_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "include/api/status.h"
#include "minddata/dataset/include/dataset/constants.h"
#include "minddata/dataset/include/dataset/transforms.h"
#include "minddata/dataset/kernels/ir/tensor_operation.h"

namespace mindspore {
namespace dataset {
// Transform operations for computer vision
namespace vision {
// Char arrays storing name of corresponding classes (in alphabetical order)
constexpr char kDvppCropJpegOperation[] = "DvppCropJpeg";
constexpr char kDvppDecodeResizeOperation[] = "DvppDecodeResize";
constexpr char kDvppDecodeResizeCropOperation[] = "DvppDecodeResizeCrop";
constexpr char kDvppDecodeJpegOperation[] = "DvppDecodeJpeg";
constexpr char kDvppDecodePngOperation[] = "DvppDecodePng";
constexpr char kDvppDecodeVideoOperation[] = "DvppDecodeVideo";
constexpr char kDvppNormalizeOperation[] = "DvppNormalize";
constexpr char kDvppResizeJpegOperation[] = "DvppResizeJpeg";

/* ####################################### Derived TensorOperation classes ################################# */

class DvppCropJpegOperation : public TensorOperation {
 public:
  explicit DvppCropJpegOperation(const std::vector<uint32_t> &resize);

  ~DvppCropJpegOperation() override = default;

  std::shared_ptr<TensorOp> Build() override;

  Status ValidateParams() override;

  std::string Name() const override { return kDvppCropJpegOperation; }

  Status to_json(nlohmann::json *out_json) override;

  static Status from_json(nlohmann::json op_params, std::shared_ptr<TensorOperation> *operation);

 private:
  std::vector<uint32_t> crop_;
};

class DvppDecodeResizeOperation : public TensorOperation {
 public:
  explicit DvppDecodeResizeOperation(const std::vector<uint32_t> &resize);

  ~DvppDecodeResizeOperation() override = default;

  std::shared_ptr<TensorOp> Build() override;

  Status ValidateParams() override;

  std::string Name() const override { return kDvppDecodeResizeOperation; }

  Status to_json(nlohmann::json *out_json) override;

  static Status from_json(nlohmann::json op_params, std::shared_ptr<TensorOperation> *operation);

 private:
  std::vector<uint32_t> resize_;
};

class DvppDecodeResizeCropOperation : public TensorOperation {
 public:
  DvppDecodeResizeCropOperation(const std::vector<uint32_t> &crop, const std::vector<uint32_t> &resize);

  ~DvppDecodeResizeCropOperation() override = default;

  std::shared_ptr<TensorOp> Build() override;

  Status ValidateParams() override;

  std::string Name() const override { return kDvppDecodeResizeCropOperation; }

  Status to_json(nlohmann::json *out_json) override;

  static Status from_json(nlohmann::json op_params, std::shared_ptr<TensorOperation> *operation);

 private:
  std::vector<uint32_t> crop_;
  std::vector<uint32_t> resize_;
};

class DvppDecodeJpegOperation : public TensorOperation {
 public:
  ~DvppDecodeJpegOperation() override = default;

  std::shared_ptr<TensorOp> Build() override;

  Status ValidateParams() override;

  std::string Name() const override { return kDvppDecodeJpegOperation; }

  static Status from_json(nlohmann::json op_params, std::shared_ptr<TensorOperation> *operation);
};

class DvppDecodeVideoOperation : public TensorOperation {
 public:
  DvppDecodeVideoOperation(const std::vector<uint32_t> &size, VdecStreamFormat type, VdecOutputFormat out_format,
                           const std::string &output);

  ~DvppDecodeVideoOperation() override = default;

  std::shared_ptr<TensorOp> Build() override;

  Status ValidateParams() override;

  std::string Name() const override { return kDvppDecodeVideoOperation; }

  Status to_json(nlohmann::json *out_json) override;

  static Status from_json(nlohmann::json op_params, std::shared_ptr<TensorOperation> *operation);

 private:
  std::vector<uint32_t> size_;
  VdecOutputFormat format_;
  VdecStreamFormat en_type_;
  std::string output_;
};

class DvppDecodePngOperation : public TensorOperation {
 public:
  ~DvppDecodePngOperation() override = default;

  std::shared_ptr<TensorOp> Build() override;

  Status ValidateParams() override;

  std::string Name() const override { return kDvppDecodePngOperation; }

  static Status from_json(nlohmann::json op_params, std::shared_ptr<TensorOperation> *operation);
};

class DvppNormalizeOperation : public TensorOperation {
 public:
  DvppNormalizeOperation(const std::vector<float> &mean, const std::vector<float> &std);

  ~DvppNormalizeOperation() override = default;

  std::shared_ptr<TensorOp> Build() override;

  Status ValidateParams() override;

  std::string Name() const override { return kDvppNormalizeOperation; }

  Status to_json(nlohmann::json *out_json) override;

  static Status from_json(nlohmann::json op_params, std::shared_ptr<TensorOperation> *operation);

 private:
  std::vector<float> mean_;
  std::vector<float> std_;
};

class DvppResizeJpegOperation : public TensorOperation {
 public:
  explicit DvppResizeJpegOperation(const std::vector<uint32_t> &resize);

  ~DvppResizeJpegOperation() override = default;

  std::shared_ptr<TensorOp> Build() override;

  Status ValidateParams() override;

  std::string Name() const override { return kDvppResizeJpegOperation; }

  Status to_json(nlohmann::json *out_json) override;

  static Status from_json(nlohmann::json op_params, std::shared_ptr<TensorOperation> *operation);

 private:
  std::vector<uint32_t> resize_;
};
}  // namespace vision
}  // namespace dataset
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_MINDDATA_DATASET_KERNELS_IR_VISION_ASCEND_VISION_IR_H_
