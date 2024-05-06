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

import mindspore.nn as nn
from mindspore.ops.operations import _sequence_ops as seq
from mindspore import context
from mindspore.common import mutable
from mindspore.ops.composite import GradOperation
from sequence_help import context_prepare

context.set_context(mode=context.GRAPH_MODE)
context_prepare()


class Net(nn.Cell):
    def __init__(self):
        super().__init__()
        self.seq_mul = seq.SequenceMul()

    def construct(self, x, y):
        return self.seq_mul(x, y)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu
@pytest.mark.env_onecard
def test_seq_mul_tuple_dy():
    """
    Feature: test sequence_mul op
    Description: first input is dynamic sequence
    Expectation: the result match with tuple result
    """
    x = mutable((1, 2, 3), True)
    y = 2
    expect = (1, 2, 3, 1, 2, 3)
    net = Net()
    res = net(x, y)
    assert res == expect


@pytest.mark.level1
@pytest.mark.platform_x86_gpu
@pytest.mark.env_onecard
def test_seq_mul_scalar_dy():
    """
    Feature: test sequence_mul op
    Description: second input is dynamic scalar
    Expectation: the result match with tuple result
    """
    x = (0, 1, 1, 2)
    y = mutable(1)
    expect = (0, 1, 1, 2)
    net = Net()
    res = net(x, y)
    assert res == expect


@pytest.mark.level0
@pytest.mark.platform_x86_gpu
@pytest.mark.env_onecard
def test_seq_mul_all_dy():
    """
    Feature: test sequence_mul op
    Description: two inputs are dynamic sequence
    Expectation: the result match with tuple result
    """
    x = mutable((1, 2, 3), True)
    y = mutable(3)
    expect = (1, 2, 3, 1, 2, 3, 1, 2, 3)
    net = Net()
    res = net(x, y)
    assert res == expect


@pytest.mark.level0
@pytest.mark.platform_x86_gpu
@pytest.mark.env_onecard
def test_seq_mul_grad():
    """
    Feature: test sequence_mul grad op
    Description: two inputs are dynamic sequence
    Expectation: the result match with tuple result
    """
    x = mutable((1, 2, 3), True)
    y = mutable(2)
    dout = (4, 5, 6, 7, 8, 9)
    net = Net()
    grad_func = GradOperation(get_all=True, sens_param=True)(net)
    grad_func(x, y, dout)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu
@pytest.mark.env_onecard
def test_seq_mul_grad_mutable_scalar():
    """
    Feature: test sequence_mul grad op
    Description: two inputs are dynamic sequence
    Expectation: the result match with tuple result
    """
    x = (1, mutable(2), 3)
    y = mutable(2)
    dout = (4, 5, 6, 7, 8, 9)
    net = Net()
    grad_func = GradOperation(get_all=True, sens_param=True)(net)
    grad_func(x, y, dout)
