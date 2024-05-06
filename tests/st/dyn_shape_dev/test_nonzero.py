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
from tests.st.utils import test_utils

from mindspore import ops
import mindspore as ms


@test_utils.run_with_cell
def nonzero_forward_func(x):
    return ops.NonZero()(x)


@test_utils.run_with_cell
def nonzero_backward_func(x):
    return ops.grad(nonzero_forward_func, 0)(x)


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.parametrize("context_mode", [ms.GRAPH_MODE])
@pytest.mark.parametrize("data_type", [np.float32])
@test_utils.run_test_with_On
def test_nonzero_op_forward(context_mode, data_type):
    """
    Feature: Ops.
    Description: test op nonzero forward.
    Expectation: expect correct result.
    """
    ms.context.set_context(mode=context_mode)
    x = ms.Tensor(np.array([1, 2, 2, 4, 3]).astype(data_type))
    out = nonzero_forward_func(x)
    expect_out = np.array([[0], [1], [2], [3], [4]]).astype(np.int64)
    np.testing.assert_array_equal(out.asnumpy(), expect_out)


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.parametrize("context_mode", [ms.GRAPH_MODE])
@pytest.mark.parametrize("data_type", [np.float32])
@test_utils.run_test_with_On
def test_nonzero_op_backward(context_mode, data_type):
    """
    Feature: Auto grad.
    Description: test auto grad of op nonzero.
    Expectation: expect correct result.
    """
    ms.context.set_context(mode=context_mode)
    x = ms.Tensor(np.array([1, 2, 2, 4, 3]).astype(data_type))
    grads = nonzero_backward_func(x)
    expect_out = np.array([0., 0., 0., 0., 0.]).astype(np.float32)
    np.testing.assert_allclose(grads.asnumpy(), expect_out, rtol=1e-3)
