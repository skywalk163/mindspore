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

import pytest
import numpy as np
import mindspore as ms
import mindspore.nn as nn
from mindspore.ops import operations as P
from mindspore.ops import functional as F
from mindspore import Tensor
import mindspore.numpy as ms_np


class RandomGammaTEST(nn.Cell):
    def __init__(self, shape=None, seed=0):
        super(RandomGammaTEST, self).__init__()
        self.shape = shape
        self.random_gamma = P.RandomGamma(seed)

    def construct(self, alpha):
        return self.random_gamma(self.shape, alpha)


class RandomGamma(nn.Cell):

    def construct(self, shape, alpha, seed=0):
        return F.random_gamma(shape, alpha, seed)


class RandomGammaDR(nn.Cell):
    def __init__(self):
        super(RandomGammaDR, self).__init__()
        self.reducesum = P.ReduceSum(keep_dims=False)

    def construct(self, shape, alpha, seed=0):
        axis = ms_np.randint(1, 2, (2,))
        rand_axis = ms_np.unique(axis)
        outshape = self.reducesum(shape, rand_axis)
        outalpha = self.reducesum(alpha, rand_axis)
        return F.random_gamma(outshape, outalpha, seed), outshape, outalpha


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
def test_dynamic_rank():
    """
    Feature: RandomGamma cpu kernel for dynamic rank
    Description: test the random gamma alpha is a tensor.
    Expectation: match to tensorflow benchmark.
    """

    ms.set_context(mode=ms.GRAPH_MODE, device_target='CPU')
    shape_ = Tensor(np.random.randint(low=1, high=8, size=(2, 2)), ms.int32)
    alpha_ = Tensor(np.random.randint(low=1, high=5, size=(2, 2)), ms.float32)
    net = RandomGammaDR()
    input_dyn_shape = [None, None]
    alpha_dyn_shape = [None, None]
    net.set_inputs(Tensor(shape=input_dyn_shape, dtype=ms.int32),
                   Tensor(shape=alpha_dyn_shape, dtype=ms.float32))
    output, out_s, out_a = net(shape_, alpha_)
    expect = np.concatenate((out_s, np.shape(out_a)), axis=0)
    assert (output.shape == expect).all()


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.parametrize("dtype", [ms.float64, ms.float32, ms.float16])
def test_random_gamma_op(dtype):
    """
    Feature: RandomGamma cpu kernel
    Description: test the random gamma alpha is a tensor.
    Expectation: match to tensorflow benchmark.
    """

    shape = Tensor(np.array([3, 1, 2]), ms.int32)
    alpha = Tensor(np.array([5, 6]), ms.float32)
    gamma_test = RandomGammaTEST(shape=shape, seed=3)
    expect = np.array([3, 1, 2, 2])

    ms.set_context(mode=ms.GRAPH_MODE, device_target='CPU')
    output = gamma_test(alpha)
    print(output)
    assert (output.shape == expect).all()

    gamma = P.RandomGamma(seed=3)
    output_2 = gamma(shape, alpha)
    print(output_2)
    assert (output_2.shape == expect).all()

    net = RandomGamma()
    input_dyn_shape = [None]
    alpha_dyn_shape = [None]
    net.set_inputs(Tensor(shape=input_dyn_shape, dtype=ms.int32),
                   Tensor(shape=alpha_dyn_shape, dtype=ms.float32), 1)
    output = net(shape, alpha, seed=1)
    print(output)


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.parametrize("dtype", [ms.float64, ms.float32, ms.float16])
def test_random_gamma_functional(dtype):
    """
    Feature: Functional interface of RandomGamma cpu kernel
    Description: test the random gamma alpha is a tensor.
    Expectation: match to tensorflow benchmark.
    """

    ms.set_context(mode=ms.GRAPH_MODE, device_target='CPU')
    shape = Tensor(np.array([10, 10]), ms.int32)
    alpha = Tensor(np.array([[3, 4], [5, 6]]), dtype)
    output = F.random_gamma(shape=shape, alpha=alpha, seed=2)
    expect = np.array([10, 10, 2, 2])

    print(output)
    assert (output.shape == expect).all()

    ms.set_context(mode=ms.PYNATIVE_MODE, device_target='CPU')
    shape = Tensor(np.array([4, 2]), ms.int32)
    alpha = Tensor(np.array([[3, 4], [5, 6]]), ms.float32)
    output = F.random_gamma(shape=shape, alpha=alpha, seed=1)
    expect = np.array([4, 2, 2, 2])

    print(output)
    assert (output.shape == expect).all()
