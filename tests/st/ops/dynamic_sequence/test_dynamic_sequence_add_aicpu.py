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
# pylint: disable=ungrouped-imports
# pylint: disable=unused-import
import os
import pytest

import mindspore.nn as nn
from mindspore.ops.operations import _sequence_ops as seq
from mindspore import context
from mindspore.common import mutable
from mindspore.nn import Cell
from mindspore.ops.composite import GradOperation
from mindspore.ops._op_impl.aicpu.sequence_add_offset import _sequence_add_offset_aicpu
from mindspore.ops._op_impl.aicpu.sequence_add import _sequence_add_aicpu

context.set_context(mode=context.GRAPH_MODE)
os.environ["GRAPH_OP_RUN"] = "0"


class NetAdd(nn.Cell):
    def __init__(self):
        super().__init__()
        self.seq_add = seq.SequenceAdd()

    def construct(self, x, y):
        return self.seq_add(x, y)


class NetAddOffset(nn.Cell):
    def __init__(self):
        super().__init__()
        self.seq_add_offset = seq.SequenceAddOffset()

    def construct(self, x, y):
        return self.seq_add_offset(x, y)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_seq_add_aicpu():
    """
    Feature: test aicpu sequence_add op
    Description: two inputs are dynamic sequence
    Expectation: the result match with tuple result
    """
    x = mutable((1, 2, 3), True)
    y = mutable((4, 5, 6), True)
    expect = (1, 2, 3, 4, 5, 6)
    net = NetAdd()
    res = net(x, y)
    assert res == expect


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_seq_add_offset_aicpu():
    """
    Feature: test aicpu sequence_add_offset op
    Description: inputs are dynamic sequence.
    Expectation: the result match with tuple result
    """
    x = mutable((1, 2, 3), True)
    y = mutable((4, 5, 6), True)
    expect = (0, 3)
    net = NetAddOffset()
    res = net(x, y)
    assert res == expect


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_seq_add_grad_aicpu():
    """
    Feature: test aicpu sequence add grad op
    Description: inputs are dynamic sequence.
    Expectation: the result match with tuple result
    """
    class Net(Cell):
        def construct(self, x, y):
            return x + y

    net_ms = Net()
    input_x = mutable((1, 2, 3), True)
    input_y = mutable((3, 4, 5, 6), True)
    dout = (1, 1, 1, 1, 1, 1, 1)
    grad_func = GradOperation(get_all=True, sens_param=True)(net_ms)
    print("grad out = ", grad_func(input_x, input_y, dout))


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_seq_add_grad_other_aicpu():
    """
    Feature: test aicpu sequence add grad op
    Description: inputs are dynamic sequence.
    Expectation: the result match with tuple result
    """
    class Net(Cell):
        def construct(self, x, y):
            return x + y

    net_ms = Net()
    input_x = mutable((1, 2, 3), True)
    input_y = (3, 4, 5, 6)
    dout = (1, 1, 1, 1, 1, 1, 1)
    grad_func = GradOperation(get_all=True, sens_param=True)(net_ms)
    print("grad out1 = ", grad_func(input_x, input_y, dout))
