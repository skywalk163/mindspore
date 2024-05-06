# Copyright 2020-2024 Huawei Technologies Co., Ltd
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

"""primitive"""
import functools
import inspect
import copy
import numpy as np
from mindspore.common.api import _wrap_func
from mindspore.log import _LogActionOnce
from mindspore import context, log as logger
from mindspore.parallel._utils import _is_in_auto_parallel_mode, _is_in_data_parallel_mode, _is_in_hybrid_parallel_mode
from mindspore.parallel._ps_context import _is_ps_mode, _is_role_sched
from mindspore.parallel.shard import Layout
from mindspore.common.api import _pynative_executor
from mindspore.common._stub_tensor import _convert_stub
from mindspore._c_expression import Primitive_, PrimitiveFunction_, prim_type, typing
from mindspore import _checkparam as Validator
from mindspore.ops import signature as sig


class Primitive(Primitive_):
    """
    Primitive is the base class of operator primitives in python.

    Args:
        name (str): Name for the current Primitive.

    Examples:
        >>> from mindspore.ops import prim_attr_register, Primitive
        >>> add = Primitive('add')
        >>>
        >>> # or work with prim_attr_register:
        >>> # init a Primitive class with attr1 and attr2
        >>> class Add(Primitive):
        ...     @prim_attr_register
        ...     def __init__(self, attr1, attr2):
        ...         '''init for add'''
        ...     # check attr1 and attr2 or do some initializations
        ...     # init a Primitive obj with attr1=1 and attr2=2
        >>> add = Add(attr1=1, attr2=2)
    """
    _repr_ignore_list = ['input_names', 'output_names']

    def __init__(self, name):
        self.name = name
        self.attrs = {}
        self.init_attrs = {"name": name}
        self._update_parameter = False
        Primitive_.__init__(self, name)
        if hasattr(self.__class__, '__mindspore_signature__'):
            out = self._fill_signature(self.__class__.__mindspore_signature__)
            self.set_signatures(out)

    def add_prim_attr(self, name, value):
        """
        Add primitive attribute.

        Args:
            name (str): Attribute Name.
            value (Any): Attribute value.

        Examples:
            >>> import mindspore.ops as ops
            >>> a = ops.Add()
            >>> a = a.add_prim_attr("attr",1)
            >>> out = a.attrs["attr"]
            >>> print(out)
            1
        """
        self.__dict__[name] = value
        self.attrs[name] = value
        self.add_attr(name, value)
        return self

    def _set_prim_arg(self, name, value):
        """
        Set primitive initialization arguments.

        Different from add_prim_attr, it is used internally to store Primitive
        initialization arguments in Python.
        """
        self.__dict__[name] = value
        self.attrs[name] = value
        return self

    def _set_prim_arg_with_handler(self, name, value, arg_handler):
        """
        Set primitive initialization arguments and with arg_handler.
        """
        value = value if value is None else arg_handler(self.__class__.__name__, name, value)
        return self._set_prim_arg(name, value)

    def set_device(self, device_target):
        """
        Set primitive been executed device.

        Args:
            device_target (str): The target device to run, support "Ascend", "GPU", and "CPU".

        Examples:
            >>> import mindspore.ops as ops
            >>> a = ops.Add()
            >>> a = a.set_device("GPU")
            >>> print(a.primitive_target)
            GPU
        """
        return self.add_prim_attr("primitive_target", device_target)

    def _fill_signature(self, signatures):
        """fills signature."""
        signatures_new = []
        for signature in signatures:
            if isinstance(signature, sig.Signature):
                signatures_new.append(signature)
            elif isinstance(signature, sig.sig_dtype):
                signatures_new.append(sig.make_sig(dtype=signature))
            else:
                if len(signature) < 3:
                    raise ValueError(f"[Internal Error]Signature for one parameter len must > 3, but {signature}")
                signatures_new.append(sig.make_sig(*signature))
        return tuple(signatures_new)

    def _clone(self):
        """
        Deeply clones the primitive object.

        Calls the __init__() method with the same arguments. This method is called in parser if the
        flag self.__setattr_flag__ is True.
        """
        cloned = copy.deepcopy(self)
        init_params = list()
        if hasattr(cloned.__init__, 'decorated_func'):
            init_params = inspect.getfullargspec(cloned.__init__.decorated_func).args[1:]
        init_args = self.init_attrs
        for name in init_params:
            value = self.attrs[name]
            init_args[name] = value
        # __init__ should be called to construct cpp object.
        cloned.__init__(**init_args)
        for name in self.attrs:
            value = self.attrs[name]
            cloned.add_prim_attr(name, value)
        if hasattr(self, 'instance_name'):
            cloned.set_prim_instance_name(self.instance_name)
        return cloned

    def _check_shard_strategy(self, strategy, log_info):
        """Check shard strategy is validate or not"""
        is_layout = []
        if not isinstance(strategy, tuple):
            raise TypeError(f'{log_info} must be tuple type, but got:{type(strategy)}')
        for in_ele in strategy:
            if not isinstance(in_ele, tuple) and not isinstance(in_ele, Layout):
                raise TypeError(f'The element of strategy must be tuple/Layout type, but got:{type(in_ele)}')
            if isinstance(in_ele, tuple):
                for in_value in in_ele:
                    if not isinstance(in_value, int):
                        raise TypeError(f'The {log_info}: {strategy} of {self.name} is not valid,'
                                        f' the value of strategy must be int type, but got:{type(in_value)}')
                is_layout.append(False)
                continue
            is_layout.append(True)
        if not is_layout:
            np_is_layout = np.array(is_layout)
            if not (np_is_layout == np_is_layout[0]).all():
                raise TypeError(f'{log_info} item must be all tuple type or all Layout type.')
        return np.array(is_layout)

    def _extract_layout_value(self, layout, log_info):
        """Extract parallel layout value"""
        layout_value = None
        if layout is not None:
            if not isinstance(layout, tuple):
                raise TypeError(f'{log_info} must be tuple type, but got:{type(layout)}')
            layout_value = ()
            for in_ele in layout:
                if not isinstance(in_ele, Layout):
                    raise TypeError(f"The {log_info} item should be a object of class Layout.")
                layout_value += (in_ele.to_dict(),)
        return layout_value

    def _check_shard_strategy_in_out_match(self, in_strategy, out_strategy):
        """Check shard in_strategy and out_strategy"""
        if in_strategy is None and out_strategy is not None:
            raise ValueError(f'The out_strategy of {self.name} is {out_strategy}, need to set in_strategy,'
                             f' but got none')
        if not _is_in_auto_parallel_mode():
            mode = context.get_auto_parallel_context("parallel_mode")
            if in_strategy is not None:
                logger.warning(f"The in_strategy/in_layout of the operator in your network "
                               f"will not take effect in {mode} mode. "
                               f"This means the the shard function called in the network is ignored. \n"
                               f"If you want to enable it, please use semi auto or auto parallel mode by "
                               f"context.set_auto_parallel_context(parallel_mode=ParallelMode.SEMI_AUTO_PARALLEL "
                               f"or context.set_auto_parallel_context(parallel_mode=ParallelMode.AUTO_PARALLEL)")
            if out_strategy is not None:
                logger.warning(f"The out_strategy/out_layout of the operator in your network "
                               f"will not take effect in {mode} mode."
                               f" This means the the shard function called in the network is ignored. \n"
                               f"If you want to enable it, please use semi auto or auto parallel mode by "
                               f"context.set_auto_parallel_context(parallel_mode=ParallelMode.SEMI_AUTO_PARALLEL "
                               f"or context.set_auto_parallel_context(parallel_mode=ParallelMode.AUTO_PARALLEL)")

    def del_prim_attr(self, name):
        """
        Delete primitive attribute.

        Args:
            name (str): Attribute Name.
        Examples:
            >>> import mindspore.ops as ops
            >>> a = ops.Add()
            >>> a = a.add_prim_attr("attr",1)
            >>> a = a.del_prim_attr("attr")
            >>> print(a.attrs)
            {}
        """
        if name in self.__dict__ and name in self.attrs:
            del self.__dict__[name]
            del self.attrs[name]
            self.del_attr(name)
        return self

    def set_stage(self, stage):
        """
        Add stage id to primitive attribute.

        Note:
            It is valid only in semi auto parallel.
            In other parallel modes, please set it to be 0.
        Args:
            stage (int): The stage id for the current operation.
        Examples:
            >>> from mindspore import ops
            >>> add = ops.Add()
            >>> print(add.set_stage(0))
            Prim[Add]<stage=0>
        """
        self.add_prim_attr("stage", stage)
        return self

    @_LogActionOnce(logger=logger, key='Primitive')
    def shard(self, in_strategy=None, out_strategy=None):
        """
        Add strategies to primitive attribute.

        Note:
            It is valid only in semi auto parallel or auto parallel mode.
            In other parallel modes, strategies set here will be ignored.

        Args:
            in_strategy (tuple): Describe the split strategy of operator input. Default: ``None`` .
            out_strategy (tuple): Describe the split strategy of operator output, it is only for certain operators,
                                  such as MatMul. Default: ``None`` .

        Examples:
            >>> from mindspore import ops
            >>> add = ops.Add()
            >>> print(add.shard(((1, 1), (1, 1))))
            Prim[Add]<in_strategy=((1, 1), (1, 1)), out_strategy=None>
            >>> # using layout
            >>> from mindspore import Layout
            >>> layout = Layout((2, 2, 2), ("dp", "sp", "mp"))
            >>> layout_tuple = (layout("dp", "sp"), layout("sp", "mp"))
            >>> from mindspore import ops
            >>> matmul = ops.MatMul()
            >>> print(matmul.shard(layout_tuple))
            Prim[MatMul]<in_layout=({'device_matrix': (2, 2, 2), 'tensor_map': (2, 1)},
            {'device_matrix': (2, 2, 2), 'tensor_map': (1, 0)})>
            >>> # using layout with None
            >>> from mindspore import Layout
            >>> layout = Layout((2, 2, 2), ("dp", "sp", "mp"))
            >>> layout_tuple = (layout("dp", "sp"), layout("sp", "None")) # "None" means the axis would not be split
            >>> from mindspore import ops
            >>> matmul = ops.MatMul()
            >>> print(matmul.shard(layout_tuple))
            Prim[MatMul]<in_layout=({'device_matrix': (2, 2, 2), 'tensor_map': (2, 1)},
            {'device_matrix': (2, 2, 2), 'tensor_map': (1, -1)})>
        """
        in_is_layout = None
        out_is_layout = None
        if in_strategy is not None:
            in_is_layout = self._check_shard_strategy(in_strategy, "in_strategy")

        if out_strategy is not None:
            out_is_layout = self._check_shard_strategy(out_strategy, "out_strategy")
        self._check_shard_strategy_in_out_match(in_strategy, out_strategy)
        if in_is_layout is not None and out_is_layout is not None and in_is_layout[0] != out_is_layout[0]:
            raise ValueError(f'The in_strategy type must equal to the out_strategy type, '
                             f'one using tuple(tuple) and the other using tuple(Layout) is not allowed.')
        in_layout_value = None
        out_layout_value = None
        if in_is_layout is not None and in_is_layout[0]:
            in_layout_value = self._extract_layout_value(in_strategy, "in_strategy")
        if out_is_layout is not None and out_is_layout[0]:
            out_layout_value = self._extract_layout_value(out_strategy, "out_strategy")


        if in_is_layout is not None and not in_is_layout[0]:
            self.add_prim_attr("in_strategy", in_strategy)
        if out_is_layout is not None and not out_is_layout[0]:
            self.add_prim_attr("out_strategy", out_strategy)
        if in_layout_value:
            self.add_prim_attr("in_layout", in_layout_value)
        if out_layout_value:
            self.add_prim_attr("out_layout", out_layout_value)
        return self

    def set_prim_instance_name(self, instance_name):
        """
        Set instance name to primitive operator.

        Note:
            It will be called by default when user defines primitive operator.

        Args:
            instance_name (str): Instance name of primitive operator set by user.

        Examples:
            >>> import mindspore.ops as ops
            >>> a = ops.Add()
            >>> a = a.set_prim_instance_name("add")
            >>> print(a.instance_name)
            add
        """
        self.set_instance_name(instance_name)
        self.instance_name = instance_name
        return self

    def __getattr__(self, item):
        if item == 'infer_dynamic_shape':
            return None
        if item in super().get_attr_dict():
            return super().get_attr_dict()[item]
        if item in self.attrs:
            return self.attrs[item]
        err_msg = "'{prim}' object has no attribute '{attr}'".format(prim=self.name, attr=item)
        raise AttributeError(err_msg)

    def check_elim(self, *args):
        """
        Check if the primitive can be eliminated. Subclass in need should override this method.

        Args:
            args(Primitive args): Same as arguments of current Primitive.

        Returns:
            A tuple consisting of two elements.
            The first element means if the primitive can be calculated in compiling stage,
            the second element is calculated result.

        Examples:
            >>> import numpy as np
            >>> import mindspore
            >>> from mindspore import Tensor
            >>> from mindspore.ops import prim_attr_register, Primitive
            >>> class AddN(Primitive):
            ...     @prim_attr_register
            ...     def __init__(self):
            ...         self.init_prim_io_names(inputs=["inputs"], outputs=["sum"])
            ...     def check_elim(self, inputs):
            ...         if len(inputs) != 1:
            ...             return (False, None)
            ...         if isinstance(inputs[0], Tensor):
            ...             return (True, inputs[0])
            ...
            >>> addn = AddN()
            >>> input_x = Tensor(np.array([1, 2, 3]), mindspore.float32)
            >>> output = addn.check_elim((input_x,))
            >>> print(output)
            (True, Tensor(shape=[3], dtype=Float32, value= [ 1.00000000e+00,  2.00000000e+00,  3.00000000e+00]))
    """
        return (False, None)

    def __call__(self, *args):
        should_elim, output = self.check_elim(*args)
        if should_elim:
            return output
        return _run_op(self, self.name, args)

    def __getstate__(self):
        return self.__dict__

    def __setstate__(self, d):
        self.__dict__.update(d)

    def __deepcopy__(self, memo):
        return type(self)(**self.init_attrs)

    def __repr__(self):
        attr = ', '.join([f'{k}={self.attrs.get(k)}' for k in self.attrs if k not in Primitive._repr_ignore_list])
        info_str = f'Prim[{self.name}]'
        if attr:
            info_str += f'<{attr}>'
        return info_str

    def init_prim_io_names(self, inputs, outputs):
        """
        Initialize the name of inputs and outputs of Tensor or attributes.

        Args:
            inputs (list[str]): list of inputs names.
            outputs (list[str]): list of outputs names.
        Examples:
            >>> import mindspore.ops as ops
            >>> a = ops.Add()
            >>> a.init_prim_io_names(["x","y"],["sum"])
            >>> print(a.input_names)
            ['x','y']
            >>> print(a.output_names)
            ['sum']
        """
        # for checking para names with kernel implementation
        self.add_prim_attr("input_names", inputs)
        # for checking output number with kernel implementation
        self.add_prim_attr("output_names", outputs)

    @property
    def update_parameter(self):
        """Return whether the primitive will update the value of parameter."""
        return self._update_parameter

    def recompute(self, mode=True):
        """
        Set the primitive recomputed. If a primitive set recomputed feeds into some backward nodes
        for computing gradient, rather than storing the intermediate activation computed in forward
        pass, we will recompute it in backward pass.

        Note:

            - If the computation involves something like randomization or global variable, the equivalence
              is not guaranteed currently.
            - Not supported in pynative mode

        Args:
            mode (bool): Specifies whether the primitive is recomputed. Default: ``True`` .

        Examples:
            >>> import numpy as np
            >>> import mindspore as ms
            >>> from mindspore import Tensor, ops, nn
            >>> class NetRecompute(nn.Cell):
            ...     def __init__(self):
            ...         super(NetRecompute,self).__init__()
            ...         self.relu = ops.ReLU().recompute()
            ...         self.sqrt = ops.Sqrt()
            ...     def construct(self, x):
            ...         out = self.relu(x)
            ...         return self.sqrt(out)
            ...
            >>> class GradNet(nn.Cell):
            ...     def __init__(self, network):
            ...         super(GradNet,self).__init__()
            ...         self.network = network
            ...         self.grad = ops.GradOperation()
            ...     def construct(self, x):
            ...         g_out = self.grad(self.network)(x)
            ...         return g_out
            ...
            >>> x = Tensor(np.array([-1,1]).astype(np.float32))
            >>> net = NetRecompute()
            >>> grad = GradNet(net)
            >>> a = grad(x)
            >>> print(a)
            [0. 0.5]
        """
        if context.get_context("mode") == context.PYNATIVE_MODE:
            raise TypeError("Recompute is not supported in pynative mode currently.")
        Validator.check_bool(mode)
        self.add_prim_attr("recompute", mode)
        return self

    def place(self, role, rank_id):
        """
        Set the label for this primitive.
        This label tells MindSpore compiler on which process this operator should be launched.
        And each process's identical label consists of input 'role' and 'rank_id'.
        So by setting different operators with different labels,
        which will be launched on different processes, users can launch a distributed training job.

        Note:
            - This method is effective only after
              "mindspore.communication.init()" is called for dynamic cluster building.

        Args:
            role (str): The role of the process on which this operator will be launched.
                        Only 'MS_WORKER' is supported for now.
            rank_id (int): The rank id of the process on which this operator will be launched.
                           The rank_id is unique in processes with the same role.

        Examples:
            >>> from mindspore import context
            >>> import mindspore.ops as ops
            >>> context.set_context(mode=context.GRAPH_MODE)
            >>> matmul = ops.MatMul()
            >>> matmul.place('MS_WORKER', 0)
        """
        if _is_role_sched():
            return

        Validator.check_non_negative_int(rank_id, "rank_id", "Primitive.place")
        Validator.check_string(role, "MS_WORKER", "role", "Primitive.place")

        if context.get_context("mode") == context.PYNATIVE_MODE:
            raise RuntimeError("You are calling Primitive.place in pynative mode."
                               "It's only supported in graph mode. Please switch to graph mode.")

        # Get the execution context and check whether calling of this 'place' method is valid.
        # This is because placing operators to arbitrary processes while other distributed training mode
        # is enabled is very unpredictable and may cause fatal error.
        # Some of these cases are under development and others should not be supported.
        if _is_ps_mode():
            raise RuntimeError(
                "You are calling Primitive.place mixed with Parameter Server training. "
                "This case is not supported yet. "
                "Please call Primitive.place without Parameter Server training.")
        if _is_in_auto_parallel_mode() or _is_in_data_parallel_mode() or _is_in_hybrid_parallel_mode():
            raise RuntimeError(
                "You are calling Primitive.place mixed with other parallel features: "
                "'auto_parallel', 'data_parallel' and 'hybrid_parallel'. "
                "This case is still under development and not supported yet. "
                "Please call Primitive.place without these features.")
        self.add_prim_attr("ms_role", role)
        self.add_prim_attr("rank_id", rank_id)


