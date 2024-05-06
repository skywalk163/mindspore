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
""" test_dict_get """
import pytest
from mindspore import context, jit

context.set_context(mode=context.GRAPH_MODE)


@pytest.mark.level2
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_dict_get_2():
    """
    Feature: dict get.
    Description: support dict get, which the return is None.
    Expectation: No exception.
    """
    @jit
    def dict_net_2():
        x = {'aa': 1, 'bb': 2}
        res = x.get('c')
        return res

    out = dict_net_2()
    assert out is None
