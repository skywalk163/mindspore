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
import pytest
import numpy as np
import mindspore.context as context
import mindspore.nn as nn
from mindspore import Tensor
from mindspore.ops import operations as P
from mindspore.ops.composite import GradOperation


dtype = np.float16
x0 = Tensor(np.random.randn(3, 4, 3, 3).astype(dtype))
x1 = Tensor(np.random.randn(3, 4, 3, 3).astype(dtype))


class Net(nn.Cell):
    def __init__(self, keep_prob):
        super(Net, self).__init__()
        self.drop = P.Dropout2D(keep_prob=keep_prob)

    def construct(self, x):
        return self.drop(x)


class Grad(nn.Cell):
    def __init__(self, network):
        super(Grad, self).__init__()
        self.grad = GradOperation(get_all=True, sens_param=True)
        self.network = network
        self.network.set_train()

    def construct(self, x, y):
        return self.grad(self.network)(x, y)


@pytest.mark.level0
@pytest.mark.env_onecard
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.parametrize("context_mode", [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_net_float32(context_mode):
    """
    Feature: aicpu ops Dropout2D.
    Description: test Dropout2D forward.
    Expectation: expect correct result.
    """
    context.set_context(mode=context_mode, device_target="Ascend")
    net = Net(0.7)
    output, mask = net(x0)
    print(x0)
    print(output)

    y = (output.asnumpy() == (x0.asnumpy()/0.7).astype(dtype)).reshape(3*4, 3*3)
    output_reshape = output.asnumpy().reshape(3*4, 3*3)
    for i in range(3*4):
        if not y[i].all():
            assert output_reshape[i].sum() == 0
    return output, mask


@pytest.mark.level0
@pytest.mark.env_onecard
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.parametrize("context_mode", [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_net_grad(context_mode):
    """
    Feature: aicpu ops Dropout2D.
    Description: test Dropout2D forward.
    Expectation: expect correct result.
    """
    context.set_context(mode=context_mode, device_target="Ascend")
    net = Grad(Net(0.7))
    y = test_net_float32(context_mode)
    output = net(x1, y)
    print("input: ", x1)
    print("forward output: ", y)
    print("backward output: ", output)
