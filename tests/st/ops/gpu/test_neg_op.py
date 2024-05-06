# Copyright 2019-2021 Huawei Technologies Co., Ltd
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

import mindspore.context as context
import mindspore.nn as nn
from mindspore import Tensor
from mindspore.ops import operations as P
from mindspore.common import dtype as mstype


class NetNeg(nn.Cell):
    def __init__(self):
        super(NetNeg, self).__init__()
        self.neg = P.Neg()

    def construct(self, x):
        return self.neg(x)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize("dtype", [np.int8, np.int16, np.int32, np.int64,
                                   np.uint8, np.uint16, np.uint32, np.uint64,
                                   np.float16, np.float32, np.float64,
                                   np.complex64, np.complex128])
@pytest.mark.parametrize("mode", [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_neg_all_types(dtype, mode):
    """
    Feature: neg
    Description: test cases for neg
    Expectation: success
    """
    x0_np = np.random.uniform(-2, 2, (2, 3, 4, 4)).astype(dtype)
    x1_np = np.random.uniform(-2, 2, 1).astype(dtype)
    x0 = Tensor(x0_np)
    x1 = Tensor(x1_np)
    expect0 = np.negative(x0_np)
    expect1 = np.negative(x1_np)
    error0 = np.ones(shape=expect0.shape) * 1.0e-3
    error1 = np.ones(shape=expect1.shape) * 1.0e-3

    context.set_context(mode=mode, device_target="GPU")
    neg_net = NetNeg()
    output0 = neg_net(x0)
    diff0 = output0.asnumpy() - expect0
    assert np.all(diff0 < error0)
    assert output0.shape == expect0.shape
    output1 = neg_net(x1)
    diff1 = output1.asnumpy() - expect1
    assert np.all(diff1 < error1)
    assert output1.shape == expect1.shape


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_neg_complex():
    """
    Feature: Test neg in complex dtype.
    Description: Test neg in complex dtype.
    Expectation: the result match given one.
    """
    x0_np = P.Complex()(Tensor(3.0), Tensor(5.0))
    x0 = Tensor(x0_np)
    neg_net = NetNeg()
    output0 = neg_net(x0)
    expect0 = np.asarray(np.complex(-3.0 - 5.0j), dtype=np.complex64)
    assert np.allclose(output0.asnumpy(), expect0)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_neg_tensor_api_modes(mode):
    """
    Feature: Test neg tensor api.
    Description: Test neg tensor api for Graph and PyNative modes.
    Expectation: The result match to the expect value.
    """
    context.set_context(mode=mode, device_target="GPU")
    x = Tensor([1, 2, -1, 2, 0, -3.5], mstype.float32)
    output = x.neg()
    expected = np.array([-1, -2, 1, -2, 0, 3.5], np.float32)
    np.testing.assert_array_equal(output.asnumpy(), expected)
