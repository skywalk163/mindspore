# Copyright 2022-2023 Huawei Technologies Co., Ltd
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
"""test function grad in graph mode"""
import numpy as np
import pytest
import mindspore.nn as nn
import mindspore as ms
from mindspore import Tensor, Parameter, ops
from mindspore.ops import GradOperation, grad, get_grad
from mindspore.common import dtype as mstype
from mindspore.ops import composite as C


class GradOperationNet(nn.Cell):
    def __init__(self, net, get_all=False, get_by_list=False, sens_param=False):
        super(GradOperationNet, self).__init__()
        self.net = net
        self.grad_op = GradOperation(get_all=get_all, get_by_list=get_by_list, sens_param=sens_param)

    def construct(self, *args):
        gradient_function = self.grad_op(self.net)
        return gradient_function(*args)


class GradOperationNetWrtParameter(nn.Cell):
    def __init__(self, net, get_all=False, get_by_list=False):
        super(GradOperationNetWrtParameter, self).__init__()
        self.net = net
        self.params = net.trainable_params()
        self.grad_op = GradOperation(get_all=get_all, get_by_list=get_by_list)

    def construct(self, *args):
        gradient_function = self.grad_op(self.net, self.params[0])
        return gradient_function(*args)


class GradOperationNetWrtParameterTuple(nn.Cell):
    def __init__(self, net, get_all=False, get_by_list=False):
        super(GradOperationNetWrtParameterTuple, self).__init__()
        self.net = net
        self.params = net.trainable_params()
        self.grad_op = GradOperation(get_all=get_all, get_by_list=get_by_list)

    def construct(self, *args):
        gradient_function = self.grad_op(self.net, self.params)
        return gradient_function(*args)


class GradOperationNetWrtParameterNone(nn.Cell):
    def __init__(self, net, get_all=False, get_by_list=False):
        super(GradOperationNetWrtParameterNone, self).__init__()
        self.net = net
        self.grad_op = GradOperation(get_all=get_all, get_by_list=get_by_list)

    def construct(self, *args):
        gradient_function = self.grad_op(self.net, None)
        return gradient_function(*args)


def check_grad_result(output, expect):
    if isinstance(expect, Tensor):
        assert isinstance(output, Tensor)
        assert np.all(output.asnumpy() == expect.asnumpy())
    elif isinstance(expect, tuple):
        assert isinstance(output, tuple)
        assert len(output) == len(expect)
        for x, y in zip(output, expect):
            check_grad_result(x, y)
    else:
        raise TypeError("expect must be Tensor or tuple, but got {}.".format(type(expect)))


