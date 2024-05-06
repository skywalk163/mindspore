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
from mindspore import Tensor, nn, ops


class Net(nn.Cell):
    def construct(self, x, y):
        return ops.zeta(x, y)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_net(mode):
    """
    Feature: test ops.zeta
    Description: verify the result of zeta
    Expectation: assertion success
    """
    ms.set_context(mode=mode)
    x = Tensor(np.array([2., 4.]), ms.float32)
    net = Net()
    output = net(x, 1)
    np_out = np.array([1.6449, 1.0823])
    assert np.allclose(output.asnumpy(), np_out, atol=0.0001, rtol=0.0001)
