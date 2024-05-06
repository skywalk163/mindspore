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
import mindspore.ops as ops
from mindspore import Tensor


class Net(nn.Cell):
    def construct(self, x, fill_value):
        output = ops.full_like(x, fill_value)
        return output


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_full_normal(mode):
    """
    Feature: ops.full
    Description: Verify the result of ops.full
    Expectation: success
    """
    ms.set_context(mode=mode)
    net = Net()
    x = Tensor([[[0, 1, 2, 3], [4, 5, 6, 7], [8, 9, 10, 11]]])
    fill_value = 11
    out = net(x, fill_value)
    expect_out = np.array([[[11, 11, 11, 11], [11, 11, 11, 11], [11, 11, 11, 11]]])
    assert np.allclose(out.asnumpy(), expect_out)
