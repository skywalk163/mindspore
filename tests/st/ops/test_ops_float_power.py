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

import pytest
import numpy as np

import mindspore as ms
import mindspore.nn as nn
import mindspore.ops as ops


class Net(nn.Cell):
    def construct(self, x, exp):
        output = ops.float_power(x, exp)
        return output


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_float_power_real(mode):
    """
    Feature: ops.float_power
    Description: Verify the result of float_power
    Expectation: success
    """
    ms.set_context(mode=mode)
    net = Net()
    input_case = ms.Tensor(np.array([-3., -2, -1, 1, 2, 3]), ms.float32)
    output_case = net(input_case, 2)
    except_case = np.array([9.0000, 4.0000, 1.0000, 1.0000, 4.0000, 9.0000], dtype=np.float32)
    assert output_case.asnumpy().dtype == np.float64
    assert np.allclose(output_case.asnumpy(), except_case)
