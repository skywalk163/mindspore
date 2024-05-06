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

import numpy as np
import pytest

import mindspore as ms
import mindspore.ops.operations as P
from mindspore import context, Tensor
from mindspore.nn import Cell
from mindspore import dtype as mstype


class MaxPoolWithArgmaxV2Net(Cell):
    def __init__(self, kernel_size, strides, pads, dilation, ceil_mode, argmax_type=mstype.int64):
        super(MaxPoolWithArgmaxV2Net, self).__init__()
        self.maxpool_with_argmax_v2 = P.MaxPoolWithArgmaxV2(kernel_size, strides, pads, dilation, ceil_mode,
                                                            argmax_type)

    def construct(self, input_data):
        output, argmax = self.maxpool_with_argmax_v2(input_data)
        return output, argmax


class DynamicShapeMaxPoolWithArgmaxV2Net(Cell):
    def __init__(self, net, axis=0):
        super(DynamicShapeMaxPoolWithArgmaxV2Net, self).__init__()
        self.net = net
        self.unique = P.Unique()
        self.gather = P.Gather()
        self.axis = axis

    def construct(self, x, indices):
        unique_indices, _ = self.unique(indices)
        x = self.gather(x, unique_indices, self.axis)
        return self.net(x)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_maxpool_with_argmax_v2_float16(mode):
    """
    Feature: Test MaxPoolWithArgmaxV2.
    Description: Test MaxPoolWithArgmaxV2 with float16 inputs.
    Expectation: success.
    """
    ms.set_context(mode=mode)
    attributes = {'kernel_size': (3, 2), 'strides': (2, 1), 'pads': 0, 'dilation': 1,
                  'ceil_mode': False, 'argmax_type': mstype.int64}
    x = Tensor(np.arange(20 * 16 * 50 * 32).reshape((20, 16, 50, 32)), mstype.float16)
    net = MaxPoolWithArgmaxV2Net(**attributes)
    output, argmax = net(x)
    assert output.shape == (20, 16, 24, 31)
    assert argmax.shape == (20, 16, 24, 31)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_dynamic_maxpool_with_argmax_v2(mode):
    """
    Feature: Test MaxPoolWithArgmaxV2.
    Description: Test MaxPoolWithArgmaxV2 following Unique and gather ops.
    Expectation: success.
    """
    ms.set_context(mode=mode)
    attributes = {'kernel_size': (3, 2), 'strides': (2, 1), 'pads': 0, 'dilation': 1,
                  'ceil_mode': False, 'argmax_type': mstype.int64}
    x = Tensor(np.arange(20 * 16 * 50 * 32).reshape((20, 16, 50, 32)), mstype.float16)
    indices = Tensor(np.array([0, 1, 2, 0]).astype(np.int32))
    net = MaxPoolWithArgmaxV2Net(**attributes)
    dy_net = DynamicShapeMaxPoolWithArgmaxV2Net(net)
    output, argmax = dy_net(x, indices)
    assert output.shape == (3, 16, 24, 31)
    assert argmax.shape == (3, 16, 24, 31)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_maxpool_with_argmax_v2_dynamic_shape(mode):
    """
    Feature: Test MaxPoolWithArgmaxV2.
    Description: Test MaxPoolWithArgmaxV2 with dynamic shape.
    Expectation: success.
    """
    ms.set_context(mode=mode)
    attributes = {'kernel_size': (3, 2), 'strides': (2, 1), 'pads': 0, 'dilation': 1,
                  'ceil_mode': False, 'argmax_type': mstype.int64}
    x = Tensor(np.arange(20 * 16 * 50 * 32).reshape((20, 16, 50, 32)), mstype.float16)
    x_dyn = Tensor(shape=[None for _ in x.shape], dtype=mstype.float16)
    net = MaxPoolWithArgmaxV2Net(**attributes)
    net.set_inputs(x_dyn)
    output, argmax = net(x)
    assert output.shape == (20, 16, 24, 31)
    assert argmax.shape == (20, 16, 24, 31)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_maxpool_with_argmax_v2_ceil_mode_true(mode):
    """
    Feature: Test MaxPoolWithArgmaxV2.
    Description: Test MaxPoolWithArgmaxV2 with `ceil_mode` is True.
    Expectation: success.
    """
    ms.set_context(mode=mode)
    attributes = {'kernel_size': (3, 2), 'strides': (2, 1), 'pads': 0, 'dilation': 1,
                  'ceil_mode': True, 'argmax_type': mstype.int64}
    x = Tensor(np.arange(20 * 16 * 50 * 32).reshape((20, 16, 50, 32)), mstype.float16)
    net = MaxPoolWithArgmaxV2Net(**attributes)
    output, argmax = net(x)
    assert output.shape == (20, 16, 25, 31)
    assert argmax.shape == (20, 16, 25, 31)
