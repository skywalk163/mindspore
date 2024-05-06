# Copyright 2022-2023 Huawei Technologies Co., Ltd
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
from mindspore import Tensor, ops
import mindspore.context as context
from mindspore.common import dtype as mstype


def test_equal_tensor_api():
    """
    Feature: test equal tensor API.
    Description: testcase for equal tensor API.
    Expectation: the result match with expected result.
    """
    x = Tensor(np.array([1, 2, 3]), mstype.int32)
    y = Tensor(np.array([1, 2, 4]), mstype.int32)
    output = x.equal(y)
    expected = np.array([True, True, False])
    np.testing.assert_array_equal(output.asnumpy(), expected)


def test_f_equal_api_bfloat16():
    """
    Feature: test equal functional API.
    Description: testcase for equal functional API.
    Expectation: the result match with expected result.
    """
    x = Tensor(np.array([1.0, 2.5, 3.8]), mstype.bfloat16)
    y = Tensor(np.array([1.0, 2.5, 4.0]), mstype.bfloat16)
    output = ops.equal(x, y)
    expected = np.array([True, True, False])
    np.testing.assert_array_equal(output.asnumpy(), expected)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_equal_tensor_modes():
    """
    Feature: test equal tensor API in PyNative and Graph modes.
    Description: test case for equal tensor API.
    Expectation: the result match with expected result.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")
    test_equal_tensor_api()
    context.set_context(mode=context.PYNATIVE_MODE, device_target="Ascend")
    test_equal_tensor_api()


@pytest.mark.level1
@pytest.mark.platform_arm_ascend910b_training
@pytest.mark.env_onecard
def test_f_equal_api_modes():
    """
    Feature: test equal functional API in PyNative and Graph modes.
    Description: test case for equal functional API.
    Expectation: the result match with expected result.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")
    test_f_equal_api_bfloat16()
    context.set_context(mode=context.PYNATIVE_MODE, device_target="Ascend")
    test_f_equal_api_bfloat16()
