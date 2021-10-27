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
""" test numpy ops """
import pytest
import numpy as np

import mindspore.nn as nn
from mindspore import Tensor, ms_function, context
from mindspore.ops import operations as P
from mindspore.ops import functional as F
import mindspore.common.dtype as mstype
import mindspore.common._monad as monad

context.set_context(mode=context.GRAPH_MODE)

# `add_func` is defined in current file.
def add_func(x, y):
    return x + y


@ms_function
def do_increment(i):
    add_1 = F.partial(add_func, 1)
    return add_1(i)


def test_increment():
    a = do_increment(9)
    assert a == 10


@ms_function
def use_monad(x, y):
    res = P.Mul()(x, y)
    res = F.depend(res, monad.U)
    return res


def test_use_monad():
    x = Tensor(1.0, mstype.float32)
    y = Tensor(1.0, mstype.float32)
    print(use_monad(x, y))


class Net(nn.Cell):
    def __init__(self):
        super(Net, self).__init__()
        self.x = Tensor([2, 3, 4])

    def construct(self):
        x_len = len(self.x)
        for i in range(x_len):
            print(i)
        return x_len


def test_builtins_len():
    net = Net()
    net()


@ms_function
def np_fallback_func():
    array_x = tuple([2, 3, 4, 5])
    np_x = np.array(array_x).astype(np.float32)
    me_x = Tensor(np_x)
    me_x = me_x + me_x
    return me_x


@pytest.mark.skip(reason='Not support graph fallback feature yet')
def test_np_fallback_func():
    print(np_fallback_func())


# Test `return` interpret node.
@ms_function
def div_mod_func1():
    x = 8
    y = 3
    a = divmod(x, y)
    return Tensor(a)


@pytest.mark.skip(reason='Not support graph fallback feature yet')
def test_div_mod_func1():
    print(div_mod_func1())  # (2, 2)


# Test interpret node with parameters as input.
@ms_function
def div_mod_func2(x, y):
    a = divmod(x, y)
    return Tensor(a)


@pytest.mark.skip(reason='Not support graph fallback feature yet')
def test_div_mod_func2_scalar():
    """
    Feature: JIT Fallback
    Description: Test divmod in graph.
    Expectation: No exception.
    """
    print(div_mod_func2(8, 3))  # (2, 2)


@pytest.mark.skip(reason='Not support graph fallback feature yet')
def test_div_mod_func2_tensor():
    """
    Feature: JIT Fallback
    Description: Test divmod in graph.
    Expectation: No exception.
    """
    print(div_mod_func2(Tensor(8), Tensor(3)))  # name 'x' is not defined


# NameError: name 'Tensor' is not defined.
@ms_function
def select_func(cond, x, y):
    if isinstance(cond, (tuple, list)):
        output = y
    elif isinstance(cond, Tensor):
        output = F.select(cond, x, y)
    else:
        output = x
    return output


def test_select_func():
    cond = Tensor([True, False])
    x = Tensor([2, 3], mstype.float32)
    y = Tensor([1, 2], mstype.float32)
    print(select_func(cond, x, y))


# Not interpret 'Tensor'.
@ms_function
def select_func2(cond, x, y):
    if isinstance(cond, (tuple, list)):
        output = y
    if isinstance(cond, Tensor):
        output = F.select(cond, x, y)
    else:
        output = x
    return output


def test_select_func2():
    cond = Tensor([True, False])
    x = Tensor([2, 3], mstype.float32)
    y = Tensor([1, 2], mstype.float32)
    print(select_func2(cond, x, y))


# NameError: name 'Tensor' is not defined.
@ms_function
def slice_func(a, b):
    a[1:3, ::] = b
    return a


def test_slice_func():
    a = Tensor(np.arange(60).reshape(3, 4, 5), dtype=mstype.float32)
    b = Tensor([1], dtype=mstype.float32)
    print(slice_func(a, b))


@ms_function
def np_fallback_func_tensor_index(x):
    array_x = tuple([2, 3, 4, 5])
    np_x = np.array(array_x).astype(np.float32)
    me_x = Tensor(np_x)
    me_x = me_x + me_x
    return me_x[x]


@pytest.mark.skip(reason='Not support graph fallback feature yet')
def test_np_fallback_func_tensor_index():
    """
    Feature: Fallback feature: support Tensor index.
    Description: Fallback feature: support Tensor index.
    Expectation: Fallback feature: support Tensor index.
    """
    x = Tensor(1, mstype.int32)
    output = np_fallback_func_tensor_index(x)
    output_expect = Tensor(6, mstype.float32)
    assert output == output_expect


class ControlNet(nn.Cell):
    def __init__(self):
        super(ControlNet, self).__init__()

    def inner_function_1(self, a, b):
        return a + b

    def inner_function_2(self, a, b):
        return a - b

    def construct(self, x):
        a = Tensor(np.array(4), mstype.int32)
        b = Tensor(np.array(5), mstype.int32)
        if a + b > x:
            return self.inner_function_1(a, b)
        return self.inner_function_2(a, b)


@pytest.mark.skip(reason='Not support graph fallback feature yet')
def test_fallback_control_sink_tensor():
    """
    Feature: Fallback feature: support define Tensor in Class construct.
    Description: Fallback feature: support define Tensor in Class construct.
    Expectation: Fallback feature: support define Tensor in Class construct.
    """
    x = Tensor(np.array(1), mstype.int32)
    net = ControlNet()
    output = net(x)
    output_expect = Tensor(9, mstype.int32)
    assert output == output_expect
