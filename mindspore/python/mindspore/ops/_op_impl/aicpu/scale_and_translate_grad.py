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

"""ScaleAndTranslateGrad op"""
from mindspore.ops.op_info_register import op_info_register, AiCPURegOp, DataType

scale_and_translate_grad_op_info = AiCPURegOp("ScaleAndTranslateGrad") \
    .fusion_type("OPAQUE") \
    .attr("kernel_type", "str") \
    .attr("antialias", "bool") \
    .input(0, "grads", "required") \
    .input(1, "original_image", "required") \
    .input(2, "scale", "required") \
    .input(3, "translation", "required") \
    .output(0, "y", "required") \
    .dtype_format(DataType.F32_Default, DataType.F32_Default, \
                  DataType.F32_Default, DataType.F32_Default, DataType.F32_Default) \
    .get_op_info()


@op_info_register(scale_and_translate_grad_op_info)
def _scale_and_translate_grad_aicpu():
    """ScaleAndTranslateGrad AiCPU register"""
    return
