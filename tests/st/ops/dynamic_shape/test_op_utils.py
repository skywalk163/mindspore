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

"""utils for tesing op's dynamic shape rapidly"""

import copy
import os

import numpy as np
from mindspore import Tensor, context, nn, ops
from mindspore.common import mutable, JitConfig


IR_LEVEL = 2
INT = 0
FLOAT = 1
BOOL = 2
TUPLE = 3
LIST = 4

JIT_CONFIG = None

class HelpNet(nn.Cell):
    def __init__(self, prim):
        super().__init__()
        self.op = prim

    # Inorder to run the net twice, the inputs with the type of list/tuple/scalar in repalced by help tensor
    def construct(self, *args):
        # the last two args indicates the index and type(tuple/list/scalar) of inputs which are replaced by help tensor
        index = args[-2]
        types = args[-1]

        new_args = list(args[:-2])
        for i, idx in enumerate(index):
            if types[i] == INT:
                new_args[idx] = int(args[idx])
            elif types[i] == FLOAT:
                new_args[idx] = float(args[idx])
            elif types[i] == BOOL:
                new_args[idx] = bool(args[idx])
            elif types[i] == TUPLE:
                new_args[idx] = ops.TensorToTuple()(args[idx])
            elif types[i] == LIST:
                new_args[idx] = ops.TensorToTuple()(args[idx])

        return self.op(*new_args)


class OpNet(nn.Cell):
    def __init__(self, prim):
        super().__init__()
        self.op = prim

    def construct(self, *args):
        return self.op(*args)


class OpFunctionNet(nn.Cell):
    def __init__(self, prim_func):
        super().__init__()
        self.prim_func = prim_func

    def construct(self, *args):
        return self.prim_func(*args)


class GradNet(nn.Cell):
    def __init__(self, net):
        super().__init__()
        self.net = net
        self.grad_func = ops.GradOperation()

    def construct(self, *args):
        func = self.grad_func(self.net)
        forward_out = self.net(*args)
        grad_out = func(*args)
        return forward_out, grad_out


class DynamicEnv():
    """Dynamic environment"""
    def __init__(self, env_type):
        if env_type not in ["DYNAMIC_SHAPE", "DYNAMIC_RANK"]:
            raise ValueError(f"The supported dynamic mode is [DYNAMIC_SHAPE, DYNAMIC_RANK], but got {env_type}.")

        self.env_name = 'MS_DEV_AUTO_' + env_type

    def __enter__(self):
        os.environ[self.env_name] = 'on'

    def __exit__(self, exc_type, exc_val, exc_tb):
        os.environ.pop(self.env_name, None)


def get_env(env_type):
    return DynamicEnv(env_type)


def compare(expect, actual, grad):
    if not grad:
        compare_result(expect, actual, 'Forward')
    else:
        expect_forward = expect[0]
        expect_grad = expect[1]
        actual_forward = actual[0]
        actual_grad = actual[1]

        compare_result(expect_forward, actual_forward, 'Forward')
        compare_result(expect_grad, actual_grad, 'Grad')


def compare_result(expect, actual, stage='', index=None):
    if not isinstance(actual, type(expect)):
        print("Compare Failed because the types of static-shape out(as expect) and dynamic-shape out(as actual) "
              "are not matched(exepct is a sequence).")
        print(f"  static-shape out(as expect): {expect}")
        print(f"  dynamic-shape out(as actual): {actual}")
        assert False

    if isinstance(expect, (list, tuple)):
        if len(expect) != len(actual):
            print(f"Compare Failed because of the length of static-shape out(as expect) "
                  f"is not equal to dynamic-shape out(as actual). "
                  f"static-shape out(as expect) length: {len(expect)}, "
                  f"dynamic-shape out(as actual) length: {len(actual)}")
            assert False
        if is_numerical_sequence(expect) and is_numerical_sequence(actual):
            result = np.allclose(expect, actual, rtol=1e-03, atol=1e-03)
            print(f"Compare {['Success'] if result else ['Failed']} for " \
                  f"{0 if index is None else index}'th output of {stage}.")
            assert result
            return

        for i, (exp, act) in enumerate(zip(expect, actual)):
            compare_result(exp, act, stage, i)
    else:
        if isinstance(expect, Tensor):
            result = np.allclose(expect.asnumpy(), actual.asnumpy(), rtol=1e-03, atol=1e-03)
        else:
            result = np.allclose(expect, actual, rtol=1e-03, atol=1e-03)
        print(f"Compare {['Success'] if result else ['Failed']} for " \
              f"{0 if index is None else index}'th output of {stage}.")
        assert result


