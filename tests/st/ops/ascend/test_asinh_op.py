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
import pytest
import mindspore.context as context
from mindspore import Tensor

# all cases tested against dchip


def test_asinh_forward_tensor_api(nptype):
    """
    Feature: test asinh forward tensor api for given input dtype.
    Description: test inputs for given input dtype.
    Expectation: the result match with expected result.
    """
    x = Tensor(np.array([-5.0, 1.5, 3.0, 100.0]).astype(nptype))
    output = x.asinh()
    expected = np.array([-2.3124382, 1.1947632, 1.8184465, 5.298342]).astype(nptype)
    np.testing.assert_array_almost_equal(output.asnumpy(), expected)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_asinh_forward_float32_tensor_api():
    """
    Feature: test asinh forward tensor api.
    Description: test float32 inputs.
    Expectation: the result match with expected result.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")
    test_asinh_forward_tensor_api(np.float32)
    context.set_context(mode=context.PYNATIVE_MODE, device_target="Ascend")
    test_asinh_forward_tensor_api(np.float32)
