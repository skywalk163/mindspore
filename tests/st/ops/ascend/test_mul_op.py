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

from mindspore import Tensor
import mindspore.context as context
from mindspore.common import dtype as mstype


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_mul_tensor_api_modes(mode):
    """
    Feature: Test mul tensor api.
    Description: Test mul tensor api for Graph and PyNative modes.
    Expectation: The result match to the expect value.
    """
    context.set_context(mode=mode, device_target="Ascend")
    x = Tensor([1.0, 2.0, 3.0], mstype.float32)
    y = Tensor([4.0, 5.0, 6.0], mstype.float32)
    output = x.mul(y)
    expected = np.array([4., 10., 18.], np.float32)
    np.testing.assert_array_equal(output.asnumpy(), expected)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend910b_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_mul_tensor_api_modes_bf16(mode):
    """
    Feature: Test mul tensor api.
    Description: Test mul tensor api for Graph and PyNative modes.
    Expectation: The result match to the expect value.
    """
    context.set_context(mode=mode, device_target="Ascend")
    x = Tensor([1.0, 2.0, 3.0], mstype.bfloat16)
    y = Tensor([4.0, 5.0, 6.0], mstype.bfloat16)
    output = x.mul(y)
    expected = np.array([4., 10., 18.], np.float32)
    np.testing.assert_array_equal(output.float().asnumpy(), expected)
