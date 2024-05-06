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
from mindspore import nn
from mindspore.ops import operations as P
from mindspore import context
from mindspore import Tensor
from .test_grad_of_dynamic import TestDynamicGrad


class NetReduceSum(nn.Cell):
    def __init__(self):
        super(NetReduceSum, self).__init__()
        self.reducesum = P.ReduceSum()
        self.axis = (0,)

    def construct(self, x):
        return self.reducesum(x, self.axis)


class NetReduceSumOther(nn.Cell):
    def __init__(self):
        super(NetReduceSumOther, self).__init__()
        self.reducesum = P.ReduceSum()

    def construct(self, x, axis):
        return self.reducesum(x, axis)


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_dynamic_reducesum_shape():
    """
    Feature: ReduceSum Grad DynamicShape.
    Description: Test case of dynamic shape for ReduceSum grad operator on CPU, GPU and Ascend.
    Expectation: success.
    """
    context.set_context(mode=context.PYNATIVE_MODE)
    test_dynamic = TestDynamicGrad(NetReduceSum())
    x = Tensor(np.random.randn(3, 4, 5).astype(np.float32))
    test_dynamic.test_dynamic_grad_net(x)


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_dynamic_reducesum_rank():
    """
    Feature: ReduceSum Grad DynamicRank.
    Description: Test case of dynamic rank for ReduceSum grad operator on CPU, GPU and Ascend.
    Expectation: success.
    """
    context.set_context(mode=context.PYNATIVE_MODE)
    test_dynamic = TestDynamicGrad(NetReduceSum())
    x = Tensor(np.random.randn(3, 4, 5).astype(np.float32))
    test_dynamic.test_dynamic_grad_net(x, True)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_dynamic_reducesum_shape_for_dynamic_axis():
    """
    Feature: ReduceSum Grad DynamicShape.
    Description: Test case of dynamic shape for ReduceSum grad operator on CPU and GPU.
    Expectation: success.
    """
    context.set_context(mode=context.PYNATIVE_MODE)
    test_dynamic = TestDynamicGrad(NetReduceSumOther())
    x = Tensor(np.random.randn(3, 4, 5).astype(np.float32))
    axis = Tensor([1, 2])
    test_dynamic.test_dynamic_grad_net((x, axis))
    