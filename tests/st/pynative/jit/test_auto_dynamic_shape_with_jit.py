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

import platform
import numpy as np
import pytest

from mindspore import nn
from mindspore import ops
from mindspore import context, Tensor
from mindspore import jit

context.set_context(mode=context.PYNATIVE_MODE)


class NetInner(nn.Cell):
    def __init__(self):
        super(NetInner, self).__init__()
        self.log = ops.Log()
        self.exp = ops.Exp()
        self.addn = ops.AddN()
        self.relu = nn.ReLU()

    @jit
    def construct(self, x, y):
        x = self.addn((x, y))
        x = self.log(x)
        x = self.exp(x)
        x = self.relu(x)
        x = self.addn((x, y))
        return x


class Net(nn.Cell):
    def __init__(self):
        super(Net, self).__init__()
        self.log = ops.Log()
        self.exp = ops.Exp()
        self.addn = ops.AddN()
        self.relu = nn.ReLU()
        self.inner = NetInner()

    def construct(self, x, y):
        x = self.addn((x, y))
        x = self.inner(x, y)
        x = self.log(x)
        x = self.exp(x)
        x = self.relu(x)
        return x


class NetOuter(nn.Cell):
    def __init__(self):
        super(NetOuter, self).__init__()
        self.log = ops.Log()
        self.exp = ops.Exp()
        self.addn = ops.AddN()
        self.relu = nn.ReLU()
        self.inner = NetInner()

    @jit
    def construct(self, x, y):
        x = self.addn((x, y))
        x = self.inner(x, y)
        x = self.log(x)
        x = self.exp(x)
        x = self.relu(x)
        return x


class CmpNetInner(nn.Cell):
    def __init__(self):
        super(CmpNetInner, self).__init__()
        self.log = ops.Log()
        self.exp = ops.Exp()
        self.addn = ops.AddN()
        self.relu = nn.ReLU()

    @jit
    def construct(self, x, y):
        x = self.addn((x, y))
        x = self.log(x)
        x = self.exp(x)
        x = self.relu(x)
        x = self.addn((x, y))
        return x


class CmpNet(nn.Cell):
    def __init__(self):
        super(CmpNet, self).__init__()
        self.log = ops.Log()
        self.exp = ops.Exp()
        self.addn = ops.AddN()
        self.relu = nn.ReLU()
        self.inner = CmpNetInner()

    def construct(self, x, y):
        x = self.addn((x, y))
        x = self.inner(x, y)
        x = self.log(x)
        x = self.exp(x)
        x = self.relu(x)
        return x


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_pynative_auto_dynamic_shape_with_inner_jit():
    """
    Feature: PyNative auto dynamic shape.
    Description: The static shape is automatically converted to a dynamic shape.
    Expectation: The calculation result is correct.
    """
    if platform.system() == 'Windows':
        return

    net = Net()
    grad_op = ops.GradOperation(get_all=True, get_by_list=False, sens_param=True)

    # run first shape
    input_x = Tensor(np.random.rand(2, 3, 6, 8).astype(np.float32) * 2)
    input_y = Tensor(np.random.rand(2, 3, 6, 8).astype(np.float32) * 5)
    out = net(input_x, input_y)
    _ = grad_op(net)(input_x, input_y, out)

    # run second shape
    input_x2 = Tensor(np.random.rand(2, 3, 6, 16).astype(np.float32) * 2)
    input_y2 = Tensor(np.random.rand(2, 3, 6, 16).astype(np.float32) * 5)
    out = net(input_x2, input_y2)
    _ = grad_op(net)(input_x2, input_y2, out)

    # run third shape
    input_x3 = Tensor(np.random.rand(2, 3, 6, 34).astype(np.float32) * 2)
    input_y3 = Tensor(np.random.rand(2, 3, 6, 34).astype(np.float32) * 5)
    out = net(input_x3, input_y3)
    grad = grad_op(net)(input_x3, input_y3, out)

    cmp_net = CmpNet()
    cmp_out = cmp_net(input_x3, input_y3)
    cmp_grad = grad_op(cmp_net)(input_x3, input_y3, cmp_out)
    assert np.allclose(grad[0].asnumpy(), cmp_grad[0].asnumpy(), 0.00001, 0.00001)
    assert np.allclose(grad[1].asnumpy(), cmp_grad[1].asnumpy(), 0.00001, 0.00001)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_pynative_auto_dynamic_shape_with_outer_jit():
    """
    Feature: PyNative auto dynamic shape.
    Description: The static shape is automatically converted to a dynamic shape.
    Expectation: The calculation result is correct.
    """
    if platform.system() == 'Windows':
        return

    net = NetOuter()
    grad_op = ops.GradOperation(get_all=True, get_by_list=False, sens_param=True)

    # run first shape
    input_x = Tensor(np.random.rand(2, 3, 6, 8).astype(np.float32) * 2)
    input_y = Tensor(np.random.rand(2, 3, 6, 8).astype(np.float32) * 5)
    out = net(input_x, input_y)
    _ = grad_op(net)(input_x, input_y, out)

    # run second shape
    input_x2 = Tensor(np.random.rand(2, 3, 6, 16).astype(np.float32) * 2)
    input_y2 = Tensor(np.random.rand(2, 3, 6, 16).astype(np.float32) * 5)
    out = net(input_x2, input_y2)
    _ = grad_op(net)(input_x2, input_y2, out)

    # run third shape
    input_x3 = Tensor(np.random.rand(2, 3, 6, 34).astype(np.float32) * 2)
    input_y3 = Tensor(np.random.rand(2, 3, 6, 34).astype(np.float32) * 5)
    out = net(input_x3, input_y3)
    grad = grad_op(net)(input_x3, input_y3, out)

    cmp_net = CmpNet()
    cmp_out = cmp_net(input_x3, input_y3)
    cmp_grad = grad_op(cmp_net)(input_x3, input_y3, cmp_out)
    assert np.allclose(grad[0].asnumpy(), cmp_grad[0].asnumpy(), 0.00001, 0.00001)
    assert np.allclose(grad[1].asnumpy(), cmp_grad[1].asnumpy(), 0.00001, 0.00001)
