# Copyright 2023 Huawei Technologies Co., Ltd
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
from mindspore import ops
import mindspore as ms
from tests.st.utils import test_utils

ms.context.set_context(ascend_config={"precision_mode": "force_fp32"})

@test_utils.run_with_cell
def relu_forward_func(x):
    return ops.ReLU()(x)


@test_utils.run_with_cell
def relu_backward_func(x):
    return ops.grad(relu_forward_func, (0,))(x)


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
@test_utils.run_test_with_On
def test_relu(mode):
    """
    Feature: Ops.
    Description: test op relu.
    Expectation: expect correct result.
    """
    ms.context.set_context(mode=mode)
    x = ms.Tensor(np.array([[[[-1, 1, 10],
                              [1, -1, 1],
                              [10, 1, -1]]]]).astype(np.float32))
    out = relu_forward_func(x)
    expect_out = np.array([[[[0, 1, 10],
                             [1, 0, 1],
                             [10, 1, 0.]]]]).astype(np.float32)
    assert (out.asnumpy() == expect_out).all()

    grad = relu_backward_func(x)
    expect_grad = np.array([[[[0, 1, 1],
                              [1, 0, 1],
                              [1, 1, 0]]]]).astype(np.float32)
    assert (grad.asnumpy() == expect_grad).all()


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
@test_utils.run_test_with_On
def test_relu_vmap(mode):
    """
    Feature: test vmap function.
    Description: test relu op vmap.
    Expectation: expect correct result.
    """
    ms.context.set_context(mode=mode)
    axes = -1
    x = ms.Tensor(np.random.uniform(low=-1, high=1, size=(4, 3, 2)).astype(np.float32))
    net_vmap = ops.vmap(ops.vmap(relu_forward_func, in_axes=axes, out_axes=axes), in_axes=axes, out_axes=axes)
    out = net_vmap(x)
    expect_out = relu_forward_func(x)
    assert (out.asnumpy() == expect_out.asnumpy()).all()
