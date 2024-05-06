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
import mindspore as ms
import mindspore.nn as nn
from mindspore import ops


class Net(nn.Cell):
    def construct(self, x, periodic=True):
        return ops.hamming_window(x, periodic)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_hanmming_window(mode):
    """
    Feature: hamming_window
    Description: Verify the result of hamming_window
    Expectation: success
    """
    ms.set_context(mode=mode)
    window_length = 6
    net = Net()
    output = net(window_length)
    expect_output = np.array([0.08, 0.31, 0.77, 1., 0.77, 0.31], dtype=np.float32)
    assert np.allclose(output.asnumpy(), expect_output)
