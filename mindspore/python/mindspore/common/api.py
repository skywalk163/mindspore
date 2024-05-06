# This is the Python adaptation and derivative work of Myia (https://github.com/mila-iqia/myia/).
#
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
"""Providing interface methods."""
from __future__ import absolute_import

import types
import sys
import os
import time
import ast
import inspect
import importlib
import hashlib
import contextlib
from collections import OrderedDict, namedtuple
from functools import wraps
import numpy as np
import mindspore as ms
from mindspore import context
from mindspore import log as logger
from mindspore._extends.remote import kernel_build_server
from mindspore.common.jit_config import JitConfig
from mindspore.common.tensor import Tensor as PythonTensor
from mindspore.common.sparse_tensor import CSRTensor as PythonCSRTensor
from mindspore.common.sparse_tensor import COOTensor as PythonCOOTensor
from mindspore.common.sparse_tensor import RowTensor as PythonRowTensor
from mindspore._c_expression import GraphExecutor_, Tensor, CSRTensor, RowTensor, COOTensor, \
    PyNativeExecutor_, verify_inputs_signature, init_exec_dataset, _set_dataset_mode_config, init_pipeline, \
    _ms_memory_recycle, _bind_device_ctx, jit_mode_pi_enable, jit_mode_pi_compile
from mindspore.parallel._ps_context import _is_role_sched
from mindspore.parallel._utils import _check_full_batch, _get_parameter_broadcast, _is_pynative_parallel, \
    _is_in_auto_parallel_mode
from mindspore import _checkparam as Validator
from mindspore._checkparam import is_stub_tensor
from mindspore.common._utils import is_shape_unknown
from mindspore.common.mutable import mutable
from mindspore.common._register_for_adapter import ms_adapter_registry
from mindspore.common.auto_dynamic_shape import get_auto_dynamic_shape_args, update_auto_dynamic_shape_phase, \
    get_auto_dynamic_shape_args_with_check_input_signature, update_auto_dynamic_shape_phase_with_check_input_signature


# Store ms_function class compiled pipeline cache.
ms_compile_cache = set()
# Store cell compiled pipeline cache.
cells_compile_cache = {}
# Store function compiled times information.
function_phases = dict()

BROADCAST_PHASE = "_broadcast_"
_PYNATIVE_PARALLEL_FUNC_NAME = "after_shard"


def _ms_adapter_tensor_as_parameter_output(data):
    """Check whether the data is an output from a parameter which is a ms_adapter tensor.
       Pylint: disable=unidiomatic-typecheck.
    """
    return ms_adapter_registry.is_registered and isinstance(data, ms_adapter_registry.tensor) \
           and hasattr(data, "__ms_parameter_output__") and getattr(data, "__ms_parameter_output__")


def _convert_python_data(data):
    """
    Convert C++ data to python.

    Args:
        data : The data need be convert.

    Returns:
        data, a data convert C++ to python
    """
    if isinstance(data, (Tensor, PythonTensor)) and data.adapter_flag:
        return ms_adapter_registry.tensor(data)
    if _ms_adapter_tensor_as_parameter_output(data):
        return data.tensor
    if isinstance(data, Tensor) and not isinstance(data, PythonTensor):
        return PythonTensor(data, internal=True)
    if isinstance(data, CSRTensor) and not isinstance(data, PythonCSRTensor):
        return PythonCSRTensor(csr_tensor=data)
    if isinstance(data, COOTensor) and not isinstance(data, PythonCOOTensor):
        return PythonCOOTensor(coo_tensor=data)
    if isinstance(data, RowTensor) and not isinstance(data, PythonRowTensor):
        return PythonRowTensor(row_tensor=data)
    if data.__class__ is tuple:
        # Handle namedtuple since its type is tuple.
        if hasattr(data, "_fields"):
            type_name = data.__class__.__name__
            data_dict = data._asdict()
            fields = data_dict.keys()
            return namedtuple(type_name, fields)(**_convert_python_data(data_dict))
        return tuple(_convert_python_data(x) for x in data)
    if data.__class__ is list:
        # Keep list object not change for inplace operation.
        for i in range(len(data)):
            data[i] = _convert_python_data(data[i])
        return data
    if data.__class__ is dict:
        # Keep the dict object not change.
        keys = tuple(data.keys())
        for key in keys:
            data[_convert_python_data(key)] = _convert_python_data(data.pop(key))
        return data
    return data


def _wrap_func(fn):
    """
    Wrapper function, convert return data to tensor or tuple of tensor.

    Args:
        fn (Function): The function need be wrapped.

    Returns:
        Function, a new function with return suitable format data.
    """

    @wraps(fn)
    def wrapper(*arg, **kwargs):
        results = fn(*arg, **kwargs)
        return _convert_python_data(results)

    return wrapper


def _check_all_tensor(sequence):
    for element in sequence:
        if not isinstance(element, Tensor) and not is_stub_tensor(element) and not (isinstance(element, tuple)
                                                                                    and _check_all_tensor(element)):
            return False
    return True


def _handle_func_args(func, *args, **kwargs):
    """Handle the *args and **kwargs inputs of the function."""
    if not isinstance(func, (types.FunctionType, types.MethodType)):
        raise RuntimeError('fn {} is not function or method'.format(func))
    if kwargs:
        bound_arguments = inspect.signature(func).bind(*args, **kwargs)
        bound_arguments.apply_defaults()
        args = bound_arguments.args
        kwargs = bound_arguments.kwargs

    positional_args = 0
    default_args = 0
    has_var = False
    for value in inspect.signature(func).parameters.values():
        if value.kind is inspect.Parameter.VAR_POSITIONAL or value.kind is inspect.Parameter.VAR_KEYWORD:
            has_var = True
        if value.kind is inspect.Parameter.POSITIONAL_OR_KEYWORD:
            if value.default is inspect.Parameter.empty:
                positional_args += 1
            else:
                default_args += 1

    if has_var:
        return args, kwargs

    if len(args) < positional_args:
        raise TypeError(f"Function {func.__name__} needs {positional_args} positional argument, but got {len(args)}.")
    if len(args) > positional_args + default_args:
        raise TypeError(f"Function {func.__name__} needs {positional_args} positional argument and {default_args} "
                        f"default argument, total {positional_args + default_args}, but got {len(args)}.")
    return args, kwargs


sys_path = list(sys.path)
# Get the entry script path.
if sys.argv and sys.argv[0] != '':
    entry_script_path = os.path.realpath(sys.argv[0])
    entry_script_path_dir = os.path.split(entry_script_path)[0]
    if entry_script_path_dir in sys_path:
        sys_path.remove(entry_script_path_dir)


def _in_sys_path(file_path):
    for path in sys_path:
        if file_path.startswith(path):
            return True
    return False


def __get_compile_cache_dep_files(file_path, compile_cache_dep_files, pkg):
    """Get the dependency files of the network"""
    with open(file_path) as fh:
        root = ast.parse(fh.read(), file_path)
    for node in ast.iter_child_nodes(root):
        module_name = ""
        if isinstance(node, ast.ImportFrom):
            if node.module is not None:
                module_name = node.module
            module_name = "." * node.level + module_name
        elif not isinstance(node, ast.Import):
            continue
        # Do not care the files in mindspore package
        if module_name.startswith("mindspore"):
            continue

        for n in node.names:
            if n.name.startswith("mindspore"):
                continue
            if module_name == "":
                whole_module = n.name
            else:
                whole_module = module_name
                if n.name is not None:
                    whole_module += "." + n.name
            try:
                module_spec = importlib.util.find_spec(whole_module, pkg)
            except (ModuleNotFoundError, ValueError):
                whole_module = whole_module[0:whole_module.rfind('.')]
                module_spec = importlib.util.find_spec(whole_module, pkg)
            if module_spec is None:
                continue
            module = importlib.util.module_from_spec(module_spec)
            if hasattr(module, '__file__'):
                dep_file_path = module.__file__
            else:
                continue
            # Exclude the installed modules.
            if not _in_sys_path(dep_file_path) and dep_file_path not in compile_cache_dep_files:
                logger.debug(f"dependent file path: {dep_file_path}")
                compile_cache_dep_files.append(dep_file_path)
                __get_compile_cache_dep_files(dep_file_path, compile_cache_dep_files, module.__package__)


