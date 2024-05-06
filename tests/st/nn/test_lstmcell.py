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

import mindspore as ms
import mindspore.nn as nn


class Net(nn.Cell):
    def __init__(self):
        super(Net, self).__init__()
        self.lstmcell = nn.LSTMCell(10, 16, dtype=ms.float16)

    def construct(self, x, y):
        out = self.lstmcell(x, y)
        return out


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_lstmcell_para_customed_dtype(mode):
    """
    Feature: LSTMCell
    Description: Verify the result of LSTMCell specifying customed para dtype.
    Expectation: success
    """
    ms.set_context(mode=mode)
    net = Net()
    x = ms.Tensor(np.ones([5, 3, 10]).astype(np.float16))
    h = ms.Tensor(np.ones([3, 16]).astype(np.float16))
    c = ms.Tensor(np.ones([3, 16]).astype(np.float16))
    output = []
    for i in range(5):
        hx = net(x[i], (h, c))
        output.append(hx)
    expect_output_shape = (3, 16)
    assert np.allclose(expect_output_shape, output[0][0].shape)
