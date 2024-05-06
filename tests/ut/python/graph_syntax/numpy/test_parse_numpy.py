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
""" test_parse_numpy """
import numpy as np
from mindspore import nn, context, jit, Tensor

context.set_context(mode=context.GRAPH_MODE)


np_array_data = np.array([1, 2, 3])


def test_use_numpy_constant():
    class Net(nn.Cell):
        def __init__(self):
            super(Net, self).__init__()

        def construct(self):
            ret = np.pi
            return ret

    net = Net()
    output = net()
    assert np.allclose(output, np.pi)


def test_use_numpy_method():
    class Net(nn.Cell):
        def __init__(self):
            super(Net, self).__init__()

        def construct(self):
            ret = np.linspace(1, 10, 4)
            return ret

    net = Net()
    net()


def test_use_numpy_module():
    class Net(nn.Cell):
        def __init__(self):
            super(Net, self).__init__()

        def construct(self):
            ret = np.random.randint(0, 10, [1, 10])
            return ret

    net = Net()
    net()


def test_np_calculate():
    """
    Feature: Fallback feature.
    Description: Support numpy calculation.
    Expectation: No exception.
    """
    @jit
    def np_calculate():
        x = np.array([3, 1, 2, 4, 5])
        y = x % 2
        z = Tensor(y)
        return z
    assert np.all(np_calculate().asnumpy() == np.array([1, 1, 0, 0, 1]))


def test_np_array():
    """
    Feature: Fallback feature.
    Description: Support numpy calculation.
    Expectation: No exception.
    """
    @jit
    def func():
        return Tensor(np_array_data)

    assert np.all(func().asnumpy() == np_array_data)
