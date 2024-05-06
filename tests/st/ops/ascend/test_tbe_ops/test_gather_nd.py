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
import mindspore
import mindspore.context as context
import mindspore.nn as nn
import mindspore.ops as ops
from mindspore import Tensor
from mindspore.ops import operations as P


class TensorTestNet(nn.Cell):
    def construct(self, x, indices):
        return x.gather_nd(indices)


class GatherNdNet(nn.Cell):
    def __init__(self):
        super(GatherNdNet, self).__init__()
        self.gather_nd = P.GatherNd()

    def construct(self, input_x, indices):
        return self.gather_nd(input_x, indices)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_tensor_graph_mode():
    """
    Feature: gather_nd tensor test on graph mode.
    Description: test gather_nd tensor's interface on graph mode.
    Expectation: the result equal to expect.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")
    input_x = Tensor(np.array([[-0.1, 0.3, 3.6], [0.4, 0.5, -3.2]]), mindspore.float32)
    indices = Tensor(np.array([[0, 0], [1, 1]]), mindspore.int32)
    net = TensorTestNet()
    output = net(input_x, indices)
    expect_np = np.array([-0.1, 0.5])
    rtol = 1.e-4
    atol = 1.e-4
    assert np.allclose(output.asnumpy(), expect_np, rtol, atol, equal_nan=True)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_tensor_pynative_mode():
    """
    Feature: gather_nd tensor test on pynative mode.
    Description: test gather_nd tensor's interface on pynative mode.
    Expectation: the result equal to expect.
    """
    context.set_context(mode=context.PYNATIVE_MODE, device_target="Ascend")
    input_x = Tensor(np.array([[-0.1, 0.3, 3.6], [0.4, 0.5, -3.2]]), mindspore.float32)
    indices = Tensor(np.array([[0, 0], [1, 1]]), mindspore.int32)
    output = input_x.gather_nd(indices)
    expect_np = np.array([-0.1, 0.5])
    rtol = 1.e-4
    atol = 1.e-4
    assert np.allclose(output.asnumpy(), expect_np, rtol, atol, equal_nan=True)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_functional_pynative_mode():
    """
    Feature: gather_nd functional test on pynative mode.
    Description: test gather_nd functional's interface on pynative mode.
    Expectation: the result equal to expect.
    """
    context.set_context(mode=context.PYNATIVE_MODE, device_target="Ascend")
    input_x = Tensor(np.array([[-0.1, 0.3, 3.6], [0.4, 0.5, -3.2]]), mindspore.float32)
    indices = Tensor(np.array([[0, 0], [1, 1]]), mindspore.int32)
    output = ops.gather_nd(input_x, indices)
    expect_np = np.array([-0.1, 0.5])
    rtol = 1.e-4
    atol = 1.e-4
    assert np.allclose(output.asnumpy(), expect_np, rtol, atol, equal_nan=True)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_gather_nd_static_pynative_mode():
    """
    Feature: gather_nd static shape test on pynative mode.
    Description: test static shape for gather_nd on pynative mode.
    Expectation: the result equal to expect.
    """
    context.set_context(mode=context.PYNATIVE_MODE, device_target="Ascend")
    input_x = Tensor(np.array([[-0.1, 0.3, 3.6], [0.4, 0.5, -3.2]]), mindspore.float32)
    indices = Tensor(np.array([[0, 0], [1, 1]]), mindspore.int32)
    expect_np = np.array([-0.1, 0.5])
    net = GatherNdNet()
    output = net(Tensor(input_x), Tensor(indices))
    rtol = 1.e-4
    atol = 1.e-4
    assert np.allclose(output.asnumpy(), expect_np, rtol, atol, equal_nan=True)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_gather_nd_static_graph_mode():
    """
    Feature: gather_nd static shape test on graph mode.
    Description: test static shape for gather_nd on graph mode.
    Expectation: the result equal to expect.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")
    input_x = Tensor(np.array([[-0.1, 0.3, 3.6], [0.4, 0.5, -3.2]]), mindspore.float32)
    indices = Tensor(np.array([[0, 0], [1, 1]]), mindspore.int32)
    expect_np = np.array([-0.1, 0.5])
    net = GatherNdNet()
    output = net(Tensor(input_x), Tensor(indices))
    rtol = 1.e-4
    atol = 1.e-4
    assert np.allclose(output.asnumpy(), expect_np, rtol, atol, equal_nan=True)
