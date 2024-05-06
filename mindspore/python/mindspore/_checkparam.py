# Copyright 2020-2021 Huawei Technologies Co., Ltd
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
"""Check parameters."""
from __future__ import absolute_import

import re
import inspect
import math
from types import FunctionType, MethodType
from functools import reduce, wraps
from itertools import repeat
from collections.abc import Iterable
import numpy as np

from mindspore import context
from mindspore import log as logger
from mindspore.common import dtype as mstype
from mindspore._c_expression import Tensor as Tensor_


EQ = 1  # ==
NE = 2  # !=
LT = 3  # <
LE = 4  # <=
GT = 5  # >
GE = 6  # >=
# scalar range check
INC_NEITHER = 7  # (), include neither
INC_LEFT = 8  # [), include left
INC_RIGHT = 9  # (], include right
INC_BOTH = 10  # [], include both
# collection in, not in
IN = 11
NOT_IN = 12


def _check_binary_rel(val1, val2, rel):
    """check binary relation"""
    if rel == EQ:
        return val1 == val2
    if rel == NE:
        return val1 != val2
    if rel == LT:
        return val1 < val2
    if rel == LE:
        return val1 <= val2
    if rel == GT:
        return val1 > val2
    if rel == GE:
        return val1 >= val2
    if rel == IN:
        return val1 in val2
    if rel == NOT_IN:
        return val1 not in val2

    return False


def _check_inc_rel(val, lower, upper, rel):
    """check include relation"""
    if rel == INC_NEITHER:
        return not (val <= lower or val >= upper)
    if rel == INC_LEFT:
        return not (val < lower or val >= upper)
    if rel == INC_RIGHT:
        return not (val <= lower or val > upper)
    if rel == INC_BOTH:
        return not (val < lower or val > upper)

    return False


def _format_str_one_value(value, rel):
    """format string"""
    if rel == EQ:
        return f"= {value}"
    if rel == NE:
        return f"!= {value}"
    if rel == LT:
        return f"< {value}"
    if rel == LE:
        return f"<= {value}"
    if rel == GT:
        return f"> {value}"
    if rel == GE:
        return f">= {value}"
    if rel == IN:
        return f"in {value}"
    if rel == NOT_IN:
        return f"not in {value}"

    return ""


def _format_str_two_value(val1, val2, rel):
    """format string"""
    if rel == INC_NEITHER:
        return f"({val1}, {val2})"
    if rel == INC_LEFT:
        return f"[{val1}, {val2})"
    if rel == INC_RIGHT:
        return f"({val1}, {val2}]"
    if rel == INC_BOTH:
        return f"[{val1}, {val2}]"

    return ""


def _check_3d_int_or_tuple(arg_name, arg_value, prim_name, allow_five=False, ret_five=False,
                           greater_zero=True, third_one=False, three_input=False):
    """
    Checks whether an argument is a positive int or tuple with 3 or 5(when allow_five is True) positive int elements.
    """

    def _raise_message(third_one_flag=False, three_input_flag=False):
        if third_one_flag:
            raise ValueError(f"For '{prim_name}', the depth of parameter '{arg_name}' must be 1, " \
                             f"but got {ret_value[-3]}.")
        if three_input_flag:
            raise ValueError(f"For '{prim_name}', the parameter '{arg_name}' must be an positive integer " \
                             f"or a tuple of three positive integer, but got {arg_value}.")
        raise ValueError(f"For '{prim_name}', the parameter '{arg_name}' must be an positive integer or " \
                         f"a tuple of three {'or five ' if allow_five else ''}positive integer, but got {arg_value}")

    def _get_return_value():
        def _check():
            if not isinstance(arg_value, int):
                if len(arg_value) == 5:
                    if not allow_five:
                        _raise_message()
                elif not len(arg_value) == 3:
                    _raise_message()

        _check()
        if isinstance(arg_value, int):
            ret = (1, 1, arg_value, arg_value, arg_value) if ret_five else (arg_value, arg_value, arg_value)
        elif len(arg_value) == 3:
            ret = (1, 1, arg_value[0], arg_value[1], arg_value[2]) if ret_five else arg_value
        else: # case: len(arg_value) == 5
            ret = arg_value if ret_five else (arg_value[2], arg_value[3], arg_value[4])

        return ret

    def _check_value(ret_value):
        for item in ret_value:
            if isinstance(item, int) and not isinstance(item, bool):
                if greater_zero and item > 0:
                    continue
                if not greater_zero and item >= 0:
                    continue
            _raise_message()

    def _check_third_one(ret_value):
        if third_one:
            if ret_value[-3] != 1:
                _raise_message(third_one_flag=third_one)

    check_value_type(arg_name, arg_value, (int, tuple), prim_name)
    if three_input and isinstance(arg_value, tuple):
        if len(arg_value) != 3:
            _raise_message(three_input_flag=three_input)
    ret_value = _get_return_value()
    _check_value(ret_value)
    _check_third_one(ret_value)

    return tuple(ret_value)


def _check_dup(axes):
    for item in axes:
        count = 0
        for item2 in axes:
            if item == item2:
                count += 1

        if count > 1:
            raise ValueError(f"The element of parameter 'axis' can not be duplicate, but got {axes}.")


def _check_number(arg_value, value, rel, arg_type=int, arg_name=None, prim_name=None):
    """
    Check argument integer.

    Usage:
    - arg_value = _check_number(arg_value, 2, GT, int, "value", None)
    """
    prim_name = f"For \'{prim_name}\', the " if prim_name else 'The '
    arg_name = f"\'{arg_name}\'" if arg_name else 'input value'

    def _check_param():
        prim_info = f'{prim_name}' + f'{arg_name}'
        if isinstance(arg_value, arg_type):
            if math.isinf(arg_value) or math.isnan(arg_value) or np.isinf(arg_value) or np.isnan(arg_value):
                raise ValueError(f"{prim_info} must be a legal value, but got '{arg_value}'.")
        else:
            raise TypeError(f"{prim_info} must be {arg_type.__name__}, but got '{type(arg_value).__name__}'")

        type_mismatch = not isinstance(arg_value, arg_type) or isinstance(arg_value, bool)
        rel_ret = _check_binary_rel(arg_value, value, rel)
        if type_mismatch or not rel_ret:
            rel_str = _format_str_one_value(value, rel)
            msg = f"{prim_info} must be {arg_type.__name__} and must {rel_str}, " \
                  f"but got '{arg_value}' with type '{type(arg_value).__name__}'."
            if type_mismatch:
                raise TypeError(msg)
            raise ValueError(msg)

    _check_param()
    return arg_value


