# Copyright 2021-2022 Huawei Technologies Co., Ltd
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
from mindspore.ops import operations as P
from mindspore.common import dtype as mstype


class SortNet(nn.Cell):
    def __init__(self, axis, descending):
        super(SortNet, self).__init__()
        self.sort = P.Sort(axis, descending)

    def construct(self, x):
        return self.sort(x)


def sort_1d(descending, nptype):
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")

    x_numpy = np.array([1, 0, 3, 4]).astype(nptype)
    x = Tensor(x_numpy)
    sort_net = SortNet(0, descending)
    output, indices = sort_net(x)

    expected_output = np.sort(x_numpy, 0)
    expected_indices = np.array([1, 0, 2, 3])
    expected_indices_bool_descend = np.array([0, 2, 3, 1])
    expected_indices_bool_ascend = np.array([1, 0, 2, 3])

    if descending:
        expected_output = expected_output[::-1]
        expected_indices = expected_indices[::-1]

    np.testing.assert_array_equal(output.asnumpy(), expected_output)
    if nptype is np.bool:
        if descending:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_descend)
        else:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_ascend)
    else:
        np.testing.assert_array_equal(indices.asnumpy(), expected_indices)


def sort_3d(descending, nptype):
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")

    x_numpy = np.array([[[1, 2, 3, 4],
                         [8, 7, 2, 0],
                         [9, 4, 1, 8]],
                        [[5, 4, 1, 8],
                         [2, 9, 0, 7],
                         [6, 1, 7, 4]]]).astype(nptype)
    x = Tensor(x_numpy)

    axis = -1
    sort_net = SortNet(axis, descending)
    output, indices = sort_net(x)

    expected_output = np.sort(x_numpy, axis, kind="stable")
    expected_indices = np.array([[[0, 1, 2, 3],
                                  [3, 2, 1, 0],
                                  [2, 1, 3, 0]],
                                 [[2, 1, 0, 3],
                                  [2, 0, 3, 1],
                                  [1, 3, 0, 2]]])
    expected_indices_bool_ascend = np.array([[[0, 1, 2, 3],
                                              [3, 0, 1, 2],
                                              [0, 1, 2, 3]],
                                             [[0, 1, 2, 3],
                                              [2, 0, 1, 3],
                                              [0, 1, 2, 3]]])
    expected_indices_bool_descend = np.array([[[0, 1, 2, 3],
                                               [0, 1, 2, 3],
                                               [0, 1, 2, 3]],
                                              [[0, 1, 2, 3],
                                               [0, 1, 3, 2],
                                               [0, 1, 2, 3]]])
    if descending:
        expected_output = expected_output[:, :, ::-1]
        expected_indices = expected_indices[:, :, ::-1]

    np.testing.assert_array_equal(output.asnumpy(), expected_output)
    if nptype is np.bool:
        if descending:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_descend)
        else:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_ascend)
    else:
        np.testing.assert_array_equal(indices.asnumpy(), expected_indices)

    axis = 1
    sort_net = SortNet(axis, descending)
    output, indices = sort_net(x)

    expected_output = np.sort(x_numpy, axis, kind="stable")
    expected_indices = np.array([[[0, 0, 2, 1],
                                  [1, 2, 1, 0],
                                  [2, 1, 0, 2]],
                                 [[1, 2, 1, 2],
                                  [0, 0, 0, 1],
                                  [2, 1, 2, 0]]])
    expected_indices_bool_ascend = np.array([[[0, 0, 0, 1],
                                              [1, 1, 1, 0],
                                              [2, 2, 2, 2]],
                                             [[0, 0, 1, 0],
                                              [1, 1, 0, 1],
                                              [2, 2, 2, 2]]])
    expected_indices_bool_descend = np.array([[[0, 0, 0, 0],
                                               [1, 1, 1, 2],
                                               [2, 2, 2, 1]],
                                              [[0, 0, 0, 0],
                                               [1, 1, 2, 1],
                                               [2, 2, 1, 2]]])
    if descending:
        expected_output = expected_output[:, ::-1, :]
        expected_indices = expected_indices[:, ::-1, :]

    np.testing.assert_array_equal(output.asnumpy(), expected_output)
    if nptype is np.bool:
        if descending:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_descend)
        else:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_ascend)
    else:
        np.testing.assert_array_equal(indices.asnumpy(), expected_indices)

    axis = -3
    sort_net = SortNet(axis, descending)
    output, indices = sort_net(x)

    expected_output = np.sort(x_numpy, axis, kind="stable")
    expected_indices = np.array([[[0, 0, 1, 0],
                                  [1, 0, 1, 0],
                                  [1, 1, 0, 1]],
                                 [[1, 1, 0, 1],
                                  [0, 1, 0, 1],
                                  [0, 0, 1, 0]]])
    expected_indices_bool_ascend = np.array([[[0, 0, 0, 0],
                                              [0, 0, 1, 0],
                                              [0, 0, 0, 0]],
                                             [[1, 1, 1, 1],
                                              [1, 1, 0, 1],
                                              [1, 1, 1, 1]]])
    expected_indices_bool_descend = np.array([[[0, 0, 0, 0],
                                               [0, 0, 0, 1],
                                               [0, 0, 0, 0]],
                                              [[1, 1, 1, 1],
                                               [1, 1, 1, 0],
                                               [1, 1, 1, 1]]])
    if descending:
        expected_output = expected_output[::-1, :, :]
        expected_indices = expected_indices[::-1, :, :]

    np.testing.assert_array_equal(output.asnumpy(), expected_output)
    if nptype is np.bool:
        if descending:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_descend)
        else:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_ascend)
    else:
        np.testing.assert_array_equal(indices.asnumpy(), expected_indices)


