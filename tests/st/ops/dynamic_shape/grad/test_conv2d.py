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
from mindspore import nn, ops, context, Tensor
from .test_grad_of_dynamic import TestDynamicGrad


class NetConv2d(nn.Cell):
    def __init__(self):
        super(NetConv2d, self).__init__()
        self.op = ops.Conv2D(4, (3, 3))

    def construct(self, x, y):
        return self.op(x, y)


def grad_dyn_case(is_dynamic_rank):
    test_dynamic = TestDynamicGrad(NetConv2d())
    x = Tensor(np.ones([4, 4, 4, 4]).astype(np.float32))
    y = Tensor(np.ones([4, 4, 3, 3]).astype(np.float32))
    test_dynamic.test_dynamic_grad_net([x, y], is_dynamic_rank)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_grad_dynamic_shape():
    """
    Feature: test Conv2D dynamic shape.
    Description: input is dynamic shape.
    Expectation: the result match with static shape
    """
    context.set_context(mode=context.PYNATIVE_MODE)
    grad_dyn_case(False)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_grad_dynamic_rank():
    """
    Feature: test Conv2D dynamic rank.
    Description: input is dynamic rank.
    Expectation: the result match with static shape
    """
    context.set_context(mode=context.PYNATIVE_MODE)
    grad_dyn_case(True)
