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
import mindspore.context as context
import mindspore.nn as nn
from mindspore import Tensor
from mindspore.ops import functional as F
from mindspore.ops import operations as P


def test_bias_add_forward_functional(nptype):
    """
    Feature: test bias_add forward for given input dtype.
    Description: test inputs for given input dtype.
    Expectation: the result match with expected result.
    """
    input_x = Tensor(np.arange(6).reshape((2, 3)).astype(nptype))
    bias = Tensor(np.arange(3).reshape((3)).astype(nptype))
    output = F.bias_add(input_x, bias)
    expected = np.array([[0., 2., 4.], [3., 5., 7.]]).astype(nptype)
    np.testing.assert_array_almost_equal(output.asnumpy(), expected)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_bias_add_forward_float32_functional():
    """
    Feature: test bias_add forward.
    Description: test float32 inputs.
    Expectation: the result match with expected result.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")
    test_bias_add_forward_functional(np.float32)
    context.set_context(mode=context.PYNATIVE_MODE, device_target="GPU")
    test_bias_add_forward_functional(np.float32)


class Net(nn.Cell):
    def __init__(self, data_format="NCHW"):
        super(Net, self).__init__()
        self.bias_add = P.BiasAdd(data_format)

    def construct(self, x, b):
        return self.bias_add(x, b)


def run_bias_add(dtype):
    x = Tensor(np.random.normal(0, 10, (2, 3)).astype(dtype))
    b = Tensor(np.ones((3,)).astype(dtype))
    net = Net()
    net(x, b)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_bias_add_gpu_valid_type():
    """
    Feature: test bias_add.
    Description: test all gpu valid type of bias_add.
    Expectation: run without exception.
    """
    run_bias_add(np.float32)
    run_bias_add(np.float16)
    run_bias_add(np.int8)
