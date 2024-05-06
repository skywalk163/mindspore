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
# ============================================================================

"""

Array Operators

"""

from mindspore.ops.operations.array_ops import ArgMaxWithValue, ArgMinWithValue
from mindspore.ops._primitive_cache import _get_cache_prim
from mindspore.ops.auto_generate.gen_ops_prim import gather_d_op

# define Primitive global variables


def gather(input, dim, index):
    r"""
    Gather data from a tensor by indices.

    .. math::
        output[(i_0, i_1, ..., i_{dim}, i_{dim+1}, ..., i_n)] =
        input[(i_0, i_1, ..., index[(i_0, i_1, ..., i_{dim}, i_{dim+1}, ..., i_n)], i_{dim+1}, ..., i_n)]

    .. warning::
        On Ascend, the behavior is unpredictable in the following cases:

        - the value of `index` is not in the range `[-input.shape[dim], input.shape[dim])` in forward;
        - the value of `index` is not in the range `[0, input.shape[dim])` in backward.

    Args:
        input (Tensor): The target tensor to gather values.
        dim (int): the axis to index along, must be in range `[-input.rank, input.rank)`.
        index (Tensor): The index tensor, with int32 or int64 data type. An valid `index` should be:

            - `index.rank == input.rank`;
            - `index.shape[axis] <= input.shape[axis]` where axis goes through all dimensions of `input` except `dim`;
            - the value of `index` is in range `[-input.shape[dim], input.shape[dim])`.

    Returns:
        Tensor, has the same type as `input` and the same shape as `index`.

    Raises:
        ValueError: If the shape of `index` is illegal.
        ValueError: If `dim` is not in `[-input.rank, input.rank)`.
        ValueError: If the value of `index` is out of the valid range.
        TypeError: If the type of `index` is illegal.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> input = Tensor(np.array([[-0.1, 0.3, 3.6], [0.4, 0.5, -3.2]]), mindspore.float32)
        >>> index = Tensor(np.array([[0, 0], [1, 1]]), mindspore.int32)
        >>> output = ops.extend.gather(input, 1, index)
        >>> print(output)
        [[-0.1 -0.1]
        [ 0.5  0.5]]
    """
    return gather_d_op(input, dim, index)


def max(input, dim, keepdim=False):
    """
    Calculates the maximum value along with the given axis for the input tensor.

    Args:
        input (Tensor): The input tensor, can be any dimension. Complex tensor is not supported for now.
        dim (int): The dimension to reduce.
        keepdim (bool): Whether to reduce dimension, if true, the output will keep same dimension with the input,
            the output will reduce dimension if false. Default: ``False`` .

    Returns:
        tuple (Tensor), tuple of 2 tensors, containing the maximum value of the input tensor and the corresponding
        index.

        - values (Tensor) - The maximum value of input tensor, with same dtype as `input`. If `keepdim`
          is true, the shape of output tensors is :math:`(input_1, input_2, ..., input_{axis-1}, 1, input_{axis+1},
          ..., input_N)` . Otherwise, the shape is :math:`(input_1, input_2, ..., input_{axis-1}, input_{axis+1},
          ..., input_N)` .
        - index (Tensor) - The index for the maximum value of the input tensor, with the same shape as `values`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> y = Tensor(np.array([[0.0, 0.3, 0.4, 0.5, 0.1],
        ...                      [3.2, 0.4, 0.1, 2.9, 4.0]]), mindspore.float32)
        >>> output, index = ops.extend.max(y, 0, True)
        >>> print(output, index)
        [[3.2 0.4 0.4 2.9 4. ]] [[1 1 0 1 1]]
    """
    argmax_with_value_op = _get_cache_prim(ArgMaxWithValue)(dim, keepdim)
    indices, values = argmax_with_value_op(input)
    return values, indices


def min(input, dim, keepdim=False):
    """
    Calculates the minimum value along with the given axis for the input tensor.

    Args:
        input (Tensor): The input tensor, can be any dimension. Complex tensor is not supported for now.
        dim (int): The dimension to reduce.
        keepdim (bool): Whether to reduce dimension, if true, the output will keep same dimension with the input,
            the output will reduce dimension if false. Default: ``False`` .

    Returns:
        tuple (Tensor), tuple of 2 tensors, containing the minimum value of the input tensor and the corresponding
        index.

        - values (Tensor) - The minimum value of input tensor, with same dtype as `input`. If `keepdim`
          is true, the shape of output tensors is :math:`(input_1, input_2, ..., input_{axis-1}, 1, input_{axis+1},
          ..., input_N)` . Otherwise, the shape is :math:`(input_1, input_2, ..., input_{axis-1}, input_{axis+1},
          ..., input_N)` .
        - index (Tensor) - The index for the minimum value of the input tensor, with same shape as `values`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([0.0, 0.4, 0.6, 0.7, 0.1]), mindspore.float32)
        >>> output, index = ops.extend.min(x, 0, keepdim=True)
        >>> print(output, index)
        [0.0] [0]
    """
    argmin_with_value_op = _get_cache_prim(ArgMinWithValue)(dim, keepdim)
    indices, values = argmin_with_value_op(input)
    return values, indices

__all__ = ['gather', 'max', 'min']