def check_is_number(arg_value, arg_type, arg_name=None, prim_name=None):
    """
    Checks input value is float type or not.

    Usage:
    - number = check_is_number(number, int)
    - number = check_is_number(number, int, "bias")
    - number = check_is_number(number, int, "bias", "bias_class")
    """
    prim_name = f"For \'{prim_name}\', the" if prim_name else 'The'
    arg_name = f"\'{arg_name}\'" if arg_name else 'input value'

    def _check_param():
        if isinstance(arg_value, arg_type) and not isinstance(arg_value, bool):
            if math.isinf(arg_value) or math.isnan(arg_value) or np.isinf(arg_value) or np.isnan(arg_value):
                raise ValueError(f"{prim_name} {arg_name} must be a legal float, but got '{arg_value}'.")
        else:
            raise TypeError(f"{prim_name} type of {arg_name} must be '{arg_type.__name__}', " \
                            f"but got '{type(arg_value).__name__}'.")
    _check_param()
    return arg_value


def check_number_range(arg_value, lower_limit, upper_limit, rel, value_type, arg_name=None, prim_name=None):
    """
    Method for checking whether an int value is in some range.

    Usage:
    - number = check_number_range(number, 0.0, 1.0, INC_NEITHER, "number", float) # number in [0.0, 1.0]
    - number = check_number_range(number, 0, 1, INC_NEITHER, "number", int) # number in [0, 1]
    """
    prim_name = f"For \'{prim_name}\', the" if prim_name else 'The'
    arg_name = f"\'{arg_name}\'" if arg_name else 'input value'

    def _check_param():
        type_mismatch = not isinstance(arg_value, (np.ndarray, np.generic, value_type)) or isinstance(arg_value, bool)
        if type_mismatch:
            raise TypeError(f"{prim_name} {arg_name} must be '{value_type.__name__}',  " \
                            f"but got '{type(arg_value).__name__}'.")

        if not _check_inc_rel(arg_value, lower_limit, upper_limit, rel):
            rel_str = _format_str_two_value(lower_limit, upper_limit, rel)
            raise ValueError(f"{prim_name} {arg_name} must be in range of {rel_str}, " \
                             f"but got {arg_value} with type '{type(arg_value).__name__}'.")
    _check_param()
    return arg_value


def check(arg_name, arg_value, value_name, value, rel=EQ, prim_name=None, excp_cls=ValueError):
    """
    Method for judging relation between two int values or list/tuple made up of ints.
    This method is not suitable for judging relation between floats, since it does not consider float error.
    """
    def _check():
        if not _check_binary_rel(arg_value, value, rel):
            rel_str = _format_str_one_value(f'{value_name}: {value}', rel)
            msg_prefix = f'For \'{prim_name}\', the' if prim_name else "The"
            msg_subject = f"{msg_prefix} \'{arg_name}\'" if " " not in arg_name else f"{msg_prefix} {arg_name}"
            raise excp_cls(f'{msg_subject} should be {rel_str}, but got {arg_value}.')

    _check()
    return arg_value


def check_int(arg_value, value, rel, arg_name=None, prim_name=None):
    """
    Checks input integer value `arg_value` compare to `value`.

    Usage:
    - number = check_int(number, 0, GE, "number", None) # number >= 0
    """
    return _check_number(arg_value, value, rel, int, arg_name, prim_name)


def check_is_int(arg_value, arg_name=None, prim_name=None):
    """
    Checks input value is float type or not.

    Usage:
    - number = check_is_int(number, int)
    - number = check_is_int(number, int, "bias")
    - number = check_is_int(number, int, "bias", "bias_class")
    """
    return check_is_number(arg_value, int, arg_name, prim_name)


def check_equal_int(arg_value, value, arg_name=None, prim_name=None):
    """
    Checks input integer value `arg_value` compare to `value`.

    Usage:
    - number = check_equal_int(number, 0, "number", None) # number == 0
    """
    return _check_number(arg_value, value, EQ, int, arg_name, prim_name)


def check_positive_int(arg_value, arg_name=None, prim_name=None):
    """
    Check argument is positive integer, which mean arg_value > 0.

    Usage:
    - number = check_positive_int(number)
    - number = check_positive_int(number, "bias")
    """
    return _check_number(arg_value, 0, GT, int, arg_name, prim_name)


def check_positive_int_sequence(sequence, arg_name=None, prim_name=None):
    """
    Check argument is positive int sequence, which mean all element > 0 in sequence.

    Usage:
    - sequence = check_positive_int_sequence(sequence)
    - sequence = check_positive_int_sequence(sequence, "dims")
    """
    for idx in range(len(sequence)):
        element = sequence[idx]
        arg_idx = f"{arg_name if arg_name else 'arg_name'}[{idx}]"
        _check_number(element, 0, GT, int, arg_idx, prim_name)
    return sequence


def check_negative_int(arg_value, arg_name=None, prim_name=None):
    """
    Check argument is negative integer, which mean arg_value < 0.

    Usage:
    - number = check_negative_int(number)
    - number = check_negative_int(number, "bias")
    """
    return _check_number(arg_value, 0, LT, int, arg_name, prim_name)


def check_non_positive_int(arg_value, arg_name=None, prim_name=None):
    """
    Check argument is non-negative integer, which mean arg_value <= 0.

    Usage:
    - number = check_non_positive_int(number)
    - number = check_non_positive_int(number, "bias")
    """
    return _check_number(arg_value, 0, LE, int, arg_name, prim_name)


def check_non_negative_int(arg_value, arg_name=None, prim_name=None):
    """
    Check argument is non-negative integer, which mean arg_value >= 0.

    Usage:
    - number = check_non_negative_int(number)
    - number = check_non_negative_int(number, "bias")
    """
    return _check_number(arg_value, 0, GE, int, arg_name, prim_name)


def check_non_negative_int_sequence(sequence, arg_name=None, prim_name=None):
    """
    Check argument is positive sequence, which mean all element >= 0 in sequence.

    Usage:
    - sequence = check_non_negative_int_sequence(sequence)
    - sequence = check_non_negative_int_sequence(sequence, "dims")
    """
    for idx in range(len(sequence)):
        element = sequence[idx]
        arg_idx = f"{arg_name if arg_name else 'arg_name'}[{idx}]"
        _check_number(element, 0, GE, int, arg_idx, prim_name)
    return sequence


def check_float(arg_value, value, rel, arg_name=None, prim_name=None):
    """
    Checks input float value `arg_value` compare to `value`.

    Usage:
    - number = check_float(number, 0.0, GE, "number", None) # number >= 0
    """
    return _check_number(arg_value, value, rel, float, arg_name, prim_name)


def check_is_float(arg_value, arg_name=None, prim_name=None):
    """
    Checks input value is float type or not.

    Usage:
    - number = check_is_float(number)
    - number = check_is_float(number, "bias")
    - number = check_is_float(number, "bias", "bias_class")
    """
    return check_is_number(arg_value, float, arg_name, prim_name)


