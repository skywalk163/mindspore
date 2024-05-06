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
    def construct(self, x, y):
        return x.type_as(y)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_tensor_type_as(mode):
    """
    Feature: tensor.type_as
    Description: Verify the result of output
    Expectation: success
    """
    ms.set_context(mode=mode)
    x = Tensor([[-12.1, 0.3, 3.6], [12.4, 4.5, -3.2]], dtype=ms.float32)
    y = Tensor([1, 2, 3], dtype=ms.int32)
    net = Net()
    output = net(x, y)
    expect_output = [[-12, 0, 3], [12, 4, -3]]
    assert np.allclose(output.asnumpy(), expect_output)
