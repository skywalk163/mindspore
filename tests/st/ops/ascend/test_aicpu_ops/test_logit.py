# Copyright 2024 Huawei Technologies Co., Ltd
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
import mindspore.context as context
from mindspore import Tensor
from mindspore.ops import operations as P


class Net(nn.Cell):
    def __init__(self, eps):
        super(Net, self).__init__()
        self.logit = P.Logit(eps)

    def construct(self, x):
        return self.logit(x)


@pytest.mark.level0
@pytest.mark.env_onecard
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_net(mode):
    """
    Feature: aicpu Logit
    Description: test Logit on Acsend
    Expectation: success.
    """
    context.set_context(mode=mode, device_target="Ascend")
    net = Net(eps=1e-5)
    output = net(Tensor([0.1, 0.2, 0.3], mindspore.float32))

    assert np.allclose(output.asnumpy(), [-2.1972246, -1.3862944, -0.8472978])
