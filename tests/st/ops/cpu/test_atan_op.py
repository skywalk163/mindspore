# Copyright 2021-2022 Huawei Technologies Co., Ltd
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
from mindspore import Tensor
from mindspore import context
from mindspore.ops import operations as P
context.set_context(mode=context.GRAPH_MODE, device_target="CPU")


class NetAtan(nn.Cell):
    def __init__(self):
        super(NetAtan, self).__init__()
        self.atan = P.Atan()

    def construct(self, x):
        return self.atan(x)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [np.float32, np.float64])
def test_atan(dtype):
    """
    Feature: ALL To ALL
    Description: test cases for Atan
    Expectation: the result match to numpy
    """
    np_array = np.array([-1, -0.5, 0, 0.5, 1], dtype=dtype)
    input_x = Tensor(np_array)
    net = NetAtan()
    output = net(input_x)
    print(output)
    expect = np.arctan(np_array)
    assert np.allclose(output.asnumpy(), expect)


def test_atan_forward_tensor_api(nptype):
    """
    Feature: test atan forward tensor api for given input dtype.
    Description: test inputs for given input dtype.
    Expectation: the result match with expected result.
    """
    x = Tensor(np.array([1.0, 0.0]).astype(nptype))
    output = x.atan()
    expected = np.array([0.7853982, 0.0]).astype(nptype)
    np.testing.assert_array_almost_equal(output.asnumpy(), expected)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_atan_forward_float32_tensor_api():
    """
    Feature: test atan forward tensor api.
    Description: test float32 inputs.
    Expectation: the result match with expected result.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="CPU")
    test_atan_forward_tensor_api(np.float32)
    context.set_context(mode=context.PYNATIVE_MODE, device_target="CPU")
    test_atan_forward_tensor_api(np.float32)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [np.float16, np.float32, np.float64])
def test_atan_float(dtype):
    """
    Feature: ALL To ALL
    Description: test cases for Atan
    Expectation: the result match to numpy
    """
    np_array = np.array([-1, -0.5, 0, 0.5, 1], dtype=dtype)
    input_x = Tensor(np_array)
    net = NetAtan()
    output = net(input_x)
    print(output)
    expect = np.arctan(np_array)
    assert np.allclose(output.asnumpy(), expect)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [np.complex64, np.complex128])
def test_atan_complex(dtype):
    """
    Feature: ALL To ALL
    Description: test cases for Atan
    Expectation: the result match to numpy
    """
    np_array = np.array([-1, -0.5, 0, 0.5, 1], dtype=dtype)
    np_array = np_array + 0.5j * np_array
    input_x = Tensor(np_array)
    net = NetAtan()
    output = net(input_x)
    print(output)
    expect = np.arctan(np_array)
    assert np.allclose(output.asnumpy(), expect)


if __name__ == '__main__':
    test_atan_forward_float32_tensor_api()