def check_args(inputs_seq, tensor_dynamic_type, nontensor_dynamic_type, target):
    """validate the args"""
    if not isinstance(inputs_seq, list):
        raise TypeError(f"Arg 'inputs_seq' must be type of [list, tuple], but got {type(inputs_seq)}.")

    if len(inputs_seq) != 2:
        raise RuntimeError(f"For complete test, you must provide 2 groups of inputs, but got {len(inputs_seq)}.")

    if tensor_dynamic_type not in ['DYNAMIC_SHAPE', 'DYNAMIC_RANK', 'BOTH']:
        raise ValueError(f"tensor_dynamic_type must be one of [DYNAMIC_SHAPE, DYNAMIC_RANK, BOTH], " \
                         f"but got {tensor_dynamic_type}.")

    if target is not None and target not in ['CPU', 'GPU', 'Ascend']:
        raise ValueError(f"target must be one of [CPU, GPU, Ascend], but got {target}")

    if nontensor_dynamic_type not in ['STATIC_LEN', 'MUTABLE_LEN', 'BOTH', 'None']:
        raise ValueError(f"nontensor_dynamic_type must be one of [STATIC_LEN, MUTABLE_LEN, BOTH, None], " \
                         f"but got {nontensor_dynamic_type}.")


def get_nontensor_dynamic_type_list(nontensor_dynamic_type):
    if nontensor_dynamic_type == 'None':
        return []

    if nontensor_dynamic_type == 'BOTH':
        return ['STATIC_LEN', 'MUTABLE_LEN']

    return [nontensor_dynamic_type]


def run_in_dynamic_env(prim, inputs, dump_ir, ir_path, dynamic_type, grad):
    """set dynamic env before execute"""
    out_actual = None
    global JIT_CONFIG
    with get_env(dynamic_type) as _:
        if dump_ir:
            context.set_context(save_graphs=IR_LEVEL, save_graphs_path=ir_path)

        dynamic_net = create_net(prim, grad)
        if JIT_CONFIG:
            dynamic_net.set_jit_config(JIT_CONFIG)
        out_actual = dynamic_net(*inputs)

    return out_actual


def is_scalar(x):
    return isinstance(x, (int, float))


def is_numerical_sequence(seq):
    if isinstance(seq, (tuple, list)):
        if seq:
            return is_scalar(seq[0])
        return True
    return False


def is_nontensor(x):
    if is_numerical_sequence(x) or is_scalar(x):
        return True
    return False


def has_nontensor(inputs):
    for item in inputs:
        if is_nontensor(item):
            return True
    return False


def replace_nontensor_with_help_tensor(inputs):
    """replace_nontensor_with_help_tensor"""
    nontensor_input_index = []
    nontensor_input_type = []
    new_inputs = copy.deepcopy(inputs)
    for i, x in enumerate(inputs):
        if isinstance(x, tuple) and is_numerical_sequence(x):
            nontensor_input_type += [TUPLE]
            nontensor_input_index += [i]
            new_inputs[i] = Tensor(x)
        elif isinstance(x, list) and is_numerical_sequence(x):
            nontensor_input_type += [LIST]
            nontensor_input_index += [i]
            new_inputs[i] = Tensor(x)
        elif isinstance(x, bool):
            nontensor_input_type += [BOOL]
            nontensor_input_index += [i]
            new_inputs[i] = Tensor(x)
        elif isinstance(x, int):
            nontensor_input_type += [INT]
            nontensor_input_index += [i]
            new_inputs[i] = Tensor(x)
        elif isinstance(x, float):
            nontensor_input_type += [FLOAT]
            nontensor_input_index += [i]
            new_inputs[i] = Tensor(x)
        elif not isinstance(x, (Tensor, tuple, list, str)):
            raise TypeError(f"Unsupported type: {type(x)}")

    new_inputs += [nontensor_input_index, nontensor_input_type]
    return new_inputs


