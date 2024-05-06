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
import os
import sys
import time
import tempfile
from contextlib import contextmanager
import pytest
import numpy as np
from mindspore import Tensor, jit, context, Parameter, ops
from mindspore.nn import Cell
from mindspore.nn.probability import distribution
import mindspore.nn as nn
import mindspore.common.dtype as mstype
import mindspore as ms

context.set_context(mode=context.GRAPH_MODE)


class Capture():
    def __init__(self):
        self._old_stdout = sys.stdout
        self._stdout_fd = sys.stdout.fileno()
        self._saved_stdout_fd = os.dup(sys.stdout.fileno())
        self._file = tempfile.TemporaryFile(mode='w+t')
        self.output = ''

    def start(self):
        os.dup2(self._file.fileno(), self._stdout_fd)

    def stop(self):
        os.dup2(self._saved_stdout_fd, self._stdout_fd)
        os.close(self._saved_stdout_fd)
        sys.stdout = self._old_stdout
        self._file.seek(0)
        self.output = self._file.read()
        self._file.close()


@contextmanager
def capture(cap):
    cap.start()
    try:
        yield cap
    finally:
        cap.stop()


def check_output(output, patterns):
    assert output, "Capture output failed!"
    for pattern in patterns:
        assert output.find(pattern) != -1, "Unexpected output:\n" + output + "\n--- pattern ---\n" + pattern


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_none_compare():
    """
    Feature: Support None.
    Description: Support None compare with string, bool, and empty list.
    Expectation: No exception.
    """
    @jit
    def foo():  # pylint: disable=R1711
        a = ''
        b = False
        c = []
        d = 0
        print(a is None)
        print(b is None)
        print(c is None)
        print(d is None)
        return None

    cap = Capture()
    with capture(cap):
        res = foo()
        assert res is None
        sys.stdout.flush()
        time.sleep(0.1)

    patterns = {'False\nFalse\nFalse\nFalse'}
    check_output(cap.output, patterns)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_sequence_input():
    """
    Feature: Support None.
    Description: Support None is the input of sequence.
    Expectation: No exception.
    """
    @jit
    def foo(input_tuple, input_list, input_dict):
        num = 0
        for input_t in input_tuple:
            if input_t is None:
                num = num + 1
        for input_l in input_list:
            if input_l is None:
                num = num + 1
        for key in input_dict:
            if input_dict[key] is None:
                num = num + 1
        return num

    input_x = (None, None, 1, 2, 3, 4)
    input_y = [None, None, None]
    input_z = {"a": None, "b": None, "c": 5, "d": 6, "e": 7, "f": 8}
    res = foo(input_x, input_y, input_z)
    assert res == 7


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_inner_function_has_not_return():
    """
    Feature: Support None.
    Description: Support has no return function.
    Expectation: No exception.
    """

    @jit
    def foo():
        x = 3
        print("x:", x)

    cap = Capture()
    with capture(cap):
        res = foo()
        assert res is None
        sys.stdout.flush()
        time.sleep(0.1)

    patterns = {'x:\n3'}
    check_output(cap.output, patterns)


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_inner_function_has_not_return_2():
    """
    Feature: Support None.
    Description: Support None is the output of inner function.
    Expectation: No exception.
    """
    def func1():
        x = 3
        print("x:", x)

    def func2():
        return None  # pylint: disable=R1711

    def func3():
        return

    @jit
    def foo():
        obj1 = func1()  # pylint: disable=E1111
        obj2 = func2()  # pylint: disable=E1128
        obj3 = func3()  # pylint: disable=E1128
        assert obj1 is None
        assert obj2 is None
        assert obj3 is None
        return 0

    res = foo()
    assert res == 0


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_none_is_default_value_of_parameter():
    """
    Feature: Support None.
    Description: Support None is the default value of parameter.
    Expectation: No exception.
    """
    @jit
    def foo(x, y=None):
        if y is not None:
            print("y:", y)
        else:
            print("y is None")
        print("x:", x)
        return y

    cap = Capture()
    with capture(cap):
        x = [1, 2]
        res = foo(x)
        assert res is None
        sys.stdout.flush()
        time.sleep(0.1)

    patterns = {'y is None\nx:\n[1, 2]'}
    check_output(cap.output, patterns)


