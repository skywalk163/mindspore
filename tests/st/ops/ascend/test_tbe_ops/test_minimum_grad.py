# Copyright 2020-2023 Huawei Technologies Co., Ltd
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
import mindspore as ms
import mindspore.context as context
from mindspore import Tensor
from mindspore.nn import Cell
from mindspore.ops import composite as C
from mindspore.ops.operations import Minimum
from tests.st.utils import test_utils

context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")
grad = C.GradOperation(get_all=True, sens_param=True)


class MinNetMe(Cell):
    def __init__(self):
        super(MinNetMe, self).__init__()
        self.min = Minimum()

    def construct(self, inputA, inputB):
        x = self.min(inputA, inputB)
        return x


class GradWrap(Cell):
    def __init__(self, network):
        super(GradWrap, self).__init__()
        self.network = network

    def construct(self, inputA, inputB, sens):
        gout = grad(self.network)(inputA, inputB, sens)
        return gout


def gen_data(inputA_np, inputB_np, grad_=None, ms_type=ms.float32):
    inputA_me = inputA_np
    if isinstance(inputA_np, np.ndarray):
        inputA_me = Tensor(inputA_me, ms_type)

    inputB_me = inputB_np
    if isinstance(inputB_np, np.ndarray):
        inputB_me = Tensor(inputB_np, ms_type)

    if grad_ is None:
        grad_ = np.random.randn(1, 3, 2, 2).astype(np.float32)
    grad_me = Tensor(grad_, ms_type)

    net_me = GradWrap(MinNetMe())
    net_me.set_train()
    output_me = net_me(inputA_me, inputB_me, grad_me)
    if ms_type == ms.bfloat16:
        ms_inputA_grad = output_me[0].float().asnumpy()
        ms_inputB_grad = output_me[1].float().asnumpy()
    else:
        ms_inputA_grad = output_me[0].asnumpy()
        ms_inputB_grad = output_me[1].asnumpy()
    print("---me---")
    print(ms_inputA_grad)
    print(ms_inputB_grad)


@pytest.mark.level0
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_min_tensor_grad_4d():
    """
    Feature: test minimum grad on ascend
    Description: test the minimumgrad with 4D input.
    Expectation: result match to torch result.
    """
    inputA_np = np.random.randn(1, 3, 2, 2).astype(np.float32)
    inputB_np = np.random.randn(1, 3, 2, 2).astype(np.float32)
    gen_data(inputA_np, inputB_np, ms_type=ms.float32)


@pytest.mark.level0
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@test_utils.run_test_with_On
def test_min_tensor_grad_with_same_input():
    """
    Feature: test minimum grad on ascend
    Description: test the minimumgrad with same input.
    Expectation: result match to torch result.
    """
    inputA_np = np.array([2.1, 9.6, 6.7]).astype(np.float32)
    inputB_np = np.array([2.3, 9.6, 5.8]).astype(np.float32)
    grad_ = np.array([1.0, -1.0, 0]).astype(np.float32)
    gen_data(inputA_np, inputB_np, grad_, ms_type=ms.float32)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend910b_training
@pytest.mark.env_onecard
def test_min_tensor_grad_with_bfloat16():
    """
    Feature: test minimum grad on ascend
    Description: test the minimumgrad with bfloat16.
    Expectation: result match to torch result.
    """
    inputA_np = np.random.randn(3, 3).astype(np.float32)
    inputB_np = np.random.randn(3, 3).astype(np.float32)
    grad_ = np.random.randn(3, 3).astype(np.float32)
    gen_data(inputA_np, inputB_np, grad_, ms_type=ms.bfloat16)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend910b_training
@pytest.mark.env_onecard
def test_min_tensor_grad_with_bf16():
    """
    Feature: test minimumgrad on Ascend(910B)
    Description: test the minimumgrad with same input bfloat16 tensor.
    Expectation: result match to expected result.
    """
    x_np = np.array([1.7, 9.6, 5.8]).astype(np.float32)
    y_np = np.array([1.7, 9.6, 5.8]).astype(np.float32)
    grad_ = np.array([1.0, -1.0, 0]).astype(np.float32)
    net_me = GradWrap(MinNetMe())
    output = net_me(Tensor(x_np, ms.bfloat16), Tensor(y_np, ms.bfloat16), Tensor(grad_, ms.bfloat16))
    print(output[0].float().asnumpy())
    print(output[1].float().asnumpy())
    expect0 = np.array([0.5, -0.5, 0.])
    expect1 = np.array([0.5, -0.5, 0.])
    assert np.allclose(output[0].float().asnumpy(), expect0, rtol=1e-6, atol=1e-4)
    assert np.allclose(output[1].float().asnumpy(), expect1, rtol=1e-6, atol=1e-4)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend910b_training
@pytest.mark.env_onecard
def test_min_tensor_grad_with_input_nan():
    """
    Feature: test minimumgrad on Ascend(910B)
    Description: test the minimumgrad with input nan.
    Expectation: result match to expected result.
    """
    x_np = np.full((3,), np.nan).astype(np.float32)
    y_np = np.array([1.7, 9.6, 5.8]).astype(np.float32)
    grad_ = np.array([1.0, -1.0, 0]).astype(np.float32)
    net_me = GradWrap(MinNetMe())
    output = net_me(Tensor(x_np, ms.bfloat16), Tensor(y_np, ms.bfloat16), Tensor(grad_, ms.bfloat16))
    print(output[0].float().asnumpy())
    print(output[1].float().asnumpy())
    expect0 = np.array([1.0, -1.0, 0])
    expect1 = np.array([1.0, -1.0, 0])
    assert np.allclose(output[0].float().asnumpy(), expect0, rtol=1e-6, atol=1e-4)
    assert np.allclose(output[1].float().asnumpy(), expect1, rtol=1e-6, atol=1e-4)
