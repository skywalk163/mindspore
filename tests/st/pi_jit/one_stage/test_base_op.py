# Copyright 2024 Huawei Technologies Co., Ltd
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
"""Test basic operation with one stage"""
import pytest
import numpy as np
import mindspore.nn as nn
from mindspore import Tensor
from mindspore import context
from mindspore.common.api import jit

cfg = {
    "replace_nncell_by_construct": True,
    "print_after_all": False,
    "compile_by_trace": True,
    "print_bb": False,
    "MAX_INLINE_DEPTH": 10,
    "allowed_inline_modules": ["mindspore"],  # buildsubgraph
}


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_make_tuple():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            return (x, x+1, x+2)

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = Tensor([1])
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a)
    assert isinstance(ret, tuple)
    assert len(ret) == 3
    assert ret[0] == Tensor([1])
    assert ret[1] == Tensor([2])
    assert ret[2] == Tensor([3])


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_make_list():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            return [x, x+1, x+2]

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = Tensor([1])
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a)
    assert isinstance(ret, list)
    assert len(ret) == 3
    assert ret[0] == Tensor([1])
    assert ret[1] == Tensor([2])
    assert ret[2] == Tensor([3])


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_return_add_result_tuple():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y):
            return x + y

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = (1, 2, 3)
    b = (4, 5, 6)
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a, b)
    assert ret == (1, 2, 3, 4, 5, 6)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_return_add_result_list():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y):
            return x + y

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = [1, 2, 3]
    b = [4, 5, 6]
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a, b)
    assert ret == [1, 2, 3, 4, 5, 6]


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_tuple_slice():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            m = (x, x+1, x+2)
            return m[0:2:1]

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = Tensor([1])
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a)
    assert isinstance(ret, tuple)
    assert len(ret) == 2
    assert ret[0] == Tensor([1])
    assert ret[1] == Tensor([2])


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_list_slice():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            m = [x, x+1, x+2]
            return m[0:2:1]

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = Tensor([1])
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a)
    assert isinstance(ret, list)
    assert len(ret) == 2
    assert ret[0] == Tensor([1])
    assert ret[1] == Tensor([2])


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_list_slice_with_default_parameter():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            m = [x, x+1, x+2]
            return m[0:2]

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = Tensor([1])
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a)
    assert isinstance(ret, list)
    assert len(ret) == 2
    assert ret[0] == Tensor([1])
    assert ret[1] == Tensor([2])


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_list_slice_with_default_parameter_2():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            m = [x, x+1, x+2]
            return m[::]

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = Tensor([1])
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a)
    assert isinstance(ret, list)
    assert len(ret) == 3
    assert ret[0] == Tensor([1])
    assert ret[1] == Tensor([2])
    assert ret[2] == Tensor([3])


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_list_slice_with_default_parameter_3():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            m = [x, x+1, x+2]
            return m[:]

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = Tensor([1])
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a)
    assert isinstance(ret, list)
    assert len(ret) == 3
    assert ret[0] == Tensor([1])
    assert ret[1] == Tensor([2])
    assert ret[2] == Tensor([3])


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_make_dict():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            m = {"x": x, "y": x+1}
            return m["x"]

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = Tensor([1])
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a)
    assert ret == Tensor([1])


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_make_dict_2():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            m = {}
            m["x"] = x
            return m["x"]

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = Tensor([1])
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a)
    assert ret == Tensor([1])


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_make_dict_3():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            m = {"x": x+1}
            return m["x"]

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = Tensor([1])
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a)
    assert ret == Tensor([2])


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_tuple_input():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y):
            return x/y

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = (1.0, 2.0, 3.0)
    b = Tensor(np.ones([2, 3]).astype(np.float32))
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a, b)
    expect = np.array([[1.0, 2.0, 3.0], [1.0, 2.0, 3.0]])
    assert np.allclose(ret.asnumpy(), expect)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_list_input():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x, y):
            return x/y

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    a = [1.0, 2.0, 3.0]
    b = Tensor(np.ones([2, 3]).astype(np.float32))
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(a, b)
    expect = np.array([[1.0, 2.0, 3.0], [1.0, 2.0, 3.0]])
    assert np.allclose(ret.asnumpy(), expect)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_handle_constant():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            a, b = x
            return (a, b)

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    m = (1, 2)
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(m)
    assert ret == (1, 2)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_handle_constant_2():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, x):
            a, b = x
            return (a, b)

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    m = [1, 2]
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(m)
    assert ret == (1, 2)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_handle_mutable_kwargs_args():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, a, *args, b=1, **kwargs):
            return a + b + args[0] + kwargs["s"]

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(1, 10, 100, s=1000)
    assert ret == 1012


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_handle_mutable_kwargs_args_2():
    """
    Feature: One stage basic operation.
    Description: Test one stage basic operation.
    Expectation: No exception.
    """
    class Net(nn.Cell):
        def construct(self, a, *args, b=1, **kwargs):
            return a + b + args[0]

    context.set_context(mode=context.PYNATIVE_MODE)
    net = Net()
    jit(net.construct, mode="PIJit", jit_config=cfg)
    ret = net(1, 10, 100, s=1000)
    assert ret == 12
