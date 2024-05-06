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
def maximum_forward_func(x, y):
    return ops.Maximum()(x, y)


@test_utils.run_with_cell
def maximum_backward_func(x, y):
    return ops.grad(maximum_forward_func, (0, 1))(x, y)


@test_utils.run_with_cell
def maximum_vmap_func(x, y):
    return ops.vmap(maximum_forward_func, in_axes=0, out_axes=0)(x, y)


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.parametrize("context_mode", [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
@pytest.mark.parametrize("data_type", [np.float32])
def test_maximum_op_forward(context_mode, data_type):
    """
    Feature: Ops.
    Description: test op maximum forward.
    Expectation: expect correct result.
    """
    ms.context.set_context(mode=context_mode)
    x = ms.Tensor(np.array([1, 2, 4]).astype(data_type))
    y = ms.Tensor(np.array([2, 4, 3]).astype(data_type))
    out = maximum_forward_func(x, y)
    expect_out = np.array([2., 4., 4.]).astype(np.float32)
    np.testing.assert_allclose(out.asnumpy(), expect_out, rtol=1e-3)


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.parametrize("context_mode", [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
@pytest.mark.parametrize("data_type", [np.float32])
def test_maximum_op_backward(context_mode, data_type):
    """
    Feature: Auto grad.
    Description: test auto grad of op maximum.
    Expectation: expect correct result.
    """
    ms.context.set_context(mode=context_mode)
    x = ms.Tensor(np.array([1, 2, 4]).astype(data_type))
    y = ms.Tensor(np.array([2, 4, 3]).astype(data_type))
    grads = maximum_backward_func(x, y)
    expect_out = np.array([[0., 0., 1.], [1., 1., 0.]]).astype(np.float32)
    np.testing.assert_allclose(grads[0].asnumpy(), expect_out[0], rtol=1e-3)
    np.testing.assert_allclose(grads[1].asnumpy(), expect_out[1], rtol=1e-3)


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.parametrize("context_mode", [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
@pytest.mark.parametrize("data_type", [np.float32])
def test_maximum_op_vmap(context_mode, data_type):
    """
    Feature: test vmap function.
    Description: test maximum op vmap.
    Expectation: expect correct result.
    """
    ms.context.set_context(mode=context_mode)
    x = ms.Tensor(np.array([[3, 5, 11], [7, 5, 6]]).astype(data_type))
    y = ms.Tensor(np.array([[6, 6, 8], [3, 2, 7]]).astype(data_type))
    out = maximum_vmap_func(x, y)
    expect_out = np.array([[6., 6., 11.], [7., 5., 7.]]).astype(np.float32)
    np.testing.assert_allclose(out.asnumpy(), expect_out, rtol=1e-3)


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.parametrize("context_mode", [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_maximum_op_dynamic_shape(context_mode):
    """
    Feature: maximum ops.
    Description: test ops maximum dynamic tensor input.
    Expectation: output the right result.
    """
    ms.context.set_context(mode=context_mode)
    x_dyn = ms.Tensor(shape=[None], dtype=ms.float32)
    y_dyn = ms.Tensor(shape=[None], dtype=ms.float32)
    test_cell = test_utils.to_cell_obj(maximum_forward_func)
    test_cell.set_inputs(x_dyn, y_dyn)
    x = ms.Tensor(np.array([6, 1, 2, 4]).astype(np.float32))
    y = ms.Tensor(np.array([9, 2, 4, 3]).astype(np.float32))
    out = test_cell(x, y)
    expect_out = np.array([9., 2., 4., 4.]).astype(np.float32)
    np.testing.assert_allclose(out.asnumpy(), expect_out, rtol=1e-3)
    x_2 = ms.Tensor(np.array([3, 8, 11]).astype(np.float32))
    y_2 = ms.Tensor(np.array([6, 5, 7]).astype(np.float32))
    out_2 = test_cell(x_2, y_2)
    expect_out_2 = np.array([6., 8., 11.]).astype(np.float32)
    np.testing.assert_allclose(out_2.asnumpy(), expect_out_2, rtol=1e-3)


@pytest.mark.level1
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
# @pytest.mark.platform_arm_ascend_training 与master现象一致
@pytest.mark.parametrize("context_mode", [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_maximum_op_dynamic_rank(context_mode):
    """
    Feature: maximum ops.
    Description: test ops maximum dynamic tensor input.
    Expectation: output the right result.
    """
    ms.context.set_context(mode=context_mode)
    x_dyn = ms.Tensor(shape=None, dtype=ms.float32)
    y_dyn = ms.Tensor(shape=None, dtype=ms.float32)
    test_cell = test_utils.to_cell_obj(maximum_forward_func)
    test_cell.set_inputs(x_dyn, y_dyn)
    x = ms.Tensor(np.array([6, 1, 2, 4]).astype(np.float32))
    y = ms.Tensor(np.array([9, 2, 4, 3]).astype(np.float32))
    out = test_cell(x, y)
    expect_out = np.array([9., 2., 4., 4.]).astype(np.float32)
    np.testing.assert_allclose(out.asnumpy(), expect_out, rtol=1e-3)
    x_2 = ms.Tensor(np.array([3, 8, 11]).astype(np.float32))
    y_2 = ms.Tensor(np.array([6, 5, 7]).astype(np.float32))
    out_2 = test_cell(x_2, y_2)
    expect_out_2 = np.array([6., 8., 11.]).astype(np.float32)
    np.testing.assert_allclose(out_2.asnumpy(), expect_out_2, rtol=1e-3)


# 反向动态shape有公共问题，待解决后再放开用例
@pytest.mark.skip
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.parametrize("context_mode", [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_maximum_op_dynamic_shape_backward(context_mode):
    """
    Feature: maximum ops.
    Description: test ops maximum dynamic tensor input backward.
    Expectation: output the right result.
    """
    ms.context.set_context(mode=context_mode)
    x_dyn = ms.Tensor(shape=[None], dtype=ms.float32)
    y_dyn = ms.Tensor(shape=[None], dtype=ms.float32)
    test_cell = test_utils.to_cell_obj(maximum_backward_func)
    test_cell.set_inputs(x_dyn, y_dyn)
    x = ms.Tensor(np.array([1, 2, 4]).astype(np.float32))
    y = ms.Tensor(np.array([2, 4, 3]).astype(np.float32))
    grads = test_cell(x, y)
    expect_out = np.array([[0., 0., 1.], [1., 1., 0.]]).astype(np.float32)
    np.testing.assert_allclose(grads[0].asnumpy(), expect_out[0], rtol=1e-3)
    np.testing.assert_allclose(grads[1].asnumpy(), expect_out[1], rtol=1e-3)
    x = ms.Tensor(np.array([6, 1, 2, 4]).astype(np.float32))
    y = ms.Tensor(np.array([9, 2, 4, 3]).astype(np.float32))
    grads = test_cell(x, y)
    expect_out = np.array([[0., 0., 0., 1.], [1., 1., 1., 0.]]).astype(np.float32)
    np.testing.assert_allclose(grads[0].asnumpy(), expect_out[0], rtol=1e-3)
    np.testing.assert_allclose(grads[1].asnumpy(), expect_out[1], rtol=1e-3)


# 反向动态shape有公共问题，待解决后再放开用例
@pytest.mark.skip
@pytest.mark.env_onecard
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.parametrize("context_mode", [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_maximum_op_dynamic_rank_backward(context_mode):
    """
    Feature: maximum ops.
    Description: test ops maximum dynamic tensor input backward.
    Expectation: output the right result.
    """
    ms.context.set_context(mode=context_mode)
    x_dyn = ms.Tensor(shape=None, dtype=ms.float32)
    y_dyn = ms.Tensor(shape=None, dtype=ms.float32)
    test_cell = test_utils.to_cell_obj(maximum_backward_func)
    test_cell.set_inputs(x_dyn, y_dyn)
    x = ms.Tensor(np.array([1, 2, 4]).astype(np.float32))
    y = ms.Tensor(np.array([2, 4, 3]).astype(np.float32))
    grads = test_cell(x, y)
    expect_out = np.array([[0., 0., 1.], [1., 1., 0.]]).astype(np.float32)
    np.testing.assert_allclose(grads[0].asnumpy(), expect_out[0], rtol=1e-3)
    np.testing.assert_allclose(grads[1].asnumpy(), expect_out[1], rtol=1e-3)
    x = ms.Tensor(np.array([6, 1, 2, 4]).astype(np.float32))
    y = ms.Tensor(np.array([9, 2, 4, 3]).astype(np.float32))
    grads = test_cell(x, y)
    expect_out = np.array([[0., 0., 0., 1.], [1., 1., 1., 0.]]).astype(np.float32)
    np.testing.assert_allclose(grads[0].asnumpy(), expect_out[0], rtol=1e-3)
    np.testing.assert_allclose(grads[1].asnumpy(), expect_out[1], rtol=1e-3)