def convert_tensor_to_dynamic(inputs, dynamic_type):
    """create tesnor with dynamic shape"""
    new_inputs = copy.deepcopy(inputs)
    for i, x in enumerate(inputs):
        if isinstance(x, Tensor):
            ori_shape = x.shape
            if dynamic_type == 'DYNAMIC_SHAPE':
                new_shape = [None for _ in ori_shape]
            else:
                new_shape = None

            new_input = Tensor(shape=new_shape, dtype=x.dtype)
            new_inputs[i] = new_input

        if isinstance(x, (tuple, list)):
            new_input = convert_tensor_to_dynamic(x, dynamic_type)
            new_inputs[i] = new_input

    return new_inputs


def convert_sequence_of_tensor_to_mutable(inputs):
    for i, item in enumerate(inputs):
        if isinstance(item, (tuple, list)):
            if item and isinstance(item[0], Tensor):
                inputs[i] = mutable(item)
    return inputs


def run_with_dynamic_resize(prim, inputs_seq, dump_ir, ir_path, expect_resize):
    """test resize"""
    print("Start testing with [Resize]...")
    out_actual = None
    global JIT_CONFIG
    if dump_ir:
        context.set_context(save_graphs=IR_LEVEL, save_graphs_path=ir_path)

    compile_inputs = convert_tensor_to_dynamic(inputs_seq[0], 'DYNAMIC_RANK')
    compile_inputs = replace_nontensor_with_help_tensor(compile_inputs)
    compile_inputs = convert_sequence_of_tensor_to_mutable(compile_inputs)
    run_inputs = replace_nontensor_with_help_tensor(inputs_seq[0])
    run_inputs = convert_sequence_of_tensor_to_mutable(run_inputs)

    dynamic_net = HelpNet(prim)
    if JIT_CONFIG:
        dynamic_net.set_jit_config(JIT_CONFIG)
    dynamic_net.set_inputs(*compile_inputs)
    dynamic_net(*run_inputs)

    run_inputs = replace_nontensor_with_help_tensor(inputs_seq[1])
    run_inputs = convert_sequence_of_tensor_to_mutable(run_inputs)
    out_actual = dynamic_net(*run_inputs)

    compare_result(expect_resize, out_actual, 'Resize')
    print("End")


def convert_nontensor_to_mutable(inputs, dynamic_type):
    """convert list/tuple/scalar of int to mutable"""
    mutable_len = dynamic_type == 'MUTABLE_LEN'
    for i, x in enumerate(inputs):
        if is_nontensor(x):
            if is_scalar(x):
                x = mutable(x)
            else:
                x = mutable(x, mutable_len)
            inputs[i] = x
    return inputs


def run_and_compare(prim, inputs, dump_ir, prefix_dir, post_str, tensor_dynamic_type, expect, grad):
    ir_path = f"{prefix_dir}/{tensor_dynamic_type}_{post_str}"
    print(f"Start testing with [{tensor_dynamic_type}] [{post_str}]...")
    out_actual = run_in_dynamic_env(prim, inputs, dump_ir, ir_path, tensor_dynamic_type, grad)

    compare(expect, out_actual, grad)
    print("End")


def is_has_scalar_only(inputs):
    is_scalar_only = True
    has_scalar = False
    for item in inputs:
        if isinstance(item, (list, tuple)):
            is_scalar_only = False
        elif isinstance(item, (float, int)):
            has_scalar = True

    return has_scalar and is_scalar_only


def has_tensor(inputs):
    for item in inputs:
        if isinstance(item, Tensor):
            return True
    return False


