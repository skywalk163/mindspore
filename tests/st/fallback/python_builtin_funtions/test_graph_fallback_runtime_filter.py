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
import operator
import pytest
import numpy as np
from mindspore import Tensor, jit, context

context.set_context(mode=context.GRAPH_MODE)


def filter_fn(x):
    return x > 2


@pytest.mark.skip(reason="No support yet.")
@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_fallback_filter_asnumpy():
    """
    Feature: JIT Fallback
    Description: Test filter in fallback runtime
    Expectation: No exception.
    """
    @jit
    def foo():
        x = Tensor(np.array([1, 2, 3, 4])).asnumpy()
        ret1 = filter(lambda x: x > 2, x)
        ret2 = filter(filter_fn, x)
        return tuple(ret1), tuple(ret2)

    out = foo()
    assert operator.eq(out[0], (3, 4)) and operator.eq(out[1], (3, 4))
