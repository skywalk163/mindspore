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

import numpy as np
import pytest

import mindspore as ms
import mindspore.nn as nn
import mindspore.ops as ops


class Net(nn.Cell):
    def construct(self, x):
        return ops.nanmedian(x, axis=0, keepdims=True)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_ops_nanmedian(mode):
    """
    Feature: ops.nanmedian
    Description: Verify the result of nanmedian
    Expectation: success
    """
    ms.set_context(mode=mode)
    x = ms.Tensor([[float("nan"), float("nan"), -256.9], [float("nan"), 128.1, 128]], ms.float32)
    net = Net()
    output = net(x)
    expect_output = [[float("nan"), 128.1, -256.9]]
    expect_index = [[0, 1, 0]]
    assert np.allclose(output[0].asnumpy(), expect_output, equal_nan=True)
    assert np.allclose(output[1].asnumpy(), expect_index, equal_nan=True)