def run_with_dynamic(prim, inputs_seq, tensor_dynamic_type, nontensor_dynamic_type, grad,
                     dump_ir, prefix_name, expect, expect_second):
    """run_with_dynamic"""
    types = []
    if tensor_dynamic_type == 'BOTH':
        types = ['DYNAMIC_SHAPE', 'DYNAMIC_RANK']
    else:
        types = [tensor_dynamic_type,]

    if has_tensor(inputs_seq[0]) and (nontensor_dynamic_type == 'None' or nontensor_dynamic_type == 'BOTH'):
        # Test dynamic Tensor
        for item in types:
            run_and_compare(prim, inputs_seq[0], dump_ir, prefix_name, 'None', item, expect, grad)

    # Test Resize of the KernelMod
    if expect_second is not None:
        ir_path = f"{prefix_name}/Resize"
        expect_resize = expect_second[0] if grad else expect_second
        run_with_dynamic_resize(prim, inputs_seq, dump_ir, ir_path, expect_resize)

    # Test dynamic nontensor
    seq_dynamic_list = get_nontensor_dynamic_type_list(nontensor_dynamic_type)
    has_scalar_only = is_has_scalar_only(inputs_seq[0])
    has_nontensor_input = has_nontensor(inputs_seq[0])

    if not has_nontensor_input:
        return

    if 'STATIC_LEN' in seq_dynamic_list:
        inputs_new = convert_nontensor_to_mutable(inputs_seq[0], 'STATIC_LEN')
        for item in types:
            run_and_compare(prim, inputs_new, dump_ir, prefix_name, 'VARIABLE_NONTENSOR_STATIC_LEN',
                            item, expect, grad)

    if 'MUTABLE_LEN' in seq_dynamic_list and not has_scalar_only:
        inputs_new = convert_nontensor_to_mutable(inputs_seq[0], 'MUTABLE_LEN')
        for item in types:
            run_and_compare(prim, inputs_new, dump_ir, prefix_name, 'VARIABLE_NONTENSOR_MUTABLE_LEN',
                            item, expect, grad)


def get_name_by_op(prim):
    name = str(prim)
    if "Prim" in name:
        name = name.replace('Prim[', '')
        name = name.replace(']', '')
        name = name.replace('=', '-')
        name = name.replace(',', '_')
        name = name.replace('<', '_')
        name = name.replace('>', '')
        name = name.replace(" ", '')
        return "ir_" + name

    if '0x' in name:
        name = name.replace('<', '')
        name = name.replace('>', '')
        name = name.replace(" ", '_')
        pos = name.find('_at')
        return "ir_" + name[:pos]

    name = name.replace('<', '_')
    name = name.replace('>', '')
    name = name.replace(" ", '')
    return "ir_" + name


def create_net(prim, grad):
    if isinstance(prim, ops.Primitive):
        net_class = OpNet
    else:
        net_class = OpFunctionNet

    if grad:
        return GradNet(net_class(prim))

    return net_class(prim)