def _get_compile_cache_dep_files():
    """Get the dependency files of the network"""
    if entry_script_path is None:
        logger.warning("Can not get the entry script file path.")
        return []
    compile_cache_dep_files = []
    logger.debug(f"entry script file path: {entry_script_path}")
    compile_cache_dep_files.append(entry_script_path)
    __get_compile_cache_dep_files(entry_script_path, compile_cache_dep_files, None)
    return compile_cache_dep_files


def _restore_mutable_attr(args_list, compile_args):
    """Restore the mutable attr for every arg."""
    new_compile_args = ()
    for idx, arg in enumerate(args_list):
        if hasattr(arg, "__ms_mutable__") and getattr(arg, "__ms_mutable__") and \
                not (hasattr(arg, "const_arg") and getattr(arg, "const_arg")):
            if hasattr(arg, "__ms_dynamic_len__"):
                new_compile_args += (mutable(compile_args[idx], getattr(arg, "__ms_dynamic_len__")),)
            else:
                new_compile_args += (mutable(compile_args[idx], False),)
        else:
            new_compile_args += (compile_args[idx],)
    return new_compile_args


def _get_parameter_layout():
    graph_executor = GraphExecutor_.get_instance()
    layout = dict()
    for phase in ms_compile_cache:
        layout.update(graph_executor.get_parameter_layout(phase))
    return layout


def _handle_arg(obj, arg, compile_arg):
    """Handle arg for runtime .If need handle the arg, return True"""
    if isinstance(arg, PythonTensor):
        if arg.has_init:
            arg.init_data()
        if not arg.const_arg:
            return arg
    elif isinstance(arg, (Tensor, CSRTensor, COOTensor)):
        return arg
    elif compile_arg is not None and hasattr(compile_arg, "__ms_mutable__") and getattr(compile_arg, "__ms_mutable__"):
        # mutable([]) will be eliminated by FuncGraphSpecializer, and empty list is not supported by backend.
        if isinstance(arg, list) and not arg:
            return None
        return arg
    elif context.get_context("grad_for_scalar") and isinstance(arg, (int, float)):
        return arg
    elif hasattr(obj, "enable_tuple_broaden") and obj.enable_tuple_broaden and isinstance(arg, tuple) and \
            _check_all_tensor(arg):
        return arg
    return None


def _handle_arg_predict(obj, arg, compile_arg):
    """Handle arg for runtime .If need handle the arg, return True"""
    if arg is None:
        return None

    if isinstance(arg, (int, float)):
        return None

    if isinstance(arg, (list, tuple)):
        if compile_arg is not None and hasattr(compile_arg, "__ms_mutable__") and \
                getattr(compile_arg, "__ms_mutable__"):
            # mutable([]) will be eliminated by FuncGraphSpecializer, and empty list is not supported by backend.
            if isinstance(arg, list) and not arg:
                return None
            return arg
        if hasattr(obj, "enable_tuple_broaden") and obj.enable_tuple_broaden and isinstance(arg, tuple) and \
                _check_all_tensor(arg):
            return arg
        return None
    return arg


def _get_args_for_run(obj, args, kwargs, compile_args):
    """Get the actual input args and kwargs for runtime."""
    new_args = []
    for arg, compile_arg in zip(args, compile_args):
        new_arg = _handle_arg(obj, arg, compile_arg)
        if new_arg is not None:
            new_args.append(new_arg)

    for _, value in kwargs.items():
        new_value = _handle_arg(obj, value, None)
        if new_value is not None:
            new_args.append(new_value)

    return new_args

def _get_args_for_run_predict(obj, args, kwargs, compile_args):
    """Get the actual input args and kwargs for runtime."""
    new_args = []
    for arg, compile_arg in zip(args, compile_args):
        new_arg = _handle_arg_predict(obj, arg, compile_arg)
        if new_arg is not None:
            new_args.append(new_arg)

    for _, value in kwargs.items():
        new_value = _handle_arg_predict(obj, value, None)
        if new_value is not None:
            new_args.append(new_value)

    return new_args


