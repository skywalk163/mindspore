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
import pytest
import numpy as np

import mindspore.context as context
from mindspore import Tensor
from mindspore import ops
import tests.st.utils.test_utils as test_utils


@test_utils.run_with_cell
def forward_func(x, indices):
    return ops.max_unpool2d(x, indices, kernel_size=1, stride=1, padding=0)


@test_utils.run_with_cell
def backward_func(x, indices):
    return ops.grad(forward_func, (0))(x, indices)


@pytest.mark.level0
@pytest.mark.env_onecard
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.parametrize("context_mode", [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_maxunpool2d_float32(context_mode):
    """
    Feature: maxunpool2d
    Description: test maxunpool2d
    Expectation: expect correct result.
    """
    context.set_context(mode=context_mode, device_target="Ascend")
    x = Tensor(np.array([[[[0, 1], [8, 9]]]]).astype(np.float32))
    indices = Tensor(np.array([[[[0, 1], [2, 3]]]]).astype(np.int64))
    output = forward_func(x, indices)
    expected = np.array([[[[0., 1.],
                           [8., 9.]]]], np.float32)
    np.testing.assert_allclose(output.asnumpy(), expected, rtol=1e-3)


@pytest.mark.level0
@pytest.mark.env_onecard
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.parametrize("context_mode", [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_maxunpool2d_grad_float32(context_mode):
    """
    Feature: maxunpool2d grad
    Description: test maxunpool2d grad
    Expectation: expect correct result.
    """
    context.set_context(mode=context_mode, device_target="Ascend")
    x = Tensor(np.array([[[[0, 1], [8, 9]]]]).astype(np.float32))
    indices = Tensor(np.array([[[[0, 1], [2, 3]]]]).astype(np.int64))
    x_grad = backward_func(x, indices)
    assert x_grad.asnumpy().shape == x.shape
