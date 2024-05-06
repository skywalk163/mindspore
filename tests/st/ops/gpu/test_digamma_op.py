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
import torch
import pytest
import mindspore.context as context
import mindspore.nn as nn
from mindspore.ops.operations.math_ops import Digamma
from mindspore import Tensor
from mindspore.common.api import ms_function


class DigammaNet(nn.Cell):
    def __init__(self):
        super(DigammaNet, self).__init__()
        self.digamma = Digamma()

    @ms_function
    def construct(self, x):
        return self.digamma(x)


def get_dtype(dtype="float16"):
    if dtype == "float16":
        nptype = np.float16
        pttype = np.float64
    elif dtype == "float32":
        nptype = np.float32
        pttype = np.float32
    elif dtype == "float64":
        nptype = np.float64
        pttype = np.float64
    else:
        print("The attr 'dtype' must in [float16, float32, float64]")
    return nptype, pttype


def digamma(dtype, input_x, loss):
    context.set_context(mode=context.GRAPH_MODE, device_target='GPU')
    nptype, pttype = get_dtype(dtype)
    digamma_net = DigammaNet()
    digamma_output = digamma_net(Tensor(input_x.astype(nptype)))
    digamma_expect = torch.digamma(torch.tensor(input_x.astype(pttype)))
    assert np.allclose(digamma_output.asnumpy(), digamma_expect.numpy().astype(nptype), loss, loss)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_digamma_graph_true_float16():
    """
    Feature: ALL To ALL
    Description: test cases for BartlettWindow
    Expectation: the result match to torch
    """
    input_x = np.array([1.5, 0.5, 9])
    digamma(dtype="float16", input_x=input_x, loss=1.0e-3)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_digamma_graph_true_float32():
    """
    Feature: ALL To ALL
    Description: test cases for BartlettWindow
    Expectation: the result match to torch
    """
    input_x = np.array([5, 0.5, 9, 5.6])
    digamma(dtype="float32", input_x=input_x, loss=1.0e-4)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_digamma_graph_true_float64():
    """
    Feature: ALL To ALL
    Description: test cases for BartlettWindow
    Expectation: the result match to torch
    """
    input_x = np.array([5, 0.5, 9, 5.6])
    digamma(dtype="float64", input_x=input_x, loss=1.0e-5)
