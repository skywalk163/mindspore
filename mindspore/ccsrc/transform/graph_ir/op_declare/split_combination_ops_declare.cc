/**
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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

#include "transform/graph_ir/op_declare/split_combination_ops_declare.h"
#include <vector>
#include "ops/array_ops.h"

namespace mindspore::transform {
// SplitD
INPUT_MAP(SplitD) = {{1, INPUT_DESC(x)}};
INPUT_ATTR_MAP(SplitD) = {{2, ATTR_DESC(split_dim, AnyTraits<int64_t>())},
                          {3, ATTR_DESC(num_split, AnyTraits<int64_t>())}};
ATTR_MAP(SplitD) = EMPTY_ATTR_MAP;
DYN_OUTPUT_MAP(SplitD) = {{0, DYN_OUTPUT_DESC(y)}};
REG_ADPT_DESC(Split, kNameSplit, ADPT_DESC(SplitD))

// Split
INPUT_MAP(Split) = {{1, INPUT_DESC(split_dim)}, {2, INPUT_DESC(x)}};
ATTR_INPUT_MAP(Split) = {{"axis", "split_dim"}};
ATTR_MAP(Split) = {{"output_num", ATTR_DESC(num_split, AnyTraits<int64_t>())}};
DYN_OUTPUT_MAP(Split) = {{0, DYN_OUTPUT_DESC(y)}};
REG_ADPT_DESC(SplitD, kSplitDOpName, ADPT_DESC(Split))

// Pack
INPUT_MAP(Pack) = EMPTY_INPUT_MAP;
DYN_INPUT_MAP(Pack) = {{1, DYN_INPUT_DESC(x)}};
ATTR_MAP(Pack) = {{kAttrDynInputSizes, ATTR_DESC(N, AnyTraits<std::vector<int64_t>>(), 0)},
                  {"axis", ATTR_DESC(axis, AnyTraits<int64_t>())}};
OUTPUT_MAP(Pack) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(Stack, mindspore::kStackOpName, ADPT_DESC(Pack))
REG_ADPT_DESC(Pack, prim::kPrimPack->name(), ADPT_DESC(Pack))

// ParallelConcat
INPUT_MAP(ParallelConcat) = EMPTY_INPUT_MAP;
DYN_INPUT_MAP(ParallelConcat) = {{1, DYN_INPUT_DESC(values)}};
ATTR_MAP(ParallelConcat) = {
  {"shape", ATTR_DESC(shape, AnyTraits<std::vector<int64_t>>())},
  {kAttrDynInputSizes, ATTR_DESC(N, AnyTraits<std::vector<int64_t>>(), 0)},
};
OUTPUT_MAP(ParallelConcat) = {{0, OUTPUT_DESC(output_data)}};
REG_ADPT_DESC(ParallelConcat, kNameParallelConcat, ADPT_DESC(ParallelConcat))

// ConcatD
INPUT_MAP(ConcatD) = EMPTY_INPUT_MAP;
DYN_INPUT_MAP(ConcatD) = {{1, DYN_INPUT_DESC(x)}};
ATTR_MAP(ConcatD) = {
  {"axis", ATTR_DESC(concat_dim, AnyTraits<int64_t>())},
  {kAttrDynInputSizes, ATTR_DESC(N, AnyTraits<std::vector<int64_t>>(), 0)},
};
OUTPUT_MAP(ConcatD) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(ConcatD, prim::kPrimConcatD->name(), ADPT_DESC(ConcatD))

// Concat
INPUT_MAP(Concat) = {{2, INPUT_DESC(concat_dim)}};
DYN_INPUT_MAP(Concat) = {{1, DYN_INPUT_DESC(x)}};
ATTR_MAP(Concat) = {{kAttrDynInputSizes, ATTR_DESC(N, AnyTraits<std::vector<int64_t>>(), 0)}};
OUTPUT_MAP(Concat) = {{0, OUTPUT_DESC(y)}};
// Rollback to ConcatD for the support of dynamic input scene is incomplete.
REG_ADPT_DESC(Concat, prim::kPrimConcat->name(), ADPT_DESC(Concat))

// ConcatV2 Inference for tf
DYN_INPUT_MAP(ConcatV2) = {{1, DYN_INPUT_DESC(x)}};
INPUT_MAP(ConcatV2) = {{2, INPUT_DESC(concat_dim)}};
ATTR_MAP(ConcatV2) = {{kAttrDynInputSizes, ATTR_DESC(N, AnyTraits<std::vector<int64_t>>(), 0)}};
OUTPUT_MAP(ConcatV2) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(ConcatV2, kNameConcatV2, ADPT_DESC(ConcatV2))

// SplitV
INPUT_MAP(SplitV) = {{1, INPUT_DESC(x)}, {2, INPUT_DESC(size_splits)}, {3, INPUT_DESC(split_dim)}};
ATTR_MAP(SplitV) = {{"num_split", ATTR_DESC(num_split, AnyTraits<int64_t>())}};
ATTR_INPUT_MAP(SplitV) = {{"size_splits", "size_splits"}, {"split_dim", "split_dim"}};
DYN_OUTPUT_MAP(SplitV) = {{0, DYN_OUTPUT_DESC(y)}};
REG_ADPT_DESC(SplitV, prim::kPrimSplitV->name(), ADPT_DESC(SplitV))
REG_ADPT_DESC(SplitVD, prim::kPrimSplitVD->name(), ADPT_DESC(SplitV))

// ConcatOffset
CUST_DYN_INPUT_MAP(ConcatOffset) = {{1, DYN_INPUT_DESC(x)}};
CUST_INPUT_MAP(ConcatOffset) = EMPTY_INPUT_MAP;
CUST_ATTR_MAP(ConcatOffset) = {{"axis", ATTR_DESC(axis, AnyTraits<int64_t>())},
                               {kAttrDynInputSizes, ATTR_DESC(N, AnyTraits<std::vector<int64_t>>(), 0)}};
CUST_DYN_OUTPUT_MAP(ConcatOffset) = {{0, DYN_OUTPUT_DESC(y)}};
CUST_OUTPUT_MAP(ConcatOffset) = EMPTY_OUTPUT_MAP;
REG_ADPT_DESC(ConcatOffset, prim::kPrimConcatOffset->name(), CUST_ADPT_DESC(ConcatOffset));
}  // namespace mindspore::transform