class PrimitiveWithCheck(Primitive):
    """
    PrimitiveWithCheck is the base class of primitives in python, which defines functions to check the input arguments
    of operators, but uses the infer method registered in c++ source codes.

    There are three methods can be overridden to define the check logic of the primitive: __check__(), check_shape(),
    check_dtype(). If __check__() is defined in primitive, the __check__() has the highest priority to be called.
    If __check__() is not defined, check_shape() and check_dtype() can be defined to describe the check logic of
    the shape and type. Method infer_value() can also be defined (such as PrimitiveWithInfer) for constant propagation.

    More on how to customize a Op, please refer to `Custom Operators
    <https://www.mindspore.cn/tutorials/experts/en/master/operation/op_custom.html>`_.

    Args:
        name (str): Name of the current Primitive.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import dtype as mstype
        >>> from mindspore.ops import prim_attr_register, PrimitiveWithCheck
        >>> # init a Primitive class with check
        >>> class Flatten(PrimitiveWithCheck):
        ...     @prim_attr_register
        ...     def __init__(self):
        ...         pass
        ...     def check_shape(self, input_x):
        ...         Validator.check_int(len(input_x), 1, validator.GE, 'input_x rank', self.name)
        ...
        ...     def check_dtype(self, input_x):
        ...         Validator.check_subclass("input_x", input_x, mstype.tensor_type, self.name)
        ...
        >>> # init a Primitive obj
        >>> add = Flatten()
    """

    def __init__(self, name):
        Primitive.__init__(self, name)
        self.set_prim_type(prim_type.py_infer_check)

    def __check__(self, *args):
        """Checking the input shape and the input type of ops is valid """
        check_dtype_fn = getattr(self, 'check_dtype')
        check_dtype_fn(*(x['dtype'] for x in args))

        is_shape_known = True
        for x in args:
            shape = x['shape']
            if shape is None or -1 in shape or -2 in shape:
                is_shape_known = False
                break
        if is_shape_known:
            check_shape_fn = getattr(self, 'check_shape')
            check_shape_fn(*(x['shape'] for x in args))

    def _clone(self):
        """
        Deeply clones the primitive object.

        Calls the __init__() method with the same arguments. This method is called in parser if the
        flag self.__setattr_flag__ is True.
        """
        cloned_prim = Primitive._clone(self)
        return cloned_prim

    def check_shape(self, *args):
        """
        Check shapes of input args.

        Note:
            The shape of scalar is an empty tuple.

        Args:
            args (tuple(int)): shapes of input tensors.

        Return:
            None.
        """
        return None

    def check_dtype(self, *args):
        """
        Check data types of input args.

        Args:
            args (:class:`mindspore.dtype`): data type of inputs.

        Return:
            None.
        """
        return None


