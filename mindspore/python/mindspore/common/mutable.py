# Copyright 2022-2024 Huawei Technologies Co., Ltd
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
"""mutable function for setting constants mutable."""
from __future__ import absolute_import

from mindspore.common.tensor import Tensor
from mindspore._c_expression import Tensor as Tensor_
from mindspore import log as logger

_check_elements_set = set()


class _Int(int):
    pass


class _Float(float):
    pass


class _Tuple(tuple):
    pass


class _List(list):
    pass


class _Dict(dict):
    pass


# pylint: disable=super-init-not-called
class _Bool(int):
    """Define a _Bool class that inherits from int, because base class 'bool' is a marked final."""
    def __init__(self, value):
        self.value = bool(value)

    @property
    def __class__(self):
        return bool

    def __and__(self, x):
        return self.value & x

    def __rand__(self, x):
        return x & self.value

    def __or__(self, x):
        return self.value | x

    def __ror__(self, x):
        return x | self.value

    def __xor__(self, x):
        return self.value ^ x

    def __rxor__(self, x):
        return x ^ self.value

    def __str__(self):
        return str(self.value)

    def __repr__(self):
        return repr(self.value)

    def __ms_mutable_bool__(self):
        pass


def _check_element_type_recursion(value):
    """Check if all the elements are valid or self reference."""
    if id(value) in _check_elements_set:
        return False
    _check_elements_set.add(id(value))

    if isinstance(value, (tuple, list)):
        for element in value:
            if not _check_element_type_recursion(element):
                return False
            _check_elements_set.remove(id(element))
        return True
    if isinstance(value, dict):
        for element in value.values():
            if not _check_element_type_recursion(element):
                return False
            _check_elements_set.remove(id(element))
        return True
    return isinstance(value, (Tensor, Tensor_, int, float))


def _check_element_type(value):
    """Check if all the elements are valid."""
    flag = _check_element_type_recursion(value)
    _check_elements_set.clear()
    return flag


def mutable(input_data, dynamic_len=False):
    """
    Make a constant value mutable.

    Currently, all the inputs of Cell except Tensor such as scalar, tuple, list and dict, are regarded as constant
    values. The constant values are non-differentiable and used to do constant folding in the optimization process.

    Besides, currently when the network input is tuple[Tensor], list[Tensor] or dict[Tensor], even without changing
    the shape and dtype of the Tensors, the network will be re-compiled when calling this network repeatedly because
    the these inputs are regarded as constant values.

    To solve the above problems, we provide api `mutable` to make the constant inputs of Cell 'mutable'. A 'mutable'
    input means that it is changed to be a variable input just like Tensor and the most important thing is that it
    will be differentiable.

    When the `input_data` is tuple or list and `dynamic_len` is False, `mutable` will return a constant length tuple
    or list with all mutable elements. If `dynamic_len` is True, the length of the return tuple or list will be dynamic.

    If a dynamic length tuple or list is used as the input of the network and the network is repeatedly called, and
    the length of the tuple or list is different for each run, it does not need to be re-compiled.

    Args:
        input_data (Union[Tensor, scalar, tuple, list, dict]): The input data to be made mutable. If
            'input_data' is list/tuple/dict, the type of each element should also in the valid types.
        dynamic_len (bool): Whether to set the whole sequence to be dynamic length. In graph compilation, if
            `dynamic_len` is ``True`` , the `input_data` must be list or tuple and the elements of `input_data` must
            have the same type and shape. Default: ``False`` .

    .. warning::
        This is an experimental API that is subject to change or deletion.
        `dynamic_len` is an experimental argument. Currently, `dynamic_len` is not supported to be ``True`` .

    Note:
        Currently this api only works in GRAPH mode.

    Returns:
        The origin input data which has been set mutable.

    Raises:
        TypeError: If `input_data` is not one of Tensor, scalar, tuple, list, dict or their nested structure.
        TypeError: If `dynamic_len` is ``True`` and `input_data` is not tuple or list.
        ValueError: If `dynamic_len` is ``True`` , `input_data` is tuple or list but the elements within `input_data`
            do not have the same type.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import mutable, nn, ops, Tensor, context
        >>> from mindspore import dtype as mstype
        >>> context.set_context(mode=context.GRAPH_MODE)
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.matmul = ops.MatMul()
        ...
        ...     def construct(self, z):
        ...         x = z[0]
        ...         y = z[1]
        ...         out = self.matmul(x, y)
        ...         return out
        ...
        >>> class GradNetWrtX(nn.Cell):
        ...     def __init__(self, net):
        ...         super(GradNetWrtX, self).__init__()
        ...         self.net = net
        ...         self.grad_op = ops.GradOperation()
        ...
        ...     def construct(self, z):
        ...         gradient_function = self.grad_op(self.net)
        ...         return gradient_function(z)
        ...
        >>> z = mutable((Tensor([[0.5, 0.6, 0.4], [1.2, 1.3, 1.1]], dtype=mstype.float32),
        ...              Tensor([[0.01, 0.3, 1.1], [0.1, 0.2, 1.3], [2.1, 1.2, 3.3]], dtype=mstype.float32)))
        >>> output = GradNetWrtX(Net())(z)
        >>> print(output)
        (Tensor(shape=[2, 3], dtype=Float32, value=
        [[ 1.41000009e+00,  1.60000002e+00,  6.59999943e+00],
         [ 1.41000009e+00,  1.60000002e+00,  6.59999943e+00]]), Tensor(shape=[3, 3], dtype=Float32, value=
        [[ 1.70000005e+00,  1.70000005e+00,  1.70000005e+00],
         [ 1.89999998e+00,  1.89999998e+00,  1.89999998e+00],
         [ 1.50000000e+00,  1.50000000e+00,  1.50000000e+00]]))
    """
    if not _check_element_type(input_data):
        raise TypeError(
            f"For 'mutable', the 'input_data' should be one of (bool, int, float, Tensor, tuple, list, dict) "
            f"or their nested structures with no self-reference, but got {type(input_data).__name__}: {input_data}.")

    if not isinstance(dynamic_len, bool):
        raise TypeError(f"For 'mutable', the second input should be bool, but got: {type(input_data).__name__}")

    if dynamic_len and not isinstance(input_data, (tuple, list)):
        raise TypeError(
            f"For 'mutable', when the variable_len is True, the first input should be list or tuple, "
            f"but got: {type(input_data).__name__}")

    ret = input_data
    if isinstance(input_data, bool):
        ret = _Bool(input_data)
    elif isinstance(input_data, int):
        ret = _Int(input_data)
    elif isinstance(input_data, float):
        ret = _Float(input_data)
    elif isinstance(input_data, list):
        ret = _List(input_data)
    elif isinstance(input_data, tuple):
        ret = _Tuple(input_data)
    elif isinstance(input_data, dict):
        ret = _Dict(input_data)
    elif isinstance(input_data, Tensor):
        logger.info("For 'mutable', the Tensor in 'input_data' must not be constant. \
                     We will add set_const_arg=False statement automatically.")
        ret.set_const_arg(False)
    elif isinstance(input_data, Tensor_):
        ret = Tensor(input_data, internal=True)
        logger.info("For 'mutable', the Tensor_ in 'input_data' must not be constant. \
                     We will add set_const_arg=False statement automatically.")
        ret.set_const_arg(False)

    setattr(ret, "__ms_mutable__", True)
    setattr(ret, "__ms_dynamic_len__", dynamic_len)
    setattr(ret, "__ms_origin_object__", input_data)
    _check_elements_set.clear()
    return ret
