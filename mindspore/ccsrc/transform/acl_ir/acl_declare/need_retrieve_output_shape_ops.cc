/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#include "transform/acl_ir/acl_adapter_info.h"

namespace mindspore {
namespace transform {
REGISTER_ACL_OP(MaskedSelect).set_is_need_retrieve_output_shape();

REGISTER_ACL_OP(Unique).set_is_need_retrieve_output_shape();

REGISTER_ACL_OP(NonMaxSuppressionV3).set_is_need_retrieve_output_shape();

REGISTER_ACL_OP(CTCGreedyDecoder).set_is_need_retrieve_output_shape();

REGISTER_ACL_OP(DynamicGetNextV2).set_is_need_retrieve_output_shape();

REGISTER_ACL_OP(NonZero).set_is_need_retrieve_output_shape();

REGISTER_ACL_OP(UniqueConsecutive).set_is_need_retrieve_output_shape();

REGISTER_ACL_OP(CustCSRSparseMatrixToSparseTensor).set_is_need_retrieve_output_shape();

REGISTER_ACL_OP(ComputeAccidentalHits).set_is_need_retrieve_output_shape();

REGISTER_ACL_OP(CustCoalesce).set_is_need_retrieve_output_shape();
}  // namespace transform
}  // namespace mindspore
