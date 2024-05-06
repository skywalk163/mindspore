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
from mindspore import context
from mindspore.ops.operations import _sequence_ops as _seq
from mindspore.common import mutable
from mindspore.ops.composite import GradOperation
from sequence_help import context_prepare

context.set_context(mode=context.GRAPH_MODE)
context_prepare()


class NetTupleEqual(nn.Cell):
    def __init__(self):
        super().__init__()
        self.seq_equal = _seq.tuple_equal()

    def construct(self, x, y):
        return self.seq_equal(x, y)


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_seq_dyn_equal():
    """
    Feature: test sequence equal op
    Description: equal operation on tuple type
    Expectation: the behavior is matched to python style
    """
    x = mutable((1, 2, 3, 4, 5, 6), True)
    y = mutable((1, 2, 3, 2, 6), True)
    expect = False
    net = NetTupleEqual()
    res = net(x, y)
    assert res == expect


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_seq_dyn_equal1():
    """
    Feature: test sequence equal op
    Description: equal operation on tuple type
    Expectation: the behavior is matched to python style
    """
    x = mutable((1, 2, 3, 4, 5, 6), True)
    y = (1, 2, 3, 4, 5, 6)
    expect = True
    net = NetTupleEqual()
    res = net(x, y)
    assert res == expect


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_seq_equal():
    """
    Feature: test sequence equal op
    Description: equal operation on tuple type
    Expectation: the behavior is matched to python style
    """
    x = (1, 2, 3, 4, 5)
    y = (True, 2, 3, 4, 5)
    expect = False
    net = NetTupleEqual()
    res = net(x, y)
    assert res == expect


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_seq_equal_grad():
    """
    Feature: test sequence equal grad op
    Description: equal operation on tuple type
    Expectation: the behavior is matched to python style
    """
    net_ms = NetTupleEqual()
    x = mutable((1, 2, 3, 4, 5, 6), True)
    y = mutable((1, 2, 3, 4, 5, 6), True)
    dout = True
    grad_func = GradOperation(get_all=True, sens_param=True)(net_ms)
    print("grad out1 = ", grad_func(x, y, dout))
