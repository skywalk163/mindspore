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
from mindspore import Tensor


class Net(nn.Cell):
    def __init__(self, axis=None):
        super(Net, self).__init__()
        self.axis = axis

    def construct(self, x):
        output = ops.squeeze(x, self.axis)
        return output


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_ops_squeeze(mode):
    """
    Feature: ops.squeeze
    Description: Verify the result of squeeze
    Expectation: success
    """
    ms.set_context(mode=mode)
    x = Tensor(np.arange(0, 6).reshape(2, 1, 3, 1), ms.float32)
    net = Net()
    out = net(x)
    print(out)
    expect_out = [[0., 1., 2.],
                  [3., 4., 5.]]
    assert out.shape == (2, 3)
    assert np.allclose(out.asnumpy(), expect_out)

    net = Net((1, 3))
    out = net(x)
    assert out.shape == (2, 3)
    assert np.allclose(out.asnumpy(), expect_out)

    net = Net([1, 3])
    out = net(x)
    assert out.shape == (2, 3)
    assert np.allclose(out.asnumpy(), expect_out)

    net = Net(3)
    out = net(x)
    print(out)
    expect_out = [[[0., 1., 2.]],
                  [[3., 4., 5.]]]
    assert out.shape == (2, 1, 3)
    assert np.allclose(out.asnumpy(), expect_out)
