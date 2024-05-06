# Copyright 2024 Huawei Technologies Co., Ltd
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

import pytest
import numpy as np

import mindspore as ms
import mindspore.nn as nn
from mindspore import Tensor
from mindspore import context
from mindspore.common.api import _cell_graph_executor
from mindspore.ops import operations as P
from mindspore.ops import composite as C
from mindspore.parallel.shard import Layout
from mindspore.ops.operations._inner_ops import SiLU
from tests.ut.python.ops.test_math_ops import VirtualLoss

activation_ops_map = {
    "gelu": P.GeLU(),
    "silu": SiLU(),
    "relu": P.ReLU(),
    "sigmoid": P.Sigmoid(),
    "softmax": P.Softmax(),
}

def setup_function():
    context.set_auto_parallel_context(dataset_strategy="full_batch")


grad_all = C.GradOperation(get_all=True)


class NetWithLoss(nn.Cell):
    def __init__(self, network):
        super(NetWithLoss, self).__init__()
        self.loss = VirtualLoss()
        self.network = network

    def construct(self, y):
        predict = self.network(y)
        return self.loss(predict)


class GradWrap(nn.Cell):
    def __init__(self, network):
        super(GradWrap, self).__init__()
        self.network = network

    def construct(self, y):
        return grad_all(self.network)(y)


def compile_net(net, input_x):
    net.set_auto_parallel()
    net.set_train()
    phase, _ = _cell_graph_executor.compile(net, input_x)
    return phase


class Net(nn.Cell):
    def __init__(self, in_layout, out_layout=None, ops_name=None):
        super().__init__()
        self.activation_ops = activation_ops_map[ops_name]
        self.activation_ops.shard(in_strategy=in_layout, out_strategy=out_layout)

    def construct(self, y):
        out = self.activation_ops(y)
        return out


x = Tensor(np.ones([1024, 1024]), dtype=ms.float32)


@pytest.mark.parametrize('ops_name', ["gelu", "silu", "relu", "sigmoid"])
def test_layout_extend_base(ops_name):
    """
    Feature: test layout extend
    Description: dev_num is 4.
    Expectation: compile success
    """
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel", device_num=4, global_rank=0)
    layout = Layout((2, 2), ("dp", "mp"))
    layout1 = (layout("dp", "mp"),)
    net = Net(layout1, ops_name=ops_name)
    compile_net(net, x)


@pytest.mark.parametrize('ops_name', ["gelu", "silu", "relu", "sigmoid"])
def test_layout_extend_batch_multi_shard(ops_name):
    """
    Feature: test layout extend
    Description: dev_num is 8, batch dim multi shard.
    Expectation: compile success
    """
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel", device_num=8, global_rank=0)
    layout = Layout((2, 2, 2), ("dp", "sp", "mp"))
    layout1 = (layout(("dp", "mp"), "sp"),)
    net = Net(layout1, ops_name=ops_name)
    compile_net(net, x)


@pytest.mark.parametrize('ops_name', ["gelu", "silu", "relu", "sigmoid"])
def test_layout_extend_reduce_axis_multi_shard(ops_name):
    """
    Feature: test layout extend
    Description: dev_num is 8, reduce dim multi shard.
    Expectation: compile success
    """
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel", device_num=8, global_rank=0)
    layout = Layout((2, 2, 2), ("dp", "sp", "mp"))
    layout1 = (layout("dp", ("mp", "sp")),)
    net = Net(layout1, ops_name=ops_name)
    compile_net(net, x)
