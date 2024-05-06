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
# ============================================================================

import numpy as np
import pytest

import mindspore.nn as nn
from mindspore import dtype
from mindspore import Tensor
import mindspore.context as context
from mindspore.ops import functional as F
from mindspore.ops import operations as P

context.set_context(mode=context.GRAPH_MODE, device_target="CPU")


class Net(nn.Cell):
    def __init__(self):
        super(Net, self).__init__()
        self.op = P.CumProd()

    def construct(self, x):
        return self.op(x, 0)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_cumprod_dshape():
    """
    Feature: Test cumprod dynamic shape.
    Description: Test cumprod dynamic shape.
    Expectation: Success.
    """
    net = Net()
    input_x_dyn = Tensor(shape=[3, None], dtype=dtype.float32)
    net.set_inputs(input_x_dyn)
    input_x = Tensor(np.random.random(([3, 10])), dtype=dtype.float32)
    output = net(input_x)
    expect_shape = (3, 10)
    assert output.asnumpy().shape == expect_shape


def test_cumprod_functional_api():
    """
    Feature: test cumprod functional API.
    Description: testcase for cumprod functional API.
    Expectation: the result match with expected result.
    """
    dtype_op = P.DType()
    x = Tensor(np.array([1, 2, 3]), dtype.float32)
    output = F.cumprod(x, 0, dtype.int32)
    expected = np.array([1, 2, 6], np.int32)
    assert dtype_op(output) == dtype.int32
    np.testing.assert_array_equal(output.asnumpy(), expected)


def test_cumprod_tensor_api():
    """
    Feature: test cumprod tensor API.
    Description: testcase for cumprod tensor API.
    Expectation: the result match with expected result.
    """
    dtype_op = P.DType()
    x = Tensor(np.array([1, 2, 3]), dtype.float32)
    output = x.cumprod(0, dtype.int32)
    expected = np.array([1, 2, 6], np.int32)
    assert dtype_op(output) == dtype.int32
    np.testing.assert_array_equal(output.asnumpy(), expected)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_cumprod_functional_tensor_modes():
    """
    Feature: test cumprod functional and tensor APIs in PyNative and Graph modes.
    Description: test case for cumprod functional and tensor APIs.
    Expectation: the result match with expected result.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="CPU")
    test_cumprod_functional_api()
    test_cumprod_tensor_api()
    context.set_context(mode=context.PYNATIVE_MODE, device_target="CPU")
    test_cumprod_functional_api()
    test_cumprod_tensor_api()
