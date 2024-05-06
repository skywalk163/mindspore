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

import numpy as np
import pytest
import mindspore as ms
import mindspore.context as context
import mindspore.nn as nn
from mindspore import Tensor
from mindspore.ops.operations.array_ops import RightShift


class NetRightShift(nn.Cell):

    def __init__(self):
        super(NetRightShift, self).__init__()
        self.rightshift = RightShift()

    def construct(self, x, y):
        return self.rightshift(x, y)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu
@pytest.mark.env_onecard
def test_rightshift_dyn():
    """
    Feature: test RightShift ops in gpu.
    Description: test the ops in dynamic shape.
    Expectation: expect correct shape result.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")

    net = NetRightShift()
    x_dyn = Tensor(shape=[None, None], dtype=ms.int8)
    y_dyn = Tensor(shape=[None], dtype=ms.int8)
    net.set_inputs(x_dyn, y_dyn)

    x = Tensor([[1, 2, 3], [1, 2, 3]], dtype=ms.int8)
    y = Tensor([1, 1, 1], dtype=ms.int8)
    out = net(x, y)

    expect_shape = (2, 3)
    assert out.asnumpy().shape == expect_shape


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_rightshift_1d():
    """
    Feature: RightShift gpu TEST.
    Description: 1d test case for RightShift
    Expectation: the result match to numpy
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")
    x_np = (np.array([-1, -5, -3, -14, 64])).astype(np.int8)
    y_np = (np.array([5, 0, 7, 11, 1])).astype(np.int8)
    z_np = np.right_shift(x_np, y_np)

    x_ms = Tensor(x_np)
    y_ms = Tensor(y_np)
    net = NetRightShift()
    z_ms = net(x_ms, y_ms)

    assert np.allclose(z_np, z_ms.asnumpy())


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_rightshift_2d():
    """
    Feature: RightShift gpu TEST.
    Description: 2d test case for RightShift
    Expectation: the result match to numpy
    """
    context.set_context(mode=context.PYNATIVE_MODE, device_target="GPU")
    x_np = (np.array([[-1, -5, -3], [-14, 64, 0]])).astype(np.int8)
    y_np = (np.array([[5, 0, 7], [11, 1, 0]])).astype(np.int8)
    z_np = np.right_shift(x_np, y_np)

    x_ms = Tensor(x_np)
    y_ms = Tensor(y_np)
    net = NetRightShift()
    z_ms = net(x_ms, y_ms)

    assert np.allclose(z_np, z_ms.asnumpy())
