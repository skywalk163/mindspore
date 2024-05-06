# Copyright 2022 Huawei Technologies Co., Ltd
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

"""FractionalMaxPoolGradWithFixedKsize op"""
from mindspore.ops.op_info_register import op_info_register, AiCPURegOp, DataType

fractional_max_pool_grad_with_fixed_ksize_op_info = AiCPURegOp("FractionalMaxPoolGradWithFixedKsize") \
    .fusion_type("OPAQUE") \
    .attr("data_format", "str", "NCHW") \
    .input(0, "origin_input", "required") \
    .input(1, "out_backprop", "required") \
    .input(2, "argmax", "required") \
    .output(0, "y", "required") \
    .dtype_format(DataType.I32_NCHW, DataType.F16_NCHW, DataType.I64_Default, DataType.F16_NCHW) \
    .dtype_format(DataType.I32_NCHW, DataType.F32_NCHW, DataType.I64_Default, DataType.F32_NCHW) \
    .dtype_format(DataType.I32_NCHW, DataType.F64_NCHW, DataType.I64_Default, DataType.F64_NCHW) \
    .dtype_format(DataType.I32_NCHW, DataType.I32_NCHW, DataType.I64_Default, DataType.I32_NCHW) \
    .dtype_format(DataType.I32_NCHW, DataType.I64_NCHW, DataType.I64_Default, DataType.I64_NCHW) \
    .dtype_format(DataType.I64_NCHW, DataType.F16_NCHW, DataType.I64_Default, DataType.F16_NCHW) \
    .dtype_format(DataType.I64_NCHW, DataType.F32_NCHW, DataType.I64_Default, DataType.F32_NCHW) \
    .dtype_format(DataType.I64_NCHW, DataType.F64_NCHW, DataType.I64_Default, DataType.F64_NCHW) \
    .dtype_format(DataType.I64_NCHW, DataType.I32_NCHW, DataType.I64_Default, DataType.I32_NCHW) \
    .dtype_format(DataType.I64_NCHW, DataType.I64_NCHW, DataType.I64_Default, DataType.I64_NCHW) \
    .get_op_info()


@op_info_register(fractional_max_pool_grad_with_fixed_ksize_op_info)
def _fractional_max_pool_grad_with_fixed_ksize_aicpu():
    """FractionalMaxPoolGradWithFixedKsize aicpu register"""
    return