def check_positive_float(arg_value, arg_name=None, prim_name=None):
    """
    Check argument is positive float, which mean arg_value > 0.

    Usage:
    - number = check_positive_float(number)
    - number = check_positive_float(number, "bias")
    - number = check_positive_float(number, "bias", "bias_class")
    """
    return _check_number(arg_value, 0, GT, float, arg_name, prim_name)


def check_positive_float_sequence(sequence, arg_name=None, prim_name=None):
    """
    Check argument is positive sequence, which mean all element > 0 in sequence.

    Usage:
    - sequence = check_positive_float_sequence(sequence)
    - sequence = check_positive_float_sequence(sequence, "dims")
    """
    for idx in range(len(sequence)):
        element = sequence[idx]
        arg_idx = f"{arg_name if arg_name else 'arg_name'}[{idx}]"
        _check_number(element, 0, GT, float, arg_idx, prim_name)
    return sequence


def check_negative_float(arg_value, arg_name=None, prim_name=None):
    """
    Check argument is negative float, which mean arg_value < 0.

    Usage:
    - number = check_negative_float(number)
    - number = check_negative_float(number, "bias")
    """
    return _check_number(arg_value, 0, LT, float, arg_name, prim_name)


def check_non_positive_float(arg_value, arg_name=None, prim_name=None):
    """
    Check argument is non-negative float, which mean arg_value <= 0.

    Usage:
    - number = check_non_positive_float(number)
    - number = check_non_positive_float(number, "bias")
    """
    return _check_number(arg_value, 0, LE, float, arg_name, prim_name)


def check_non_negative_float(arg_value, arg_name=None, prim_name=None):
    """
    Check argument is non-negative float, which mean arg_value >= 0.

    Usage:
    - number = check_non_negative_float(number)
    - number = check_non_negative_float(number, "bias")
    """
    return _check_number(arg_value, 0, GE, float, arg_name, prim_name)


def check_number(arg_name, arg_value, value, rel, prim_name):
    """Number value judgment."""
    def _check():
        if not _check_binary_rel(arg_value, value, rel):
            rel_str = _format_str_one_value(value, rel)
            raise ValueError(f'For \'{prim_name}\', the argument \'{arg_name}\' ' \
                             f'must {rel_str}, but got {arg_value}.')
    _check()
    return arg_value


def check_isinstance(arg_name, arg_value, classes):
    """Check arg isinstance of classes"""
    def _check():
        if not isinstance(arg_value, classes):
            raise ValueError(f'The parameter \'{arg_name}\' must be isinstance of {classes}, but got {arg_value}.')
    _check()
    return arg_value


def check_bool(arg_value, arg_name=None, prim_name=None):
    """
    Check argument is instance of bool.

    Usage:
    - has_bias = check_bool(has_bias)
    - has_bias = check_bool(has_bias, "has_bias")
    """
    prim_name = f"For '{prim_name}', the" if prim_name else 'The'
    arg_name = f"'{arg_name}'" if arg_name else 'input value'

    def _check():
        if not isinstance(arg_value, bool):
            raise TypeError(f"{prim_name} {arg_name} must be a bool, but got {type(arg_value).__name__}.")
    _check()
    return arg_value


def check_int_range(arg_value, lower_limit, upper_limit, rel, arg_name=None, prim_name=None):
    """
    Method for checking whether input value is in int range.

    Usage:
    - number = check_int_range(number, 0, 1, INC_NEITHER) # number in [0, 1]
    - number = check_int_range(number, 0, 1, INC_NEITHER, "number") # number in [0, 1]
    """
    return check_number_range(arg_value, lower_limit, upper_limit, rel, int, arg_name, prim_name)


def check_float_range(arg_value, lower_limit, upper_limit, rel, arg_name=None, prim_name=None):
    """
    Method for checking whether input value is in float range.

    Usage:
    - number = check_float_range(number, 0.0, 1.0, INC_NEITHER) # number in [0.0, 1.0]
    - number = check_float_range(number, 0.0, 1.0, INC_NEITHER, "number") # number in [0.0, 1.0]
    """
    return check_number_range(arg_value, lower_limit, upper_limit, rel, float, arg_name, prim_name)


def check_string(arg_value, valid_values, arg_name=None, prim_name=None):
    """
    Check whether string is in some value list.

    Usage:
    - method = check_string(method, ["string1", "string2", "string3"], "method")
    """
    arg_name = arg_name if arg_name else "parameter"
    msg_prefix = f'For \'{prim_name}\', the' if prim_name else "The"

    def _check():
        if not (isinstance(arg_value, str) and arg_value in valid_values):
            raise ValueError(f"{msg_prefix} '{arg_name}' must be str and must be in '{valid_values}'," \
                             f" but got '{arg_value}'.")
    _check()
    return arg_value


def check_str_by_regular(target, reg=None, flag=re.ASCII, prim_name=None):
    if reg is None:
        # Named string regular expression
        reg = r"^\w+[0-9a-zA-Z\_\.]*$"
    if re.match(reg, target, flag) is None:
        prim_name = f"For '{prim_name}', the" if prim_name else "The"
        raise ValueError(f"{prim_name} '{target}' is illegal, it must be match regular'{reg}' by flags'{flag}.'")
    return True


# pylint: disable=missing-docstring
def check_str_and_none_by_regular(target, reg=None, flag=re.ASCII, prim_name=None):
    if reg is None:
        # Named string regular expression
        reg = r"^\w*[0-9a-zA-Z\_\.\-]*$"
    if re.match(reg, target, flag) is None:
        prim_name = f"For '{prim_name}', the" if prim_name else "The"
        raise ValueError(f"{prim_name} '{target}' is illegal, it must be match regular'{reg}' by flags'{flag}.'")
    return True


def check_file_name_by_regular(target, reg=None, prim_name=None):
    """Check whether file name is legitimate."""
    if not isinstance(target, str):
        prim_name = f"For '{prim_name}', the" if prim_name else "The"
        raise TypeError(f"{prim_name} '{target}' must be string, but got {type(target)}.")
    if target.endswith("\\") or target.endswith("/"):
        prim_name = f"For '{prim_name}', the" if prim_name else "The"
        raise ValueError(f"{prim_name} '{target}' cannot be a directory path.")
    if reg is None:
        reg = r"^[0-9a-zA-Z@\_\-\.\:\/\\]+$"
    if re.match(reg, target) is None:
        prim_name = f"For '{prim_name}', the" if prim_name else "The"
        raise ValueError(f"{prim_name} '{target}' is illegal, it must be match regular '{reg}'.")

    return True


def check_pad_value_by_mode(pad_mode, padding, prim_name):
    """Validates value of padding according to pad_mode"""
    if pad_mode != 'pad' and padding != 0:
        raise ValueError(f"For '{prim_name}', padding must be zero when pad_mode is '{pad_mode}'," \
                         f" but got {padding}.")
    return padding


