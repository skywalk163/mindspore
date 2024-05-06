# Copyright 2020 Huawei Technologies Co., Ltd
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

from mindspore import context
from mindspore import log as logger
from mindspore.common.tensor import Tensor
from mindspore.nn import Cell
from mindspore.ops import operations as P
from mindspore.train import Model
from mindspore.common import dtype as mstype

context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")


class Greater(Cell):
    def __init__(self):
        super(Greater, self).__init__()
        self.greater = P.Greater()

    def construct(self, inputa, inputb):
        return self.greater(inputa, inputb)


def me_greater(inputa, inputb):
    net = Greater()
    net.set_train()
    model = Model(net)

    out = model.predict(inputa, inputb)
    logger.info("Check input a: ")
    logger.info(inputa)
    logger.info("Check input b: ")
    logger.info(inputb)
    return out.asnumpy()


@pytest.mark.ssd_tbe
def test_greater_2d_scalar0():
    a = np.random.randint(-5, 5, [8, 32]).astype(np.int32)
    b = np.random.randint(-5, 5, [8, 32]).astype(np.int32)
    out_me = me_greater(Tensor(a), Tensor(b))
    logger.info("Check me result:")
    logger.info(out_me)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend910b_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_greater_bf16(mode):
    """
    Feature: primitive greater
    Description: Verify the result of primitive Greater
    Expectation: success
    """
    context.set_context(mode=mode)
    net = Greater()

    inputs = Tensor(np.array([[1.0, 0.0], [1.0, 2.0]]), mstype.bfloat16)
    other = Tensor(np.array([[1.0, 1.0], [0.0, 1.0]]), mstype.bfloat16)
    value = net(inputs, other)
    expect_value = np.array([[False, False], [True, True]])
    np.testing.assert_array_equal(expect_value, value.asnumpy())