class _MindsporeFunctionExecutor:
    """
    Represents a function compiled by graph compiler.

    _MindsporeFunctionExecutor will compile the original function for every combination
    of argument types and shapes it is given (as well as their values, optionally).

    Args:
        fn (Function): The root function to compile.
        input_signature (Function): User defines signature to verify input.
        ms_create_time(TimeStamp): Time the function was created
        obj (Object): If function is a method, obj is the owner of function,
             else, obj is none.

    Returns:
        The result of pipeline running in graph mode.
    """
    def __init__(self, fn, ms_create_time, input_signature=None, obj=None, jit_config=None):
        init_pipeline()
        if not isinstance(fn, (types.FunctionType, types.MethodType)):
            raise RuntimeError('fn {} is not function or method'.format(fn))

        self.fn = fn
        self.input_signature = input_signature
        self.obj = None
        if obj and hasattr(obj, fn.__name__):
            self.obj = obj
        self.shard_parent_obj = obj
        self.enable_tuple_broaden = False
        self._graph_executor = GraphExecutor_.get_instance()
        self._create_time = ms_create_time
        self._compile_args = None
        self.jit_config_dict = jit_config.jit_config_dict if jit_config else None


    @_wrap_func
    def __call__(self, *args, **kwargs):
        args_list = args
        if self.obj is not None:
            args_list = args_list[1:]
        phase = ""
        try:
            if context.get_context("mode") == context.PYNATIVE_MODE:
                _pynative_executor.set_jit_compile_status(True, phase)
                phase = self.compile(self.fn.__name__, *args_list, **kwargs)
                _pynative_executor.set_jit_compile_status(False, phase)
            else:
                phase = self.compile(self.fn.__name__, *args_list, **kwargs)
        except Exception as err:
            _pynative_executor.clear_res()
            raise err

        if context.get_context("precompile_only"):
            return None

        new_inputs = self._generate_run_args(args_list, kwargs)
        output = self._graph_executor(tuple(new_inputs), phase)
        if context.get_context("mode") == context.PYNATIVE_MODE:
            output = _pynative_executor.grad_jit(output, *new_inputs)

        return output


    def compile(self, method_name, *args, **kwargs):
        """Returns pipeline for the given args."""
        # Check whether hook function registered on Cell object.
        if self.obj and hasattr(self.obj, "_hook_fn_registered"):
            if self.obj._hook_fn_registered():
                logger.warning(f"For 'Cell', it's not support hook function when using 'jit' decorator. "
                               f"If you want to use hook function, please use context.set_context to set "
                               f"pynative mode and remove 'jit' decorator.")
        # Chose dynamic shape tensors or actual input tensors as compile args.
        compile_args = self._generate_compile_args(args)
        key_id = self._get_key_id()
        compile_args = get_auto_dynamic_shape_args_with_check_input_signature(compile_args, key_id,
                                                                              self.input_signature)

        # Restore the mutable attr for every arg.
        compile_args = _restore_mutable_attr(args, compile_args)
        self._compile_args = compile_args
        generate_name, echo_function_name = self._get_generate_name()
        # The full Function name
        full_function_name = generate_name
        create_time = ''

        # Add key with obj
        if self.obj is not None:
            if self.obj.__module__ != self.fn.__module__:
                logger.info(
                    f'The module of `self.obj`: `{self.obj.__module__}` is not same with the module of `self.fn`: '
                    f'`{self.fn.__module__}`')
            self.obj.__parse_method__ = method_name
            if isinstance(self.obj, ms.nn.Cell):
                generate_name = generate_name + '.' + str(self.obj.create_time)
                create_time = str(self.obj.create_time)
            else:
                generate_name = generate_name + '.' + str(self._create_time)
                create_time = str(self._create_time)

            generate_name = generate_name + '.' + str(id(self.obj))
            full_function_name = generate_name
        else:
            # Different instance of same class may use same memory(means same obj_id) at diff times.
            # To avoid unexpected phase matched, add create_time to generate_name.
            generate_name = generate_name + '.' + str(self._create_time)
            create_time = str(self._create_time)

        self.enable_tuple_broaden = False
        if hasattr(self.obj, "enable_tuple_broaden"):
            self.enable_tuple_broaden = self.obj.enable_tuple_broaden

        self._graph_executor.set_enable_tuple_broaden(self.enable_tuple_broaden)
        key = self._graph_executor.generate_arguments_key(self.fn, compile_args, kwargs, self.enable_tuple_broaden)
        phase = generate_name + '.' + str(key)

        update_auto_dynamic_shape_phase_with_check_input_signature(compile_args, key_id, phase, self.input_signature)

        if phase in ms_compile_cache:
            # Release resource should be released when CompileInner won't be executed, such as cur_convert_input_
            # generated in generate_arguments_key.
            self._graph_executor.clear_compile_arguments_resource()
            return phase

        self._check_recompile(full_function_name, create_time, echo_function_name)

        # If enable compile cache, get the dependency files list and set to graph executor.
        self._set_compile_cache_dep_files()
        if self.jit_config_dict:
            self._graph_executor.set_jit_config(self.jit_config_dict)
        else:
            jit_config_dict = JitConfig().jit_config_dict
            self._graph_executor.set_jit_config(jit_config_dict)

        if self.obj is None:
            # Set an attribute to fn as an identifier.
            if isinstance(self.fn, types.MethodType):
                setattr(self.fn.__func__, "__jit_function__", True)
            else:
                setattr(self.fn, "__jit_function__", True)
            is_compile = self._graph_executor.compile(self.fn, compile_args, kwargs, phase, True)
            if isinstance(self.fn, types.MethodType):
                delattr(self.fn.__func__, "__jit_function__")
            else:
                delattr(self.fn, "__jit_function__")
        else:
            if isinstance(self.obj, ms.nn.Cell):
                self._graph_executor.set_weights_values(self.obj.parameters_dict())
            is_compile = self._graph_executor.compile(self.obj, compile_args, kwargs, phase, True)

        if not is_compile:
            raise RuntimeError("Executor compile failed.")
        ms_compile_cache.add(phase)

        return phase


    @staticmethod
    def _check_recompile(full_function_name, create_time, echo_function_name):
        """Warning when the function has been compiled."""
        ignore_dirs = ["mindspore/ops", "mindspore/nn"]
        if any((lambda x: x in full_function_name)(x) for x in ignore_dirs):
            return

        if full_function_name in function_phases:
            warning_times = 1
            if len(function_phases[full_function_name]) >= warning_times \
                    and create_time not in function_phases[full_function_name]:
                tips = "Try to decorate the function with @jit(hash_args=...) " \
                       "or @jit(compile_once=True) to reduce the compile time. " \
                       "For more details, get instructions about `jit` at " \
                       "https://www.mindspore.cn/search?inputValue=jit."

                logger.warning(f"The {echo_function_name} has been compiled again. "
                               f"{tips} ")
        else:
            function_phases[full_function_name] = set()

        function_phases[full_function_name].add(create_time)


    @staticmethod
    def _optimizer_state_init(opt_states):
        """set data for all optimizer states in case it is executed in graph mode"""
        prefix_list = ["moments", "accum", "moment1", "moment2", "lamb_m", "lamb_v", "mean_grad",
                       "mean_square", "prev"]
        for opt_param in opt_states:
            prefix = opt_param.name[:opt_param.name.find(".")]
            if opt_param.has_init and (prefix in prefix_list or opt_param.name == "global_step"):
                opt_param.init_data()


    def _get_key_id(self):
        """get key id."""
        if isinstance(self.obj, ms.nn.Cell):
            key_id = str(id(self.obj)) + str(self.obj.create_time)
        else:
            key_id = str(id(self.obj)) + str(self._create_time)

        if _pynative_executor.grad_flag():
            key_id = key_id + ".grad"
        return key_id


    def _get_generate_name(self):
        """get generate name."""
        generate_name = self.fn.__module__ + "." + self.fn.__name__ + "." + self.fn.__code__.co_filename + "." + str(
            self.fn.__code__.co_firstlineno)
        echo_function_name = "function \"" + self.fn.__name__ + "\" at the file \"" + self.fn.__code__.co_filename \
                             + "\", line " + str(self.fn.__code__.co_firstlineno)
        if _pynative_executor.grad_flag():
            generate_name = generate_name + ".grad"
        if _is_pynative_parallel():
            generate_name = generate_name[:generate_name.rfind(str(id(self.fn)))] + str(id(self.shard_parent_obj))
        return generate_name, echo_function_name


    def _set_compile_cache_dep_files(self):
        # If enable compile cache, get the dependency files list
        enable_compile_cache = context.get_context("enable_compile_cache")
        if enable_compile_cache is not True and enable_compile_cache != "1":
            enable_compile_cache = os.getenv('MS_COMPILER_CACHE_ENABLE')
        if enable_compile_cache is True or enable_compile_cache == "1":
            self._graph_executor.set_compile_cache_dep_files(_get_compile_cache_dep_files())


    def _generate_compile_args(self, args_list):
        """Chose dynamic shape tensors or actual input tensors as compile args."""
        # Case: If the shape of input args is dynamic, get dynamic shape tensor from context and use it to compile.
        compile_args = _pynative_executor.get_dynamic_input(args_list)
        # Case: The `set_inputs()` of Cell object has been set, using these dynamic shape args as compile args.
        if self.fn.__name__ == 'construct' and isinstance(self.obj, ms.nn.Cell) and self.obj.get_inputs():
            compile_args = self.obj.get_inputs()
            if len(compile_args) != len(args_list):
                raise ValueError(f"The number of actual input tensors: {len(args_list)} is not equal to the number of "
                                 f"dynamic shape tensors: {len(compile_args)}.")
            for i, elem in enumerate(compile_args):
                if isinstance(elem, PythonTensor):
                    Validator.check_dynamic_shape(compile_args[i], args_list[i], i)
            Validator.check_symbolic_shape(compile_args, args_list)

        # Case: If dynamic shape tensors have been assigned to `input_signature`, they are preferred as compile args.
        if self.input_signature is not None:
            if not isinstance(self.input_signature, (tuple, list)):
                self.input_signature = (self.input_signature,)
            self.input_signature = list(self.input_signature)
            dyn_shape = False
            for i, elem in enumerate(self.input_signature):
                if isinstance(elem, PythonTensor) and is_shape_unknown(elem.shape):
                    Validator.check_dynamic_shape(self.input_signature[i], args_list[i], i)
                    dyn_shape = True
            Validator.check_symbolic_shape(self.input_signature, args_list)
            if dyn_shape:
                # Checkout whether the `sens` has been added to args_list.
                if len(self.input_signature) == len(args_list) - 1:
                    logger.warning(f"The number of actual input args '{len(args_list)}' is one more than the number "
                                   f"of input_signature args '{len(self.input_signature)}'. The last actual args may "
                                   f"be 'sens' and added it to compile args.")
                    self.input_signature.append(args_list[-1])
                compile_args = tuple(self.input_signature)
                if self.obj is not None:
                    _pynative_executor.set_dynamic_input(self.obj, *compile_args)
                else:
                    _pynative_executor.set_dynamic_input(self.fn, *compile_args)
            else:
                if not verify_inputs_signature(self.input_signature, args_list):
                    raise ValueError("The input args is incompatible with the args in `input_signature`!")
        return compile_args

    def _generate_run_args(self, args_list, kwargs):
        """
        Generate input args, which are required for running.

        Args:
            args_list (Tuple): Actual input args.
            kwargs (Dict): Actual input kwargs.

        Returns:
            new_inputs, new input args, which are required for running.
        """
        return _get_args_for_run(self, args_list, kwargs, self._compile_args)


