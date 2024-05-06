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
import pytest

import mindspore as ms
import mindspore.context as context

context.set_context(mode=context.GRAPH_MODE, device_target="CPU")


class ArrayLen1(ms.nn.Cell):
    def construct(self, a):
        return len(a)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
def test_arraylen_2d():
    """
    Feature: test_arraylen_2d op in cpu.
    Description: test the ops.
    Expectation: expect correct len result.
    """
    net = ArrayLen1()
    input_dyn = ms.Tensor(shape=[None, None], dtype=ms.int32)
    net.set_inputs(input_dyn)
    input1 = ms.Tensor([[1, 2], [1, 3]], dtype=ms.int32)
    output = net(input1)
    assert output == 2
