/**
 * Copyright 2019 Huawei Technologies Co., Ltd
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

#include "transform/graph_ir/op_declare/nn_batch_norm_ops_declare.h"
#include <string>
#include <vector>
#include "ops/ascend_op_name.h"
#include "ops/nn_op_name.h"

namespace mindspore::transform {
// BatchNorm
INPUT_MAP(BatchNorm) = {{1, INPUT_DESC(x)},
                        {2, INPUT_DESC(scale)},
                        {3, INPUT_DESC(offset)},
                        {4, INPUT_DESC(mean)},
                        {5, INPUT_DESC(variance)}};
INPUT_ATTR_MAP(BatchNorm) = {
  {6, ATTR_DESC(is_training, AnyTraits<bool>())},
  {7, ATTR_DESC(epsilon, AnyTraits<float>())},
  {9, ATTR_DESC(data_format, AnyTraits<GEDataFormat>())},
};
ATTR_MAP(BatchNorm) = EMPTY_ATTR_MAP;
OUTPUT_MAP(BatchNorm) = {{0, OUTPUT_DESC(y)},
                         {1, OUTPUT_DESC(batch_mean)},
                         {2, OUTPUT_DESC(batch_variance)},
                         {3, OUTPUT_DESC(reserve_space_1)},
                         {4, OUTPUT_DESC(reserve_space_2)}};
REG_ADPT_DESC(BatchNorm, kNameBatchNorm, ADPT_DESC(BatchNorm))
REG_ADPT_DESC(FusedBatchNorm, kNameFusedBatchNorm, ADPT_DESC(BatchNorm))

// BNInference is BatchNorm for caffe
INPUT_MAP(BNInference) = {{1, INPUT_DESC(x)},        {2, INPUT_DESC(mean)},  {3, INPUT_DESC(variance)},
                          {4, INPUT_DESC(momentum)}, {5, INPUT_DESC(scale)}, {6, INPUT_DESC(offset)}};
ATTR_MAP(BNInference) = {{"epsilon", ATTR_DESC(epsilon, AnyTraits<float>())},
                         {"use_global_stats", ATTR_DESC(use_global_stats, AnyTraits<bool>())},
                         {"mode", ATTR_DESC(mode, AnyTraits<int64_t>())}};
OUTPUT_MAP(BNInference) = {{0, OUTPUT_DESC(y)}};

REG_ADPT_DESC(BNInference, kNameBNInference, ADPT_DESC(BNInference))
REG_ADPT_DESC(BNInferenceD, kBNInferenceDOpName, ADPT_DESC(BNInference))

// BNInfer
INPUT_MAP(BNInfer) = {{1, INPUT_DESC(x)},
                      {2, INPUT_DESC(scale)},
                      {3, INPUT_DESC(offset)},
                      {4, INPUT_DESC(mean)},
                      {5, INPUT_DESC(variance)}};
INPUT_ATTR_MAP(BNInfer) = {{7, ATTR_DESC(epsilon, AnyTraits<float>())}};
ATTR_MAP(BNInfer) = {{"epsilon", ATTR_DESC(epsilon, AnyTraits<float>())}};
OUTPUT_MAP(BNInfer) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(BNInfer, kBNInferOpName, ADPT_DESC(BNInfer))

// BNInferGrad
INPUT_MAP(BNInferGrad) = {{1, INPUT_DESC(grads)}, {2, INPUT_DESC(scale)}, {3, INPUT_DESC(batch_variance)}};
ATTR_MAP(BNInferGrad) = {{"epsilon", ATTR_DESC(epsilon, AnyTraits<float>())}};
OUTPUT_MAP(BNInferGrad) = {{0, OUTPUT_DESC(x_backprop)}};
REG_ADPT_DESC(BNInferGrad, kBNInferGradOpName, ADPT_DESC(BNInferGrad))

// BatchNormGrad
INPUT_MAP(BatchNormGrad) = {{1, INPUT_DESC(y_backprop)},      {2, INPUT_DESC(x)},
                            {3, INPUT_DESC(scale)},           {4, INPUT_DESC(reserve_space_1)},
                            {5, INPUT_DESC(reserve_space_2)}, {6, INPUT_DESC(reserve_space_3)}};
INPUT_ATTR_MAP(BatchNormGrad) = {{7, ATTR_DESC(is_training, AnyTraits<bool>())},
                                 {8, ATTR_DESC(epsilon, AnyTraits<float>())},
                                 {9, ATTR_DESC(data_format, AnyTraits<GEDataFormat>())}};
ATTR_MAP(BatchNormGrad) = EMPTY_ATTR_MAP;
OUTPUT_MAP(BatchNormGrad) = {{0, OUTPUT_DESC(x_backprop)},
                             {1, OUTPUT_DESC(scale_backprop)},
                             {2, OUTPUT_DESC(offset_backprop)},
                             {3, OUTPUT_DESC(reserve_space_4)},
                             {4, OUTPUT_DESC(reserve_space_5)}};
REG_ADPT_DESC(BatchNormGrad, kNameBatchNormGrad, ADPT_DESC(BatchNormGrad))

CUST_INPUT_MAP(BatchNormGradGrad) = {{1, INPUT_DESC(x)},
                                     {2, INPUT_DESC(dy)},
                                     {3, INPUT_DESC(scale)},
                                     {4, INPUT_DESC(reserve_space_1)},
                                     {5, INPUT_DESC(reserve_space_2)},
                                     {6, INPUT_DESC(ddx)},
                                     {7, INPUT_DESC(ddscale)},
                                     {8, INPUT_DESC(ddoffset)}};
CUST_INPUT_ATTR_MAP(BatchNormGradGrad) = {{9, ATTR_DESC(is_training, AnyTraits<bool>())},
                                          {10, ATTR_DESC(epsilon, AnyTraits<float>())},
                                          {11, ATTR_DESC(data_format, AnyTraits<GEDataFormat>())}};
CUST_ATTR_MAP(BatchNormGradGrad) = EMPTY_ATTR_MAP;
CUST_OUTPUT_MAP(BatchNormGradGrad) = {{0, OUTPUT_DESC(dx)}, {1, OUTPUT_DESC(ddy)}, {2, OUTPUT_DESC(dscale)}};
REG_ADPT_DESC(BatchNormGradGrad, kNameBatchNormGradGrad, CUST_ADPT_DESC(BatchNormGradGrad))

// L2NormalizeGrad
INPUT_MAP(L2NormalizeGrad) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(y)}, {3, INPUT_DESC(dy)}};
ATTR_MAP(L2NormalizeGrad) = {
  {"axis", ATTR_DESC(dim, AnyTraits<std::vector<int64_t>>(), AnyTraits<std::vector<int64_t>>())},
  {"epsilon", ATTR_DESC(eps, AnyTraits<float>())}};
OUTPUT_MAP(L2NormalizeGrad) = {{0, OUTPUT_DESC(dx)}};
REG_ADPT_DESC(L2NormalizeGrad, kNameL2NormalizeGrad, ADPT_DESC(L2NormalizeGrad))

// L2Normalize
INPUT_MAP(L2Normalize) = {{1, INPUT_DESC(x)}};
ATTR_MAP(L2Normalize) = {
  {"axis", ATTR_DESC(axis, AnyTraits<std::vector<int64_t>>(), AnyTraits<std::vector<int64_t>>())},
  {"epsilon", ATTR_DESC(eps, AnyTraits<float>())}};
OUTPUT_MAP(L2Normalize) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(L2Normalize, kNameL2Normalize, ADPT_DESC(L2Normalize))
}  // namespace mindspore::transform