# The attributes used to identify a given object.
attr_op = {"__str__": lambda x: x.__str__(),
           "__hash__": lambda x: str(x.__hash__()),
           "__module__": lambda x: x.__module__,
           "__name__": lambda x: x.__name__,
           "__qualname__": lambda x: x.__qualname__,
           "__len__": lambda x: str(x.__len__()),
           "__code__": lambda x: x.__code__.co_filename + str(x.__code__.co_firstlineno)
           }


def _get_obj_id(input_obj):
    """Get hash id of single object."""
    obj_id = ".".join(
        (map(lambda x: attr_op.get(x)(input_obj) if hasattr(input_obj, x) and getattr(input_obj, x) else "", attr_op)))
    return obj_id + str(id(input_obj))


def _get_jit_hash(hash_input):
    """Get hash value of single object or list of objects."""
    if isinstance(list, tuple):
        return ".".join(map(_get_obj_id, hash_input))
    return _get_obj_id(hash_input)


def jit(fn=None, mode="PSJit", input_signature=None, hash_args=None, jit_config=None, compile_once=False):
    """
    Create a callable MindSpore graph from a Python function.

    This allows the MindSpore runtime to apply optimizations based on graph.

    Args:
        fn (Function): The Python function that will be run as a graph. Default: ``None`` .
        mode (str): The type of jit used, the value of mode should be ``PIJit`` or ``PSJit``. Default: ``PSJit`` .

            - `PSJit <https://www.mindspore.cn/docs/en/master/note/static_graph_syntax_support.html>`_ : MindSpore GRAPH_MODE.
            - `PIJit <https://www.mindspore.cn/docs/en/master/design/dynamic_graph_and_static_graph.html>`_ : MindSpore PYNATIVE_MODE.

        input_signature (Tensor): The Tensor which describes the input arguments. The shape and dtype of the Tensor
            will be supplied to this function. If input_signature is specified, each input to `fn` must be a `Tensor`.
            And the input parameters of `fn` cannot accept `**kwargs`. The shape and dtype of actual inputs should
            keep the same as input_signature. Otherwise, TypeError will be raised. Default: ``None`` .
        hash_args (Union[Object, List or Tuple of Objects]): The local free variables used inside `fn`,
            like functions or objects of class defined outside `fn`. Calling `fn` again with change of `hash_args`
            will trigger recompilation. Default: ``None`` .
        jit_config (JitConfig): Jit config for compile. Default: ``None`` .
        compile_once(bool): ``True``: The function would be compiled once when it was created many times.
            But it may be wrong if the free variables were changed. ``False`` : It would be recompiled when
            it was created again.
            Default: ``False`` .

    Note:
        If `input_signature` is specified, each input of `fn` must be a Tensor. And the input arguments for `fn`
        will not accept `**kwargs`.

    Returns:
        Function, if `fn` is not None, returns a callable function that will execute the compiled function; If `fn` is
        None, returns a decorator and when this decorator invokes with a single `fn` argument, the callable function is
        equal to the case when `fn` is not None.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor
        >>> from mindspore import ops
        >>> from mindspore import jit
        ...
        >>> x = Tensor(np.ones([1, 1, 3, 3]).astype(np.float32))
        >>> y = Tensor(np.ones([1, 1, 3, 3]).astype(np.float32))
        ...
        >>> # create a callable MindSpore graph by calling decorator @jit
        >>> def tensor_add(x, y):
        ...     z = x + y
        ...     return z
        ...
        >>> tensor_add_graph = jit(fn=tensor_add)
        >>> out = tensor_add_graph(x, y)
        ...
        >>> # create a callable MindSpore graph through decorator @jit
        >>> @jit
        ... def tensor_add_with_dec(x, y):
        ...     z = x + y
        ...     return z
        ...
        >>> out = tensor_add_with_dec(x, y)
        ...
        >>> # create a callable MindSpore graph through decorator @jit with input_signature parameter
        >>> @jit(input_signature=(Tensor(np.ones([1, 1, 3, 3]).astype(np.float32)),
        ...                       Tensor(np.ones([1, 1, 3, 3]).astype(np.float32))))
        ... def tensor_add_with_sig(x, y):
        ...     z = x + y
        ...     return z
        ...
        >>> out = tensor_add_with_sig(x, y)
        ...
        ... # Set hash_args as fn, otherwise cache of compiled closure_fn will not be reused.
        ... # While fn differs during calling again, recompilation will be triggered.
        >>> def func(x):
        ...     return ops.exp(x)
        ...
        >>> def closure_fn(x, fn):
        ...     @jit(hash_args=fn)
        ...     def inner_fn(a):
        ...         return fn(a)
        ...     return inner_fn(x)
        ...
        >>> inputs = Tensor(np.ones([10, 10, 10]).astype(np.float32))
        >>> for i in range(10):
        ...     closure_fn(inputs, func)
        ...
        ... # Set compile_once = True, otherwise the train_step will be compiled again.
        >>> def train(x):
        ...     @jit(compile_once = True)
        ...     def train_step(x):
        ...         return ops.exp(x)
        ...     for i in range(10):
        ...         train_step(x)
        ...
        >>> inputs = Tensor(np.ones([10, 10, 10]).astype(np.float32))
        >>> for i in range(10):
        ...     train(inputs)
    """

    def wrap_mindspore(func):
        if not isinstance(compile_once, bool):
            logger.warning(f"The parameter `compile_once` of jit should be a bool, "
                           f"but got {type(compile_once)}.")
        if hash_args:
            hash_obj = _get_jit_hash(hash_args)
        elif compile_once:
            hash_obj = 0
        else:
            hash_obj = int(time.time() * 1e9)

        @wraps(func)
        def staging_specialize(*args, **kwargs):
            if os.getenv("MS_JIT") == '0':
                return func(*args, **kwargs)

            args, kwargs = _handle_func_args(func, *args, **kwargs)

            process_obj = None
            if args and not isinstance(args[0], PythonTensor) and hasattr(args[0], func.__name__):
                process_obj = args[0]
            # only the function or cell instance wrapped by shard will fall into this branch
            if _is_pynative_parallel() and func.__name__ == _PYNATIVE_PARALLEL_FUNC_NAME:
                process_obj = hash_args
            out = _MindsporeFunctionExecutor(func, hash_obj, input_signature, process_obj, jit_config)(*args, **kwargs)
            return out

        return staging_specialize

    def pi_wrap_mindspore(decorated):
        func = decorated
        if isinstance(func, ms.nn.Cell):
            func = func.construct
        if isinstance(func, type) and issubclass(func, ms.nn.Cell):
            func = func.construct
        if isinstance(func, types.MethodType):
            func = func.__func__
        if not isinstance(func, types.FunctionType):
            logger.warning("only support function and mindspore.nn.Cell instance")
            return decorated

        # generator, coroutine, awaitable and a function that return them is unsupported
        UNSUPPORTED_CODE_TYPE = (inspect.CO_GENERATOR | inspect.CO_COROUTINE |
                                 inspect.CO_ASYNC_GENERATOR | inspect.CO_ITERABLE_COROUTINE)
        if func.__code__.co_flags & UNSUPPORTED_CODE_TYPE:
            return decorated

        config = dict()
        if isinstance(jit_config, JitConfig):
            config.update(jit_config.jit_config_dict)
        elif jit_config is not None:
            config.update(jit_config)
        jit_mode_pi_enable()

        if jit_mode_pi_compile(func, config) is False:
            logger.warning('add fn {} to compile failed '.format(func))

        return decorated

    wrap_func = wrap_mindspore
    if mode == "PIJit":
        wrap_func = pi_wrap_mindspore

    if fn is not None:
        return wrap_func(fn)
    return wrap_func