def dynamic_sort_3d(descending, nptype):
    """
    Feature: test sort dynamic function interface.
    Description: test interface.
    Expectation: the result match with numpy result
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")

    x_numpy = np.array([[[1, 2, 3, 4],
                         [8, 7, 2, 0],
                         [9, 4, 1, 8]],
                        [[5, 4, 1, 8],
                         [2, 9, 0, 7],
                         [6, 1, 7, 4]]]).astype(nptype)
    x = Tensor(x_numpy)

    axis = -1
    sort_net = SortNet(axis, descending)

    dy_shape = [None for _ in x_numpy.shape]
    input_dyn = Tensor(shape=dy_shape, dtype=x.dtype)
    sort_net.set_inputs(input_dyn)
    output, indices = sort_net(x)

    expected_output = np.sort(x_numpy, axis, kind="stable")
    expected_indices = np.array([[[0, 1, 2, 3],
                                  [3, 2, 1, 0],
                                  [2, 1, 3, 0]],
                                 [[2, 1, 0, 3],
                                  [2, 0, 3, 1],
                                  [1, 3, 0, 2]]])
    expected_indices_bool_ascend = np.array([[[0, 1, 2, 3],
                                              [3, 0, 1, 2],
                                              [0, 1, 2, 3]],
                                             [[0, 1, 2, 3],
                                              [2, 0, 1, 3],
                                              [0, 1, 2, 3]]])
    expected_indices_bool_descend = np.array([[[0, 1, 2, 3],
                                               [0, 1, 2, 3],
                                               [0, 1, 2, 3]],
                                              [[0, 1, 2, 3],
                                               [0, 1, 3, 2],
                                               [0, 1, 2, 3]]])
    if descending:
        expected_output = expected_output[:, :, ::-1]
        expected_indices = expected_indices[:, :, ::-1]

    np.testing.assert_array_equal(output.asnumpy(), expected_output)
    if nptype is np.bool:
        if descending:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_descend)
        else:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_ascend)
    else:
        np.testing.assert_array_equal(indices.asnumpy(), expected_indices)

    axis = 1
    sort_net = SortNet(axis, descending)
    dy_shape = [None for _ in x_numpy.shape]
    input_dyn = Tensor(shape=dy_shape, dtype=x.dtype)
    sort_net.set_inputs(input_dyn)
    output, indices = sort_net(x)

    expected_output = np.sort(x_numpy, axis, kind="stable")
    expected_indices = np.array([[[0, 0, 2, 1],
                                  [1, 2, 1, 0],
                                  [2, 1, 0, 2]],
                                 [[1, 2, 1, 2],
                                  [0, 0, 0, 1],
                                  [2, 1, 2, 0]]])
    expected_indices_bool_ascend = np.array([[[0, 0, 0, 1],
                                              [1, 1, 1, 0],
                                              [2, 2, 2, 2]],
                                             [[0, 0, 1, 0],
                                              [1, 1, 0, 1],
                                              [2, 2, 2, 2]]])
    expected_indices_bool_descend = np.array([[[0, 0, 0, 0],
                                               [1, 1, 1, 2],
                                               [2, 2, 2, 1]],
                                              [[0, 0, 0, 0],
                                               [1, 1, 2, 1],
                                               [2, 2, 1, 2]]])
    if descending:
        expected_output = expected_output[:, ::-1, :]
        expected_indices = expected_indices[:, ::-1, :]

    np.testing.assert_array_equal(output.asnumpy(), expected_output)
    if nptype is np.bool:
        if descending:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_descend)
        else:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_ascend)
    else:
        np.testing.assert_array_equal(indices.asnumpy(), expected_indices)

    axis = -3
    sort_net = SortNet(axis, descending)
    dy_shape = [None for _ in x_numpy.shape]
    input_dyn = Tensor(shape=dy_shape, dtype=x.dtype)
    sort_net.set_inputs(input_dyn)
    output, indices = sort_net(x)

    expected_output = np.sort(x_numpy, axis, kind="stable")
    expected_indices = np.array([[[0, 0, 1, 0],
                                  [1, 0, 1, 0],
                                  [1, 1, 0, 1]],
                                 [[1, 1, 0, 1],
                                  [0, 1, 0, 1],
                                  [0, 0, 1, 0]]])
    expected_indices_bool_ascend = np.array([[[0, 0, 0, 0],
                                              [0, 0, 1, 0],
                                              [0, 0, 0, 0]],
                                             [[1, 1, 1, 1],
                                              [1, 1, 0, 1],
                                              [1, 1, 1, 1]]])
    expected_indices_bool_descend = np.array([[[0, 0, 0, 0],
                                               [0, 0, 0, 1],
                                               [0, 0, 0, 0]],
                                              [[1, 1, 1, 1],
                                               [1, 1, 1, 0],
                                               [1, 1, 1, 1]]])
    if descending:
        expected_output = expected_output[::-1, :, :]
        expected_indices = expected_indices[::-1, :, :]

    np.testing.assert_array_equal(output.asnumpy(), expected_output)
    if nptype is np.bool:
        if descending:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_descend)
        else:
            np.testing.assert_array_equal(indices.asnumpy(), expected_indices_bool_ascend)
    else:
        np.testing.assert_array_equal(indices.asnumpy(), expected_indices)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize("descending", [True, False])
@pytest.mark.parametrize("nptype", [np.bool, np.int8, np.int16, np.int32, np.int64, np.uint8, np.float16, np.float32,
                                    np.float64])
def test_sort1d(descending, nptype):
    """
    Feature: test gpu sort accuracy with 1d input.
    Description: test interface.
    Expectation: the result match with numpy result.
    """
    sort_1d(descending, nptype)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize("descending", [True, False])
@pytest.mark.parametrize("nptype", [np.bool, np.int8, np.int16, np.int32, np.int64, np.uint8, np.float16, np.float32,
                                    np.float64])
def test_sort3d(descending, nptype):
    """
    Feature: test gpu sort accuracy with 3d input.
    Description: test interface.
    Expectation: the result match with numpy result.
    """
    sort_3d(descending, nptype)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize("descending", [True, False])
@pytest.mark.parametrize("nptype", [np.bool, np.int8, np.int16, np.int32, np.int64, np.uint8, np.float16, np.float32,
                                    np.float64])
def test_gpu_dynamic_sort3d(descending, nptype):
    """
    Feature: test gpu sort dynamic function interface.
    Description: test interface.
    Expectation: the result match with numpy result.
    """
    dynamic_sort_3d(descending, nptype)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_sort_tensor_api_modes(mode):
    """
    Feature: Test sort tensor api.
    Description: Test sort tensor api for Graph and PyNative modes.
    Expectation: The result match to the expect value.
    """
    context.set_context(mode=mode, device_target="GPU")
    x = Tensor([[8, 2, 1], [5, 9, 3], [4, 6, 7]], mstype.float16)
    (output_1, output_2) = x.sort()
    expected_1 = np.array([[1, 2, 8], [3, 5, 9], [4, 6, 7]], np.float16)
    expected_2 = np.array([[2, 1, 0], [2, 0, 1], [0, 1, 2]])
    np.testing.assert_array_equal(output_1.asnumpy(), expected_1)
    np.testing.assert_array_equal(output_2.asnumpy(), expected_2)
