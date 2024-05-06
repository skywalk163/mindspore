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
from mindspore.ops import functional as F


def adaptive_max_pool1d_forward_functional(nptype):
    input_x = Tensor(np.ones((1, 3, 6)).astype(nptype))
    output = F.adaptive_max_pool1d(input_x, output_size=2)
    expected = np.ones((1, 3, 2)).astype(nptype)
    np.testing.assert_array_almost_equal(output.asnumpy(), expected)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_adaptive_max_pool1d_forward_float32_functional():
    """
    Feature: test adaptive_max_pool1d forward.
    Description: test float32 inputs.
    Expectation: the result match with expected result.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="CPU")
    adaptive_max_pool1d_forward_functional(np.float32)
    context.set_context(mode=context.PYNATIVE_MODE, device_target="CPU")
    adaptive_max_pool1d_forward_functional(np.float32)


if __name__ == '__main__':
    test_adaptive_max_pool1d_forward_float32_functional()