@pytest.mark.skip(reason="print pyexecute list will change the print format")
@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_none_is_default_value_of_parameter_2():
    """
    Feature: Support None.
    Description: Support None is the default value of parameter.
    Expectation: No exception.
    """
    @jit
    def foo(x, y=None):
        if y is not None:
            print("y:", y)
        else:
            print("y is None")
        print("x:", x)
        return y

    cap = Capture()
    with capture(cap):
        x = [1, 2]
        y = [3, 4]
        res = foo(x, y)
        assert res == [3, 4]
        sys.stdout.flush()
        time.sleep(0.1)

    patterns = {'y:\n[3, 4]\nx:\n[1, 2]'}
    check_output(cap.output, patterns)


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_slice_in_list():
    """
    Feature: Support None.
    Description: Support None is slice in list.
    Expectation: No exception.
    """
    @jit
    def foo():
        arr1 = np.array([1, 2, 3])
        print(arr1[0:2])
        print(arr1[:, None])
        print(arr1[None, :])
        arr2 = np.array([[[1, 2], [3, 4], [5, 6]],
                         [[1, 2], [3, 4], [5, 6]]])
        print(arr2[0:2, :])
        print(arr2[:, 0])
        print(arr2[0, :])
        print(arr2[None, :])
        print(arr2[:, None, :])
        print(arr2[:, :, None])
        return 0

    res = foo()
    assert res == 0


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_none_assign_print():
    """
    Feature: Support None.
    Description: Support None assign and print.
    Expectation: No exception.
    """
    @jit
    def foo():
        x = None
        print("x:", x)
        print("convert bool:", bool(None))
        return x

    cap = Capture()
    with capture(cap):
        res = foo()
        assert res is None
        sys.stdout.flush()
        time.sleep(0.1)

    patterns = {'x:\nNone\nconvert bool:\nFalse\n'}
    check_output(cap.output, patterns)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_input():
    """
    Feature: Support None.
    Description: Support None is arg of top graph.
    Expectation: No exception.
    """
    @jit
    def foo(x):
        return x

    input_x = None
    res = foo(input_x)
    assert res is None


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_condition():
    """
    Feature: Support None.
    Description: Support None is condition.
    Expectation: No exception.
    """
    @jit
    def foo(x):
        if not x:
            print("The input is None!")
        else:
            print("The input is not None!")
        return x

    cap = Capture()
    with capture(cap):
        input_x = None
        res = foo(input_x)
        assert res is None
        sys.stdout.flush()
        time.sleep(0.1)

    patterns = {'The input is None!'}
    check_output(cap.output, patterns)


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_inner_function_output():
    """
    Feature: Support None.
    Description: Support None is the output of inner function.
    Expectation: No exception.
    """
    def inner_fun1(input_x):
        if input_x == 0:
            return None, input_x
        return None, input_x * 2

    @jit
    def foo(input_x):  # pylint: disable=R1711
        out1 = inner_fun1(input_x)
        if not out1[1]:
            print('The output of inner function is None!')
        else:
            print('The output of inner function is not None!')
        return None

    cap = Capture()
    with capture(cap):
        x = Tensor([1])
        res = foo(x)
        assert res is None
        sys.stdout.flush()
        time.sleep(0.1)

    patterns = {'The output of inner function is not None!'}
    check_output(cap.output, patterns)


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_output_of_function_with_side_effect_is():
    """
    Feature: Support None.
    Description: Support None is the output of_function with side effect.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self):
            super().__init__()
            self.param = Parameter(Tensor([1], dtype=mstype.int64), name="param")

        def func(self, y):  # pylint: disable=R1711
            if y == self.param:
                self.param = y * 2
            return None

        def construct(self, x):
            if self.func(x) is None:
                self.param = self.param + 2 * x
            else:
                self.param = self.param + 10 * x
            return self.param

    net = Net()
    input_x = Tensor([1], dtype=mstype.int32)
    res = net(input_x)
    print("res:", res)
    assert res == 4


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_output_of_function_with_side_effect_equal():
    """
    Feature: Support None.
    Description: Support None is the output of_function with side effect.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def __init__(self):
            super().__init__()
            self.param = Parameter(Tensor([1], dtype=mstype.int64), name="param")

        def func(self, y):  # pylint: disable=R1711
            if y == self.param:
                self.param = y * 2
            return None

        def construct(self, x):
            if self.func(x) == None:  # pylint: disable=C0121
                self.param = self.param + 2 * x
            else:
                self.param = self.param + 10 * x
            return self.param

    net = Net()
    input_x = Tensor([1], dtype=mstype.int32)
    res = net(input_x)
    print("res:", res)
    assert res == 4


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_input_of_dict_return():
    """
    Feature: Support None.
    Description: Support None is input of dict, and the dict is return.
    Expectation: No exception.
    """

    @jit
    def foo():
        x = {'a': 'a', 'b': 'b'}
        y = x.get('a')
        z = dict(y=y, u=9, v=False, w=None)
        return z

    out = foo()
    assert out == {'y': 'a', 'u': 9, 'v': False, 'w': None}


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_nested_input_of_dict_return():
    """
    Feature: Support None.
    Description: Support None is input of dict, and the dict is return.
    Expectation: No exception.
    """

    @jit
    def foo():
        x = {'a': 'a', 'b': 'b'}
        y = x.get('a')
        z = dict(y=y, u=9, v=False, w=(None, None), q=[1, (2, None), None])
        return z

    out = foo()
    assert out == {'y': 'a', 'u': 9, 'v': False, 'w': (None, None), 'q': [1, (2, None), None]}


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_input_of_tuple_return():
    """
    Feature: Support None.
    Description: Support None is input of tuple, and the tuple is return.
    Expectation: No exception.
    """

    @jit
    def foo():
        return 1, "a", None

    out = foo()
    assert out == (1, "a", None)


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_input_of_tuple_return_2():
    """
    Feature: Support None.
    Description: Support None is input of tuple, and the tuple is return.
    Expectation: No exception.
    """
    class BernoulliCrossEntropy(Cell):
        def __init__(self, probs, seed=10, dtype=ms.int32, name='Bernoulli', dist='Bernoulli'):
            super().__init__()
            self.b = distribution.Bernoulli(probs, seed, dtype, name)
            self.dist = dist

        def construct(self, probs1_b, probs1=None):
            if probs1 is None:
                out1 = self.b.cross_entropy(self.dist, probs1_b)
                out2 = self.b.kl_loss(self.dist, probs1_b)
            else:
                out1 = self.b.cross_entropy(self.dist, probs1_b, probs1)
                out2 = self.b.kl_loss(self.dist, probs1_b, probs1)
            out3 = self.b.probs
            return out1, out2, out3

    probs = None
    probs1 = 0.2
    probs1_b = 0.9
    context.set_context(mode=context.GRAPH_MODE)
    net_graph = BernoulliCrossEntropy(probs)
    out_me_graph = net_graph(Tensor(probs1_b), Tensor(probs1))
    print("out_me_graph: ", out_me_graph)
    context.set_context(mode=context.PYNATIVE_MODE)
    net_pynative = BernoulliCrossEntropy(probs)
    out_me_pynative = net_pynative(Tensor(probs1_b), Tensor(probs1))
    print("out_me_pynative: ", out_me_pynative)
    assert out_me_graph == out_me_pynative
    context.set_context(mode=context.GRAPH_MODE)


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_return_of_sub_graph_control_flow():
    """
    Feature: Support None.
    Description: Support None is the return of sub_graph in control flow.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def check_value(self, x):  # pylint: disable=R1711
            if x[0][0] < 2:
                print("The input is less 2.")
            return None

        def construct(self, x):
            self.check_value(x)
            return x

    cap = Capture()
    with capture(cap):
        net = Net()
        data = Tensor(np.ones([2, 3]), dtype=ms.float32)
        out = net(data)
        assert (out.asnumpy() == data).all()
        sys.stdout.flush()
        time.sleep(0.1)

    patterns = {'The input is less 2.'}
    check_output(cap.output, patterns)


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_return_of_sub_graph_control_flow_raise():
    """
    Feature: Support None.
    Description: Support None is the return of sub_graph in control flow. And Raise node is in sub_graph.
    Expectation: No exception.
    """
    class RaiseNet(nn.Cell):
        def inner_func(self, x):  # pylint: disable=R1711
            if x == 2:
                raise ValueError("The input should not be ", x)
            return None

        def construct(self, x):
            self.inner_func(x)
            return x

    net = RaiseNet()
    res = net(Tensor(1))
    print("res:", res)
    assert res.asnumpy() == 1


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_none_is_return_raise():
    """
    Feature: Support None.
    Description: Support None is the return of sub_graph in control flow. And Raise node is in sub_graph.
    Expectation: No exception.
    """
    def check_test(shp):  # pylint: disable=R1711
        if shp[0] > 5:
            raise ValueError('raise value error.')
        return None

    class Net(nn.Cell):
        def __init__(self):
            super(Net, self).__init__()
            self.one = Tensor(1, dtype=ms.float32)

        def construct(self, x):
            shp = x.shape
            check_test(shp)
            return x

    with pytest.raises(ValueError, match="raise value error."):
        np_data = np.random.randint(6, size=(6,))
        data = Tensor(np_data, dtype=ms.float32)
        dyn_tensor = Tensor(shape=[None], dtype=ms.float32)
        net = Net()
        net.set_inputs(dyn_tensor)
        out = net(data)
        assert out == data


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_raise_none_with_variable_control_flow3():
    """
    Feature: graph raise none by JIT Fallback.
    Description: Test raise with none.
    Expectation: No exception.
    """
    def _raise_func(script):
        raise ValueError(script)

    def check_test(shp, x):
        if shp[0] > 3:
            _raise_func(f"ddddd, {x}.")

    class Net(nn.Cell):
        def construct(self, x):
            shp = x.shape
            check_test(shp, x)
            return x

    class GradNet(nn.Cell):
        def __init__(self, net):
            super().__init__()
            self.net = net
            self.grad = ops.GradOperation()

        def construct(self, x):
            func = self.grad(self.net)
            return func(x)

    np_data = np.random.randint(6, size=(4,))
    data = Tensor(np_data, dtype=ms.float32)
    dyn_tensor = Tensor(shape=[None], dtype=ms.float32)
    with pytest.raises(ValueError) as control:
        net = Net()
        grad = GradNet(net)
        grad.set_inputs(dyn_tensor)
        print(grad(data))
    assert "ddddd, [" in str(control.value)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_none_in_value_list_tuple_dict():
    """
    Feature: Support None.
    Description: Support None in list.
    Expectation: No exception.
    """
    @jit
    def foo():
        return list((1, "a", None, [1, "a", None], dict(y=1, u=None)))

    out = foo()
    assert out == [1, "a", None, [1, "a", None], {"y": 1, "u": None}]


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_none_in_nest_tuple():
    """
    Feature: Support None.
    Description: Support None in nested tuple.
    Expectation: No exception.
    """
    @jit
    def foo():
        return None, ("a", None)

    out = foo()
    assert out == (None, ("a", None))


@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_parser_fallback_none_control_flow():
    """
    Feature: Support None.
    Description: Support None is the return of sub_graph in control flow.
    Expectation: No exception.
    """
    class NoneNet(nn.Cell):
        def __init__(self):
            super().__init__()
            self.add = ops.Add()

        @jit
        def func(self, z):
            if z == 0:
                out = None
            else:
                out = None
            return out, z

        def construct(self, x, y, z):
            if self.func(z)[0] is None:
                out = self.add(x, y)
            else:
                out = self.add(2 * x, y)
            return out

    net_ms = NoneNet()
    res = net_ms(Tensor(1), Tensor(10), Tensor(100))
    assert res == 11


@pytest.mark.skip("PyExecute condition is wrong.")
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_parser_fallback_none_control_flow_is_not():
    """
    Feature: Support None.
    Description: Support None is the return of sub_graph in control flow.
    Expectation: No exception.
    """
    class NoneNet(nn.Cell):
        def __init__(self):
            super().__init__()
            self.add = ops.Add()

        @jit
        def func(self, z):
            if z == 0:
                out = None
            else:
                out = None
            return out, z

        def construct(self, x, y, z):
            if self.func(z)[0] is not None:
                out = self.add(2 * x, y)
            else:
                out = self.add(x, y)
            return out

    net_ms = NoneNet()
    res = net_ms(Tensor(1), Tensor(10), Tensor(100))
    assert res == 11


@pytest.mark.level0
@pytest.mark.platform_x86_gpu
@pytest.mark.env_onecard
def test_return_tuple_none_control_flow():
    """
    Feature: Support None.
    Description: Support None is the tuple return in control flow.
    Expectation: No exception.
    """
    @ms.jit
    def func(input_x):
        if input_x:
            return None, input_x, {"1": input_x}
        return None, input_x * 2, {"2": input_x * 2}

    x = ms.Tensor(10)
    out = func(x)
    assert out == (None, 10, {"1": 10})


@pytest.mark.level0
@pytest.mark.platform_x86_gpu
@pytest.mark.env_onecard
def test_none_in_nest_tuple_list_control_flow():
    """
    Feature: Support None.
    Description: Support None is the tuple return in control flow.
    Expectation: No exception.
    """
    @ms.jit
    def func(input_x):
        if input_x:
            return None, (None, [input_x, None])
        return None, (None, [input_x * 2, None])

    x = ms.Tensor(10)
    out = func(x)
    assert out == (None, (None, [x, None]))


@pytest.mark.level0
@pytest.mark.platform_x86_gpu
@pytest.mark.env_onecard
def test_grad_with_return_none():
    """
    Feature: Support None.
    Description: Support None used in grad.
    Expectation: No exception.
    """
    @ms.jit
    def func(input_x):
        return None

    x = ms.Tensor([10])
    out = ops.GradOperation()(func)(x)
    assert out == Tensor([0])


@pytest.mark.level0
@pytest.mark.platform_x86_gpu
@pytest.mark.env_onecard
def test_grad_with_return_none_2():
    """
    Feature: Support None.
    Description: Support None used in grad.
    Expectation: No exception.
    """
    @ms.jit
    def func(input_x):
        return [None]

    x = ms.Tensor([10])
    out = ops.GradOperation()(func)(x)
    assert out == Tensor([0])