def ms_function(fn=None, input_signature=None, hash_args=None, jit_config=None):
    """
    Create a callable MindSpore graph from a Python function.

    This allows the MindSpore runtime to apply optimizations based on graph.

    Note:
        - `ms_function` will be deprecated and removed in a future version. Please use :func:`mindspore.jit` instead.
        - If `input_signature` is specified, each input of `fn` must be a Tensor. And the input arguments for `fn`
          will not accept `**kwargs`.

    Args:
        fn (Function): The Python function that will be run as a graph. Default: ``None`` .
        input_signature (Tensor): The Tensor which describes the input arguments. The shape and dtype of the Tensor
            will be supplied to this function. The shape and dtype of actual inputs of `fn` should
            keep the same as input_signature. Otherwise, TypeError will be raised. Default: ``None`` .
        hash_args (Union[Object, List or Tuple of Objects]): The local free variables used inside `fn`,
            like functions or objects of class defined outside `fn`. Calling `fn` again with change of `hash_args`
            will trigger recompilation. Default: ``None`` .
        jit_config (JitConfig): Jit config for compile. Default: ``None`` .

    Returns:
        Function, if `fn` is not None, returns a callable function that will execute the compiled function; If `fn` is
        None, returns a decorator and when this decorator invokes with a single `fn` argument, the callable function is
        equal to the case when `fn` is not None.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor
        >>> from mindspore import ops
        >>> from mindspore import ms_function
        ...
        >>> x = Tensor(np.ones([1, 1, 3, 3]).astype(np.float32))
        >>> y = Tensor(np.ones([1, 1, 3, 3]).astype(np.float32))
        ...
        >>> # create a callable MindSpore graph by calling ms_function
        >>> def tensor_add(x, y):
        ...     z = x + y
        ...     return z
        ...
        >>> tensor_add_graph = ms_function(fn=tensor_add)
        >>> out = tensor_add_graph(x, y)
        ...
        >>> # create a callable MindSpore graph through decorator @ms_function
        >>> @ms_function
        ... def tensor_add_with_dec(x, y):
        ...     z = x + y
        ...     return z
        ...
        >>> out = tensor_add_with_dec(x, y)
        ...
        >>> # create a callable MindSpore graph through decorator @ms_function with input_signature parameter
        >>> @ms_function(input_signature=(Tensor(np.ones([1, 1, 3, 3]).astype(np.float32)),
        ...                               Tensor(np.ones([1, 1, 3, 3]).astype(np.float32))))
        ... def tensor_add_with_sig(x, y):
        ...     z = x + y
        ...     return z
        ...
        >>> out = tensor_add_with_sig(x, y)
        ...
        ... # Set hash_args as fn, otherwise cache of compiled `closure_fn` will not be reused.
        ... # While fn differs during calling again, recompilation will be triggered.
        >>> def func(x):
        ...     return ops.exp(x)
        ...
        >>> def closure_fn(x, fn):
        ...     @ms_function(hash_args=fn)
        ...     def inner_fn(a):
        ...         return fn(a)
        ...     return inner_fn(x)
        ...
        >>> inputs = Tensor(np.ones([10, 10, 10]).astype(np.float32))
        >>> for i in range(10):
        ...     closure_fn(inputs, func)
    """

    logger.warning("'mindspore.ms_function' will be deprecated and removed in a future version. "
                   "Please use 'mindspore.jit' instead.")
    return jit(fn=fn, input_signature=input_signature, hash_args=hash_args, jit_config=jit_config)


def _core(fn=None, **flags):
    """
    A decorator that adds a flag to the function.

    By default, the function is marked as True, enabling to use this decorator to
    set flag to a graph.

    Args:
        fn (Function): Function to add flag. Default: ``None``.
        flags (dict): The following flags can be set core, which indicates that this is a core function or
                      other flag. Default: ``None``.

    Returns:
        Function, the function with core flag.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``
    """

    # need set the attr and access on c++
    def deco(fn):
        fn._func_graph_flags = {
            'core': True,
            **flags,
        }
        return fn

    if fn is not None:
        ret = deco(fn)
    else:
        ret = deco
    return ret


def _add_flags(fn=None, **flags):
    """
    A decorator that adds a flag to the function.

    Note:
        Only supports bool value.

    Args:
        fn (Function): Function or cell to add flag. Default: ``None``.
        flags (dict): Flags use kwargs. Default: ``None``.

    Returns:
        Function, the function with added flags.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``
    """

    def deco(fn):
        # need set the attr and access on c++
        if not hasattr(fn, "_func_graph_flags"):
            fn._func_graph_flags = {}

        fn._func_graph_flags.update({**flags})
        return fn

    ret = deco
    if fn is not None:
        ret = deco(fn)
    return ret


def _no_recursive(callable_obj):
    """
    Method or function decorator for ignoring recursive check.

    This allows MindSpore to skip the procedure of checking function or method recursive.

    Args:
        callable_obj (Union(method, function)): The function or method to call.

    Returns:
        Function or method with no_recursive flag.

    Raises:
        TypeError: If ms_class is used for non-class types or nn.Cell.
        AttributeError: If the private attributes or magic methods of the class decorated by ms_class is called.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``
    """
    is_cell_subclass = inspect.isclass(callable_obj) and issubclass(callable_obj, ms.nn.Cell)
    if not is_cell_subclass and not inspect.ismethod(callable_obj) and not inspect.isfunction(callable_obj):
        raise TypeError(f"Decorator no_recursive is used for callable object, but got {callable_obj}.")
    _add_flags(callable_obj, no_recursive=True)
    return callable_obj


def ms_class(cls):
    """
    Class decorator for user-defined classes.

    This allows MindSpore to identify user-defined classes and thus obtain their attributes and methods.

    Note:
        `ms_class` will be deprecated and removed in a future version. Please use :func:`mindspore.jit_class` instead.

    Args:
        cls (Class): User-defined class.

    Returns:
        Class.

    Raises:
        TypeError: If ms_class is used for non-class types or nn.Cell.
        AttributeError: If the private attributes or magic methods of the class decorated with ms_class is called.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore.nn as nn
        >>> from mindspore import ms_class
        ...
        >>> @ms_class
        ... class UserDefinedNet:
        ...     def __init__(self):
        ...         self.value = 10
        ...
        ...     def func(self, x):
        ...         return 2 * x
        ...
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.net = UserDefinedNet()
        ...
        ...     def construct(self, x):
        ...         out = self.net.value + self.net.func(x)
        ...         return out
        ...
        >>> net = Net()
        >>> out = net(5)
        >>> print(out)
        20
    """

    logger.warning("'mindspore.ms_class' will be deprecated and removed in a future version. "
                   "Please use 'mindspore.jit_class' instead.")

    # Check if cls is of type class.
    if not inspect.isclass(cls):
        raise TypeError(f'Decorator ms_class can only be used for class type, but got {cls}.')
    # Check if cls is nn.Cell.
    if issubclass(cls, ms.nn.Cell):
        raise TypeError(f"Decorator ms_class is used for user-defined classes and cannot be used for nn.Cell: {cls}.")
    logger.info(f'Found ms_class: {cls}.')
    setattr(cls, '__ms_class__', True)
    return cls


def jit_class(cls):
    """
    Class decorator for user-defined classes.

    This allows MindSpore to identify user-defined classes and thus obtain their attributes and methods.

    Args:
        cls (Class): User-defined class.

    Returns:
        Class.

    Raises:
        TypeError: If `jit_class` is used for non-class types or nn.Cell.
        AttributeError: If the private attributes or magic methods of the class decorated with `jit_class` is called.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore.nn as nn
        >>> from mindspore import jit_class
        ...
        >>> @jit_class
        ... class UserDefinedNet:
        ...     def __init__(self):
        ...         self.value = 10
        ...
        ...     def func(self, x):
        ...         return 2 * x
        ...
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.net = UserDefinedNet()
        ...
        ...     def construct(self, x):
        ...         out = self.net.value + self.net.func(x)
        ...         return out
        ...
        >>> net = Net()
        >>> out = net(5)
        >>> print(out)
        20
    """
    from mindspore import nn
    # Check if cls is of type class.
    if not inspect.isclass(cls):
        raise TypeError(f'Decorator jit_class can only be used for class type, but got {cls}.')
    # Check if cls is nn.Cell.
    if issubclass(cls, nn.Cell):
        raise TypeError(f"Decorator jit_class is used for user-defined classes and cannot be used for nn.Cell: {cls}.")
    setattr(cls, '__ms_class__', True)
    return cls