def check_subclass(arg_name, type_, template_types, prim_name, addition_error_info=None):
    """Checks whether some type is subclass of another type"""
    if not isinstance(template_types, Iterable):
        template_types = (template_types,)
    hit = False
    for template_type in template_types:
        if isinstance(template_type, mstype.Type):
            if mstype._issubclass_(type_, template_type):  # pylint: disable=W0212
                hit = True
                break
        elif type_ is template_type:
            hit = True
            break
    if not hit:
        if addition_error_info is None:
            addition_error_info = ''
        else:
            addition_error_info = ' ' + addition_error_info
        type_str = (f"type '{type(type_).__name__}'" if isinstance(type_, (tuple, list)) else str(type_))
        raise TypeError(f"For '{prim_name}', the element of '{arg_name}'" \
                        f" must be {'one of ' if len(template_types) > 1 else ''}" \
                        f"{', '.join((str(x) for x in template_types))}, but got {type_str}" \
                        f"{addition_error_info}.The supported data types depend on the hardware that" \
                        f" executes the operator, for more details, please refer to the MindSpore official " \
                        f"website to get more information about the data type.")


def check_valid_input(arg_name, arg_value, prim_name):
    """Checks valid value."""
    def _check():
        if arg_value is None:
            raise ValueError(f"For \'{prim_name}\', the argument '{arg_name}'" \
                             f"can not be None, but got {arg_value}.")
    _check()
    return arg_value


def check_types_same_and_valid(args, valid_values, prim_name):
    """Checks whether the types of inputs are the same and valid."""

    def _check_type_valid(arg):
        arg_key, arg_val = arg
        elem_type = arg_val
        check_subclass(arg_key, elem_type, valid_values, prim_name)
        return (arg_key, elem_type)

    def _check_types_same(arg1, arg2):
        arg1_name, arg1_type = arg1
        arg2_name, arg2_type = arg2
        if arg1_type != arg2_type:
            raise TypeError(f"For '{prim_name}', the type of '{arg2_name}' should be same as '{arg1_name}'," \
                            f" but got '{arg1_name}' with type {arg1_type}" \
                            f" and '{arg2_name}' with type {arg2_type}.")
        return arg1

    elem_types = map(_check_type_valid, args.items())
    reduce(_check_types_same, elem_types)


def check_tensors_dtypes_same_and_valid(args, valid_dtypes, prim_name):
    """Checks whether the element types of input tensors are the same and valid."""
    valid_dtypes = valid_dtypes if isinstance(valid_dtypes, Iterable) else [valid_dtypes]
    tensor_types = [mstype.TensorType(t) for t in valid_dtypes]
    check_types_same_and_valid(args, tensor_types, prim_name)


def check_tensor_dtype_valid(arg_name, arg_type, valid_dtypes, prim_name):
    """Checks whether the element types of input tensors are valid."""
    valid_dtypes = valid_dtypes if isinstance(valid_dtypes, Iterable) else [valid_dtypes]
    tensor_types = [mstype.TensorType(t) for t in valid_dtypes]
    check_subclass(arg_name, arg_type, tensor_types, prim_name)


def check_scalar_or_tensor_types_same(args, valid_values, prim_name, allow_mix=False):
    """
    Checks whether the types of inputs are the same. If the input args are tensors, checks their element types.
    If `allow_mix` is True, Tensor(float32) and float32 are type compatible, otherwise an exception will be raised.
    """

    def _check_argument_type(arg):
        arg_key, arg_val = arg
        if isinstance(arg_val, type(mstype.tensor_type)):
            arg_val = arg_val.element_type()
        if arg_val not in valid_values:
            raise TypeError(f'For \'{prim_name}\', the type of \'{arg_key}\' must be in {valid_values},' \
                            f' but got {arg_val}.')
        return arg

    def _check_types_same(arg1, arg2):
        arg1_name, arg1_type = arg1
        arg2_name, arg2_type = arg2
        except_flag = False
        if isinstance(arg1_type, type(mstype.tensor_type)) and isinstance(arg2_type, type(mstype.tensor_type)):
            arg1_type = arg1_type.element_type()
            arg2_type = arg2_type.element_type()
        elif not (isinstance(arg1_type, type(mstype.tensor_type)) or isinstance(arg2_type, type(mstype.tensor_type))):
            pass
        elif allow_mix:
            arg1_type = arg1_type.element_type() if isinstance(arg1_type, type(mstype.tensor_type)) else arg1_type
            arg2_type = arg2_type.element_type() if isinstance(arg2_type, type(mstype.tensor_type)) else arg2_type
        else:
            except_flag = True

        if except_flag or arg1_type != arg2_type:
            raise TypeError(f"For '{prim_name}', the type of '{arg2_name}' must be same as '{arg1_name}'," \
                            f" but got '{arg1_name}' with type {arg1_type}" \
                            f" and '{arg2_name}' with type {arg2_type}.")
        return arg1

    args_map = map(_check_argument_type, args.items())
    reduce(_check_types_same, args_map)


def check_value_type(arg_name, arg_value, valid_types, prim_name=None):
    """Checks whether a value is instance of some types."""
    valid_types = valid_types if isinstance(valid_types, Iterable) else (valid_types,)

    def raise_error_msg(cond, arg_value):
        """func for raising error message when check failed"""
        if not cond:
            return
        type_names = [t.__name__ if hasattr(t, '__name__') else str(t) for t in valid_types]
        num_types = len(valid_types)
        msg_prefix = f"For '{prim_name}', the" if prim_name else "The"
        raise TypeError(f'{msg_prefix} type of \'{arg_name}\' should be {"one of " if num_types > 1 else ""}' \
                        f'\'{type_names if num_types > 1 else type_names[0]}\', ' \
                        f'but got type \'{type(arg_value).__name__}\'.')

    # Notice: bool is subclass of int, so `check_value_type('x', True, [int])` will check fail, and
    #         `check_value_type('x', True, [bool, int])` will check pass
    cond = isinstance(arg_value, bool) and bool not in tuple(valid_types)
    raise_error_msg(cond, arg_value)
    if isinstance(arg_value, float) and float not in tuple(valid_types):
        arg_value = round(arg_value, 6)
    cond = not isinstance(arg_value, tuple(valid_types))
    raise_error_msg(cond, arg_value)
    return arg_value


def check_type_name(arg_name, arg_type, valid_types, prim_name):
    """Checks whether a type in some specified types"""
    valid_types = valid_types if isinstance(valid_types, Iterable) else (valid_types,)

    def raise_error_msg(cond, arg_type):
        """func for raising error message when check failed"""
        if not cond:
            return
        type_names = [t.__name__ if hasattr(t, '__name__') else t for t in valid_types]
        num_types = len(valid_types)
        msg_prefix = f"For '{prim_name}', the" if prim_name else "The"
        raise TypeError(f"{msg_prefix} '{arg_name}' should be {'one of ' if num_types > 1 else ''}" \
                        f"{type_names if num_types > 1 else type_names[0]}, " \
                        f"but got '{arg_type.__name__ if hasattr(arg_type, '__name__') else repr(arg_type)}'.")

    if isinstance(arg_type, type(mstype.tensor_type)):
        arg_type = arg_type.element_type()
    cond = arg_type not in valid_types
    raise_error_msg(cond, arg_type)
    return arg_type


