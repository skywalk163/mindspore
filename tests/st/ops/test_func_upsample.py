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
import mindspore.nn as nn
from mindspore import Tensor
from mindspore import ops


class Net(nn.Cell):
    def __init__(self, mode):
        super(Net, self).__init__()
        self.mode = mode

    def construct(self, x, size=None, scale_factor=None, align_corners=None, recompute_scale_factor=None):
        return ops.upsample(x, size, scale_factor, self.mode, align_corners, recompute_scale_factor)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_upsample_area_3d(mode):
    """
    Feature: upsample
    Description: Verify the result of upsample area mode
                1. 3D size
                2. 3D scale_factor
    Expectation: success
    """
    ms.set_context(mode=mode)
    net = Net("area")

    # 1. 3D(1, 3, 4)
    input_3d = np.array([[[0.476130, 0.196372, 0.320748, 0.574267],
                          [0.558406, 0.186530, 0.144793, 0.017598],
                          [0.043675, 0.360826, 0.367078, 0.607198]]], dtype=np.float32)
    except_3d_1 = np.array([[[0.336251, 0.447508],
                             [0.372468, 0.081196],
                             [0.202250, 0.487138]]], dtype=np.float32)
    size = 2
    output_3d_1 = net(Tensor(input_3d), size=size)
    assert np.allclose(output_3d_1.asnumpy(), except_3d_1, atol=1e-3, rtol=1e-3)

    # 2. 3D(1, 3, 4) scale_factor=0.3
    except_3d_2 = np.array([[[0.391879],
                             [0.226832],
                             [0.344694]]], dtype=np.float32)
    scale_factor = 0.3
    output_3d_2 = net(Tensor(input_3d), scale_factor=scale_factor)
    assert np.allclose(output_3d_2.asnumpy(), except_3d_2, atol=1e-3, rtol=1e-3)


@pytest.mark.level2
@pytest.mark.platform_x86_cpu
@pytest.mark.platform_arm_cpu
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_upsample_bilinear(mode):
    """
    Feature: upsample
    Description: Verify the result of upsample bilinear mode
                1. 4D size
                2. 4D size align_corners=True
                3. 4D scale_factor recompute_scale_factor=True
    Expectation: success
    """
    ms.set_context(mode=mode)
    net = Net("bilinear")
    input_4d = np.array([[[[0.003088, 0.313131, 0.481231, 0.326219, 0.190293],
                           [0.711616, 0.58399, 0.718121, 0.258823, 0.121847],
                           [0.781316, 0.591508, 0.858185, 0.091935, 0.444639]],
                          [[0.389884, 0.894497, 0.471427, 0.188708, 0.557449],
                           [0.998047, 0.380719, 0.570574, 0.722258, 0.997173],
                           [0.195751, 0.050744, 0.002008, 0.482685, 0.708559]]]],
                        dtype=np.float32)
    size = (5, 3)

    # 1. 4D size=(5, 3)
    except_4d_1 = np.array([[[[0.106436, 0.481231, 0.235602],
                              [0.331491, 0.575987, 0.208363],
                              [0.669074, 0.718121, 0.167506],
                              [0.698458, 0.802159, 0.263245],
                              [0.718047, 0.858185, 0.327071]],
                             [[0.558088, 0.471427, 0.434535],
                              [0.651761, 0.511086, 0.622935],
                              [0.792271, 0.570574, 0.905535],
                              [0.405358, 0.229434, 0.742174],
                              [0.147415, 0.002008, 0.633268]]]], dtype=np.float32)
    output_4d_1 = net(Tensor(input_4d), size=size)
    assert np.allclose(output_4d_1.asnumpy(), except_4d_1, atol=1e-5, rtol=1e-5)

    # 2. 4D size=(5, 3), align_corners=True
    except_4d_2 = np.array([[[[0.003088, 0.481231, 0.190293],
                              [0.357352, 0.599676, 0.15607],
                              [0.711616, 0.718121, 0.121847],
                              [0.746466, 0.788153, 0.283243],
                              [0.781316, 0.858185, 0.444639]],

                             [[0.389884, 0.471427, 0.557449],
                              [0.693965, 0.521001, 0.777311],
                              [0.998047, 0.570574, 0.997173],
                              [0.596899, 0.286291, 0.852866],
                              [0.195751, 0.002008, 0.708559]]]], dtype=np.float32)
    output_4d_2 = net(Tensor(input_4d), size=size, align_corners=True)
    assert np.allclose(output_4d_2.asnumpy(), except_4d_2, atol=1e-5, rtol=1e-5)

    # 3. scale_factor=(0.5, 1.5), recompute_scale_factor=True
    scale_factor = (0.5, 1.5)
    except_4d_3 = np.array([[[[0.711616, 0.638687, 0.622313, 0.718121, 0.390051, 0.200119,
                               0.121847]],

                             [[0.998047, 0.645288, 0.434963, 0.570574, 0.67892, 0.840079,
                               0.997173]]]], dtype=np.float32)
    output_4d_3 = net(Tensor(input_4d), scale_factor=scale_factor, recompute_scale_factor=True)
    assert np.allclose(output_4d_3.asnumpy(), except_4d_3, atol=1e-5, rtol=1e-5)
