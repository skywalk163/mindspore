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
import mindspore.nn as nn
import mindspore.ops.operations.math_ops as ops
from mindspore.common import dtype as mstype


class TrilIndicesNet(nn.Cell):
    def __init__(self, row, col, offset=0, dtype=mstype.int32):
        super().__init__()
        self.tril_indices = ops.TrilIndices(row, col, offset, dtype)

    def construct(self):
        return self.tril_indices()


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_tril_indices_int32_positive_offset():
    """
    Feature: TrilIndcies GPU TEST.
    Description: dtype int32 and positive offset for TrilIndices.
    Expectation: the result match to numpy.
    """
    for mode in [context.PYNATIVE_MODE, context.GRAPH_MODE]:
        context.set_context(mode=mode, device_target="GPU")
        tril_indices = TrilIndicesNet(row=300, col=200, offset=50, dtype=mstype.int32)
        output = tril_indices()
        expect = np.array(np.tril_indices(n=300, m=200, k=50)).astype(np.int32)
        assert(output.asnumpy() == expect).all()


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_tril_indices_int64_negative_offset():
    """
    Feature: TrilIndcies GPU TEST.
    Description: dtype int64 and negative offset for TrilIndices.
    Expectation: the result match to numpy.
    """
    for mode in [context.PYNATIVE_MODE, context.GRAPH_MODE]:
        context.set_context(mode=mode, device_target="GPU")
        tril_indices = TrilIndicesNet(row=500, col=700, offset=-200, dtype=mstype.int64)
        output = tril_indices()
        expect = np.array(np.tril_indices(n=500, m=700, k=-200)).astype(np.int64)
        assert(output.asnumpy() == expect).all()