def check_grad_with_ids_result(output, expect):
    if isinstance(expect, Tensor):
        assert isinstance(output, Tensor)
        assert np.all(output.asnumpy() == expect.asnumpy())
    elif isinstance(expect, int):
        assert isinstance(output, int)
        assert expect == output
    elif isinstance(expect, str):
        assert isinstance(output, str)
        assert expect == output
    elif isinstance(expect, tuple):
        assert isinstance(output, tuple)
        assert len(output) == len(expect)
        for x, y in zip(output, expect):
            check_grad_with_ids_result(x, y)
    else:
        raise TypeError(
            "expect must be Tensor or tuple, but got {}.".format(type(expect)))


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_default_single_input(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with default args in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x):
            return self.w * x + self.b

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = Tensor([6], mstype.int32)
    out = GradOperationNet(Net(w, b))(x)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_default_multiple_inputs(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with default args in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = Tensor([6], mstype.int32)
    out = GradOperationNet(Net(w, b))(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_default_no_input(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation without input in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self):
            return self.w + self.b

    ms.set_context(mode=mode)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ()
    out = GradOperationNet(Net(w, b))()
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_single_input(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with single input in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x):
            return self.w * x + self.b

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = (Tensor([6], mstype.int32),)
    out = GradOperationNet(Net(w, b), get_all=True)(x)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_multiple_inputs(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with multiple inputs in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = (Tensor([6], mstype.int32), Tensor([2], mstype.int32))
    out = GradOperationNet(Net(w, b), get_all=True)(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_no_input(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation without input in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self):
            return self.w + self.b

    ms.set_context(mode=mode)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ()
    out = GradOperationNet(Net(w, b), get_all=True)()
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_single_param(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with single Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x):
            return self.w * x + self.b

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect_graph = Tensor([10], mstype.int32)
    out = GradOperationNetWrtParameter(Net(w, b), get_by_list=True)(x)
    check_grad_result(out, expect_graph)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_single_param_tuple(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with single Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')

        def construct(self, x):
            return self.w * x

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    w = Tensor([6], mstype.int32)
    expect = (Tensor([10], mstype.int32),)
    out = GradOperationNetWrtParameterTuple(Net(w), get_by_list=True)(x)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_multiple_params(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with multiple Parameters in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x):
            return self.w * x + self.b

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = (Tensor([10], mstype.int32), Tensor([1], mstype.int32))
    out = GradOperationNetWrtParameterTuple(Net(w, b), get_by_list=True)(x)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_no_param(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation without Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            return 6 * x

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    expect = ()
    out = GradOperationNetWrtParameterTuple(Net(), get_by_list=True)(x)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_single_input_and_single_param(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with single input and single Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x):
            return self.w * x + self.b

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ((Tensor([6], mstype.int32),), Tensor([10], mstype.int32))
    out = GradOperationNetWrtParameter(Net(w, b), get_all=True, get_by_list=True)(x)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_single_input_and_single_param_tuple(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with single input and single Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')

        def construct(self, x):
            return self.w * x

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    w = Tensor([6], mstype.int32)
    expect = ((Tensor([6], mstype.int32),), (Tensor([10], mstype.int32),))
    out = GradOperationNetWrtParameterTuple(Net(w), get_all=True, get_by_list=True)(x)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_single_input_and_multiple_params(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with single input and multiple Parameters in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x):
            return self.w * x + self.b

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ((Tensor([6], mstype.int32),), (Tensor([10], mstype.int32), Tensor([1], mstype.int32)))
    out = GradOperationNetWrtParameterTuple(Net(w, b), get_all=True, get_by_list=True)(x)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_multiple_inputs_and_single_param(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with multiple inputs and single Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ((Tensor([6], mstype.int32), Tensor([2], mstype.int32)), Tensor([10], mstype.int32))
    out = GradOperationNetWrtParameter(Net(w, b), get_all=True, get_by_list=True)(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_multiple_inputs_and_single_param_tuple(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with multiple inputs and single Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')

        def construct(self, x, y):
            return self.w * x + y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    expect = ((Tensor([6], mstype.int32), Tensor([1], mstype.int32)), (Tensor([10], mstype.int32),))
    out = GradOperationNetWrtParameterTuple(Net(w), get_all=True, get_by_list=True)(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_multiple_inputs_and_multiple_params(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with multiple inputs and multiple Parameters in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ((Tensor([6], mstype.int32), Tensor([2], mstype.int32)),
              (Tensor([10], mstype.int32), Tensor([20], mstype.int32)))
    out = GradOperationNetWrtParameterTuple(Net(w, b), get_all=True, get_by_list=True)(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_no_input_and_single_param(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with single Parameter without input in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self):
            return self.w + self.b

    ms.set_context(mode=mode)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ((), Tensor([1], mstype.int32))
    out = GradOperationNetWrtParameter(Net(w, b), get_all=True, get_by_list=True)()
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_no_input_and_single_param_tuple(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with single Parameter without input in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')

        def construct(self):
            return self.w

    ms.set_context(mode=mode)
    w = Tensor([6], mstype.int32)
    expect = ((), (Tensor([1], mstype.int32),))
    out = GradOperationNetWrtParameterTuple(Net(w), get_all=True, get_by_list=True)()
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_no_input_and_multiple_params(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with single input and multiple Parameters in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self):
            return self.w + self.b

    ms.set_context(mode=mode)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ((), (Tensor([1], mstype.int32), Tensor([1], mstype.int32)))
    out = GradOperationNetWrtParameterTuple(Net(w, b), get_all=True, get_by_list=True)()
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_single_input_and_no_param(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with single input without Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            return 3 * x

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    expect = ((Tensor([3], mstype.int32),), ())
    out = GradOperationNetWrtParameterTuple(Net(), get_all=True, get_by_list=True)(x)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_multiple_inputs_and_no_param(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with multiple inputs without Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y):
            return 3 * x + y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    expect = ((Tensor([3], mstype.int32), Tensor([1], mstype.int32)), ())
    out = GradOperationNetWrtParameterTuple(Net(), get_all=True, get_by_list=True)(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_no_input_and_no_param(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation without input or Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self):
            return 3

    ms.set_context(mode=mode)
    expect = ((), ())
    out = GradOperationNetWrtParameterTuple(Net(), get_all=True, get_by_list=True)()
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_single_input_and_none_param(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with single input and None Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            return 3 * x

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    expect = ((Tensor([3], mstype.int32),), ())
    out = GradOperationNetWrtParameterNone(Net(), get_all=True, get_by_list=True)(x)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_multiple_inputs_and_none_param(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with multiple inputs and None Parameter in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y):
            return 3 * x + y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    expect = ((Tensor([3], mstype.int32), Tensor([1], mstype.int32)), ())
    out = GradOperationNetWrtParameterNone(Net(), get_all=True, get_by_list=True)(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_no_input_and_none_param(mode):
    """
    Features: ops.GradOperation.
    Description: Test ops.GradOperation with None Parameter and without input in graph mode.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self):
            return 3

    ms.set_context(mode=mode)
    expect = ((), ())
    out = GradOperationNetWrtParameterNone(Net(), get_all=True, get_by_list=True)()
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_int_position(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with None parameter when position is int.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = Tensor([6], mstype.int32)
    out = grad(Net(w, b), grad_position=0, weights=None)(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_tuple_position(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with None parameter when position is tuple.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y, z):
            return self.w * x + self.b * y + z

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    z = Tensor([30], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = (Tensor([2], mstype.int32), Tensor([1], mstype.int32))
    out = grad(Net(w, b), grad_position=(1, 2), weights=None)(x, y, z)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_none_position(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with single parameter when position is None.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')

        def construct(self, x, y):
            return self.w * x + y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    with pytest.raises(ValueError):
        grad(Net(w), grad_position=None, weights=None)(x, y)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_grad_int_position_no_input():
    """
    Features: ops.grad.
    Description: Test ops.grad with None parameter when position is int.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self):
            return self.w + self.b

    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)

    ms.set_context(mode=ms.GRAPH_MODE)
    with pytest.raises(IndexError):
        grad(Net(w, b), grad_position=0, weights=None)()


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_grad_tuple_position_no_input():
    """
    Features: ops.grad.
    Description: Test ops.grad with None parameter when position is tuple.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self):
            return self.w + self.b

    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)

    ms.set_context(mode=ms.GRAPH_MODE)
    with pytest.raises(IndexError):
        grad(Net(w, b), grad_position=(0, 1), weights=None)()


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_grad_tuple_position_single_input():
    """
    Features: ops.grad.
    Description: Test ops.grad with None parameter when position is tuple.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x):
            return self.w * x + self.b

    x = Tensor([10], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)

    ms.set_context(mode=ms.GRAPH_MODE)
    with pytest.raises(IndexError):
        grad(Net(w, b), grad_position=(0, 1), weights=None)(x)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_constant_tensor(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with constant tensor.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y):
            out = x + y
            return out

    ms.set_context(mode=mode)
    out = grad(Net())(1, 2)
    assert out == ()


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_int_position_and_single_param(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with single parameter when position is int.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = (Tensor([6], mstype.int32), Tensor([10], mstype.int32))
    net = Net(w, b)
    out = grad(net, grad_position=0, weights=net.trainable_params()[0])(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_int_position_and_single_param_tuple(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with single parameter when position is int.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')

        def construct(self, x, y):
            return self.w * x + y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    expect = (Tensor([6], mstype.int32), (Tensor([10], mstype.int32),))
    net = Net(w)
    out = grad(net, grad_position=0, weights=net.trainable_params())(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_int_position_and_multiple_params(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with multiple parameters when position is int.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = (Tensor([6], mstype.int32), (Tensor([10], mstype.int32), Tensor([20], mstype.int32)))
    net = Net(w, b)
    out = grad(net, grad_position=0, weights=net.trainable_params())(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_int_position_and_no_param(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad without parameter when position is int.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y):
            return 3 * x + y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    expect = (Tensor([3], mstype.int32), ())
    net = Net()
    out = grad(net, grad_position=0, weights=net.trainable_params())(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_tuple_position_and_single_param(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with single parameter when position is tuple.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y, z):
            return self.w * x + self.b * y + z

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    z = Tensor([30], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ((Tensor([2], mstype.int32), Tensor([1], mstype.int32)), Tensor([10], mstype.int32))
    net = Net(w, b)
    out = grad(net, grad_position=(1, 2), weights=net.trainable_params()[0])(x, y, z)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_tuple_position_and_single_param_tuple(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with single parameter when position is tuple.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')

        def construct(self, x, y, z):
            return self.w * x + 3 * y + z

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    z = Tensor([30], mstype.int32)
    w = Tensor([6], mstype.int32)
    expect = ((Tensor([3], mstype.int32), Tensor([1], mstype.int32)), (Tensor([10], mstype.int32),))
    net = Net(w)
    out = grad(net, grad_position=(1, 2), weights=net.trainable_params())(x, y, z)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_tuple_position_and_multiple_params(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with multiple parameters when position is tuple.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y, z):
            return self.w * x + self.b * y + z

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    z = Tensor([30], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ((Tensor([2], mstype.int32), Tensor([1], mstype.int32)),
              (Tensor([10], mstype.int32), Tensor([20], mstype.int32)))
    net = Net(w, b)
    out = grad(net, grad_position=(1, 2), weights=net.trainable_params())(x, y, z)
    check_grad_result(out, expect)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_tuple_position_and_no_param(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad without parameter when position is tuple.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y, z):
            return 3 * x + 4 * y + z

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    z = Tensor([30], mstype.int32)
    expect = ((Tensor([4], mstype.int32), Tensor([1], mstype.int32)), ())
    net = Net()
    out = grad(net, grad_position=(1, 2), weights=net.trainable_params())(x, y, z)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_none_position_and_single_param(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with single parameter when position is None.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = Tensor([10], mstype.int32)
    net = Net(w, b)
    out = grad(net, grad_position=None, weights=net.trainable_params()[0])(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_none_position_and_single_param_tuple(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with single parameter when position is None.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')

        def construct(self, x, y):
            return self.w * x + y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    expect = (Tensor([10], mstype.int32),)
    net = Net(w)
    out = grad(net, grad_position=None, weights=net.trainable_params())(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_none_position_and_multiple_params(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with multiple parameters when position is None.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = (Tensor([10], mstype.int32), Tensor([20], mstype.int32))
    net = Net(w, b)
    out = grad(net, grad_position=None, weights=net.trainable_params())(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_none_position_and_no_param(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad without parameter when position is None.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y):
            return 3 * x + y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    expect = ()
    net = Net()
    out = grad(net, grad_position=None, weights=net.trainable_params())(x, y)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_empty_position_and_single_param(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with single parameter when position is empty.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    with pytest.raises(RuntimeError):
        net = Net(w, b)
        grad(net, grad_position=(), weights=net.trainable_params()[0])(x, y)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_empty_position_and_single_param_tuple(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with single parameter when position is empty.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')

        def construct(self, x, y):
            return self.w * x + y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    with pytest.raises(RuntimeError):
        net = Net(w)
        grad(net, grad_position=(), weights=net.trainable_params())(x, y)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_empty_position_and_multiple_params(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with multiple parameters when position is empty.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    with pytest.raises(RuntimeError):
        net = Net(w, b)
        grad(net, grad_position=(), weights=net.trainable_params())(x, y)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_empty_position_and_no_param(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad without parameter when position is empty.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y):
            return 3 * x + y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    with pytest.raises(RuntimeError):
        net = Net()
        grad(net, grad_position=(), weights=net.trainable_params())(x, y)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_hypermap_control_flow(mode):
    """
    Features: ops.grad.
    Description: Test ops.GradOperation with control flow.
    Expectation: No exception.
    """
    ctrl = C.MultitypeFuncGraph("ctrl")
    @ctrl.register("Tensor", "Tuple")
    def _if(x, y):
        if x > 0:
            return y[0]
        return y[1]

    class Net(nn.Cell):
        def __init__(self, mtfg):
            super().__init__()
            self.hyper_map = C.HyperMap(mtfg)

        def construct(self, x, y, z):
            return self.hyper_map((x, y), ((z, x), (y, z)))

    ms.set_context(mode=mode)
    x = Tensor(2, mstype.int32)
    y = Tensor(-3, mstype.int32)
    z = Tensor(0, mstype.int32)
    expect = (Tensor(0, mstype.int32), Tensor(0, mstype.int32), Tensor(2, mstype.int32))
    out = GradOperationNet(Net(ctrl), get_all=True)(x, y, z)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_operation_dynamic_shape(mode):
    """
    Features: ops.grad.
    Description: Test ops.GradOperation with dynamic shape.
    Expectation: No exception.
    """
    class DynamicShape(nn.Cell):
        def __init__(self):
            super().__init__()
            self.dynamicshape = ops.Shape()

        def construct(self, inputx):
            return self.dynamicshape(inputx)

    ms.set_context(mode=mode)
    x = Tensor(np.random.randn(8,).astype(np.int32))
    expect = (Tensor(np.array([0, 0, 0, 0, 0, 0, 0, 0]).astype(np.int32)),)
    net = DynamicShape()
    forward_graph = net(x)
    out = GradOperationNet(DynamicShape(), get_all=True, get_by_list=False, sens_param=True)(x, forward_graph)
    check_grad_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_int_position_with_ids(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with ids with None parameter when position is int.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = (0, Tensor([6], mstype.int32))
    out = grad(Net(w, b), grad_position=0, weights=None, return_ids=True)(x, y)
    check_grad_with_ids_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_tuple_position_with_ids(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with ids with None parameter when position is tuple.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y, z):
            return self.w * x + self.b * y + z

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    z = Tensor([30], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ((1, Tensor([2], mstype.int32)), (2, Tensor([1], mstype.int32)))
    out = grad(Net(w, b), grad_position=(1, 2), weights=None, return_ids=True)(x, y, z)
    check_grad_with_ids_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_constant_tensor_with_ids(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with ids with constant tensor.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y):
            out = x + y
            return out

    ms.set_context(mode=mode)
    out = grad(Net(), return_ids=True)(1, 2)
    assert out == ()


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_int_position_and_single_param_with_ids(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with ids with single parameter when position is int.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ((0, Tensor([6], mstype.int32)), ("w", Tensor([10], mstype.int32)))
    net = Net(w, b)
    out = grad(net, grad_position=0, weights=net.trainable_params()[0], return_ids=True)(x, y)
    check_grad_with_ids_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_int_position_and_single_param_tuple_with_ids(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with ids with single parameter when position is int.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')

        def construct(self, x, y):
            return self.w * x + y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    expect = ((0, Tensor([6], mstype.int32)), (("w", Tensor([10], mstype.int32)),))
    net = Net(w)
    out = grad(net, grad_position=0, weights=net.trainable_params(), return_ids=True)(x, y)
    check_grad_with_ids_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_int_position_and_multiple_params_with_ids(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with multiple parameters when position is int.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y):
            return self.w * x + self.b * y

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = ((0, Tensor([6], mstype.int32)), (("w", Tensor(
        [10], mstype.int32)), ("b", Tensor([20], mstype.int32))))
    net = Net(w, b)
    out = grad(net, grad_position=0, weights=net.trainable_params(), return_ids=True)(x, y)
    check_grad_with_ids_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_tuple_position_and_single_param_with_ids(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with ids with single parameter when position is tuple.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y, z):
            return self.w * x + self.b * y + z

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    z = Tensor([30], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = (((1, Tensor([2], mstype.int32)), (2, Tensor(
        [1], mstype.int32))), ("w", Tensor([10], mstype.int32)))
    net = Net(w, b)
    out = grad(net, grad_position=(1, 2), weights=net.trainable_params()[0], return_ids=True)(x, y, z)
    check_grad_with_ids_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_tuple_position_and_single_param_tuple_with_ids(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with ids with single parameter when position is tuple.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')

        def construct(self, x, y, z):
            return self.w * x + 3 * y + z

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    z = Tensor([30], mstype.int32)
    w = Tensor([6], mstype.int32)
    expect = (((1, Tensor([3], mstype.int32)), (2, Tensor(
        [1], mstype.int32))), (("w", Tensor([10], mstype.int32)),))
    net = Net(w)
    out = grad(net, grad_position=(1, 2), weights=net.trainable_params(), return_ids=True)(x, y, z)
    check_grad_with_ids_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_grad_tuple_position_and_multiple_params_with_ids(mode):
    """
    Features: ops.grad.
    Description: Test ops.grad with ids with multiple parameters when position is tuple.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self, w, b):
            super(Net, self).__init__()
            self.w = Parameter(w, name='w')
            self.b = Parameter(b, name='b')

        def construct(self, x, y, z):
            return self.w * x + self.b * y + z

    ms.set_context(mode=mode)
    x = Tensor([10], mstype.int32)
    y = Tensor([20], mstype.int32)
    z = Tensor([30], mstype.int32)
    w = Tensor([6], mstype.int32)
    b = Tensor([2], mstype.int32)
    expect = (((1, Tensor([2], mstype.int32)), (2, Tensor([1], mstype.int32))),
              (("w", Tensor([10], mstype.int32)), ("b", Tensor([20], mstype.int32))))
    net = Net(w, b)
    out = grad(net, grad_position=(1, 2), weights=net.trainable_params(), return_ids=True)(x, y, z)
    check_grad_with_ids_result(out, expect)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_get_grad_by_position(mode):
    """
    Features: Function get_grad.
    Description: Test get_grad with position id and output gradient in graph mode.
    Expectation: No exception.
    """

    class ParamMultipleInputNet(nn.Cell):
        def __init__(self):
            super(ParamMultipleInputNet, self).__init__()
            self.w = Parameter(Tensor([2., 2.], mstype.float32), name="w")

        def construct(self, x, y):
            outputs = x * y * self.w
            return outputs, x, self.w

    class GradNet(nn.Cell):
        def __init__(self, net):
            super(GradNet, self).__init__()
            self.net = net
            self.weights = net.trainable_params()

        def construct(self, x, y):
            res = grad(self.net, 0, self.weights, return_ids=True)(x, y)
            grad_out = get_grad(res, 0)
            return grad_out

    ms.set_context(mode=mode)
    x = Tensor(np.array([1, 2]).astype(np.float32))
    y = Tensor(np.array([3, 3]).astype(np.float32))
    expect_grad_input = np.array([7, 7]).astype(np.float32)
    inner_net = ParamMultipleInputNet()
    grad_net = GradNet(inner_net)
    grad_out = grad_net(x, y)
    assert np.allclose(grad_out.asnumpy(), expect_grad_input)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_construct_get_grad_by_parameter(mode):
    """
    Features: Function get_grad.
    Description: Test get_grad with parameter and output gradient in graph mode.
    Expectation: No exception.
    """

    class ParamMultipleInputNet(nn.Cell):
        def __init__(self):
            super(ParamMultipleInputNet, self).__init__()
            self.w = Parameter(Tensor([2., 2.], mstype.float32), name="w")

        def construct(self, x, y):
            outputs = x * y * self.w
            return outputs, x, self.w

    class GradNet(nn.Cell):
        def __init__(self, net):
            super(GradNet, self).__init__()
            self.net = net
            self.weights = net.trainable_params()

        def construct(self, x, y):
            res = grad(self.net, 0, self.weights, return_ids=True)(x, y)
            grad_out = get_grad(res, self.net.w)
            return grad_out

    ms.set_context(mode=mode)
    x = Tensor(np.array([1, 2]).astype(np.float32))
    y = Tensor(np.array([3, 3]).astype(np.float32))
    expect_grad_input = np.array([4, 7]).astype(np.float32)
    inner_net = ParamMultipleInputNet()
    grad_net = GradNet(inner_net)
    grad_out = grad_net(x, y)
    assert np.allclose(grad_out.asnumpy(), expect_grad_input)