def set_adapter_config(config):
    """
    Register configuration information for MSAdapter.

    Args:
        config (dict): Configuration information.
    """
    if not isinstance(config, dict):
        raise TypeError(f"The input argument of 'set_adapter_config' should be a dict, but got {config}.")
    for key, value in config.items():
        if key == "Tensor":
            ms_adapter_registry.register_tensor(value)
        elif key == "Parameter":
            ms_adapter_registry.register_parameter(value)
        elif key == "convert_object_map":
            ms_adapter_registry.register_convert_map(value)
        elif key == "convert_adapter_tensor_map":
            ms_adapter_registry.register_convert_adapter_tensor_map(value)
        else:
            raise ValueError(f"Unsupported key in adapter config: {key}")


def _function_forbid_reuse(func):
    if not inspect.isfunction(func):
        raise TypeError(f'Decorator _function_forbid_reuse can only be used for function type, but got {func}.')
    setattr(func, '__function_forbid_reuse__', True)
    return func


def _build_broadcast_graph(broadcast_params_dict, broadcast_phase):
    """Build broadcast graph."""
    from mindspore.nn.wrap.cell_wrapper import _BroadCastCell
    if not broadcast_params_dict:
        broadcast_params_dict = {}
    broadcast_params = []
    for param in broadcast_params_dict.values():
        broadcast_params.append(Tensor(param.asnumpy()))
    _broadcast_net = _BroadCastCell(broadcast_params)
    _broadcast_net.phase = broadcast_phase
    broadcasted_params = _broadcast_net()
    for param_name, param in zip(broadcast_params_dict.keys(), broadcasted_params):
        broadcast_params_dict.get(param_name).set_data(param)


def _get_auto_split_param_names(parameter_layout_dict):
    auto_split_param_names = []
    for key, value in parameter_layout_dict.items():
        for dim in value[1]:
            if dim != -1:
                auto_split_param_names.append(key)
                break
    return auto_split_param_names


def _parameter_broadcast(obj):
    """
    Parameter broadcast.
    When the parallel mode is 'semi_auto_parallel' or 'auto_parallel', it will broadcast the parameters that have not
    split.
    """
    auto_split_param_names = []
    if _is_in_auto_parallel_mode():
        auto_split_param_names = _get_auto_split_param_names(obj.parameter_layout_dict)

    broadcast_params_dict = obj.parameters_broadcast_dict()
    if auto_split_param_names and broadcast_params_dict:
        broadcast_params_dict = OrderedDict()
        for param_name, param in obj.parameters_broadcast_dict().items():
            if param_name not in auto_split_param_names:
                broadcast_params_dict[param_name] = param
    broadcast_phase = "_broadcast_subgraph"
    _build_broadcast_graph(broadcast_params_dict, broadcast_phase)


class _no_grad(contextlib.ContextDecorator):
    """
    Context Manager to disable gradient calculation. When enter this context, we will disable calculate
    gradient. When exit this context, we will resume its prev state.
    Currently, it can only use in Pynative mode. It also can be used as decorator.
    """

    def __init__(self):
        self.prev_state = False

    def __enter__(self):
        if context.get_context("mode") == context.GRAPH_MODE:
            raise RuntimeError("For no_grad feature, currently only support Pynative mode, but got Graph mode.")
        self.prev_state = _pynative_executor.enable_grad()
        _pynative_executor.set_enable_grad(False)

    def __exit__(self, exc_type, exc_val, exc_tb):
        _pynative_executor.set_enable_grad(self.prev_state)
        return False


class _PyNativeExecutor:
    """
    A pynative executor used to compile/manage/run single op.

    The main functions include: construct op graph, compile op graph, auto grad and run op graph.

    Args:
        obj (Object): The python network that will be run in pynative mode.
        args (Tuple(Tensor...)): The inputs of network in tuple form.

    Returns:
        gradients (Tuple(Tensor...)): The gradients of network parameters and inputs.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``
    """

    def __init__(self):
        self._executor = PyNativeExecutor_.get_instance()
        self._executor.set_py_exe_path(sys.executable)
        self._executor.set_kernel_build_server_dir(os.path.split(kernel_build_server.__file__)[0] + os.sep)

    @staticmethod
    def parameter_broadcast(obj, phase):
        """
        Run broadcast for parameter.

        Args:
            obj (Cell): The cell instance.
            phase (str): The phase of cell instance.

        Return:
            None.
        """
        if BROADCAST_PHASE not in phase and _get_parameter_broadcast():
            _parameter_broadcast(obj)

    def real_run_op(self, *args):
        """
        Run single op.

        Args:
            args (tuple): Op prim and input arguments.

        Return:
            Tensor, result of run op.
        """
        return self._executor.real_run_op(*args)

    def run_op_async(self, *args):
        """
        Run single op async.

        Args:
            args (tuple): Op prim and input arguments.

        Return:
            StubNode, result of run op.
        """
        return self._executor.run_op_async(*args)

    def new_graph(self, obj, *args, **kwargs):
        """
        Initialize resources for building forward and backward graph.

        Args:
            obj (Function/Cell): The function or cell instance.
            args (tuple): Function or cell input arguments.
            kwargs (dict): keyword arguments.

        Return:
            None.
        """
        self._executor.new_graph(obj, *args, *(kwargs.values()))

    def end_graph(self, obj, output, *args, **kwargs):
        """
        Clean resources after building forward and backward graph.

        Args:
            obj (Function/Cell): The function or cell instance.
            output (Tensor/tuple/list): Function or cell output object.
            args (tuple): Function or cell input arguments.
            kwargs (dict): keyword arguments.

        Return:
            None.
        """
        self._executor.end_graph(obj, output, *args, *(kwargs.values()))

    def check_run(self, grad, obj, weights, grad_hash_id, *args, **kwargs):
        """
        Whether the forward graph need to construct.

        Args:
            grad (GradOperation): The gradoperation object.
            obj (Function/Cell): The function or cell instance.
            grad_hash_id (tuple): The id of objects which contribute to cache of compiled graph in pynative mode.
            args (tuple): Function or cell input arguments.
            kwargs (dict): keyword arguments.

        Return:
            bool, specifies whether the forward graph need to construct.
        """
        return self._executor.check_run(grad, obj, weights, grad_hash_id, *args, *(kwargs.values()))

    def grad(self, obj, grad, weights, grad_position, *args, **kwargs):
        """
        Get grad graph.

        Args:
            obj (Function/Cell): The function or cell instance.
            grad (GradOperation): The gradoperation object.
            weights (ParameterTuple): The weights of cell instance.
            grad_position (Union(int, tuple[int])): If int, get the gradient with respect to single input.
              If tuple, get the gradients with respect to selected inputs. 'grad_position' begins with 0. Default: 0.
            args (tuple): Function or cell input arguments.
            kwargs (dict): keyword arguments.

        Return:
            None.
        """
        return self._executor.grad(grad, obj, weights, grad_position, *args, *(kwargs.values()))

    def clear_res(self):
        """
        Clean resource for _PyNativeExecutor.

        Return:
            None.
        """
        return self._executor.clear_res()

    def sync(self):
        """
        SyncStream.

        Return:
            None.
        """
        self._executor.sync()

    def grad_jit(self, output, *args):
        """
        Building grad graph decorated by jit.

        Args:
            output (tuple): The function or cell decorated by jit output object.
            args (tuple): Function or cell decorated by jit input arguments.

        Return:
            None.
        """
        return self._executor.grad_jit(output, *args)

    def grad_flag(self):
        """
        The flag of building grad graph.

        Return:
            bool, whether building grad graph.
        """
        return self._executor.grad_flag()

    def set_grad_flag(self, flag):
        """
        Set the flag of building grad graph.

        Args:
            flag (bool): Specifying whether building grad graph.

        Return:
            None.
        """
        self._executor.set_grad_flag(flag)

    def enable_grad(self):
        """
        The global flag whether needing to calculate gradient.

        Return:
            bool, whether needing to calculate gradient.
        """
        return self._executor.enable_grad()

    def set_enable_grad(self, flag):
        """
        Set the flag of calculating gradient.

        Args:
            flag (bool): Specifying whether calculating gradient.

        Return:
            None.
        """
        self._executor.set_enable_grad(flag)

    def set_jit_compile_status(self, status, phase):
        """
        Set jit is compiling

        Args:
            status(bool): jit compile status
            phase (str): The phase of cell/function instance.
        Return:
            None.
        """
        self._executor.set_jit_compile_status(status, phase)

    def set_dynamic_input(self, obj, *args):
        """
        Set dynamic shape tensor of input arguments.

        Args:
            obj (Function/Cell): The function or cell instance.
            args (tuple): Function or cell dynamic input arguments.

        Return:
            None.
        """
        self._executor.set_dynamic_input(obj, *args)

    def get_dynamic_input(self, *actual_args):
        """
        Get dynamic shape arguments according to actual input arguments.

        Args:
            actual_args(tuple): Actual input arguments of Function or Cell.

        Return:
            dynamic_shape_args(tuple): Dynamic shape arguments of Function or Cell.
        """
        return self._executor.get_dynamic_input(*actual_args)

    def is_first_cell(self):
        """
        The flag of first cell instance.

        Return:
            bool, specifies whether is the first cell.
        """

        return self._executor.is_first_cell()

    def set_hook_changed(self, cell):
        """
        The flag of registering or removing a hook function on Cell instance.

        Args:
            cell (Cell): The cell instance.

        Return:
            None.
        """
        self._executor.set_hook_changed(cell)

    def constant_folding(self, *args):
        """
        Get value by infer value.

        Args:
            args (tuple): Op prim and input arguments.

        Return:
            Tensor, the value get by op infer.
        """
        return self._executor.constant_folding(*args)


