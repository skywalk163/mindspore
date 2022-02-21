# Copyright 2021 Huawei Technologies Co., Ltd
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
"""internal utility functions"""
from .. import ops
from .. import numpy as mnp
from ..numpy import where, zeros_like, dot, greater
from ..ops import functional as F
from ..common import Tensor
from ..common import dtype as mstype
from .utils_const import _type_convert, _raise_value_error, _callable_const, _super_check
from ..ops.composite import GradOperation

grad = GradOperation(get_all=False, get_by_list=False, sens_param=False)
_eps_net = ops.Eps()


def _convert_64_to_32(tensor):
    """Convert Tensor with float64/int64 types to float32/int32."""
    if tensor.dtype == mstype.float64:
        return tensor.astype("float32")
    if tensor.dtype == mstype.int64:
        return tensor.astype("int32")
    return tensor


def _to_tensor(*args, dtype=None):
    """Returns each input as Tensor"""
    res = ()
    for arg in args:
        if isinstance(arg, (int, float, bool, list, tuple)):
            arg = _type_convert(Tensor, arg)
            if dtype is None:
                arg = _convert_64_to_32(arg)
            else:
                arg = arg.astype(dtype)
        elif not isinstance(arg, Tensor):
            _raise_value_error("Expect input to be array like.")
        res += (arg,)
    if len(res) == 1:
        return res[0]
    return res


def _to_scalar(arr):
    """Convert a scalar Tensor or ndarray to a scalar."""
    if isinstance(arr, (int, float, bool)):
        return arr
    if isinstance(arr, Tensor):
        if arr.shape:
            return arr
        return arr.asnumpy().item()
    raise ValueError("{} are not supported.".format(type(arr)))


def _eps(x):
    return _eps_net(x[(0,) * x.ndim])


def _safe_normalize(x, threshold=None):
    """Normalize method that cast very small results to zero."""
    x_sum2 = F.reduce_sum(F.pows(x, 2.0))
    norm = F.pows(x_sum2, 1. / 2.0)
    if threshold is None:
        if x.dtype in (mstype.float32, mstype.float64):
            # pick the first element of x to get the eps
            threshold = _eps(x)
        else:
            threshold = 0
    use_norm = greater(norm, threshold)
    x_norm = x / norm
    normalized_x = where(use_norm, x_norm, zeros_like(x))
    norm = where(use_norm, norm, zeros_like(norm))
    return normalized_x, norm


def _normalize_matvec(f):
    """Normalize an argument for computing matrix-vector products."""
    if _callable_const(F.typeof(f)):
        return f

    if isinstance(f, Tensor):
        if f.ndim != 2 or f.shape[0] != f.shape[1]:
            _raise_value_error(
                'linear operator must be a square matrix, but has shape: ', f.shape, ".")
        return F.partial(dot, f)

    _raise_value_error(
        'linear operator must be either a function or Tensor: but got ', F.typeof(f), ".")
    return f


def _norm(x, ord_=None):
    if ord_ == mnp.inf:
        res = mnp.max(mnp.abs(x))
    else:
        res = mnp.sqrt(mnp.sum(x ** 2))
    return res


def _nd_transpose(a):
    dims = a.ndim
    if dims < 2:
        _raise_value_error("to do _nd_transpose for input a's ndim is not greater or equal to 2d, which is invalid.")
    axes = ops.make_range(0, dims)
    axes = axes[:-2] + (axes[-1],) + (axes[-2],)
    return ops.transpose(a, axes)


def _value_op_check(op, arg_value, valid_value, prim_name=None, arg_name=None, fmt="attr", msg=None):
    return _super_check(op, arg_value, valid_value, prim_name, arg_name, fmt, msg, True)


def _value_in_check(arg_value, valid_value, prim_name=None, arg_name=None, fmt="attr", msg=None):
    return _super_check("in", arg_value, valid_value, prim_name, arg_name, fmt, msg, True)


def _type_op_check(op, arg_value, valid_value, prim_name=None, arg_name=None, fmt="type", msg=None):
    return _super_check(op, arg_value, valid_value, prim_name, arg_name, fmt, msg, False)


def _type_in_check(arg_value, valid_value, prim_name=None, arg_name=None, fmt="attr", msg=None):
    return _super_check("in", arg_value, valid_value, prim_name, arg_name, fmt, msg, False)


def _type_is_check(arg_value, valid_value, prim_name=None, arg_name=None, fmt="type", msg=None):
    return _super_check("isinstance", arg_value, valid_value, prim_name, arg_name, fmt, msg, False)


def _is_tensor_check(arg_value, valid_value, prim_name=None, arg_name=None, fmt="tensor", msg=None):
    return _super_check("istensor", arg_value, valid_value, prim_name, arg_name, fmt, msg, False)
