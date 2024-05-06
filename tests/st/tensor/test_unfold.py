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
import pytest
import numpy as np

import mindspore.nn as nn
from mindspore import Tensor
import mindspore.context as context
from mindspore.common import dtype as mstype


class UnfoldTensorNet(nn.Cell):
    def construct(self, x):
        return x.unfold(kernel_size=3, dilation=1, stride=1)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_tensor_unfold(mode):
    """
    Feature: Test unfold tensor api.
    Description: Test unfold tensor api for Graph and PyNative modes.
    Expectation: the result match with expected result.
    """
    context.set_context(mode=mode)
    x = Tensor(np.ones((4, 4, 32, 32)), mstype.float32)
    net = UnfoldTensorNet()
    output = net(x)
    expected_shape = (4, 36, 900)
    assert output.dtype == x.dtype
    assert output.shape == expected_shape