class PrimitiveWithInfer(Primitive):
    """
    PrimitiveWithInfer is the base class of primitives in python and defines functions for tracking inference
    in python.

    There are four method can be overridden to define the infer logic of the primitive: __infer__(), infer_shape(),
    infer_dtype(), and infer_value(). If __infer__() is defined in primitive, the __infer__() has the highest priority
    to be called. If __infer__() is not defined, infer_shape() and infer_dtype() can be defined to describe the infer
    logic of the shape and type. The infer_value() is used for constant propagation.

    More on how to customize a Op, please refer to `Custom Operators
    <https://www.mindspore.cn/tutorials/experts/en/master/operation/op_custom.html>`_.

    Args:
        name (str): Name of the current Primitive.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore.ops import prim_attr_register, PrimitiveWithInfer
        >>> # init a Primitive class with infer
        >>> class Add(PrimitiveWithInfer):
        ...     @prim_attr_register
        ...     def __init__(self):
        ...         pass
        ...
        ...     def infer_shape(self, x, y):
        ...         return x # output shape same as first input 'x'
        ...
        ...     def infer_dtype(self, x, y):
        ...         return x # output type same as first input 'x'
        ...
        >>> # init a Primitive obj
        >>> add = Add()
    """

    def __init__(self, name):
        Primitive.__init__(self, name)
        self.set_prim_type(prim_type.py_infer_shape)

    def _clone(self):
        """
        Deeply clones the primitive object.

        Calls the __init__() method with the same arguments. This method is called in parser if the
        flag self.__setattr_flag__ is True.
        """
        cloned_prim = Primitive._clone(self)
        return cloned_prim

    def infer_shape(self, *args):
        """
        Infer output shape based on input shape.

        Note:
            The shape of scalar is an empty tuple.

        Args:
            args (tuple(int)): shapes of input tensors.

        Return:
            `tuple(int)`, shapes of output tensors.
        """
        return None

    def infer_dtype(self, *args):
        """
        Infer output dtype based on input dtype.

        Args:
            args (:class:`mindspore.dtype`): data type of inputs.

        Return:
            :class:`mindspore.dtype`, data type of outputs.
        """
        return None

    def infer_value(self, *args):
        """
        Infer output value based on input value at compile time.

        Args:
            args (Any): value of inputs.

        Return:
            Value of outputs. Return `None`, the value can not be inferred at compile time in this case.
        """
        return None

    def __infer__(self, *args):
        """Infer shape, type, and value at the same time by using dictionary as arguments."""
        tracks = ['dtype', 'shape', 'value']
        out = {}
        for track in tracks:
            fn = getattr(self, 'infer_' + track)
            # fn may return None
            out[track] = fn(*(x[track] for x in args))

        return out


