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
import mindspore.context as context
from mindspore import Tensor, nn
from mindspore.common import dtype as mstype
from mindspore import Parameter


class IfByIfNet(nn.Cell):
    def __init__(self):
        super().__init__()
        self.param_a = Parameter(Tensor(10, mstype.float32), name="a")
        self.zero = Parameter(Tensor(0, mstype.float32), name="zero")

    def construct(self, x):
        out = self.zero
        out1 = self.zero
        if x > 0:
            out = out + self.param_a
            out1 = out1 * self.param_a
        if x > 2:
            out1 += self.param_a
            out1 *= out
            out += self.param_a
        out1 += self.param_a
        out2 = out
        return out, out1, out2


def test_if_by_if_ge():
    """
    Feature: Control flow(if) implement
    Description: test if_in_if with ge backend.
    Expectation: success.
    """
    context.set_context(mode=context.GRAPH_MODE)
    net = IfByIfNet()
    x = Tensor(3, mstype.int32)
    out0, out1, out2 = net(x)
    assert out0 == Tensor(20, mstype.float32)
    assert out1 == Tensor(110, mstype.float32)
    assert out2 == Tensor(20, mstype.float32)

if __name__ == "__main__":
    test_if_by_if_ge()
