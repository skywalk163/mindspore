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

from mindspore import ops, nn, context, Tensor
from mindspore.ops.operations import _inner_ops as inner
from mindspore.ops import functional as F


class GradNet(nn.Cell):
    """Get gradient net by forward network."""

    def __init__(self, network):
        super(GradNet, self).__init__()
        self.grad = ops.GradOperation(get_all=True)
        self.net = network

    def construct(self, *args):
        return self.grad(self.net)(*args)


class NetConvertForward(nn.Cell):
    """Convert net output to dynamic."""
    def __init__(self, network, is_dynamic_rank=False, skip_convert_out_ids=None):
        super(NetConvertForward, self).__init__()
        self.net = network
        self.skip_convert_out_ids = skip_convert_out_ids if skip_convert_out_ids else []
        # ConvertToDynamic only support CPU, so run it with heterogeneous way in gpu/ascend.
        self.convert_to_dynamic = inner.ConvertToDynamic(
            is_dynamic_rank=is_dynamic_rank).add_prim_attr("primitive_target", "CPU")

    def construct(self, *args):
        outs = self.net(*args)
        if isinstance(outs, tuple):
            converted_outs = []
            for i, out in enumerate(outs):
                if i not in self.skip_convert_out_ids and (F.is_sequence_value_unknown(out.shape) or out.shape):
                    converted_outs.append(self.convert_to_dynamic(out))
                else:
                    converted_outs.append(out)
            return tuple(converted_outs)
        if 0 not in self.skip_convert_out_ids and (F.is_sequence_value_unknown(outs.shape) or outs.shape):
            return self.convert_to_dynamic(outs)
        return  outs


class DynamicGradNet(nn.Cell):
    """Get gradient net for dynamic-shape or dynamic-rank case by forward network."""

    def __init__(self, network, is_dynamic_rank=False, skip_convert_in_ids=None, skip_convet_out_ids=None):
        super(DynamicGradNet, self).__init__()
        self.grad = ops.GradOperation(get_all=True)
        self.skip_convert_in_ids = skip_convert_in_ids if skip_convert_in_ids else []
        self.skip_convet_out_ids = skip_convet_out_ids if skip_convet_out_ids else []
        self.net = NetConvertForward(network, is_dynamic_rank, skip_convet_out_ids)
        # ConvertToDynamic only support CPU, so run it with heterogeneous way in gpu/ascend.
        self.convert_to_dynamic = inner.ConvertToDynamic(
            is_dynamic_rank=is_dynamic_rank).add_prim_attr("primitive_target", "CPU")

        if context.get_context("mode") != context.PYNATIVE_MODE:
            print("[WARNING] Run as graph mode, it maybe not right! Please to run as pynative.")

    def construct(self, *args):
        new_args = []
        for i, arg in enumerate(args):
            if i not in self.skip_convert_in_ids and (F.is_sequence_value_unknown(arg.shape) or arg.shape):
                new_args.append(self.convert_to_dynamic(arg))
            else:
                new_args.append(arg)

        args_tuple = tuple(new_args)
        return self.grad(self.net)(*args_tuple)


def output_compare(outputs, expects):
    """Compare the outputs to expects, assert when not equal."""
    if not isinstance(outputs, (list, tuple)):
        outputs = [outputs]
    if not isinstance(expects, (list, tuple)):
        expects = [expects]

    def _make_sense(element):
        if not isinstance(element, Tensor):
            return Tensor(element)
        return element

    new_outputs = [_make_sense(e) for e in outputs]
    new_expects = [_make_sense(e) for e in expects]
    assert all(list(map(lambda x, y: np.allclose(x.asnumpy(), y.asnumpy()), new_outputs, new_expects)))


class TestDynamicGrad:
    """Test dynamic grad with dynamic-shape or dynamic-rank case."""

    def __init__(self, network, skip_convert_out_ids=None):
        self.net = network
        self.grad_net = GradNet(network)
        self.skip_convert_in_ids = []
        self.skip_convert_out_ids = skip_convert_out_ids if skip_convert_out_ids else []

    def test_dynamic_grad_net(self, inputs, is_dynamic_rank=False, is_save_graphs=False):
        r"""Verify the correctness for gradient net in dynamic cases.

        Args:
            inputs(Union[tuple, list]): all inputs for forward net.
            is_dynamic_rank(bool): test in dynamic shape case or dynamic rank one.
            is_save_graphs(bool): whether save graphs or not.

        Raises:
            RuntimeError when check fail; Assert error when compare fail.
        """
        for i, inp in enumerate(inputs):
            if not isinstance(inp, Tensor):
                self.skip_convert_in_ids.append(i)

        if not isinstance(inputs, (list, tuple)):
            inputs = [inputs]
        static_outs = self.grad_net(*inputs)

        print("Static gradient case done.")

        if is_save_graphs:
            graphs_dir = "dyn_shape" if not is_dynamic_rank else "dyn_rank"
            context.set_context(save_graphs=True)
            context.set_context(save_graphs_path="./{}_graphs".format(graphs_dir))

        dyn_grad_net = DynamicGradNet(self.net, is_dynamic_rank, self.skip_convert_in_ids, self.skip_convert_out_ids)
        dyn_outs = dyn_grad_net(*inputs)
        print("Dynamic gradient case done.")

        output_compare(dyn_outs, static_outs)
        print("Compare done!")
