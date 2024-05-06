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
from mindspore import Tensor


class Net(nn.Cell):
    def construct(self, x, y):
        output = x.eq(y)
        return output


class Net1(nn.Cell):
    def construct(self, x, y):
        return x == y


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_eq(mode):
    """
    Feature: tensor.eq
    Description: Verify the result of tensor.eq
    Expectation: success
    """
    ms.set_context(mode=mode)
    net = Net()

    inputs = Tensor(np.array([[1.0, 0.0], [0.0, 2.0]]))
    other = Tensor(np.array([[1.0, 1.0], [1.0, 2.0]]))
    value = net(inputs, other)
    expect_value = np.array([[True, False], [False, True]])
    assert np.allclose(expect_value, value.asnumpy())


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_eq_broadcast(mode):
    """
    Feature: tensor.eq broadcast
    Description: Verify the result of tensor.eq broadcast
    Expectation: success
    """
    ms.set_context(mode=mode)
    net = Net()

    inputs = Tensor(np.array([[1.0, 0.0], [0.0, 2.0]]))
    other = Tensor(np.array([1.0]))
    value = net(inputs, other)
    expect_value = np.array([[True, False], [False, False]])
    assert np.allclose(expect_value, value.asnumpy())


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_tensor_eq(mode):
    """
    Feature: __eq__ in Tensor.
    Description: Verify the result of __eq__ in Tensor.
    Expectation: success
    """
    ms.set_context(mode=mode)
    net = Net1()

    inputs = Tensor(np.array([[True, False], [False, True]]))
    other = Tensor(np.array([[True, True], [True, True]]))
    value = net(inputs, other)
    expect_value = np.array([[True, False], [False, True]])
    assert np.allclose(expect_value, value.asnumpy())
