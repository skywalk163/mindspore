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
import mindspore
import mindspore.nn as nn
import mindspore.context as context
from mindspore import Tensor
from mindspore.ops import operations as P
from tests.st.utils import test_utils

class Net(nn.Cell):
    def __init__(self, num_sample):
        super(Net, self).__init__()
        self.random_categorical = P.RandomCategorical(mindspore.int64)
        self.num_sample = num_sample

    def construct(self, logits, seed=0):
        return self.random_categorical(logits, self.num_sample, seed)

@pytest.mark.level0
@pytest.mark.env_onecard
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
@test_utils.run_test_with_On
def test_net(mode):
    """
    Feature: test RandomCategorical op.
    Description: test RandomCategorical op.
    Expectation: success.
    """
    context.set_context(mode=mode, device_target="Ascend")
    x = np.random.random((10, 5)).astype(np.float32)
    net = Net(8)
    output = net(Tensor(x))
    expect_output_shape = (10, 8)
    assert output.shape == expect_output_shape
