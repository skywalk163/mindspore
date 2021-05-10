/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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
#include <queue>
#include <memory>
#include "src/mindrt_executor.h"
#include "src/lite_mindrt.h"
#include "include/errorcode.h"
#include "src/common/tensor_util.h"

namespace mindspore::lite {

void MindrtExecutor::PrepareInputData(const std::vector<kernel::LiteKernel *> &kernels,
                                      const std::vector<Tensor *> &inputs) {
  for (size_t i = 0; i < inputs.size(); ++i) {
    for (size_t j = 0; j < kernels.size(); ++j) {
      if (!kernels[j]->in_kernels().empty()) {
        continue;
      }
      auto in_tensor_size = kernels[j]->in_tensors().size();
      for (size_t k = 0; k < in_tensor_size; ++k) {
        if (inputs[i] != kernels[j]->in_tensors()[k]) {
          continue;
        }
        auto data = std::make_shared<OpData<Tensor>>(op_actors_[j]->GetAID(), inputs[i], static_cast<int>(k));
        input_data_.emplace_back(data);
      }
    }
  }
}

void MindrtExecutor::PrepareOutputData(const std::vector<kernel::LiteKernel *> &kernels,
                                       const std::vector<Tensor *> &outputs) {
  for (size_t i = 0; i < outputs.size(); ++i) {
    for (size_t j = 0; j < kernels.size(); ++j) {
      if (!kernels[i]->out_kernels().empty()) {
        continue;
      }
      auto out_tensor_size = kernels[j]->out_tensors().size();
      for (size_t k = 0; k < out_tensor_size; ++k) {
        if (outputs[i] != kernels[j]->out_tensors()[k]) {
          continue;
        }
        auto data = std::make_shared<OpData<Tensor>>(op_actors_[j]->GetAID(), outputs[i], static_cast<int>(k));
        op_actors_[j]->AddResultIndex(output_data_.size());
        output_data_.emplace_back(data);
      }
    }
  }
}

int MindrtExecutor::Prepare(const std::vector<kernel::LiteKernel *> &kernels, const std::vector<Tensor *> &inputs,
                            const std::vector<Tensor *> &outputs) {
  auto ret = MindrtInit();
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "MindrtInit failed";
    return ret;
  }
  op_actors_ = CreateOpActor(kernels);
  if (op_actors_.size() != kernels.size()) {
    MS_LOG(ERROR) << "CreateOpActor failed";
    return RET_ERROR;
  }

  PrepareInputData(kernels, inputs);

  PrepareOutputData(kernels, outputs);

  return RET_OK;
}

int MindrtExecutor::Run(const std::vector<Tensor *> &in_tensors, const std::vector<Tensor *> &out_tensors,
                        const std::vector<kernel::LiteKernel *> &kernels, mindspore::Allocator *allocator,
                        const KernelCallBack &before, const KernelCallBack &after) {
  MS_ASSERT(nullptr != allocator);
  if (kernels.front()->type() != schema::PrimitiveType_Merge) {
    auto ret = CheckTensorsInvalid(in_tensors);
    if (RET_OK != ret) {
      MS_LOG(ERROR) << "CheckInputs failed";
      return ret;
    }
  }
  // clear ref_count
  for (auto *tensor : kernels.front()->in_tensors()) {
    tensor->set_ref_count(0);
  }

  return MindrtRun<Tensor>(input_data_, &output_data_, &before, &after);
}

}  // namespace mindspore::lite
