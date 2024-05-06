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

import numpy as np
import pytest

import mindspore.nn as nn
from mindspore import Tensor
from mindspore import context
from mindspore.ops.operations import _grad_ops as G
context.set_context(mode=context.GRAPH_MODE, device_target="CPU")


class NetAtanGrad(nn.Cell):
    def __init__(self):
        super(NetAtanGrad, self).__init__()
        self.atan_grad = G.AtanGrad()

    def construct(self, x, dy):
        return self.atan_grad(x, dy)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [np.float16, np.float32, np.float64])
def test_atan_grad_float(dtype):
    """
    Feature: ALL To ALL
    Description: test cases for AtanGrad
    Expectation: the result match to numpy
    """
    x = np.array([-0.5, 0, 0.5]).astype(dtype)
    dy = np.array([1, 0, -1]).astype(dtype)
    atan_grad = NetAtanGrad()
    output = atan_grad(Tensor(x), Tensor(dy))
    print(output)
    expect = dy / (1 + x * x)
    assert np.allclose(output.asnumpy(), expect)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [np.complex64, np.complex128])
def test_atan_grad_complex(dtype):
    """
    Feature: ALL To ALL
    Description: test cases for AtanGrad
    Expectation: the result match to numpy
    """
    x = np.array([-0.5, 0, 0.5]).astype(dtype)
    x = x + 0.5j * x
    dy = np.array([1, 0, -1]).astype(dtype)
    dy = dy + 0.3j * dy
    atan_grad = NetAtanGrad()
    output = atan_grad(Tensor(x), Tensor(dy))
    print(output)
    expect = dy / np.conjugate(1 + x * x)
    assert np.allclose(output.asnumpy(), expect)
