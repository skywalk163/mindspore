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

import numpy as np

import mindspore.context as context
import mindspore as ms
from mindspore import Tensor
from mindspore.nn.wrap.cell_wrapper import _BroadCastCell
from mindspore.common.api import _cell_graph_executor


def setup_function():
    context.set_auto_parallel_context(dataset_strategy="full_batch")


def test_param_broadcast_cell():
    """
    Feature: param broadcast cell
    Description: parallel mode is semi_auto_parallel, but the _BroadCastCell skip the auto parallel compile
    Expectation: compile success
    """
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel", device_num=8, global_rank=0)
    broadcast_params = []
    broadcast_params.append(Tensor(np.ones([8, 32]), dtype=ms.float32))
    net = _BroadCastCell(broadcast_params)
    _cell_graph_executor.compile(net)
    assert "skip_auto_parallel_compile" in net.get_flags()
    context.reset_auto_parallel_context()
