# Copyright 2020 Huawei Technologies Co., Ltd
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

import mindspore.context as context
import mindspore.nn as nn
from mindspore import Tensor
from mindspore.common.api import jit
from mindspore.ops import operations as P
from mindspore.ops import function as F
from mindspore.ops.composite import GradOperation


class Grad(nn.Cell):
    def __init__(self, network):
        super(Grad, self).__init__()
        self.grad = GradOperation(get_all=True, sens_param=True)
        self.network = network

    @jit
    def construct(self, input_, output_grad):
        return self.grad(self.network)(input_, output_grad)


class Net(nn.Cell):
    def __init__(self):
        super(Net, self).__init__()
        self.ops = P.Tanh()

    def construct(self, x):
        return self.ops(x)


def get_standard_loss(data_type):
    if data_type == np.float16:
        loss = 1e-3
    elif data_type in (np.float32, np.complex64):
        loss = 1e-4
    elif data_type in (np.float64, np.complex128):
        loss = 1e-5
    else:
        loss = 0
    return loss


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize("data_type", [np.float16, np.float32, np.float64, np.complex64, np.complex128])
def test_net(data_type):
    """
    Feature: Tanh
    Description: test cases for Tanh
    Expectation: the result match to numpy
    """
    x = np.random.randn(2, 3, 3, 4).astype(data_type)
    y_expect = np.tanh(x)
    loss = get_standard_loss(data_type)

    context.set_context(mode=context.GRAPH_MODE, device_target='CPU')
    net = Net()
    out = net(Tensor(x))

    assert out.shape == y_expect.shape
    assert np.allclose(out.asnumpy(), y_expect, rtol=loss, atol=loss)

    sens = np.random.randn(2, 3, 3, 4).astype(data_type)
    backword_net = Grad(Net())
    output = backword_net(Tensor(x), Tensor(sens))
    print(len(output))
    print(output[0].asnumpy())


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize("data_type", [np.float16, np.float32, np.float64, np.complex64, np.complex128])
def test_func(data_type):
    """
    Feature: Tanh
    Description: test cases for Tanh
    Expectation: the result match to numpy
    """
    x = np.random.randn(2, 3, 3, 4).astype(data_type)
    y_expect = np.tanh(x)
    loss = get_standard_loss(data_type)

    context.set_context(mode=context.GRAPH_MODE, device_target="CPU")
    tensor = Tensor(x)
    out = F.tanh(tensor)

    assert out.shape == y_expect.shape
    assert np.allclose(out.asnumpy(), y_expect, rtol=loss, atol=loss)

    context.set_context(mode=context.PYNATIVE_MODE, device_target="CPU")
    tensor = Tensor(x)
    out = F.tanh(tensor)

    assert out.shape == y_expect.shape
    assert np.allclose(out.asnumpy(), y_expect, rtol=loss, atol=loss)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize("data_type", [np.float16, np.float32, np.float64, np.complex64, np.complex128])
def test_tensor(data_type):
    """
    Feature: Tanh
    Description: test cases for Tanh
    Expectation: the result match to numpy
    """
    x = np.random.randn(2, 3, 3, 4).astype(data_type)
    y_expect = np.tanh(x)
    loss = get_standard_loss(data_type)

    context.set_context(mode=context.GRAPH_MODE, device_target="CPU")

    tensor = Tensor(x)
    out = tensor.tanh()

    assert out.shape == y_expect.shape
    assert np.allclose(out.asnumpy(), y_expect, rtol=loss, atol=loss)

    context.set_context(mode=context.PYNATIVE_MODE, device_target="CPU")

    tensor = Tensor(x)
    out = tensor.tanh()

    assert out.shape == y_expect.shape
    assert np.allclose(out.asnumpy(), y_expect, rtol=loss, atol=loss)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_tanh_with_abnormal_input():
    """
    Feature: Tanh
    Description: test cases for Tanh with abnormal value
    Expectation: the result match to numpy
    """
    x = np.random.choice([np.nan, np.inf, -np.inf], size=(3, 10)).astype(np.float32)
    y_expect = np.tanh(x)

    tensor = Tensor(x)
    out = F.tanh(tensor)
    assert np.allclose(out.asnumpy(), y_expect, equal_nan=True)