def check_reduce_shape(ori_shape, shape, axis, prim_name, arg_name1, arg_name2):
    """Checks whether shape is ori_shape reduced on axis"""
    axis_origin = axis
    axis = axis if isinstance(axis, Iterable) else (axis,)
    exp_shape = [ori_shape[i] for i in range(len(ori_shape)) if i not in axis]
    if list(shape) != exp_shape:
        raise ValueError(f"For '{prim_name}', " \
                         f"the shape of parameter '{arg_name1}' reduce on 'axis': {axis_origin} must " \
                         f"be equal to the shape of '{arg_name2}': {shape}, but got {ori_shape}.")


def check_astype_dtype(dtype):
    """Check whether dtype is a valid input, and convert to mstype"""
    all_types = mstype.__dtype__ + ["int", "float", "bool"]
    if isinstance(dtype, str):
        if dtype.lower() not in all_types:
            raise TypeError(f"For Tensor.astype, the input type must be one of {all_types}, but got '{dtype}'.")
        dtype = mstype.pytype_to_dtype(np.dtype(dtype.lower()))
    elif isinstance(dtype, type):
        dtype = mstype.pytype_to_dtype(dtype)
    elif not dtype in mstype.number_type + (mstype.bool_,):
        raise TypeError(f"For Tensor.astype, the input type must be one of {mstype.number_type + (mstype.bool_,)}," \
                        f" but got '{dtype}'.")
    return dtype


def check_transpose_axis(axes, ndim):
    """Check the axis argument for tensor.transpose"""
    def _check_dim():
        # if multiple arguments provided, it must be `ndim` number of ints
        if len(axes) != ndim:
            raise ValueError(f"For Tensor.transpose, the number of axes must be equal to the dimension of Tensor, " \
                             f"but got {len(axes)} in the number of axes.")

    if not axes or (len(axes) == 1 and axes[0] is None):
        return tuple(range(ndim-1, -1, -1))

    if len(axes) == 1:
        perm = axes[0]
        # if only one argument provided, it must be tuple or list
        if isinstance(perm, list):
            perm = tuple(perm)
        elif isinstance(perm, int):
            perm = (perm,)
            _check_dim()
        else:
            if not isinstance(perm, tuple):
                raise TypeError(f"For Tensor.transpose, the parameter 'axes' must be a tuple/list, " \
                                f"or series of integer, but got {type(axes[0])}")
        return perm

    _check_dim()
    return axes


def check_reshape_shp(shp):
    """Check the shape argument for tensor.reshape"""

    if len(shp) == 1:
        new_shape = shp[0]
        # if only one argument provided, it must be int, tuple or list
        if isinstance(new_shape, int):
            return shp
        if isinstance(new_shape, list):
            new_shape = tuple(new_shape)
        else:
            if not isinstance(new_shape, tuple):
                raise TypeError(
                    f"For Tensor.reshape, the parameter 'shape' must be an integer, or tuple/list, " \
                    f"or series of integer, but got {type(shp[0])}")
        return new_shape

    return shp


def check_flatten_order(order):
    """Check flatten function input order"""
    if not isinstance(order, str):
        raise TypeError(f"For Tensor.flatten, the parameter 'order' must be a string, but got {type(order)}")
    if order not in ('C', 'F'):
        raise ValueError(f"For Tensor.flatten, the parameter 'order' must be 'C' or 'F', but got '{order}'")


def check_swapaxes_axis(axes, ndim):
    """Check all the axes argument for ops.swapaxes"""
    if isinstance(axes, int):
        return check_axis_in_range(axes, ndim)
    if isinstance(axes, (tuple, list)):
        for axis in axes:
            if not isinstance(axis, int):
                raise TypeError(f"For ops.swapaxes, the axis argument must be integer, but got {type(axis)}.")
            check_axis_in_range(axis, ndim)
        tmp = ()
        for x in axes:
            tmp = tmp + ((x + ndim) % ndim,)
        return tmp
    raise TypeError(f"For ops.swapaxes, the argument 'axes' must be integer, list or tuple for check, " \
                    f"but got {type(axes)}.")


def prepare_shape_for_squeeze(shape, axes):
    """
    Creates the squeezed new shape based on the tensor and given axes.

    Args:
        shape (tuple): the shape of the tensor
        axes Union[int, tuple(int), list(int)]: the axes with dimensions need to
            be squeezed.

    Returns:
        new_shape(tuple): the shape with dimensions squeezed.
    """
    new_shape = ()
    ndim = len(shape)

    def _check(axes, ndim):
        if axes >= ndim or axes < -ndim:
            raise ValueError(f"For Tensor.squeeze, the 'axis' must be in the range of [-{ndim}, {ndim}), " \
                             f"but got {axes}.")

    def _check_for(axes, ndim):
        for axis in axes:
            _check(axis, ndim)

    if isinstance(axes, int):
        _check(axes, ndim)
        axes = (axes,)
    elif isinstance(axes, (list, tuple)):
        _check_for(axes, ndim)
        new_axes = ()
        for item in axes:
            if item not in new_axes:
                new_axes += (item,)
        axes = new_axes
    else:
        raise TypeError(f"For Tensor.squeeze, the parameter 'axes' must be one of [int, tuple, list], " \
                        f"but got {type(axes)}")

    def _check_axis(s, idx, axes, ndim):
        # if an axis is selected with shape entry greater than one, an error is raised.
        if s != 1 and ((idx in axes) or (idx - ndim in axes)):
            raise ValueError(f"For Tensor.squeeze, the shape of parameter 'axis' {axes} must be 1, but got {s}.")

    for idx in range(ndim):
        s = shape[idx]
        _check_axis(s, idx, axes, ndim)
        if s != 1 or (idx not in axes) and (idx - ndim not in axes):
            new_shape = new_shape + (s,)

    return new_shape


def check_axis_in_range(axis, ndim):
    """Checks axes are with the bounds of ndim"""
    def _check():
        if not isinstance(axis, int):
            raise TypeError(f'The axes must be integers, but got {type(axis)}')

        if axis >= ndim or axis < -ndim:
            raise ValueError(f"The 'axis' must be in the range of [-{ndim}, {ndim}), but got {axis}.")

    _check()
    return (axis + ndim) % ndim


