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
# ==============================================================================
import pytest
import numpy as np

import mindspore as ms
from mindspore.nn import Cell
from mindspore.common.parameter import Parameter
from mindspore.common import ParameterTuple
from mindspore import Tensor, context


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_parameter_1_1(mode):
    """
    Feature: Check the names of parameters and the names of inputs of construct.
    Description: If the name of the input of construct is same as the parameters, add suffix to the name of the input.
    Expectation: No exception.
    """

    class ParamNet(Cell):
        def __init__(self):
            super(ParamNet, self).__init__()
            self.param_a = Parameter(Tensor([1], ms.float32), name="name_a")
            self.param_b = Parameter(Tensor([2], ms.float32), name="name_b")

        def construct(self, name_a):
            return self.param_a + self.param_b - name_a

    context.set_context(mode=mode)
    net = ParamNet()
    res = net(Tensor([3], ms.float32))
    assert res == 0
    assert net.param_a.name == "name_a"
    assert net.param_b.name == "name_b"


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_parameter_1_2(mode):
    """
    Feature: Check the names of parameters and the names of inputs of construct.
    Description: If the name of the input of construct is same as the parameters, add suffix to the name of the input.
    Expectation: No exception.
    """

    class ParamNet(Cell):
        def __init__(self):
            super(ParamNet, self).__init__()
            self.param_a = Parameter(Tensor([1], ms.float32), name="name_a")
            self.param_b = ParameterTuple((Parameter(Tensor([2], ms.float32), name="name_b"), self.param_a))

        def construct(self, name_b):
            return self.param_a + self.param_b[0] - name_b

    context.set_context(mode=mode)
    net = ParamNet()
    res = net(Tensor([3], ms.float32))
    assert res == 0
    assert net.param_a.name == "name_a"
    assert net.param_b[0].name == "name_b"
    assert net.param_b[1].name == "name_a"


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_parameter_3(mode):
    """
    Feature: Check the names of parameters.
    Description: Check the name of parameter in init.
    Expectation: No exception.
    """

    class ParamNet(Cell):
        def __init__(self):
            super(ParamNet, self).__init__()
            self.param_a = Parameter(Tensor([1], ms.float32))
            self.param_b = Parameter(Tensor([2], ms.float32))

        def construct(self):
            return self.param_a + self.param_b

    context.set_context(mode=mode)
    net = ParamNet()
    res = net()
    assert res == 3
    assert net.param_a.name == "param_a"
    assert net.param_b.name == "param_b"


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_parameter_5_2(mode):
    """
    Feature: Check the names of parameters.
    Description: Check the name of parameter in init.
    Expectation: No exception.
    """

    class ParamNet(Cell):
        def __init__(self):
            super(ParamNet, self).__init__()
            self.param_a = Parameter(Tensor([1], ms.float32), name="name_a")
            self.res1 = ParameterTuple((Parameter(Tensor([2], ms.float32)), self.param_a))
            self.param_a = Parameter(Tensor([3], ms.float32), name="name_b")
            self.res2 = self.res1[0] + self.param_a

        def construct(self):
            return self.param_a + self.res1[0] + self.res2

    context.set_context(mode=mode)
    net = ParamNet()
    res = net()
    assert res == 10
    assert net.param_a.name == "name_b"
    assert net.res1[0].name == "Parameter$1"
    assert net.res1[1].name == "name_a"


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_parameter_list_tuple_no_name(mode):
    """
    Feature: Check the names of parameters.
    Description: Check the name of parameter in init.
    Expectation: No exception.
    """

    class ParamNet(Cell):
        def __init__(self):
            super(ParamNet, self).__init__()
            self.param_tuple = (Parameter(Tensor([5], ms.float32)), Parameter(Tensor([6], ms.float32)))
            self.param_list = [Parameter(Tensor([7], ms.float32)), Parameter(Tensor([8], ms.float32))]

        def construct(self):
            return self.param_tuple[0] + self.param_tuple[1] + self.param_list[0] + self.param_list[1]

    context.set_context(mode=mode)
    net = ParamNet()
    res = net()
    assert res == 26
    assert net.param_tuple[0].name == "Parameter$1"
    assert net.param_tuple[1].name == "Parameter$2"
    assert net.param_list[0].name == "Parameter$3"
    assert net.param_list[1].name == "Parameter$4"


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_parameter_in_tuple(mode):
    """
    Feature: Check the names of parameters.
    Description: Check the name of parameter in init.
    Expectation: No exception.
    """

    class ParamNet(Cell):
        def __init__(self):
            super(ParamNet, self).__init__()
            self.param_a = Parameter(Tensor([1], ms.float32), name="name_a")
            self.param_b = Parameter(Tensor([2], ms.float32), name="name_b")
            self.param_tuple = ParameterTuple((self.param_a, self.param_b))

        def construct(self):
            return self.param_a + self.param_b + self.param_tuple[0] + self.param_tuple[1]

    context.set_context(mode=mode)
    net = ParamNet()
    res = net()
    assert res == 6
    assert net.param_a.name == "name_a"
    assert net.param_b.name == "name_b"
    assert net.param_tuple[0].name == "name_a"
    assert net.param_tuple[1].name == "name_b"


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_parameter_parameter_tuple_2(mode):
    """
    Feature: Check the names of parameters.
    Description: Check the name of parameter in init.
    Expectation: No exception.
    """

    class ParamNet(Cell):
        def __init__(self):
            super(ParamNet, self).__init__()
            self.param_a = Parameter(Tensor([1], ms.float32), name="name_a")
            self.param_tuple = ParameterTuple((self.param_a, self.param_a, self.param_a))

        def construct(self):
            return self.param_a + self.param_tuple[0] + self.param_tuple[1] + self.param_tuple[2]

    context.set_context(mode=mode)
    net = ParamNet()
    res = net()
    assert res == 4
    assert net.param_a.name == "name_a"
    assert net.param_tuple[0].name == "name_a"
    assert net.param_tuple[1].name == "name_a"
    assert net.param_tuple[2].name == "name_a"


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_parameter(mode):
    """
    Feature: Check the names of parameters.
    Description: If parameter in list or tuple is not given a name, will give it a unique name.
    Expectation: No exception.
    """

    class ParamNet(Cell):
        def __init__(self):
            super(ParamNet, self).__init__()
            self.param_a = Parameter(Tensor([1], ms.float32), name="name_a")
            self.param_b = Parameter(Tensor([2], ms.float32), name="name_b")
            self.param_c = Parameter(Tensor([3], ms.float32))
            self.param_d = Parameter(Tensor([4], ms.float32))
            self.param_tuple = (Parameter(Tensor([5], ms.float32)),
                                Parameter(Tensor([6], ms.float32)))
            self.param_list = [Parameter(Tensor([5], ms.float32)),
                               Parameter(Tensor([6], ms.float32))]

        def construct(self, x):
            res1 = self.param_a + self.param_b + self.param_c + self.param_d
            res1 = res1 - self.param_list[0] + self.param_list[1] + x
            res2 = self.param_list[0] + self.param_list[1]
            return res1, res2

    context.set_context(mode=mode)
    net = ParamNet()
    x = Tensor([10], ms.float32)
    output1, output2 = net(x)
    output1_expect = Tensor(21, ms.float32)
    output2_expect = Tensor(11, ms.float32)
    assert output1 == output1_expect
    assert output2 == output2_expect
    assert net.param_a.name == "name_a"
    assert net.param_b.name == "name_b"
    assert net.param_c.name == "param_c"
    assert net.param_d.name == "param_d"
    assert net.param_tuple[0].name == "Parameter$1"
    assert net.param_tuple[1].name == "Parameter$2"
    assert net.param_list[0].name == "Parameter$3"
    assert net.param_list[1].name == "Parameter$4"


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_parameter_argument_and_fv(mode):
    """
    Feature: Parameter argmument in top func graph.
    Description: Use Parameter as input argmument.
    Expectation: Parameter used as argument should equal to used as FV.
    """
    y = Parameter(Tensor([1]))

    class Demo(Cell):
        def construct(self, x):
            ms.ops.Assign()(x, Tensor([0]))
            ms.ops.Assign()(y, Tensor([0]))
            return True

    context.set_context(mode=mode)
    x = Parameter(Tensor([1]))
    net = Demo()
    net(x)
    print(Tensor(x))
    print(Tensor(y))
    assert x == y


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_parameter_argument_grad(mode):
    """
    Feature: Parameter argmument in top func graph.
    Description: Use Parameter as input argmument, and pass it to varargs.
    Expectation: Parameter used as argument should equal to used as FV.
    """

    class ParameterArgumentCell(Cell):
        def __init__(self):
            super(ParameterArgumentCell, self).__init__()
            self.z = Parameter(Tensor(np.array([[1.0, 4.0], [-1, 8.0]]), ms.float32), name='z')

        def construct(self, param, x, y):
            ms.ops.Assign()(param, x * self.z)
            ms.ops.Assign()(x, x + y)
            ms.ops.Assign()(y, param)
            return param

    context.set_context(mode=mode)
    param = Parameter(Tensor(np.array([[0, 0], [0, 0]]), ms.float32), name='param')
    x = Parameter(Tensor(np.array([[4.0, -8.0], [-2.0, -5.0]]), ms.float32), name='x')
    y = Parameter(Tensor(np.array([[1, 0], [1, 1]]), ms.float32), name='y')
    net = ParameterArgumentCell()
    net(param, x, y)

    bparam = Parameter(Tensor(np.array([[0, 0], [0, 0]]), ms.float32), name='bparam')
    bx = Parameter(Tensor(np.array([[4.0, -8.0], [-2.0, -5.0]]), ms.float32), name='bx')
    by = Parameter(Tensor(np.array([[1, 0], [1, 1]]), ms.float32), name='by')
    grad_by_list = ms.ops.GradOperation(get_by_list=True)
    grad_by_list(net, ParameterTuple(net.trainable_params()))(bparam, bx, by)

    assert np.array_equal(param.asnumpy(), bparam.asnumpy())
    assert np.array_equal(x.asnumpy(), bx.asnumpy())
    assert np.array_equal(y.asnumpy(), by.asnumpy())
