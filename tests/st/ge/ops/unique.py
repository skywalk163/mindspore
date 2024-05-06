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
import mindspore.context as context
import mindspore.nn as nn
from mindspore import Tensor
import mindspore.common.dtype as mstype
from mindspore.ops import operations as P


context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")


class Net(nn.Cell):
    def __init__(self):
        super(Net, self).__init__()
        self.unique = P.Unique()

    def construct(self, x):
        x = self.unique(x)
        return (x[0], x[1])


def test_unique():
    """
    Feature: for Unique op
    Description: inputs are integers
    Expectation: the result is correct
    """
    x = Tensor(np.array([1, 1, 2, 3, 3, 3]), mstype.int32)
    unique = Net()
    output = unique(x)
    expect1 = np.array([1, 2, 3])
    expect2 = np.array([0, 0, 1, 2, 2, 2])
    assert (output[0].asnumpy() == expect1).all()
    assert (output[1].asnumpy() == expect2).all()


if __name__ == "__main__":
    test_unique()
