# Copyright 2023 Huawei Technologies Co., Ltd
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
"""Complex Operators"""
from mindspore.hypercomplex.complex.complex_relu import ReLU
from mindspore.hypercomplex.complex.complex_operators import Conv1d, Conv2d, Conv3d
from mindspore.hypercomplex.complex.complex_operators import BatchNorm1d, BatchNorm2d, BatchNorm3d
from mindspore.hypercomplex.complex.complex_operators import Dense

from mindspore.hypercomplex.hypercomplex.hc_pool import MaxPool1d, MaxPool2d, \
    AvgPool1d, AvgPool2d, AdaptiveAvgPool1d, AdaptiveAvgPool2d, \
    AdaptiveAvgPool3d, AdaptiveMaxPool1d, AdaptiveMaxPool2d
