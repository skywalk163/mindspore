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
from mindspore.ops.composite import GradOperation

context.set_context(mode=context.GRAPH_MODE, device_target='CPU')


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
        self.ops = P.Neg()

    def construct(self, x):
        return self.ops(x)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_net():
    x = np.random.randn(2, 3, 3, 4).astype(np.float32)
    y_expect = -x
    net = Net()
    out = net(Tensor(x))
    assert (out.asnumpy() == y_expect).all()
    sens = np.random.randn(2, 3, 3, 4).astype(np.float32)
    backword_net = Grad(Net())
    output = backword_net(Tensor(x), Tensor(sens))
    print(len(output))
    print(output[0].asnumpy())


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
@pytest.mark.parametrize('dtype', [np.uint8, np.uint16, np.uint32, np.uint64,
                                   np.int8, np.int16, np.int32, np.int64,
                                   np.float16, np.float32, np.float64,
                                   np.complex64, np.complex128])
def test_neg_tensor_api_modes(mode, dtype):
    """
    Feature: Test neg tensor api.
    Description: Test neg tensor api for Graph and PyNative modes.
    Expectation: The result match to the expect value.
    """
    context.set_context(mode=mode, device_target="CPU")
    x_np = np.array([1, 2, -1, 2, 0, -5], dtype=dtype)
    output_np = np.negative(x_np)
    x = Tensor(x_np)
    output = x.neg()
    np.testing.assert_array_equal(output.asnumpy(), output_np)
