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
from mindspore import context
from mindspore import Tensor, nn
import mindspore.ops as P
import mindspore as ms
import numpy as np

context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")

def test_io_index_condition_1():
    """
    Description: Test condition parameter->assign->reshape, parameter->reshape, parameter->return.
    Expectation: Run without errors and the result is correct.
    """
    class Net(nn.Cell):
        def __init__(self):
            super().__init__()
            self.param0 = ms.Parameter(Tensor([4.0], ms.float32), name="param0")
            self.param1 = ms.Parameter(Tensor([[1, 2, 3], [4, 5, 6]], ms.float32), name="param1")
            self.assign = P.Assign()
            self.reshape = P.Reshape()

        def construct(self, variable, value):
            x = self.assign(variable, value)
            y = self.reshape(x, (3, 2))
            z = self.reshape(self.param1, (3, 2))
            return x, y, z, self.param0

    net = Net()
    variable = ms.Parameter(Tensor([[0.1, 0.2, 0.3], [0.4, 0.5, 0.6]], ms.float32), name="variable")
    value = Tensor([[10, 11, 12], [13, 14, 15]], ms.float32)
    x, y, z, param0 = net(variable, value)
    assert (x.asnumpy() == np.array([[10, 11, 12], [13, 14, 15]]).astype(np.float32)).all()
    assert (y.asnumpy() == np.array([[10, 11], [12, 13], [14, 15]]).astype(np.float32)).all()
    assert (z.asnumpy() == np.array([[1, 2], [3, 4], [5, 6]]).astype(np.float32)).all()
    assert (param0.asnumpy() == np.array([4.0]).astype(np.float32)).all()
    assert (variable.asnumpy() == np.array([[10, 11, 12], [13, 14, 15]]).astype(np.float32)).all()


def test_io_index_condition_2():
    """
    Description: Test condition loss_scale.
    Expectation: Run without errors and the result is correct.
    """
    class Net(nn.Cell):
        def __init__(self):
            super().__init__()
            self.loss_scale = ms.Parameter(Tensor(5000.0, ms.float32), name="loss_scale")
            self.loss_manager = nn.DynamicLossScaleUpdateCell(loss_scale_value=10, scale_factor=2, scale_window=1000)
            self.assign = P.Assign()

        def construct(self, overflow, a, sens=None):
            if sens is None:
                sens = self.loss_scale
            y = self.assign(sens, a)
            if overflow:
                overflow = self.loss_manager(self.loss_scale, overflow)
            return y, self.loss_scale

    net = Net()
    overflow = Tensor(np.array(True), ms.bool_)
    a = Tensor(20000.0, ms.float32)
    y, loss_scale = net(overflow, a)
    assert (y.asnumpy() == np.array(20000.0).astype(np.float32)).all()
    assert (loss_scale.asnumpy() == np.array(10000.0).astype(np.float32)).all()