def check_axis_valid(axes, ndim):
    """
    Checks axes are valid given ndim, and returns axes that can be passed
    to the built-in operator (non-negative, int or tuple)
    """
    def _check_range(axes):
        for axis in axes:
            check_axis_in_range(axis, ndim)

    if axes is None:
        axes = tuple(range(ndim))
        return axes
    if isinstance(axes, (tuple, list)):
        _check_range(axes)
        tmp = ()
        for x in axes:
            tmp = tmp + ((x + ndim) % ndim,)
        _check_dup(tmp)
        return tmp
    check_axis_in_range(axes, ndim)
    return (axes % ndim,)


def max_(*args):
    """Return the maximum value of the input parameter."""
    return max(*args)


def min_(*args):
    """Return the minimum value of the input parameter."""
    return min(*args)


def is_stub_tensor(tensor):
    return hasattr(tensor, "stub")


def expanded_shape(ndim, axis_size, axis):
    """
    Returns a shape with size = 1 for all dimensions
    except at axis.
    """
    return tuple(axis_size if i == axis else 1 for i in range(ndim))


def tuple_slice(tup, start, end):
    """get sliced tuple from start and end."""
    return tup[start:end]


def infer_out_shape(*shapes):
    """
    Returns shape of output after broadcasting. Raises ValueError if shapes cannot be broadcast.
    """
    def _check(items, max_size, shapes):
        for item in items:
            if item not in (1, max_size):
                raise ValueError(f'For Tensor, the dimension on each axis must be 1 or the max value on the axis' \
                                 f'to support broadcasting, but got shapes {shapes,}')
    shape_out = ()
    max_len = max([len(it) for it in shapes])
    for i in range(max_len):
        items = [it[i-(max_len-len(it))] if i - (max_len - len(it))
                 >= 0 else 1 for it in shapes]
        max_size = 0 if 0 in items else max(items)
        _check(items, max_size, shapes)
        shape_out = shape_out + (max_size,)
    return shape_out


def check_axis_type(axis, type_int=True, type_tuple=True, type_list=True):
    """Check axis argument type."""
    if type_int and isinstance(axis, int):
        return True
    if (type_tuple and isinstance(axis, tuple)) or (type_list and isinstance(axis, list)):
        for ax in axis:
            if not isinstance(ax, int):
                raise TypeError(f"For Tensor.ptp, each axis must be integer, but got {type(ax)} in {axis}.")
        return True

    type_str = ""
    if type_int:
        type_str += "int, "
    if type_tuple:
        type_str += "tuple, "
    if type_list:
        type_str += "list, "
    raise TypeError(f"For Tensor.ptp, the axis should be {type_str}, but got {type(axis)}.")


def check_and_canonicalize_axes(axes, ndim):
    """Check whether the types and values of input axes are valid."""
    def _check(axes, ax, ndim):
        if not isinstance(ax, int):
            raise TypeError(f"Each axis should be integer, but got {type(ax)} in {axes}.")
        if ax >= ndim or ax < -ndim:
            raise ValueError(f"The 'axis' must be in the range of [-{ndim}, {ndim}), but got {ax}.")

    axes = axes if isinstance(axes, tuple) else (axes,)
    new_axes = ()
    for ax in axes:
        _check(axes, ax, ndim)
        ax = ax if ax >= 0 else ax + ndim
        new_axes += (ax,)
    _check_dup(new_axes)
    return new_axes


def check_type_support(dtype, device, supported_dtypes):
    """Checks whether the data type is supported."""
    return dtype in supported_dtypes or not context.get_context('device_target') == device


def check_sparse_tensor_input(indices, values, shape):
    """Common input check for SparseTensors."""
    if not isinstance(indices, Tensor_) and not is_stub_tensor(indices):
        raise TypeError(f"For SparseTensors, 'indices' must be Tensor, but got {type(indices)}.")
    if not isinstance(values, Tensor_) and not is_stub_tensor(values):
        raise TypeError(f"For SparseTensors, 'values' must be Tensor, but got {type(values)}.")
    if not isinstance(shape, tuple):
        raise TypeError(f"For SparseTensors, 'shape' must be tuple, but got {type(shape)}.")


def check_csr_tensor_input(indptr, indices, values, shape):
    """Checks inputs type for CSRTensor."""
    if not isinstance(indptr, Tensor_) and not is_stub_tensor(indptr):
        raise TypeError(f"For CSRTensor, 'indptr' must be Tensor, but got {type(indptr)}.")
    check_sparse_tensor_input(indices, values, shape)


def check_csr_tensor_shape(indptr_shp, indices_shp, values_shp, csr_shp):
    """Checks input tensors' shapes for CSRTensor."""
    # Support empty sparse tensor
    if (indptr_shp == (0,)) and (indices_shp == (0,)) and (values_shp == (0,)):
        return
    shape_size = 1
    val_shp_size = 1
    for item in csr_shp:
        if item <= 0:
            raise ValueError(f"For CSRTensor, the element of shape must be positive, but got {item}")
        if not isinstance(item, int):
            raise TypeError(f"For CSRTensor, the element type of shape must be int, but got {type(item)}")
        shape_size *= item
    for item in values_shp:
        if item <= 0:
            raise ValueError(f"The element of shape must be positive, but got {item}")
        val_shp_size *= item
    if shape_size < val_shp_size:
        raise ValueError(f"Shape total size: {shape_size} is too small to hold {val_shp_size} non-zero values.")
    if len(indices_shp) != 1:
        raise ValueError(f"For CSRTensor, indices must be a 1-dimensional tensor, " \
                         f"but got a {len(indices_shp)} dimension tensor.")
    if len(indptr_shp) != 1:
        raise ValueError(f"For CSRTensor, indptr must be a 1-dimensional tensor, " \
                         f"but got a {len(indptr_shp)} dimension tensor.")
    if csr_shp[0] + 1 != indptr_shp[0]:
        raise ValueError(f"For CSRTensor, indptr must have length (1 + shape[0]), " \
                         f"but got: {indptr_shp[0]}")
    if indices_shp[0] != values_shp[0]:
        err_msg1 = "For CSRTensor, indices and values must equal in their shape, "
        err_msg2 = f"but got indices shape: {indices_shp[0]}, values shape: {values_shp[0]}."
        raise ValueError(err_msg1 + err_msg2)
    if len(values_shp) + 1 != len(csr_shp):
        raise ValueError(f"Values' dimension should equal to CSRTensor's dimension - 1, but got" \
                         f"Values' dimension: {len(values_shp)} , CSRTensor's dimension: " \
                         f"{len(csr_shp)}")
    if values_shp[1:] != csr_shp[2:]:
        raise ValueError(f"CSRTensor's shape[2: ] must be equal to value's shape[1: ]," \
                         f"but CSRTensor's shape[2: ] got: {csr_shp[2: ]} and value's shape[1: ]" \
                         f"got: {values_shp[1: ]}")


