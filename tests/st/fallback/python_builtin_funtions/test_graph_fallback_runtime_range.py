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

import pytest
from mindspore import Tensor, jit, context

context.set_context(mode=context.GRAPH_MODE)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_fallback_runtime_range():
    """
    Feature: JIT Fallback
    Description: Test range() in fallback runtime
    Expectation: No exception
    """
    @jit
    def foo(x):
        y = range(x.asnumpy())
        out = 0
        for index in y:
            out += index
        return out

    x = Tensor(4)
    out = foo(x)
    assert out == 6


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_fallback_runtime_range_mutli():
    """
    Feature: JIT Fallback
    Description: Test range() in fallback runtime
    Expectation: No exception
    """
    @jit
    def foo(x):
        y = range(2, x.asnumpy())
        out1 = 0
        for index in y:
            out1 += index
        z = range(0, x.asnumpy(), 2)
        out2 = 0
        for index in z:
            out2 += index
        return out1, out2

    x = Tensor(5)
    out = foo(x)
    assert out[0] == 9, out[1] == 6
