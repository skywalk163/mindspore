# Copyright 2021 Huawei Technologies Co., Ltd
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
"""smoke tests for SparseMatrixSoftmax"""

import numpy as np
import pytest
import mindspore as ms
from mindspore import Tensor, context, nn
from mindspore.ops.operations.sparse_ops import SparseTensorDenseAdd


class Net(nn.Cell):

    def __init__(self) -> None:
        super(Net, self).__init__()
        self.op = SparseTensorDenseAdd()

    def construct(self, x1_indices, x1_values, x1_shape, x2):
        return self.op(x1_indices, x1_values, x1_shape, x2)


def generate_data(datatype="float32", indicetype="int32"):
    x1_indices = Tensor(np.array([[0, 1], [1, 2]]).astype(indicetype))
    x1_values = Tensor(np.array([1, 2]).astype(datatype))
    x1_shape = Tensor(np.array([3, 4]).astype(indicetype))
    x2 = Tensor(
        np.array([[1, 1, 1, 1], [1, 1, 1, 1], [1, 1, 1, 1]]).astype(datatype))
    data = x1_indices, x1_values, x1_shape, x2
    return data


@pytest.mark.level1
@pytest.mark.parametrize('indicetype, datatype', [("int32", "int8"),
                                                  ("int32", "int16"),
                                                  ("int32", "int32"),
                                                  ("int32", "int64"),
                                                  ("int32", "float16"),
                                                  ("int32", "float32"),
                                                  ("int32", "float64"),
                                                  ("int32", "complex64"),
                                                  ("int32", "complex128"),
                                                  ("int64", "int8"),
                                                  ("int64", "int16"),
                                                  ("int64", "int32"),
                                                  ("int64", "int64"),
                                                  ("int64", "float16"),
                                                  ("int64", "float32"),
                                                  ("int64", "float64"),
                                                  ("int64", "complex64"),
                                                  ("int64", "complex128")])
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_sparse_tensor_dense_add(indicetype, datatype):
    """
    Feature: Test sparse tensor dense add ops.
    Description: Test 2D dense sparse tensor dense add ops.
    Expectation: Success.
    """
    context.set_context(mode=context.GRAPH_MODE)
    data = generate_data(datatype=datatype, indicetype=indicetype)
    net = SparseTensorDenseAdd()
    out = net(data[0], data[1], data[2], data[3]).asnumpy()
    expected = np.array([[1, 2, 1, 1], [1, 1, 3, 1], [1, 1, 1,
                                                      1]]).astype(datatype)
    eps = 1e-6 * np.array(np.ones_like(out))
    assert np.all(expected - out < eps)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu
@pytest.mark.env_onecard
def test_sparse_tensor_dense_add_dyn():
    """
    Feature: test SparseTensorDenseAdd op in gpu.
    Description: test the ops in dynamic shape.
    Expectation: expect correct shape result.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target='GPU')
    net = Net()

    x1_indices_dyn = Tensor(shape=[None, 2], dtype=ms.int64)
    x1_values_dyn = Tensor(shape=[None], dtype=ms.float32)
    x1_shape = Tensor([3, 3], dtype=ms.int64)
    x2 = Tensor([[1, 1, 1], [1, 1, 1], [1, 1, 1]], dtype=ms.float32)
    net.set_inputs(x1_indices_dyn, x1_values_dyn, x1_shape, x2)

    x1_indices = Tensor([[0, 0], [0, 1]], dtype=ms.int64)
    x1_values = Tensor([1, 1], dtype=ms.float32)
    out = net(x1_indices, x1_values, x1_shape, x2)

    expect_out_shape = (3, 3)
    assert out.asnumpy().shape == expect_out_shape
