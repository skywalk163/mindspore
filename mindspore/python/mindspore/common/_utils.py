# This is the Python adaptation and derivative work of Myia (https://github.com/mila-iqia/myia/).
#
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
"""common utils."""

import os
import math
import ctypes
import functools

from mindspore import log as logger
from mindspore.common import dtype as mstype
from mindspore.parallel._ps_context import _is_ps_mode, _is_role_pserver, _is_role_sched


def is_shape_unknown(shape):
    """Check whether the shape is unknown."""
    flag = False
    for i in shape:
        if i < -2:
            raise ValueError(f"'shape' should not have values less than -2 but got ({shape}).")
        if i == -1:
            flag = True
    return is_dim_unknown(shape) or flag


def is_dim_unknown(shape):
    """Check whether the dim is unknown."""
    if len(shape) == 1 and shape[0] == -2:
        return True
    if -2 in shape:
        raise ValueError(f"'shape' should have only one -2 or no -2 at all but got ({shape}).")
    return False


def get_slice_num(dtype, shape):
    """Check whether size of data is too huge, and cut it to a smaller one, return slice num."""
    slice_num = 1
    need_split = _is_ps_mode() and (_is_role_pserver() or _is_role_sched())

    if not need_split:
        return slice_num

    if "MS_EMBEDDING_REMOTE_CACHE_MEMORY_SIZE" not in os.environ:
        return slice_num

    num_element = functools.reduce(lambda x, y: x * y, shape, 1)
    data_size = num_element * mstype.type_size_in_bytes(dtype)
    remote_cache_size = int(os.getenv("MS_EMBEDDING_REMOTE_CACHE_MEMORY_SIZE")) << 30
    if remote_cache_size == 0:
        raise ValueError("The remote cache size set by env variable[MS_EMBEDDING_REMOTE_CACHE_MEMORY_SIZE]"
                         " should be greater than 0.")

    if data_size <= remote_cache_size:
        return slice_num

    slice_num = math.ceil(data_size / remote_cache_size)
    return slice_num


def get_slice_shape(dtype, shape):
    """Check whether size of data is too huge, and cut it to a smaller one, return slice shape."""
    slice_num = get_slice_num(dtype, shape)
    if slice_num == 1:
        return shape

    new_shape = list(shape)
    if slice_num == 0:
        raise ValueError("The slice num is at least 1, but got 0")
    slice_first_dim = math.ceil(new_shape[0] / slice_num)
    new_shape[0] = slice_first_dim
    return tuple(new_shape)


def _jit_fallback_raise_func(type_name, script):
    """raise function for jit fallback."""
    raise type_name(script)


def _jit_fallback_set_attr(class_obj, attr_name, target_obj):
    """Set attr for object and return the object for jit fallback."""
    setattr(class_obj, attr_name, target_obj)
    return target_obj


def load_lib(lib_path):
    """load specified library."""
    try:
        ctypes.CDLL(lib_path)
    # pylint: disable=broad-except
    except Exception:
        logger.warning(f'Loading {lib_path} lib error.')
        return False
    return True


def _jit_fallback_next_func(xs):
    """Generate ms_next for xs"""
    return xs[0], xs[1:]


def _jit_fallback_has_next_func(xs):
    """Determine whether xs has next value"""
    return len(xs) > 0


def _jit_fallback_len_func(obj):
    """Calculate length for obj"""
    return len(obj)