def prim_attr_register(fn):
    """
    Primitive attributes register.

    Register the decorator of the built-in operator primitive '__init__'.
    The function will add all the parameters of '__init__' as operator attributes ,
    and init primitive name.

    Args:
        fn (function): __init__ function of primitive.

    Returns:
        function, original function.

    Examples:
        >>> from mindspore.ops import prim_attr_register, PrimitiveWithCheck
        >>> class MatMul(PrimitiveWithCheck):
        ...     @prim_attr_register
        ...     def __init__(self, transpose_a=False, transpose_b=False):
        ...         self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['output'])
        ...
        >>> # init a Primitive obj
        >>> matmul = MatMul()
    """

    @functools.wraps(fn)
    def deco(self, *args, **kwargs):
        class_name = self.__class__.__name__
        if hasattr(self.__class__, "substitute_name"):
            class_name = self.__class__.substitute_name
        if isinstance(self, PrimitiveWithInfer):
            PrimitiveWithInfer.__init__(self, class_name)
        elif isinstance(self, PrimitiveWithCheck):
            PrimitiveWithCheck.__init__(self, class_name)
        else:
            Primitive.__init__(self, class_name)
        bound_args = inspect.signature(fn).bind(self, *args, **kwargs)
        bound_args.apply_defaults()
        arguments = bound_args.arguments
        del arguments['self']
        del self.init_attrs['name']
        for name in arguments:
            value = arguments[name]
            self.add_prim_attr(name, value)
            self.init_attrs[name] = value
        fn(self, *args, **kwargs)

    deco.decorated_func = fn
    return deco