def check_csr_tensor_dtype(indptr_dtype, indices_dtype):
    """Checks input tensors' data types for CSRTensor."""
    if indptr_dtype not in (mstype.int16, mstype.int32, mstype.int64):
        raise TypeError(f"For CSRTensor, indptr must have int16 or int32 or int64 data type, " \
                        f"but got {indptr_dtype}.")
    if indices_dtype not in (mstype.int16, mstype.int32, mstype.int64):
        raise TypeError(f"For CSRTensor, indices must have int16 or int32 or int64 data type, " \
                        f"but got {indices_dtype}.")


def check_coo_tensor_input(indices, values, shape):
    """Checks inputs type for COOTensor."""
    check_sparse_tensor_input(indices, values, shape)


def check_coo_tensor_shape(indices_shp, values_shp, coo_shp):
    """Checks input tensors' shapes for COOTensor."""
    if len(coo_shp) != 2:
        raise ValueError(f"For COOTensor, the length of 'shape' must be 2, but got {coo_shp}.")
    if (indices_shp == (0,)) and (values_shp == (0,)):
        return
    shp_mul = 1
    for sh in coo_shp:
        if sh <= 0:
            raise ValueError(f"For COOTensor, the element of 'shape' must be positive, but got {sh} in {coo_shp}.")
        if not isinstance(sh, int):
            raise TypeError(f"For COOTensor, the element type of 'shape' must be int, but got {type(sh)}")
        shp_mul *= sh
    if shp_mul < values_shp[0]:
        raise ValueError(f"For COOTensor, shape is too small: ({shp_mul}) to hold all values({values_shp[0]}).")
    if len(indices_shp) != 2:
        raise ValueError(f"For COOTensor, 'indices' must be a 2-dimensional tensor, but got a {len(indices_shp)}" \
                         f"-dimensional tensor.")
    if len(values_shp) != 1:
        raise ValueError(f"For COOTensor, 'values' must be a 1-dimensional tensor, but got a {len(values_shp)}" \
                         f"-dimensional tensor.")
    if indices_shp[0] != values_shp[0]:
        raise ValueError(f"For COOTensor, 'indices.shape[0]' must be euqal to 'values.shape[0]', but got " \
                         f"'indices.shape[0]' = {indices_shp[0]} and 'values.shape[0]' = {values_shp[0]}.")
    if indices_shp[1] != 2:
        raise ValueError(f"For COOTensor, 'indices.shape[1]' must be 2, but got {indices_shp[1]}.")


def check_coo_tensor_dtype(indices_dtype):
    """Checks input tensors' data types for COOTensor."""
    if indices_dtype not in (mstype.int16, mstype.int32, mstype.int64):
        raise TypeError(f"For COOTensor, the type of 'indices' must be one of [int16, int32, int64], but got " \
                        f"{indices_dtype}.")


def check_dynamic_shape(dyn_elem, actual_input, i):
    """Check the consistency of dynamic shape tensors and actual input tensors."""
    if dyn_elem.dtype != actual_input.dtype:
        raise TypeError(f"The data type of '{i}'th args in actual input tensors should be '{dyn_elem.dtype}', " \
                        f"but got '{actual_input.dtype}'.")
    if -2 in dyn_elem.shape:
        return
    if dyn_elem.ndim != actual_input.ndim:
        raise ValueError(f"The dimension of '{i}'th args in actual input tensors should be '{dyn_elem.ndim}', " \
                         f"but got '{actual_input.ndim}'.")
    check_dyn_shape_value_equal(i, dyn_elem.shape, actual_input.shape)


def check_element_type_of_iterable(arg_name, arg_value, valid_types, prim_name=None):
    """Check type of the element of a iterabel object, execpt dict."""
    check_value_type(arg_name, arg_value, [list, tuple], prim_name)
    type_names = [t.__name__ if hasattr(t, '__name__') else str(t) for t in valid_types]
    num_types = len(valid_types)
    msg_prefix = f"For '{prim_name}', the" if prim_name else "The"
    for element in arg_value:
        if not isinstance(element, tuple(valid_types)):
            raise TypeError(f"{msg_prefix} type of '{arg_name}' should be {'one of ' if num_types > 1 else ''}" \
                            f"{type_names if num_types > 1 else type_names[0]}, " \
                            f"but got '{element}' with type '{type(element).__name__}'.")


def check_element_type_of_dict(arg_name, arg_value, key_types, value_types, prim_name=None):
    """Check the type of key and value of a dict."""
    check_value_type(arg_name, arg_value, [dict], prim_name)
    msg_prefix = f"For '{prim_name}', the" if prim_name else "The"
    type_names = [t.__name__ if hasattr(t, '__name__') else str(t) for t in key_types]
    num_types = len(key_types)
    for element in arg_value.keys():
        if not isinstance(element, tuple(key_types)):
            raise TypeError(f"{msg_prefix} type of '{arg_name}' should be {'one of ' if num_types > 1 else ''}" \
                            f"{type_names if num_types > 1 else type_names[0]}, " \
                            f"but got '{element}' with type '{type(element).__name__}'.")

    type_names = [t.__name__ if hasattr(t, '__name__') else str(t) for t in value_types]
    num_types = len(value_types)
    for element in arg_value.values():
        if not isinstance(element, tuple(value_types)):
            raise TypeError(f"{msg_prefix} type of '{arg_name}' should be {'one of ' if num_types > 1 else ''}" \
                            f"{type_names if num_types > 1 else type_names[0]}, " \
                            f"but got '{element}' with type '{type(element).__name__}'.")


def check_size_and_element_type_of_tuple(arg_name, arg_value, expect_size, expect_element_type, prim_name=None):
    """Check the size and element type of a tuple."""
    check_value_type(arg_name, arg_value, [tuple], prim_name)
    check_equal_int(len(arg_value), expect_size, arg_name + ' size', prim_name)
    check_element_type_of_iterable('arg_name', arg_value, [expect_element_type], prim_name)


def check_dyn_shape_value_equal(index, dyn_shape, actual_shape):
    """Check the consistency of dynamic shape and actual input shape."""
    for i, x in enumerate(dyn_shape):
        if x not in (-1, actual_shape[i]):
            raise ValueError(f"The {i}th shape value of `{index}`th actual input args should be `{x}`, but got " \
                             f"`{actual_shape[i]}`.")


