# Copyright 2021 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================

"""RaggedRange op"""
from mindspore.ops.op_info_register import op_info_register, AiCPURegOp, DataType

ragged_range_op_info = AiCPURegOp("RaggedRange")\
    .fusion_type("OPAQUE")\
    .input(0, "starts", "required")\
    .input(1, "limits", "required")\
    .input(2, "deltas", "required")\
    .attr("Tsplits", "Type")\
    .output(0, "rt_nested_splits", "required")\
    .output(1, "rt_dense_values", "required")\
    .dtype_format(DataType.F32_Default, DataType.F32_Default, DataType.F32_Default, DataType.I32_Default, \
                  DataType.F32_Default)\
    .dtype_format(DataType.F32_Default, DataType.F32_Default, DataType.F32_Default, DataType.I64_Default, \
                  DataType.F32_Default)\
    .dtype_format(DataType.F64_Default, DataType.F64_Default, DataType.F64_Default, DataType.I32_Default, \
                  DataType.F64_Default)\
    .dtype_format(DataType.F64_Default, DataType.F64_Default, DataType.F64_Default, DataType.I64_Default, \
                  DataType.F64_Default)\
    .dtype_format(DataType.I32_Default, DataType.I32_Default, DataType.I32_Default, DataType.I32_Default, \
                  DataType.I32_Default)\
    .dtype_format(DataType.I32_Default, DataType.I32_Default, DataType.I32_Default, DataType.I64_Default, \
                  DataType.I32_Default)\
    .dtype_format(DataType.I64_Default, DataType.I64_Default, DataType.I64_Default, DataType.I32_Default, \
                  DataType.I64_Default)\
    .dtype_format(DataType.I64_Default, DataType.I64_Default, DataType.I64_Default, DataType.I64_Default, \
                  DataType.I64_Default)\
    .get_op_info()


@op_info_register(ragged_range_op_info)
def _raggedrange_aicpu():
    """raggedrange aicpu register"""
    return