def prim_arg_register(fn):
    """
    Primitive attributes register.

    Register the decorator of the built-in operator primitive '__init__'.
    The function will add all the parameters of '__init__' as operator attributes ,
    and init primitive name.

    Args:
        fn (function): __init__ function of primitive.

    Returns:
        function, original function.

    Examples:
        >>> from mindspore.ops import prim_arg_register, PrimitiveWithCheck
        >>> class MatMul(PrimitiveWithCheck):
        ...     @prim_arg_register
        ...     def __init__(self, transpose_a=False, transpose_b=False):
        ...         self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['output'])
        ...
        >>> # init a Primitive obj
        >>> matmul = MatMul()
    """

    @functools.wraps(fn)
    def deco(self, *args, **kwargs):
        class_name = self.__class__.__name__
        if hasattr(self.__class__, "substitute_name"):
            class_name = self.__class__.substitute_name
        if isinstance(self, PrimitiveWithInfer):
            PrimitiveWithInfer.__init__(self, class_name)
        elif isinstance(self, PrimitiveWithCheck):
            PrimitiveWithCheck.__init__(self, class_name)
        else:
            Primitive.__init__(self, self.__class__.__name__)
        bound_args = inspect.signature(fn).bind(self, *args, **kwargs)
        bound_args.apply_defaults()
        arguments = bound_args.arguments
        del arguments['self']
        del self.init_attrs['name']
        for name in arguments:
            value = arguments[name]
            self._set_prim_arg(name, value)
            self.init_attrs[name] = value
        fn(self, *args, **kwargs)

    deco.decorated_func = fn
    return deco