def _check_symbol(dyn_input, net_input, index, symbolic_shape_data):
    """Check symbolic shape values."""
    actual_shape = net_input.shape
    for i, sym in enumerate(dyn_input.symbolic_shape):
        # the Symbol is converted to dict
        if not isinstance(sym, dict):
            continue
        # the value of symbols with same "id" should be equal.
        if "id" in sym:
            sym_id = sym["id"]
            if "unique_id_value" not in symbolic_shape_data:
                symbolic_shape_data["unique_id_value"] = {}
            unique_id_value = symbolic_shape_data["unique_id_value"]
            if sym_id not in unique_id_value:
                unique_id_value[sym_id] = actual_shape[i]
            elif unique_id_value[sym_id] != actual_shape[i]:
                raise ValueError(
                    f"The {i + 1}th shape value of {index + 1}th actual input args is a unique symbol, all values must "
                    f"be the same. The previous value is {unique_id_value[sym_id]}, but the current value is "
                    f"{actual_shape[i]}. Actual shape: {actual_shape}, axis: {i}.")
        # check the value in range [min, max].
        if "min" in sym and actual_shape[i] < sym["min"]:
            raise ValueError(
                f"The {i + 1}th shape value of {index + 1}th actual input args must be greater than or equal to the "
                f"'min' value '{sym['min']}' of `Symbol`, but got '{actual_shape[i]}'.  Actual shape: {actual_shape}, "
                f"axis: {i}.")
        if "max" in sym and actual_shape[i] > sym["max"]:
            raise ValueError(
                f"The {i + 1}th shape value of {index + 1}th actual input args must be less than or equal to the "
                f"'max' value '{sym['max']}' of `Symbol`, but got '{actual_shape[i]}'. Actual shape: {actual_shape}, "
                f"axis: {i}.")
        # check the shape item that satisfies the "divisor * N + remainder, N >= 1".
        d = sym.get("divisor", 1)
        r = sym.get("remainder", 0)
        if actual_shape[i] < d or actual_shape[i] % d != r:
            raise ValueError(
                f"The {i + 1}th shape value of {index + 1}th actual input args must be match the 'divisor'(d) and "
                f"'remainder'(r) of `Symbol`. The value should be 'd * N + r' for 'N > 0', got d={d} and r={r}, but "
                f"actual shape value is '{actual_shape[i]}'. Actual shape: {actual_shape}, axis: {i}")


def check_symbolic_shape(dynamic_inputs, actual_inputs):
    """Check the symboic shape"""
    symbolic_shape_data = {}

    def run_check(dyn_inputs, net_inputs):
        """the real checking function"""
        for index, (dyn_input, net_input) in enumerate(zip(dyn_inputs, net_inputs)):
            if isinstance(dyn_input, (tuple, list)):
                run_check(dyn_input, net_input)
            elif hasattr(dyn_input, "symbolic_shape"):
                _check_symbol(dyn_input, net_input, index, symbolic_shape_data)

    run_check(dynamic_inputs, actual_inputs)


def check_input_format(input_param):
    """Judge input format."""
    if input_param == "NCHW":
        return input_param
    raise ValueError(f"The data format must be NCHW, but got {input_param}.")


def _expand_tuple(n_dimensions):
    """To expand an int number to tuple."""

    def convert(m):
        if not isinstance(m, tuple):
            if isinstance(m, int) and not isinstance(m, bool):
                return tuple(repeat(m, n_dimensions))
            raise TypeError(f"When expanding an int number to tuple, input type must be integer or tuple[int], " \
                            f"but got {type(m)}")

        if not len(m) is n_dimensions:
            raise TypeError(f"When expanding an int number to tuple, input tuple dimension must be {n_dimensions}, " \
                            f"but got {m}")

        for i in m:
            if not isinstance(i, int) or isinstance(i, bool):
                raise TypeError(f"When expanding an int number to tuple, " \
                                f"the type of element in input tuple must be an integer, but got {type(i)}.")
        return m

    return convert


def _check_data_type_valid(data, valid_type):
    """Check data type valid."""
    if valid_type is None:
        return data is None
    if isinstance(data, valid_type):
        if hasattr(data, 'size') and data.size == 0:
            msg = "The input data can not be empty."
            logger.critical(msg)
            raise ValueError(msg)
        return True
    return False


def check_input_data(*data, data_class):
    """Input data check."""
    for item in data:
        if isinstance(item, (list, tuple)):
            for v in item:
                check_input_data(v, data_class=data_class)
        elif isinstance(item, dict):
            for v in item.values():
                check_input_data(v, data_class=data_class)
        else:
            if isinstance(data_class, (tuple, list)):
                ret = True in tuple(_check_data_type_valid(item, data_type) for data_type in data_class)
            else:
                ret = _check_data_type_valid(item, data_class)
            if not ret:
                data_class_str = tuple(i.__name__ if hasattr(i, '__name__') else i for i in data_class) if isinstance(
                    data_class, (tuple, list)) else (data_class if data_class is None else data_class.__name__)
                raise TypeError(f'The types of input data must be in the Union({data_class_str}, ' \
                                f'tuple[{data_class_str}], list[{data_class_str}], dict[{data_class_str}]), ' \
                                f'but got type {item if item is None else type(item).__name__}.')


def check_input_dataset(*dataset, dataset_type):
    """Input dataset check."""
    if not dataset:
        return False
    for item in dataset:
        if not isinstance(item, dataset_type):
            return False
    return True


def check_output_data(data):
    """Output data check."""
    if data is None:
        raise RuntimeError('The output data can not be None, please check your net or input data.')


once = _expand_tuple(1)
twice = _expand_tuple(2)
triple = _expand_tuple(3)


def args_type_check(*type_args, **type_kwargs):
    """Check whether input data type is correct."""

    def type_check(func):
        sig = inspect.signature(func)
        bound_types = sig.bind_partial(*type_args, **type_kwargs).arguments

        @wraps(func)
        def wrapper(*args, **kwargs):
            nonlocal bound_types
            bound_values = sig.bind(*args, **kwargs)
            argument_dict = bound_values.arguments
            if "kwargs" in bound_types:
                bound_types = bound_types["kwargs"]
            if "kwargs" in argument_dict:
                argument_dict = argument_dict["kwargs"]
            for name, value in argument_dict.items():
                if name in bound_types:
                    if value is not None and not isinstance(value, bound_types[name]):
                        raise TypeError(f"The parameter '{name}' must be {bound_types[name]}, but got {type(value)}")
            return func(*args, **kwargs)

        return wrapper

    return type_check


def check_hook_fn(hook_type, hook_fn):
    """Check hook fn"""
    if context.get_context("mode") != context.PYNATIVE_MODE:
        logger.warning(f"'{hook_type}' function is only supported in pynative mode, you can use "
                       f"context.set_context to set pynative mode.")
        return False

    if not isinstance(hook_fn, (FunctionType, MethodType)):
        raise TypeError(f"When using 'hook_type(hook_fn)', the type of 'hook_fn' must be python "
                        f"function, but got {type(hook_fn)}.")

    if hook_fn.__code__.co_name == "staging_specialize":
        raise TypeError(f"Decorating hook function {hook_fn.__name__} with '@jit' is not supported.")

    if hook_type == "register_hook" and hook_fn.__code__.co_argcount != 1:
        raise TypeError(f"Tensor hook function {hook_fn.__name__} arg num is not equal to 1.")

    return True

_set_record = {}
