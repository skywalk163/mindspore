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
""" test graph fallback control flow."""
import numpy as np
from mindspore import Tensor, jit, context
from tests.st.fallback.cases_register import case_register

context.set_context(mode=context.GRAPH_MODE)


@case_register.level0
@case_register.target_gpu
@case_register.target_ascend
def test_while_after_while_tensor():
    """
    Feature: JIT Fallback
    Description: Test fallback with control flow.
    Expectation: No exception.
    """
    @jit
    def control_flow_while_after_while():
        x = Tensor([1])
        y = Tensor([2])
        while x < y + Tensor([5]):
            y = y * x - Tensor([3])
        z = y + x
        while y > x and x < z:
            y -= x
            z = z + y
        return z
    res = control_flow_while_after_while()
    assert res == -3


@case_register.level1
@case_register.target_gpu
@case_register.target_ascend
def test_while_after_while_tensor_2():
    """
    Feature: JIT Fallback
    Description: Test fallback with control flow.
    Expectation: No exception.
    """
    @jit
    def control_flow_while_after_while():
        x = Tensor([1])
        y = Tensor([2])
        while x > y:
            y = y * x
        while x < y:
            y -= x
        z = Tensor(7) - y
        return z
    res = control_flow_while_after_while()
    assert res == 6


@case_register.level1
@case_register.target_gpu
@case_register.target_ascend
def test_while_after_while_numpy_2():
    """
    Feature: JIT Fallback
    Description: Test fallback with control flow.
    Expectation: No exception.
    """
    @jit
    def control_flow_while_after_while():
        x = np.array([3, 2])
        y = [1, 2, 3, 4]
        while len(y) <= 5:
            y.append(x[1])
        while Tensor(sum(y)) < Tensor([15]):
            y.append(x[0])
        return Tensor(x), Tensor(y)
    res_x, res_y = control_flow_while_after_while()
    assert (res_x.asnumpy() == [3, 2]).all()
    assert (res_y.asnumpy() == [1, 2, 3, 4, 2, 2, 3]).all()


@case_register.level1
@case_register.target_gpu
@case_register.target_ascend
def test_while_after_while_numpy():
    """
    Feature: JIT Fallback
    Description: Test fallback with control flow.
    Expectation: No exception.
    """
    @jit
    def control_flow_while_after_while():
        x = [1, 2, 3, 4]
        y = Tensor([8])
        while Tensor([sum(x)]) > y:
            y = Tensor([18])
        while y >= 0:
            y -= Tensor([x[0]])
        return Tensor(x), y
    res_x, res_y = control_flow_while_after_while()
    assert (res_x.asnumpy() == [1, 2, 3, 4]).all()
    assert res_y == -1