class _CellGraphExecutor:
    """
    An executor used to compile/manage/run graph for a Cell.

    Including data_graph, train_graph, eval_graph and predict graph.

    Args:
        obj (Function/Cell): The function or cell instance need compile.
        args (tuple): Function or cell input arguments.

    Returns:
        Graph, return the result of pipeline running.
    """

    def __init__(self):
        # create needed graph by lazy mode
        self.is_init = False
        self.enable_tuple_broaden = False
        self.obfuscate_config = None  # used for model's dynamic obfuscation
        self._graph_executor = GraphExecutor_.get_instance()
        self._graph_executor.set_py_exe_path(sys.executable)
        self._graph_executor.set_kernel_build_server_dir(os.path.split(kernel_build_server.__file__)[0] + os.sep)

    def init_dataset(self, queue_name, dataset_size, batch_size, dataset_types, dataset_shapes,
                     input_indexs, phase='dataset', need_run=True):
        """
        Initialization interface for calling data subgraph.

        Args:
            queue_name (str): The name of tdt queue on the device.
            dataset_size (int): The size of dataset.
            batch_size (int): The size of batch.
            dataset_types (list): The output types of element in dataset.
            dataset_shapes (list): The output shapes of element in dataset.
            input_indexs (list): The index of data with net.
            phase (str): The name of phase, e.g., train_dataset/eval_dataset. Default: 'dataset'.

        Returns:
            bool, specifies whether the data subgraph was initialized successfully.
        """
        if not init_exec_dataset(queue_name=queue_name,
                                 size=dataset_size,
                                 batch_size=batch_size,
                                 types=dataset_types,
                                 shapes=dataset_shapes,
                                 input_indexs=input_indexs,
                                 phase=phase,
                                 need_run=need_run):
            raise RuntimeError("Failure to init and dataset subgraph!")
        self._graph_executor.set_queue_name(queue_name)
        return True

    def set_queue_name(self, queue_name):
        """
        while a mode use shared dataset with others, need set queue_name which saved in data_set
        :param queue_name:
        :return:
        """
        self._graph_executor.set_queue_name(queue_name)

    def get_queue_name(self, dataset_phase):
        """
        Get cached queue name for the graph loaded from compile cache.
        :return: cached queue name
        """
        return self._graph_executor.get_queue_name(dataset_phase)

    @staticmethod
    def _set_dataset_mode(obj):
        """set dataset mode."""
        # decide whether to sink based on the sink_mode flag which is set in connect_network_with_dataset
        if 'sink_mode' in obj.get_flags().keys() and obj.get_flags()['sink_mode'] is True:
            _set_dataset_mode_config('sink')
        else:
            _set_dataset_mode_config('normal')

    @staticmethod
    def _use_vm_mode():
        enable_ge = context.get_context("enable_ge")
        enable_debug_runtime = context.get_context("enable_debug_runtime")
        exe_mode = context.get_context("mode") == context.PYNATIVE_MODE
        return not enable_ge or (enable_debug_runtime and exe_mode)

    def _build_data_graph(self, obj, phase):
        self._graph_executor.build_data_graph(obj.parameters_dict(), phase)

    def _set_compile_cache_dep_files(self, phase):
        # If enable compile cache, get the dependency files list
        enable_compile_cache = context.get_context("enable_compile_cache")
        if enable_compile_cache is not True and enable_compile_cache != "1":
            enable_compile_cache = os.getenv('MS_COMPILER_CACHE_ENABLE')
        if "train" in phase and (enable_compile_cache is True or enable_compile_cache == "1"):
            self._graph_executor.set_compile_cache_dep_files(_get_compile_cache_dep_files())

    def compile(self, obj, *args, phase='predict', do_convert=True, jit_config_dict=None, **kwargs):
        """
        Compiles graph.

        Args:
            obj (Function/Cell): The function or cell instance need compile.
            phase (str): The name of compile phase. Default: 'predict'.
            do_convert (bool): When set to True, convert ME graph to GE graph after compiling graph.
            jit_config_dict (dict): Jit config for compile. Default: ``None``.
            args (tuple): Args of the Cell object.
            kwargs (dict): Kwargs of the Cell object.

        Return:
            Str, the full phase of the cell.
            Bool, if the graph has been compiled before, return False, else return True.
        """
        obj.__parse_method__ = 'construct'
        if not hasattr(obj, obj.__parse_method__):
            raise AttributeError(
                'The class {} dose not have method {}'.format(obj.__class__.__name__, obj.__parse_method__))
        key_id = str(id(obj)) + str(obj.create_time)
        args = get_auto_dynamic_shape_args(args, key_id)

        self.enable_tuple_broaden = False
        if hasattr(obj, "enable_tuple_broaden"):
            self.enable_tuple_broaden = obj.enable_tuple_broaden
        logger.debug(f"Convert the network: {do_convert}.")
        self._graph_executor.set_enable_tuple_broaden(self.enable_tuple_broaden)
        key = self._graph_executor.generate_arguments_key(obj, args, kwargs, self.enable_tuple_broaden)
        obj.arguments_key = str(key)
        raw_phase = phase
        phase = phase + '.' + str(obj.create_time) + '.' + str(id(obj)) + '.' + obj.arguments_key
        obj.phase_cache[raw_phase] = phase
        update_auto_dynamic_shape_phase(args, key_id, phase)

        if phase in obj.compile_cache and self.has_compiled(phase):
            logger.debug("%r graph has existed.", phase)
            # Release resource should be released when CompileInner won't be executed, such as cur_convert_input_
            # generated in generate_arguments_key.
            self._graph_executor.clear_compile_arguments_resource()
            return phase, False

        obj.check_names()
        _check_full_batch()
        self._set_dataset_mode(obj)
        self._set_compile_cache_dep_files(phase)

        self._graph_executor.set_weights_values(obj.parameters_dict())
        if jit_config_dict:
            self._graph_executor.set_jit_config(jit_config_dict)
        else:
            jit_config_dict = JitConfig().jit_config_dict
            self._graph_executor.set_jit_config(jit_config_dict)
        result = self._graph_executor.compile(obj, args, kwargs, phase, self._use_vm_mode())
        obj.compile_cache.add(phase)
        if not result:
            raise RuntimeError("Executor compile failed.")
        graph = self._graph_executor.get_func_graph(phase)

        if graph is None:
            raise RuntimeError("Compile graph failed for phase {}.".format(phase))

        auto_parallel_mode = _is_in_auto_parallel_mode()
        if not auto_parallel_mode:
            replace = obj.init_parameters_data(auto_parallel_mode=auto_parallel_mode)
            self._update_param_node_default_input(phase, replace)
        elif 'skip_auto_parallel_compile' not in obj.get_flags().keys():
            obj.parameter_layout_dict = self._graph_executor.get_parameter_layout(phase)
            obj.parallel_parameter_name_list = self._graph_executor.get_parallel_parameter_name_list(phase)

        if "export.air" in phase:
            self._build_data_graph(obj, phase)
        elif BROADCAST_PHASE not in phase and _get_parameter_broadcast():
            _parameter_broadcast(obj)
        return phase, True

    def _update_param_node_default_input(self, phase, replace):
        new_param = {x.name: replace[x] for x in replace if id(x) != id(replace[x])}
        return self._graph_executor.updata_param_node_default_input(phase, new_param)

    def _get_shard_strategy(self, obj):
        real_phase = obj.phase + '.' + str(obj.create_time) + '.' + str(id(obj)) + '.' + obj.arguments_key
        return self._graph_executor.get_strategy(real_phase)

    def _get_num_parallel_ops(self, obj):
        real_phase = obj.phase + '.' + str(obj.create_time) + '.' + str(id(obj)) + '.' + obj.arguments_key
        return self._graph_executor.get_num_parallel_ops(real_phase)

    def _get_allreduce_fusion(self, obj):
        real_phase = obj.phase + '.' + str(obj.create_time) + '.' + str(id(obj)) + '.' + obj.arguments_key
        return self._graph_executor.get_allreduce_fusion(real_phase)

    def __call__(self, obj, *args, phase='predict'):
        if context.get_context("precompile_only") or _is_role_sched():
            return None
        return self.run(obj, *args, phase=phase)

    def has_compiled(self, phase='predict'):
        """
        Specify whether have been compiled.

        Args:
            phase (str): The phase name. Default: 'predict'.

        Returns:
            bool, specifies whether the specific graph has been compiled.
        """
        return self._graph_executor.has_compiled(phase)

    @_wrap_func
    def _exec_pip(self, obj, *args, phase=''):
        """Execute the generated pipeline."""
        fn = obj.construct
        obj.__parse_method__ = fn.__name__
        return self._graph_executor(args, phase)

    def run(self, obj, *args, phase='predict'):
        """
        Run the specific graph.

        Args:
            obj (Cell): The cell object.
            args (tuple): Args of the Cell object.
            phase (str): The phase name. Default: 'predict'.

        Returns:
            Tensor/Tuple, return execute result.
        """
        if phase == 'save':
            exe_phase = phase + '.' + str(obj.create_time) + '.' + str(id(obj)) + '.' + obj.arguments_key
            return self._graph_executor((), exe_phase)

        phase_real = phase + '.' + str(obj.create_time) + '.' + str(id(obj)) + '.' + obj.arguments_key
        if self.has_compiled(phase_real):
            return self._exec_pip(obj, *args, phase=phase_real)
        raise KeyError('{} graph is not exist.'.format(phase_real))

    def del_net_res(self, obj, net_id):
        """Clear the memory resource of a network."""
        self._graph_executor.del_net_res(obj, net_id)

    def _get_branch_control_input(self):
        if ('obf_ratio' not in self.obfuscate_config.keys()) or (
                'obf_random_seed' not in self.obfuscate_config.keys()):
            raise ValueError("'obf_ratio' and 'obf_random_seed' must be in obfuscate_config.")
        obf_random_seed = self.obfuscate_config.get('obf_random_seed')
        if obf_random_seed == 0:
            branch_control_input = 0
        else:
            branch_control_input = _generate_branch_control_input(obf_random_seed)
        return branch_control_input

    def _get_func_graph(self, obj, exec_id, use_prefix=False):
        """Get func graph from pipeline."""
        if use_prefix:
            exec_id = exec_id + '.' + obj.arguments_key
        if self._graph_executor.has_compiled(exec_id) is False:
            return None
        if self.obfuscate_config is not None:
            raise ValueError('For get func graph, obfuscate_config is currently not supported now.')
        return self._graph_executor.get_func_graph(exec_id)

    def _get_func_graph_proto(self, obj, exec_id, ir_type="onnx_ir", use_prefix=False, incremental=False):
        """Get graph proto from pipeline."""
        if use_prefix:
            exec_id = exec_id + '.' + obj.arguments_key
        if self._graph_executor.has_compiled(exec_id) is False:
            return None
        if self.obfuscate_config is not None:
            branch_control_input = self._get_branch_control_input()
            return self._graph_executor.get_obfuscate_func_graph_proto(exec_id, incremental,
                                                                       self.obfuscate_config['obf_ratio'],
                                                                       branch_control_input)
        return self._graph_executor.get_func_graph_proto(exec_id, ir_type, incremental)

    def get_optimize_graph_proto(self, obj):
        """Return optimize graph binary proto."""
        exec_id = obj.phase + "." + str(obj.create_time) + '.' + str(id(obj)) + '.' + obj.arguments_key
        if self._graph_executor.has_compiled(exec_id) is False:
            return None
        graph_proto = self._graph_executor.get_optimize_graph_proto(exec_id)
        if isinstance(graph_proto, str) and graph_proto == "":
            logger.warning("Can not get optimize graph proto. Instead, try to find function graph.")
            graph_proto = obj.get_func_graph_proto()
        return graph_proto

    def export(self, file_name, graph_id, enc_key=None, encrypt_func=None):
        """
        Export graph.

        Args:
            file_name (str): File name of model to export
            graph_id (str): id of graph to be exported
        """
        self._graph_executor.export_graph(file_name, graph_id, encrypt_func, enc_key)


