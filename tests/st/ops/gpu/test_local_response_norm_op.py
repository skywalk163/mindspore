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

import numpy as np
import pytest

import mindspore
import mindspore.context as context
import mindspore.nn as nn
from mindspore import Tensor
from mindspore.common.parameter import ParameterTuple
from mindspore.ops import operations as P
from mindspore.ops import composite as C


class MSLRNOpNet(nn.Cell):
    def __init__(self):
        super(MSLRNOpNet, self).__init__()
        self.lrn1 = P.LRN(depth_radius=2, bias=1.0, alpha=0.0001, beta=0.75)

    def construct(self, x):
        x = self.lrn1(x)
        return x


class MSGradNet(nn.Cell):
    def __init__(self, network):
        super(MSGradNet, self).__init__()
        self.grad = C.GradOperation(get_all=True, get_by_list=True)
        self.network = network
        self.params = ParameterTuple(network.trainable_params())

    def construct(self, x):
        grad_op = self.grad(self.network, self.params)
        output = grad_op(x)
        return output


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_lrn_ms():
    x = Tensor(np.array([[[[1.6243454, -0.6117564],
                           [-0.5281718, -1.0729686]],
                          [[0.86540765, -2.3015387],
                           [1.7448118, -0.7612069]],
                          [[0.3190391, -0.24937038],
                           [1.4621079, -2.0601406]]]]).astype(np.float32))
    y_exp = np.array([[[[1.6239204, -0.61149347],
                        [-0.5279556, -1.0724881]],
                       [[0.86518127, -2.3005495],
                        [1.7440975, -0.760866]],
                       [[0.31895563, -0.2492632],
                        [1.4615093, -2.059218]]]]).astype(np.float32)
    context.set_context(mode=context.PYNATIVE_MODE, device_target="GPU")
    net = MSLRNOpNet()
    output = net(x)
    assert np.allclose(output.asnumpy(), y_exp)
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")
    net = MSLRNOpNet()
    output = net(x)
    assert np.allclose(output.asnumpy(), y_exp)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_lrn_grad():
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")
    x = Tensor(np.array([[[[1.6243454, -0.6117564],
                           [-0.5281718, -1.0729686]],
                          [[0.86540765, -2.3015387],
                           [1.7448118, -0.7612069]],
                          [[0.3190391, -0.24937038],
                           [1.4621079, -2.0601406]]]]).astype(np.float32))
    dx_exp = np.array([[[[0.9990543, 0.9992802],
                         [0.99980265, 0.99892604]],
                        [[0.9993739, 0.99847937],
                         [0.9988902, 0.99910796]],
                        [[0.9996039, 0.99945194],
                         [0.9990037, 0.99834996]]]]).astype(np.float32)
    net = MSLRNOpNet()
    grad_net = MSGradNet(net)
    grad_net.set_train(True)
    output = grad_net(x)
    dx = output[0][0].asnumpy()
    assert np.allclose(dx, dx_exp, atol=1.0e-4, rtol=1.0e-4, equal_nan=True)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_lrn_grad_dynamic_shape():
    """
    Feature: test lrn_grad op in gpu.
    Description: test the ops in dynamic shape.
    Expectation: expect correct shape result.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")
    x_dyn = Tensor(shape=[1, 32, 64, None], dtype=mindspore.float32)
    net = MSLRNOpNet()
    grad_net = MSGradNet(net)
    grad_net.set_train(True)
    grad_net.set_inputs(x_dyn)
    x = np.random.randn(1, 32, 64, 64)
    output = grad_net(Tensor(x, mindspore.float32))
    expect_shape = (1, 32, 64, 64)
    assert output[0][0].asnumpy().shape == expect_shape
