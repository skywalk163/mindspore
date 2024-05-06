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
from mindspore import Tensor


class Net(nn.Cell):
    def construct(self, x, y, z):
        output = x.ormqr(y, z)
        return output


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_tensor_ormqr(mode):
    """
    Feature: tensor.ormqr
    Description: Verify the result of tensor.ormqr
    Expectation: success
    """
    ms.set_context(mode=mode)
    net = Net()
    x = Tensor(np.array([[-114.6, 10.9, 1.1], [-0.304, 38.07, 69.38], [-0.45, -0.17, 62]]), ms.float32)
    tau = Tensor(np.array([1.55, 1.94, 3.0]), ms.float32)
    other = Tensor(np.array([[-114.6, 10.9, 1.1], [-0.304, 38.07, 69.38], [-0.45, -0.17, 62]]), ms.float32)
    output = net(x, tau, other)
    expect_output = Tensor(np.asarray([[63.82713, -13.823125, -116.28614],
                                       [-53.659264, -28.157839, -70.42702],
                                       [-79.54292, 24.00183, -41.34253]]), ms.float32)
    assert np.allclose(output.asnumpy(), expect_output.asnumpy())
