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
import pytest
import numpy as np

from mindspore import context, Tensor
from mindspore.common import mutable
from mindspore.nn import Cell
from mindspore.ops.composite import GradOperation
from mindspore._extends.parse.standard_method import list_append
from sequence_help import context_prepare

context.set_context(mode=context.GRAPH_MODE)
context_prepare()


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_list_append1():
    """
    Feature: test sequence getitem op
    Description: setitem operation on tuple type
    Expectation: the behavior is matched to python style
    """
    class Net(Cell):
        def construct(self, x, y):
            return list_append(x, y)

    net_ms = Net()
    input_x = mutable([2], True)
    input_y = mutable(3)
    res = net_ms(input_x, input_y)
    expect = [2, 3]
    assert res == expect


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_list_append2():
    """
    Feature: test sequence getitem op
    Description: setitem operation on tuple type
    Expectation: the behavior is matched to python style
    """
    class Net(Cell):
        def construct(self, x, y):
            return list_append(x, y)

    net_ms = Net()
    input_x = mutable([Tensor(2)], True)
    input_y = Tensor(3)
    res = net_ms(input_x, input_y)
    expect = [Tensor(2), Tensor(3)]
    for i in range(2):
        assert np.all(res[i].asnumpy() == expect[i].asnumpy())


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_list_append3():
    """
    Feature: test sequence getitem op
    Description: setitem operation on tuple type
    Expectation: the behavior is matched to python style
    """
    class Net(Cell):
        def construct(self, x, y):
            return list_append(x, y)

    net_ms = Net()
    input_x = mutable([Tensor([[2, 3], [4, 5]])], True)
    input_y = Tensor([[2, 3], [4, 5]])
    res = net_ms(input_x, input_y)
    expect = [Tensor([[2, 3], [4, 5]]), Tensor([[2, 3], [4, 5]])]
    for i in range(2):
        assert np.all(res[i].asnumpy() == expect[i].asnumpy())


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_list_append_grad():
    """
    Feature: test sequence getitem grad op
    Description: setitem operation on tuple type
    Expectation: the behavior is matched to python style
    """
    class Net(Cell):
        def construct(self, x, y):
            return list_append(x, y)

    net_ms = Net()
    seq = mutable((1, 2, 3, 4, 5, 6), True)
    value = 1
    dout = (1, 2, 3, 4, 5, 6, 1)
    grad_func = GradOperation(get_all=True, sens_param=True)(net_ms)
    print("grad out1 = ", grad_func(seq, value, dout))
