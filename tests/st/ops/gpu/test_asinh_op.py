# Copyright 2020-2022 Huawei Technologies Co., Ltd
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
from mindspore.ops import operations as P
context.set_context(mode=context.PYNATIVE_MODE, device_target="GPU")
np.random.seed(1)

@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_asinh_fp32():
    """
    Feature: asinh kernel
    Description: test asinh float32
    Expectation: just test
    """
    x_np = np.random.rand(4, 2).astype(np.float32) * 10
    output_ms = P.Asinh()(Tensor(x_np))
    output_np = np.arcsinh(x_np)
    assert np.allclose(output_ms.asnumpy(), output_np, 1e-4, 1e-4)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_asinh_fp16():
    """
    Feature: asinh kernel
    Description: test asinh float16
    Expectation: just test
    """
    x_np = np.random.rand(4, 2).astype(np.float16) * 10
    output_ms = P.Asinh()(Tensor(x_np))
    output_np = np.arcsinh(x_np.astype(np.float32)).astype(np.float16)
    assert np.allclose(output_ms.asnumpy(), output_np, 1e-3, 1e-3)


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
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_asinh_forward_float32_tensor_api():
    """
    Feature: test asinh forward tensor api.
    Description: test float32 inputs.
    Expectation: the result match with expected result.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")
    test_asinh_forward_tensor_api(np.float32)
    context.set_context(mode=context.PYNATIVE_MODE, device_target="GPU")
    test_asinh_forward_tensor_api(np.float32)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_asinh_fp64():
    """
    Feature: asinh kernel
    Description: test asinh float64
    Expectation: just test
    """
    x_np = np.random.rand(4, 2).astype(np.float64) * 10
    output_ms = P.Asinh()(Tensor(x_np))
    output_np = np.arcsinh(x_np)
    assert np.allclose(output_ms.asnumpy(), output_np, 1e-5, 1e-5)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_asinh_complex64():
    """
    Feature: asinh kernel
    Description: test asinh complex64
    Expectation: just test
    """
    x_np = np.random.rand(4, 2).astype(np.complex64) * 10
    output_ms = P.Asinh()(Tensor(x_np))
    output_np = np.arcsinh(x_np)
    assert np.allclose(output_ms.asnumpy(), output_np, 1e-3, 1e-3)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_asinh_complex128():
    """
    Feature: asinh kernel
    Description: test asinh complex128
    Expectation: just test
    """
    x_np = np.random.rand(4, 2).astype(np.complex128) * 10
    output_ms = P.Asinh()(Tensor(x_np))
    output_np = np.arcsinh(x_np)
    assert np.allclose(output_ms.asnumpy(), output_np, 1e-6, 1e-6)