def _check_contains_variable(item_dtype, item_value):
    """
    Check whether the item is or contains variable.
    """
    if isinstance(item_value, (list, tuple)):
        for i, element in enumerate(item_value):
            if _check_contains_variable(item_dtype[i], element):
                return True
    elif isinstance(item_value, dict):
        if isinstance(item_dtype, typing.Keyword):
            return item_value is None
        for i in range(len(item_value)):
            if _check_contains_variable(item_dtype[i], list(item_value.keys())[i]):
                return True
        for i in range(len(item_value)):
            if _check_contains_variable(item_dtype[i], list(item_value.values())[i]):
                return True
    return item_dtype is not None and item_value is None


def constexpr(fn=None, get_instance=True, name=None, reuse_result=True, check=True):
    """Used to calculate constant in graph copmpiling process and improve compile performance in GRAPH_MODE.

    Args:
        fn (function): A `fn` use as the infer_value of the output operator. Default: ``None`` .
        get_instance (bool): If ``True`` , return the instance of operator,
                             otherwise return the operator class. Default: ``True`` .
        name (str): Defines the operator name. If `name` is ``None`` , use the function name as op name.
                             Default: ``None`` .
        reuse_result (bool): If ``True`` , the operator will be executed once and reuse the result next time,
                             otherwise the operator will always be executed. Default: ``True`` .
        check (bool): If ``True`` , the parameters will be checked
            and the warning message will raised if the parameter is not const value. Default: ``True`` .

    Examples:

        >>> import mindspore as ms
        >>> # define a constant calculate function with for loop inside and use use constexpr to accelerate the compile
        >>> # process.
        >>> @ms.constexpr
        ... def for_loop_calculate(range_num):
        ...     out = 0
        ...     for i in range(range_num):
        ...         if i %2 == 0 and i % 7 != 0:
        ...             out = out + i
        ...     return out // range_num
        ...
        >>> # construct a net and run with GRAPH_MODE.
        >>> @ms.jit
        ... def my_func(x):
        ...     new_shape = for_loop_calculate(100000)
        ...     return ms.ops.broadcast_to(x, (new_shape, ))
        ...
        >>> out = my_func(ms.Tensor([1]))
        >>> print(out.shape)
        >>> (21428, )
    """

    def decorator(fn):
        """Decorator for ProxyOp."""

        class ProxyOp(PrimitiveWithInfer):
            """
            ProxyOp is a temporary operator used to execute the constexpr function.
            """

            def __init__(self):
                op_name = name if name else fn.__name__
                super(ProxyOp, self).__init__(op_name)
                self.set_const_prim(True)
                self.fn = fn
                self.add_prim_attr('constexpr_prim', True)
                if not reuse_result:
                    self.add_prim_attr('forbid_reuse_result', True)

            def __infer__(self, *args):
                value_args = []
                for item in args:
                    item_value = item["value"]
                    if _check_contains_variable(item["dtype"], item_value) and check:
                        logger.warning("The \"" + self.name + "\" is a constexpr function." \
                                                              " The input arguments must be all constant value.")
                    value_args.append(item_value)
                return {'dtype': None, 'shape': None, 'value': fn(*value_args)}

            def __call__(self, *args, **kwargs):
                return fn(*args, **kwargs)

        if get_instance:
            return ProxyOp()
        return ProxyOp

    if fn is not None:
        return decorator(fn)
    return decorator


