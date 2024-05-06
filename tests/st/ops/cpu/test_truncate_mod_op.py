# Copyright 2020 Huawei Technologies Co., Ltd
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

import mindspore
import mindspore.nn as nn
from mindspore import Tensor, context
from mindspore.ops import operations as P
from mindspore.ops import functional as F

context.set_context(mode=context.GRAPH_MODE, device_target='CPU')


class TruncateMod(nn.Cell):
    def __init__(self):
        super(TruncateMod, self).__init__()
        self.truncmod = P.TruncateMod()

    def construct(self, x, y):
        res = self.truncmod(x, y)
        return res


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_truncatemod_output_diff_types():
    """
    Feature: TruncateMod cpu op
    Description: Test output values for different dtypes
    Expectation: Output matching expected values
    """
    input_x = Tensor(np.array([1, 4, -3]), mindspore.int32)
    input_y = Tensor(np.array([3, 3, 5]), mindspore.float32)

    input_x_1 = Tensor(np.array([1, 4, -3]), mindspore.float32)
    input_y_1 = Tensor(np.array([3, 3, 5]), mindspore.float32)

    input_x_2 = Tensor(np.array([1, 4, -3]), mindspore.int32)
    input_y_2 = Tensor(np.array([3, 3, 5]), mindspore.int32)

    input_x_3 = Tensor(np.array([1, 4, -7]), mindspore.int32)
    input_y_3 = Tensor(np.array([True]), mindspore.bool_)

    truncatemod_op = TruncateMod()
    out = truncatemod_op(input_x, input_y).asnumpy()
    exp = np.array([1, 1, -3])
    diff = np.abs(out - exp)
    err = np.ones(shape=exp.shape) * 1.0e-5
    assert np.all(diff < err)
    assert out.shape == exp.shape

    out_1 = truncatemod_op(input_x_1, input_y_1).asnumpy()
    exp = np.array([1, 1, -3])
    diff = np.abs(out_1 - exp)
    err = np.ones(shape=exp.shape) * 1.0e-5
    assert np.all(diff < err)
    assert out.shape == exp.shape

    out_2 = truncatemod_op(input_x_2, input_y_2).asnumpy()
    exp = np.array([1, 1, -3])
    diff = np.abs(out_2 - exp)
    err = np.ones(shape=exp.shape) * 1.0e-5
    assert np.all(diff < err)
    assert out.shape == exp.shape

    out_3 = truncatemod_op(input_x_3, input_y_3).asnumpy()
    exp = np.array([0, 0, 0])
    diff = np.abs(out_3 - exp)
    err = np.ones(shape=exp.shape) * 1.0e-5
    assert np.all(diff < err)
    assert out.shape == exp.shape


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_truncatemod_output_broadcasting():
    """
    Feature: TruncateMod cpu op
    Description: Test output values with broadcasting
    Expectation: Output matching expected values
    """
    input_x = Tensor(np.array([1, 4, -7]), mindspore.int32)
    input_y = Tensor(np.array([3]), mindspore.int32)

    out = TruncateMod()(input_x, input_y).asnumpy()
    exp = np.array([1, 1, -1])
    diff = np.abs(out - exp)
    err = np.ones(shape=exp.shape) * 1.0e-5
    assert np.all(diff < err)
    assert out.shape == exp.shape


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_truncatemod_output_broadcasting_scalar():
    """
    Feature: TruncateMod cpu op
    Description: Test output values for scalar input
    Expectation: Output matching expected values
    """
    input_x = Tensor(np.array([1, 4, -7]), mindspore.int32)
    input_y = 3

    out = TruncateMod()(input_x, input_y).asnumpy()
    exp = np.array([1, 1, -1])
    diff = np.abs(out - exp)
    err = np.ones(shape=exp.shape) * 1.0e-5
    assert np.all(diff < err)
    assert out.shape == exp.shape


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_truncatemod_dtype_not_supported():
    """
    Feature: TruncateMod cpu op
    Description: Test output for unsupported dtype
    Expectation: Raise TypeError exception
    """
    with pytest.raises(TypeError):
        input_x = Tensor(np.array([True, False]), mindspore.bool_)
        input_y = Tensor(np.array([True]), mindspore.bool_)

        _ = TruncateMod()(input_x, input_y).asnumpy()


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_truncatemod_cannot_broadcast():
    """
    Feature: TruncateMod cpu op
    Description: Test output when broadcast cannot happen
    Expectation: Raise ValueError exception
    """
    with pytest.raises(ValueError):
        input_x = Tensor(np.array([1, 4, -7]), mindspore.int32)
        input_y = Tensor(np.array([3, 2]), mindspore.int32)

        _ = TruncateMod()(input_x, input_y).asnumpy()


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_vmap_truncate_mod():
    """
    Feature: TruncateMod cpu op vmap feature.
    Description: test the vmap feature of TruncateMod.
    Expectation: success.
    """
    def manually_batched(func, input0, input1):
        out_manual = []
        for i in range(input0.shape[0]):
            out = func(input0[i], input1[i])
            out_manual.append(out)
        return F.stack(out_manual)

    x = Tensor(np.array([[2, 4, -1], [6, 2, 8]])).astype(np.int32)
    y = Tensor(np.array([[3, 3, 3], [5, 3, 4]])).astype(np.int32)
    net = TruncateMod()
    out_manual = manually_batched(net, x, y)
    out_vmap = F.vmap(net, in_axes=(0, 0))(x, y)
    assert np.array_equal(out_manual.asnumpy(), out_vmap.asnumpy())
