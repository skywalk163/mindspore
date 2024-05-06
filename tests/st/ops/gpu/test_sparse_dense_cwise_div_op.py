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
import mindspore.context as context
from mindspore.nn import Cell
from mindspore import Tensor
from mindspore.ops.operations.sparse_ops import SparseDenseCwiseDiv
from mindspore.common import dtype as mstype


class SparseDenseCwisediv(Cell):
    def __init__(self):
        super().__init__()
        self.sparsedensecwisediv = SparseDenseCwiseDiv()

    def construct(self, x1_indices, x1_values, x1_shape, x2):
        return self.sparsedensecwisediv(x1_indices, x1_values, x1_shape, x2)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_sparse_dense_cwise_div():
    """
    Feature:  SparseDenseCwiseDiv 4 inputs and 1 output.
    Description: compute result of SparseDenseCwiseDiv.
    Expectation: The result matches tensorflow implementation.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")

    x1_indices = np.array([[1, 1, 1], [1, 1, 2]])
    x1_values = np.array([2, 2])
    x1_shape = np.array([2, 2, 3])
    x2 = np.array([[[1, 2, 3], [4, 5, 6]], [[7, 8, 9], [10, 11, 12]]])
    expected_out = np.array([0, 0])

    net = SparseDenseCwisediv()
    out = net(Tensor(x1_indices, dtype=mstype.int64), Tensor(x1_values, dtype=mstype.int64),
              Tensor(x1_shape, dtype=mstype.int64), Tensor(x2, dtype=mstype.int64))
    np.testing.assert_almost_equal(out.asnumpy(), expected_out)
