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
from mindspore import Tensor, mutable
from mindspore import context, jit

context.set_context(mode=context.GRAPH_MODE)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_dict_fromkeys_with_variable():
    """
    Feature: dict fromkeys.
    Description: support dict fromkeys with variable key inpyt.
    Expectation: No exception.
    """
    @jit
    def foo(x):
        t = {}
        m = ("1", "2", "3")
        new_dict = t.fromkeys(m, x)
        return new_dict

    ret = foo(Tensor([1]))
    assert ret == {"1": Tensor([1]), "2": Tensor([1]), "3": Tensor([1])}


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_dict_fromkeys_with_variable_2():
    """
    Feature: dict fromkeys.
    Description: support dict fromkeys with variable key inpyt.
    Expectation: No exception.
    """
    @jit
    def foo(x):
        t = {}
        m = ("1", "2", "3")
        new_dict = t.fromkeys(m, x)
        return new_dict

    ret = foo(mutable(2))
    assert ret == {"1": 2, "2": 2, "3": 2}