def TEST_OP(op, inputs_seq, tensor_dynamic_type='BOTH', nontensor_dynamic_type='BOTH',
            mode=context.GRAPH_MODE, target=None, grad=True, dump_ir=False, custom_flag='',
            test_resize=True, jit_level=None):
    """
    This function creates several dynamic cases by converting Tensor/tuple/list/scalar inputs to dynamic shape to test the correctness of the op's InferShape
    and Resize. Both Primitive and Functional API are supported.
    For Tensor, including tuple/list of tensor, the dynamic cases include 'DYNAMIC_RANK' and 'DYNAMIC_SHAPE'.
    For tuple/list, the dynamic cases include 'NONE', 'STATIC_LEN' and 'MUTABLE_LEN'.
    For scalar, the dynamic cases include 'NONE' and 'STATIC_LEN'.
    So the all testcases should be: ['DYNAMIC_RANK', 'DYNAMIC_SHAPE'] * ['NONE', 'STATIC_LEN', 'MUTABLE_LEN'].
    The dynamic of string and other types of data is not supported yet.
    Furthermore, two groups of inputs are used to run twice with the same Cell to test the correctness of Resize.
    The expected data is generated by running with static shape.

    Inputs:
        - **op** (Union[Primitive, Function]) - The operator instance to be test.
        - **inputs_seq** (list[list, list]) - The inputs(attribute is not needed) of the operator.
          `inputs_seq` contains two groups of data: the first group will be used to running in the all dynamic cases, and the second will only be used to test `Resize`.
          The two groups of data should have the same type accordingly: if data is a Tensor, data with same dtype and different rank of shape should be given;
          if data is tuple/list/scalar, data only with different value should be given. e.g.: For ReduceSum, the two groups of inputs could be
          `[[Tensor(shape=[2, 3, 4], dtype=float32), [0]], [Tensor(shape=[4, 3, 2, 4], dtype=float32), [1]]`.
        - **tensor_dynamic_type** (str) - The dynamic type of Tensor inputs. Default 'BOTH'.
          The supported value are ['DYNAMIC_RANK', 'DYNAMIC_SHAPE', 'BOTH'] where 'BOTH' means 'DYNAMIC_RANK' and 'DYNAMIC_SHAPE'.
        - **nontensor_dynamic_type** (str) - The dynamic type of inputs expect Tensor like tuple/list/scalar. Default 'BOTH'.
          The supported value are ['NONE', 'STATIC_LEN', 'MUTABLE_LEN', 'BOTH'] where 'BOTH' means 'STATIC_LEN' and 'MUTABLE_LEN'. 'NONE' will always be test.
          'NONE' means the input is a constant.
          'STATIC_LEN' means the input is a variable but the length is fixed.
          'MUTABLE_LEN' means the input is a variable and the length is changeable.
          'MUTABLE_LEN' will be discarded If input is a scalar.
        - **mode** (int) - Running in GRAPH_MODE(0) or PYNATIVE_MODE(1). Default: GRAPH_MODE(0).
        - **target** (str) - Specified the backend. The supported value are ['CPU', 'GPU', 'Ascend']. Default None.
        - **grad** (bool) - If True, testcase will run with backward. Default True.
        - **dump_ir** (bool) - If True, 'save_graphs' will be set and save_graphs_path is generated by Primitive's name and dynamic type. Default False.
        - **custom_flag** (str) - Some log and ir path is distinguished by Primitive's name. Default ''.
          `custom_flag` can be used to distinguish the calling of TEST_OP with the same Primitive
        - **test_resize** (bool) - whether to test the Resize function. Default True.
        - ***jit_level* (str) - set JitConfig for function or Cell. Default None.

    Outputs:
        None

    Examples:
        >>> from mindspore import Tensor, ops
        >>> import numpy as np
        >>> from tests.st.ops.dynamic_shape.test_op_utils import TEST_OP
        >>> np_data1 = np.random.rand(2, 3, 4).astype(np.float32)
        >>> in1 = Tensor(np_data1)
        >>> np_data2 = np.random.rand(2, 3, 4).astype(np.float32)
        >>> tuple_in1 = (0,)
        >>> tuple_in2 = (1,)
        >>> # Testing Primitive
        >>> reducesum = ops.ReduceSum(keep_dims=True)
        >>> TEST_OP(reducesum, [[in1, tuple_in1], [in2, tuple_in2]])
        ...
        >>> # Testing functional API
        >>> def reducesum_func(x, axis, keep_dims):
        >>>     return ops.auto_generate.gen_ops_def.reduce_sum(x, axis, keep_dims)
        >>> TEST_OP(reducesum_func, [[in1, tuple_in1], [in2, tuple_in2]])
        ...
    """
    prefix_name = get_name_by_op(op)
    if custom_flag != '':
        prefix_name += '_' + custom_flag

    global JIT_CONFIG
    JIT_CONFIG = JitConfig(jit_level) if jit_level else None

    test_cast_name = f"{str(op)} custom_flag:[{custom_flag}]"
    print("******************************Begin test for " + test_cast_name + "******************************")

    check_args(inputs_seq, tensor_dynamic_type, nontensor_dynamic_type, target)

    if target is not None:
        context.set_context(device_target=target)

    context.set_context(mode=mode)
    os.system(f"rm {prefix_name} -rf")

    # setp 1: get standard data by running with static shape
    print("Start running with static shape with first inputs...")
    if dump_ir:
        ir_path = f"{prefix_name}/static_first_inputs"
        context.set_context(save_graphs=IR_LEVEL, save_graphs_path=ir_path)

    static_net = create_net(op, grad)
    if JIT_CONFIG:
        static_net.set_jit_config(JIT_CONFIG)
    out_expect = static_net(*inputs_seq[0])
    print("End")

    out_expect_second = None
    if test_resize:
        print("Start running with static shape with second inputs...")
        if dump_ir:
            ir_path = f"{prefix_name}/static_second_inputs"
            context.set_context(save_graphs=IR_LEVEL, save_graphs_path=ir_path)

        static_net_second = create_net(op, grad)
        if JIT_CONFIG:
            static_net_second.set_jit_config(JIT_CONFIG)
        out_expect_second = static_net_second(*inputs_seq[1])
        print("End")

    # step 2: run in dynamic mode and compare results
    run_with_dynamic(op, inputs_seq, tensor_dynamic_type, nontensor_dynamic_type, grad,
                     dump_ir, prefix_name, out_expect, out_expect_second)

    print("******************************End test for " + test_cast_name + "******************************")
