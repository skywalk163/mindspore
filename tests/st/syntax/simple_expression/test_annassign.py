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
""" test syntax for annassign expression """

import pytest
import mindspore.nn as nn
from mindspore import context

context.set_context(mode=context.GRAPH_MODE)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_annassign():
    """
    Feature: simple expression
    Description: annassign expression
    Expectation: No exception
    """
    class Net(nn.Cell):
        def __init__(self):
            super(Net, self).__init__()
            self.m = 1

        def construct(self):
            x: int = 1
            y: float  # pylint: disable=W0612
            self.m: int = 2
            return self.m + x

    net = Net()
    ret = net()
    assert ret == 3


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_annassign_arg():
    """
    Feature: simple expression
    Description: annassign expression
    Expectation: No exception
    """
    class Net(nn.Cell):
        def __init__(self):
            super(Net, self).__init__()
            self.m = 1

        def construct(self, x: int):
            self.m: float = 2 + x
            return self.m

    net = Net()
    ret = net(1)
    assert ret == 3
