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
""" test graph mode sequence getitem """

import pytest
import numpy as np
from mindspore import context, jit, mutable
from mindspore.common.tensor import Tensor

context.set_context(mode=context.GRAPH_MODE)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_tuple_getitem_with_constant_bool_index():
    """
    Feature: sequence getitem with bool index
    Description: sequence getitem
    Expectation: No exception
    """
    @jit
    def foo():
        m = (1, 2, 3, 4)
        return m[True], m[False]

    ret1, ret2 = foo()
    assert ret1 == 2
    assert ret2 == 1


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_tuple_getitem_with_constant_bool_index_2():
    """
    Feature: sequence getitem with bool index
    Description: sequence getitem
    Expectation: No exception
    """
    @jit
    def foo(x):
        m = (x, x+1, x+2, x+3)
        return m[True], m[False]

    ret1, ret2 = foo(Tensor([1, 2, 3, 4]))
    assert np.all(ret1.asnumpy() == np.array([2, 3, 4, 5]))
    assert np.all(ret2.asnumpy() == np.array([1, 2, 3, 4]))


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_tuple_getitem_with_variable_bool_index():
    """
    Feature: sequence getitem with bool index
    Description: sequence getitem
    Expectation: No exception
    """
    @jit
    def foo(x):
        if x > 0:
            index = True
        else:
            index = False
        m = (x, x+1, x+2, x+3)
        return m[index], m[not index]

    ret1, ret2 = foo(Tensor([1]))
    assert np.all(ret1.asnumpy() == np.array([2]))
    assert np.all(ret2.asnumpy() == np.array([1]))


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_tuple_getitem_with_variable_bool_index_2():
    """
    Feature: sequence getitem with bool index
    Description: sequence getitem
    Expectation: No exception
    """
    @jit
    def foo(x, a):
        m = (x, x+1, x+2, x+3)
        return m[a == 1], m[a == 2]

    ret1, ret2 = foo(Tensor([1]), mutable(1))
    assert np.all(ret1.asnumpy() == np.array([2]))
    assert np.all(ret2.asnumpy() == np.array([1]))


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_tuple_getitem_with_variable_bool_index_3():
    """
    Feature: sequence getitem with bool index
    Description: sequence getitem
    Expectation: No exception
    """
    @jit
    def foo(a):
        m = (1, 2, 3, 4)
        return m[a == 1], m[a == 2]

    ret1, ret2 = foo(mutable(1))
    assert ret1 == 2
    assert ret2 == 1


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_list_getitem_with_constant_bool_index():
    """
    Feature: sequence getitem with bool index
    Description: sequence getitem
    Expectation: No exception
    """
    @jit
    def foo():
        m = [1, 2, 3, 4]
        return m[True], m[False]

    ret1, ret2 = foo()
    assert ret1 == 2
    assert ret2 == 1


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_list_getitem_with_constant_bool_index_2():
    """
    Feature: sequence getitem with bool index
    Description: sequence getitem
    Expectation: No exception
    """
    @jit
    def foo(x):
        m = [x, x+1, x+2, x+3]
        return m[True], m[False]

    ret1, ret2 = foo(Tensor([1, 2, 3, 4]))
    assert np.all(ret1.asnumpy() == np.array([2, 3, 4, 5]))
    assert np.all(ret2.asnumpy() == np.array([1, 2, 3, 4]))


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_list_getitem_with_variable_bool_index():
    """
    Feature: sequence getitem with bool index
    Description: sequence getitem
    Expectation: No exception
    """
    @jit
    def foo(x):
        if x > 0:
            index = True
        else:
            index = False
        m = [x, x+1, x+2, x+3]
        return m[index], m[not index]

    ret1, ret2 = foo(Tensor([1]))
    assert np.all(ret1.asnumpy() == np.array([2]))
    assert np.all(ret2.asnumpy() == np.array([1]))


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_list_getitem_with_variable_bool_index_2():
    """
    Feature: sequence getitem with bool index
    Description: sequence getitem
    Expectation: No exception
    """
    @jit
    def foo(x, a):
        m = [x, x+1, x+2, x+3]
        return m[a == 1], m[a == 2]

    ret1, ret2 = foo(Tensor([1]), mutable(1))
    assert np.all(ret1.asnumpy() == np.array([2]))
    assert np.all(ret2.asnumpy() == np.array([1]))