def ms_memory_recycle():
    """
    Recycle memory used by MindSpore.
    When train multi Neural network models in one process, memory used by MindSpore is very large,
    this is because MindSpore cached runtime memory for every model.
    To recycle these cached memory, users can call this function after training of one model.

    Examples:
        >>> import mindspore as ms
        >>> ms.ms_memory_recycle()
    """
    if ms_compile_cache:
        _cell_graph_executor.del_net_res(None, ms_compile_cache)
        ms_compile_cache.clear()
    for cell_cache in cells_compile_cache.values():
        if cell_cache:
            _cell_graph_executor.del_net_res(None, cell_cache)
            cell_cache.clear()
    _ms_memory_recycle()


def _generate_branch_control_input(obf_random_seed):
    """Generate append network input for dynamic obfuscation in random seed mode."""
    seed_max = 2 ** 32 - 1
    int_max = 2 ** 31 - 1
    np.random.seed(obf_random_seed % seed_max)
    # generate a string as hash function inputs
    word_repo = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" + "abcdefghigklmnopqrstuvwxyz" + "0123456789"
    repo_len = len(word_repo)
    sha_string = ''
    string_len = 1024 * 1024
    for _ in range(string_len):
        rand_index = np.random.randint(0, repo_len)
        sha_string += word_repo[rand_index]
    # get hash result
    sha_result = hashlib.sha256(sha_string.encode('utf-8')).hexdigest()  # len is 64
    branch_control_input = 1
    hex_base = 16
    for item in sha_result:
        if int(item, hex_base) > 0:
            branch_control_input *= int(item, hex_base)
    branch_control_input %= int_max
    return branch_control_input


def _bind_device_context():
    """Bind device context to current thread"""
    _bind_device_ctx()


_cell_graph_executor = _CellGraphExecutor()
_pynative_executor = _PyNativeExecutor()

__all__ = ['ms_function', 'ms_memory_recycle', 'ms_class', 'jit', 'jit_class']
