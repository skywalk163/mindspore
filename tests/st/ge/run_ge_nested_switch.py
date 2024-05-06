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
import mindspore
from mindspore import context
from mindspore import Tensor, nn
import mindspore.ops as P
import numpy as np

context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")


def test_ge_nested_switch():
    """
    Description: Test GE nested switch.
    Description: Support nested switch in GE.
    Expectation: Run without errors.
    """
    class Net(nn.Cell):
        def __init__(self):
            super().__init__()
            self.add = P.Add()

        def cal(self, x, y):
            if x < 0:
                y = [y, y]
            else:
                x = x + 2
                y = [y + 1, y + 2]
                return x, y
            x = x + 1
            return x, y
        def construct(self, x, y):
            m, n = self.cal(x, y)
            m = [m, m]
            return m, n

    net = Net()
    x = Tensor(2.0, mindspore.float32)
    y = Tensor(np.zeros([2, 3]), mindspore.int32)
    net(x, y)