def _primexpr(fn=None, get_instance=True, name=None, reuse_result=True):
    """
    _primexpr is similar as constexpr except that when the input to the function decorated by _primexpr contains
    variable, the function will be compiled as graph.

    _primexpr is only for internal use.

    Args:
        fn (function): A `fn` use as the infer_value of the output operator. Default: ``None`` .
        get_instance (bool): If ``True`` , return the instance of operator,
                             otherwise return the operator class. Default: ``True`` .
        name (str): Defines the operator name. If `name` is ``None`` , use the function name as op name.
                             Default: ``None`` .
        reuse_result (bool): If ``True`` , the operator will be executed once and reuse the result next time,
                             otherwise the operator will always be executed. Default: ``True`` .
    """

    def deco(fn):
        """Decorator for CompileOp."""

        class CompileOp(PrimitiveWithInfer):
            """
            CompileOp is a temporary operator used to execute the constexpr function.
            """

            def __init__(self):
                op_name = name if name else fn.__name__
                PrimitiveWithInfer.__init__(self, op_name)
                self.set_const_prim(True)
                self.fn = fn
                self.add_prim_attr('constexpr_prim', True)
                if not reuse_result:
                    self.add_prim_attr('forbid_reuse_result', True)

            def __infer__(self, *args):
                value_args = []
                for item in args:
                    if _check_contains_variable(item["dtype"], item["value"]):
                        return {'dtype': None, 'shape': None, 'value': None, 'fn': (fn,)}
                    value_args.append(item["value"])
                return {'dtype': None, 'shape': None, 'value': fn(*value_args)}

            def __call__(self, *args, **kwargs):
                return fn(*args, **kwargs)

        if get_instance:
            return CompileOp()
        return CompileOp

    if fn is not None:
        return deco(fn)
    return deco


class _RunOpHook:
    """Hook for run op"""

    current = None

    def __init__(self, hook):
        self.hook = hook
        self.old = _RunOpHook.current

    def __enter__(self):
        _RunOpHook.current = self
        return self

    def __exit__(self, *err):
        _RunOpHook.current = self.old


def _run_op(obj, op_name, args):
    """Single op execution function supported by ge in PyNative mode."""
    if not _RunOpHook.current:
        stub = _pynative_executor.run_op_async(obj, op_name, args)
        return _convert_stub(stub)
    return _RunOpHook.current.hook(obj, args)


@_wrap_func
def _run_op_sync(obj, op_name, args):
    """Single op execution function in synchronous mode."""
    output = _pynative_executor.real_run_op(obj, op_name, args)
    return output


class _PrimitiveC(Primitive):
    def __init__(self, name, attrs):
        super().__init__(name)
        for key, value in attrs.items():
            super().add_prim_attr(key, value)


def _get_primitivec(name, attrs):
    return _PrimitiveC(name, attrs)


def _create_primitive_function_obj():
    return PrimitiveFunction_()
