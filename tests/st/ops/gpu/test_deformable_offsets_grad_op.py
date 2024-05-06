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
import mindspore.nn as nn
from mindspore import Tensor
from mindspore.ops.operations import _grad_ops as G


class NetDeformableOffsetsGrad(nn.Cell):
    def __init__(self, data_format):
        super(NetDeformableOffsetsGrad, self).__init__()
        strides = (1, 1, 1, 1)
        pads = (0, 0, 0, 0)
        ksize = (3, 3)
        self.grad_op = G.DeformableOffsetsGrad(strides, pads, ksize, data_format=data_format)

    def construct(self, grad, input_x, offsets):
        return self.grad_op(grad, input_x, offsets)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [np.float16, np.float32])
def test_deformable_offsets_grad_nchw(dtype):
    """
    Feature: DeformableOffsetsGrad gpu kernel
    Description: test the rightness of DeformableOffsetsGrad gpu kernel
    Expectation: the output is same as expected result
    """
    context.set_context(mode=context.GRAPH_MODE, device_target='GPU')
    net = NetDeformableOffsetsGrad(data_format="NCHW")
    dout = Tensor(np.ones([1, 2, 3, 3]).astype(dtype))
    x = Tensor(np.ones([1, 2, 4, 4]).astype(dtype))
    offsets = Tensor(np.ones([1, 27, 1, 1]).astype(dtype) * 0.1)
    output = net(dout, x, offsets)

    expect_grad_x = np.array([[[0.081, 0.09, 0.09, 0.009],
                               [0.09, 0.1, 0.1, 0.01],
                               [0.09, 0.1, 0.1, 0.01],
                               [0.009, 0.01, 0.01, 0.001]],
                              [[0.081, 0.09, 0.09, 0.009],
                               [0.09, 0.1, 0.1, 0.01],
                               [0.09, 0.1, 0.1, 0.01],
                               [0.009, 0.01, 0.01, 0.001]]]
                             ).astype(dtype)
    expect_grad_offset = np.array([0] * 18 + [2.0] * 9).astype(dtype).reshape([1, 27, 1, 1])
    rtol = 1e-5
    if dtype == np.float16:
        rtol = 1e-3
    assert np.allclose(output[0].asnumpy(), expect_grad_x, rtol)
    assert np.allclose(output[1].asnumpy(), expect_grad_offset, rtol)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('dtype', [np.float16, np.float32])
def test_deformable_offsets_grad_nhwc(dtype):
    """
    Feature: DeformableOffsetsGrad gpu kernel
    Description: test the rightness of DeformableOffsetsGrad gpu kernel
    Expectation: the output is same as expected result
    """
    context.set_context(mode=context.GRAPH_MODE, device_target='GPU')
    net = NetDeformableOffsetsGrad(data_format="NHWC")
    dout = Tensor(np.ones([1, 3, 3, 2]).astype(dtype))
    x = Tensor(np.ones([1, 4, 4, 2]).astype(dtype))
    offsets = Tensor(np.ones([1, 1, 1, 27]).astype(dtype) * 0.1)
    output = net(dout, x, offsets)
    expect_grad_x = np.array([[[0.081, 0.081],
                               [0.09, 0.09],
                               [0.09, 0.09],
                               [0.009, 0.009]],
                              [[0.09, 0.09],
                               [0.1, 0.1],
                               [0.1, 0.1],
                               [0.01, 0.01]],
                              [[0.09, 0.09],
                               [0.1, 0.1],
                               [0.1, 0.1],
                               [0.01, 0.01]],
                              [[0.009, 0.009],
                               [0.01, 0.01],
                               [0.01, 0.01],
                               [0.001, 0.001]]
                              ]
                             ).astype(dtype)
    expect_grad_offset = np.array([0] * 18 + [2.0] * 9).astype(dtype).reshape([1, 1, 1, 27])
    rtol = 1e-5
    if dtype == np.float16:
        rtol = 1e-3
    assert np.allclose(output[0].asnumpy(), expect_grad_x, rtol)
    assert np.allclose(output[1].asnumpy(), expect_grad_offset, rtol)
