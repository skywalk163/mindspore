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
""" padding """
from __future__ import absolute_import

from mindspore.common import Tensor
from mindspore import ops
from mindspore.ops.operations import nn_ops
from mindspore.ops.primitive import constexpr, _primexpr
from mindspore.nn.cell import Cell

__all__ = ['ConstantPad1d', 'ConstantPad2d', 'ConstantPad3d', 'ReflectionPad1d', 'ReflectionPad2d', 'ReflectionPad3d',
           'ZeroPad2d', 'ReplicationPad1d', 'ReplicationPad2d', 'ReplicationPad3d']


def _check_dim(dim, target1, target2, cls_name):
    """check dim"""
    def _check_func():
        if dim not in (target1, target2):
            raise ValueError(f"For '{cls_name}', the in_shape must have {target1} or {target2} dims, but got {dim}.")
    _check_func()


@_primexpr
def _check_padding_dimension(dimension, padding):
    r"""
    Validate the input padding and add placeholders if needed.
    Note: the input 'padding' in this function is already converted to list of lists to match MirrorPad
    """
    def _check_padding(dimension, padding):
        if dimension < len(padding):
            raise ValueError(f"For padding with length {len(padding) * 2}, the dimension of the tensor should "
                             f"be at least {len(padding)}, but got {dimension}")
    _check_padding(dimension, padding)
    # add place holders
    if dimension > len(padding):
        padding = [(0, 0) for _ in range(dimension - len(padding))] + [x for x in padding]
    return padding


def _swap_to_ms_padding_order(padding):
    r"""
    Check whether the input padding is a tuple or an int converted to a tuple.
    Check if the length of padding is divisible by 2.
    Convert the input padding to the format that MirrorPad would understand.
    """
    number_of_paddings = len(padding) // 2
    new_padding = [[0, 0]] * number_of_paddings
    for i in range(number_of_paddings):
        new_padding[i] = [padding[2 * i], padding[2 * i + 1]]
    # reverse the padding list to match the order of paddings for MirrorPad
    new_padding.reverse()
    return new_padding


@_primexpr
def _check(input_shape, padding, name):
    """
    Check relationship between input shape and padding to make sure after negative dimension padding the out is
    positive.
    """
    def _check_len(input_shape, padding):
        if len(input_shape) < len(padding):
            msg = f"For '{name}', the dimension of input must more than or equal to len(padding)/2, " \
                  f"but got {len(input_shape)}"
            raise ValueError(msg)

    def _check_item(item, input_shape, index, dim_name):
        if item < -input_shape[index]:
            msg = f"For '{name}', the shape of input after padding must be positive, the input shape is " \
                  f"{input_shape}, value of parameter 'padding' applied to the {dim_name} dimension of input must " \
                  f"no less than -{input_shape[index]}, but got {item}"
            raise ValueError(msg)

    def _check_item_two(item0, item1, input_shape, index, dim_name):
        if input_shape[index] + item0 + item1 <= 0:
            msg = f"For '{name}', the shape of input after padding must be positive, the input shape is " \
                  f"{input_shape}, but the {dim_name} dimension of input shape {input_shape[index]} plus padding " \
                  f"{item0} and {item1} resulted in a non-positive output shape."
            raise ValueError(msg)

    _check_len(input_shape, padding)
    if len(input_shape) > len(padding):
        if len(padding) == 2 and isinstance(padding[0], int):
            padding = [(0, 0) for i in range(len(input_shape) - 1)] + [padding]
        else:
            padding = [(0, 0) for i in range(len(input_shape) - len(padding))] + [x for x in padding]
    for index, item in enumerate(padding):
        if index == 0:
            dim_name = '1st'
        elif index == 1:
            dim_name = '2nd'
        elif index == 2:
            dim_name = '3rd'
        else:
            dim_name = str(index + 1) + 'th'

        _check_item(item[0], input_shape, index, dim_name)
        _check_item(item[1], input_shape, index, dim_name)
        _check_item_two(item[0], item[1], input_shape, index, dim_name)
    return padding


@_primexpr
def _get_new_padding(padding):
    """get non-negative padding and make negative position."""
    new_padding = [[item[0], item[1]] for item in padding]
    start = [0 for i in range(len(new_padding))]
    end = [0 for i in range(len(new_padding))]
    for index in range(len(new_padding)):
        item = new_padding[index]
        if item[0] < 0:
            start[index] = item[0]
            new_padding[index][0] = 0
        if item[1] < 0:
            end[index] = item[1]
            new_padding[index][1] = 0
    new_padding = tuple(new_padding)
    return new_padding, start, end


@_primexpr
def _get_begin_size(shape, begin, end):
    """Calculate begin and size for ops.Slice."""
    size = tuple([shape[i] + begin[i] + end[i] for i in range(len(shape))])
    begin = tuple([int(-i) for i in begin])
    return begin, size


class _ConstantPadNd(Cell):
    r"""
    Using a given value to pads the last n dimensions of input tensor.

    Args:
        padding (union[list, tuple]): The padding size to pad the last n dimensions of input tensor.
            The padding sequence is starting from the last dimension and moving forward.
            The length of padding must be a multiple of 2. If padding is :math:`(padding_0, padding_1, padding_2,
            padding_3, ..., padding_2m, padding_{2m+1}, ...)`. The input is `x`,
            the size of last dimension of output is :math:`padding\_0 + x.shape[-1] + padding\_1`.
            The size of penultimate dimension of output is :math:`padding\_2 + x.shape[-2] + padding\_3`.
            The size of 3rd to last dimension of output is :math:`padding\_4 + x.shape[-3] + padding\_5`.
            The size of i-td to last dimension of output is :math:`padding\_{2m} + x.shape[-m-1] + padding\_{2m+1}`.
            The remaining dimensions of the output are consistent with those of the input.
        value (union[int, float]): Padding value.
        name (str): Name of method, used for positioning error messages in the base class.

    Returns:
        Tensor, the tensor after padding.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Raises:
        ValueError: If the length of padding is not a multiple of 2.
        ValueError: If the length of input less than len(padding)/2.
        ValueError: If the output shape after padding is not positive.
    """

    def __init__(self, padding, value, name='ConstantPadNd'):
        """Initialize Pad."""
        super(_ConstantPadNd, self).__init__()
        if isinstance(padding, int):
            if name == 'ConstantPad1d':
                padding = (padding, padding)
            elif name in ['ConstantPad2d', 'ZeroPad2d']:
                padding = (padding, padding, padding, padding)
            elif name == 'ConstantPad3d':
                padding = (padding, padding, padding, padding, padding, padding)

        elif isinstance(padding, tuple):
            if len(padding) % 2 != 0:
                msg = f"For '{name}', the length of parameter 'padding' with tuple type must be a multiple of 2, " \
                      f"but got {len(padding)}"
                raise ValueError(msg)
            if name == 'ConstantPad1d' and len(padding) != 2:
                msg = f"For '{name}', the length of parameter 'padding' with tuple type must equal to 2." \
                      f"but got {len(padding)}"
                raise ValueError(msg)
            if name in ['ConstantPad2d', 'ZeroPad2d'] and len(padding) > 4:
                msg = f"For '{name}', the length of parameter 'padding' with tuple type must no more than 4." \
                      f"but got {len(padding)}"
                raise ValueError(msg)
            if name == 'ConstantPad3d' and len(padding) > 6:
                msg = f"For '{name}', the length of parameter 'padding' with tuple type must no more than 6." \
                      f"but got {len(padding)}"
                raise ValueError(msg)

        else:
            msg = f"For '{name}', the type of parameter 'padding' must be in [int, tuple], " \
                  f"but got {type(padding)}"
            raise TypeError(msg)

        if not isinstance(value, (int, float)):
            msg = f"For '{name}', the type of parameter 'value' must be in [int, float], " \
                  f"but got {type(value)}"
            raise TypeError(msg)

        self.value = value
        self.padding = padding

    def construct(self, x):
        """Construct the pad net."""
        return ops.pad(x, padding=self.padding, mode='constant', value=self.value)


class ConstantPad1d(_ConstantPadNd):
    r"""
    Using a given constant value to pads the last dimensions of input tensor.

    Args:
        padding (Union[int, tuple]): The padding size to pad the last dimension of input tensor.
            If is int, uses the same padding in both boundaries of input's last dimension.
            If a 2-tuple, uses (padding_0, padding_1) to pad. If the input is `x`, the size of last
            dimension of output is :math:`padding\_0 + x.shape[-1] + padding\_1`. The remaining dimensions
            of the output are consistent with those of the input.
            Only support non-negative value while running in Ascend.
        value (Union[int, float]): Padding value.

    Inputs:
        - **x** (Tensor) - shape is :math:`(N, *)`, where :math:`*` means, any number of additional dimensions.
          It is not supported that the size of dimensions is greater than 5 while running on Ascend.

    Returns:
        Tensor, the tensor after padding.

    Raises:
        TypeError: If `padding` is not a tuple or int.
        TypeError: If `value` is not int or float.
        ValueError: If the length of `padding` with tuple type is not equal to 2.
        ValueError: If the output shape after padding is not positive.
        ValueError: If the rank of 'x' is more than 5 while running in Ascend.
        ValueError: If `padding` contains negative value while running in Ascend.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> x = np.ones(shape=(1, 2, 3, 4)).astype(np.float32)
        >>> x = ms.Tensor(x)
        >>> # padding is tuple
        >>> padding = (0, 1)
        >>> value = 0.5
        >>> pad1d = ms.nn.ConstantPad1d(padding, value)
        >>> out = pad1d(x)
        >>> print(out)
        [[[[1.  1.  1.  1.  0.5]
           [1.  1.  1.  1.  0.5]
           [1.  1.  1.  1.  0.5]]
          [[1.  1.  1.  1.  0.5]
           [1.  1.  1.  1.  0.5]
           [1.  1.  1.  1.  0.5]]]]
        >>> print(out.shape)
        (1, 2, 3, 5)
        >>> # padding is int
        >>> padding = 1
        >>> value = 0.5
        >>> pad1d = ms.nn.ConstantPad1d(padding, value)
        >>> out = pad1d(x)
        >>> print(out)
        [[[[0.5 1.  1.  1.  1.  0.5]
           [0.5 1.  1.  1.  1.  0.5]
           [0.5 1.  1.  1.  1.  0.5]]
          [[0.5 1.  1.  1.  1.  0.5]
           [0.5 1.  1.  1.  1.  0.5]
           [0.5 1.  1.  1.  1.  0.5]]]]
        >>> print(out.shape)
        (1, 2, 3, 6)
        >>> # padding is negative
        >>> padding = (-1, 0)
        >>> value = 0.5
        >>> pad1d = ms.nn.ConstantPad1d(padding, value)
        >>> out = pad1d(x)
        >>> print(out)
        [[[[1. 1. 1.]
           [1. 1. 1.]
           [1. 1. 1.]]
          [[1. 1. 1.]
           [1. 1. 1.]
           [1. 1. 1.]]]]
        >>> print(out.shape)
        (1, 2, 3, 3)
    """

    def __init__(self, padding, value):
        super(ConstantPad1d, self).__init__(padding, value, name='ConstantPad1d')


class ConstantPad2d(_ConstantPadNd):
    r"""
    Using a given constant value to pads the last two dimensions of input tensor.

    Args:
        padding (Union[int, tuple]): The padding size to pad the last two dimensions of input tensor.
            If is int, uses the same padding in boundaries of input's last two dimensions.
            If is tuple and length of padding is 4 uses (padding_0, padding_1, padding_2, padding_3) to pad.
            If the input is `x`, the size of last dimension of output is :math:`padding\_0 + x.shape[-1] + padding\_1`.
            The size of penultimate dimension of output is :math:`padding\_2 + x.shape[-2] + padding\_3`.
            The remaining dimensions of the output are consistent with those of the input.
            Only support non-negative value while running in Ascend.
        value (Union[int, float]): Padding value.

    Inputs:
        - **x** (Tensor) - shape is :math:`(N, *)`, where :math:`*` means, any number of additional dimensions.
          It is not supported that the size of dimensions is greater than 5 while running on Ascend.

    Returns:
        Tensor, the tensor after padding.

    Raises:
        TypeError: If `padding` is not a tuple or int.
        TypeError: If `value` is not int or float.
        ValueError: If the length of `padding` is more than 4 or not a multiple of 2.
        ValueError: If the output shape after padding is not positive.
        ValueError: If the rank of 'x' is more than 5 while running in Ascend.
        ValueError: If `padding` contains negative value while running in Ascend.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> x = np.ones(shape=(1, 2, 3, 4)).astype(np.float32)
        >>> x = ms.Tensor(x)
        >>> padding = (-1, 1, 0, 1)
        >>> value = 0.5
        >>> pad2d = ms.nn.ConstantPad2d(padding, value)
        >>> out = pad2d(x)
        >>> print(out)
        [[[[1.  1.  1.  0.5]
           [1.  1.  1.  0.5]
           [1.  1.  1.  0.5]
           [0.5 0.5 0.5 0.5]]
          [[1.  1.  1.  0.5]
           [1.  1.  1.  0.5]
           [1.  1.  1.  0.5]
           [0.5 0.5 0.5 0.5]]]]
        >>> print(out.shape)
        (1, 2, 4, 4)
    """

    def __init__(self, padding, value):
        super(ConstantPad2d, self).__init__(padding, value, name='ConstantPad2d')


class ConstantPad3d(_ConstantPadNd):
    r"""
    Using a given constant value to pads the last three dimensions of input tensor.

    Args:
        padding (Union[int, tuple]): The padding size to pad the last three dimensions of input tensor.
            If is int, uses the same padding in boundaries of input's last three dimensions.
            If is tuple and length of padding is 6 uses
            (padding_0, padding_1, padding_2, padding_3, padding_4, padding_5) to pad. If the input is `x`,
            the size of last dimension of output is :math:`padding\_0 + x.shape[-1] + padding\_1`.
            The size of penultimate dimension of output is :math:`padding\_2 + x.shape[-2] + padding\_3`.
            The size of 3rd to last dimension of output is :math:`padding\_4 + x.shape[-3] + padding\_5`.
            The remaining dimensions of the output are consistent with those of the input.
            Only support non-negative value while running in Ascend.
        value (Union[int, float]): Padding value.

    Inputs:
        - **x** (Tensor) - shape is :math:`(N, *)`, where :math:`*` means, any number of additional dimensions.
          It is not supported that the size of dimensions is greater than 5 while running on Ascend.

    Returns:
        Tensor, the tensor after padding.

    Raises:
        TypeError: If `padding` is not a tuple or int.
        TypeError: If `value` is not int or float.
        ValueError: If the length of `padding` is more than 6 or not a multiple of 2.
        ValueError: If the output shape after padding is not positive.
        ValueError: If the rank of 'x' is more than 5 while running in Ascend.
        ValueError: If `padding` contains negative value while running in Ascend.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> x = np.ones(shape=(1, 2, 3, 4)).astype(np.float32)
        >>> x = ms.Tensor(x)
        >>> padding = (-1, 1, 0, 1, 1, 0)
        >>> value = 0.5
        >>> pad3d = ms.nn.ConstantPad3d(padding, value)
        >>> out = pad3d(x)
        >>> print(out)
        [[[[0.5 0.5 0.5 0.5]
           [0.5 0.5 0.5 0.5]
           [0.5 0.5 0.5 0.5]
           [0.5 0.5 0.5 0.5]]
          [[1.  1.  1.  0.5]
           [1.  1.  1.  0.5]
           [1.  1.  1.  0.5]
           [0.5 0.5 0.5 0.5]]
          [[1.  1.  1.  0.5]
           [1.  1.  1.  0.5]
           [1.  1.  1.  0.5]
           [0.5 0.5 0.5 0.5]]]]
        >>> print(out.shape)
        (1, 3, 4, 4)
    """

    def __init__(self, padding, value):
        super(ConstantPad3d, self).__init__(padding, value, name='ConstantPad3d')


class _ReflectionPadNd(Cell):
    r"""
    Using a given padding to do reflection pad on the given tensor.
    Work as a parent class, and only accepts tuple as padding input.
    """

    def __init__(self, padding, name="ReflectionPadNd"):
        super(_ReflectionPadNd, self).__init__()
        self.name = name
        # check if padding and its elements are valid
        if not isinstance(padding, tuple):
            raise TypeError(f"For '{self.name}' the input 'padding' must be an integer or tuple, "
                            f"but got {type(padding).__name__}")
        if len(padding) % 2 != 0:
            raise ValueError(f"For '{self.name}' the length of input 'padding' must be divisible by 2, "
                             f"but got padding of length {len(padding)}. ")
        if not all(isinstance(i, int) for i in padding):
            raise TypeError(f"For '{self.name}' every element in 'padding' must be integer, "
                            f"but got {padding}. ")
        if not all(i >= 0 for i in padding):
            raise ValueError(f"For '{self.name}' every element in 'padding' must be >= 0. "
                             f"but got {padding}. ")
        self.padding = _swap_to_ms_padding_order(padding)

    def construct(self, x):
        input_shape = x.shape
        if not self.padding:
            return x
        padding = _check_padding_dimension(len(input_shape), self.padding)
        x = ops.MirrorPad(mode='REFLECT')(x, Tensor(padding))
        return x


class ReflectionPad1d(_ReflectionPadNd):
    r"""
    Using a given padding to do reflection pad on the given tensor.

    Args:
        padding (union[int, tuple]): The padding size to pad the last dimension of input tensor.
            If padding is an integer: all directions will be padded with the same size.
            If padding is a tuple: uses :math:`(pad\_left, pad\_right)` to pad.

    Inputs:
        - **x** (Tensor) - 2D or 3D, shape: :math:`(C, W_{in})` or :math:`(N, C, W_{in})`.

    Outputs:
        Tensor, after padding. Shape: :math:`(C, W_{out})` or :math:`(N, C, W_{out})`,
        where :math:`W_{out} = W_{in} + pad\_left + pad\_right`.

    Raises:
        TypeError: If 'padding' is not a tuple or int.
        TypeError: If there is an element in 'padding' that is not int.
        ValueError: If the length of 'padding' is not divisible by 2.
        ValueError: If there is an element in 'padding' that is negative.
        ValueError: If the there is a dimension mismatch between the padding and the tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> x = ms.Tensor(np.array([[[0, 1, 2, 3], [4, 5, 6, 7]]]).astype(np.float32))
        >>> # x has shape (1, 2, 4)
        >>> padding = (3, 1)
        >>> # The first and the second dimension of x remain the same.
        >>> # The third dimension of x: W_out = W_in + pad_left + pad_right = 4 + 3 + 1 = 8
        >>> pad1d = ms.nn.ReflectionPad1d(padding)
        >>> out = pad1d(x)
        >>> # The shape of out is (1, 2, 8)
        >>> print(out)
        [[[3. 2. 1. 0. 1. 2. 3. 2.]
          [7. 6. 5. 4. 5. 6. 7. 6.]]]
    """

    def __init__(self, padding):
        if isinstance(padding, int):
            padding = (padding, padding)
        super(ReflectionPad1d, self).__init__(padding, 'ReflectionPad1d')


class ReflectionPad2d(_ReflectionPadNd):
    r"""
    Using a given padding to do reflection pad the given tensor.

    Args:
        padding (union[int, tuple]): The padding size to pad the input tensor.
            If padding is an integer: all directions will be padded with the same size.
            If padding is a tuple: uses :math:`(pad\_left, pad\_right, pad\_up, pad\_down)` to pad.

    Inputs:
        - **x** (Tensor) - 3D or 4D, shape: :math:`(C, H_{in}, W_{in})` or :math:`(N, C, H_{in}, W_{in})`.

    Outputs:
        Tensor, after padding. Shape: :math:`(C, H_{out}, W_{out})` or :math:`(N, C, H_{out}, W_{out})`,
        where :math:`H_{out} = H_{in} + pad\_up + pad\_down`,  :math:`W_{out} = W_{in} + pad\_left + pad\_right`.

    Raises:
        TypeError: If 'padding' is not a tuple or int.
        TypeError: If there is an element in 'padding' that is not int.
        ValueError: If the length of 'padding' is not divisible by 2.
        ValueError: If there is an element in 'padding' that is negative.
        ValueError: If the there is a dimension mismatch between the padding and the tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> x = ms.Tensor(np.array([[[0, 1, 2], [3, 4, 5], [6, 7, 8]]]).astype(np.float32))
        >>> # x has shape (1, 3, 3)
        >>> padding = (1, 1, 2, 0)
        >>> pad2d = ms.nn.ReflectionPad2d(padding)
        >>> # The first dimension of x remains the same.
        >>> # The second dimension of x: H_out = H_in + pad_up + pad_down = 3 + 1 + 1 = 5
        >>> # The third dimension of x: W_out = W_in + pad_left + pad_right = 3 + 2 + 0 = 5
        >>> out = pad2d(x)
        >>> # The shape of out is (1, 5, 5)
        >>> print(out)
        [[[7. 6. 7. 8. 7.]
          [4. 3. 4. 5. 4.]
          [1. 0. 1. 2. 1.]
          [4. 3. 4. 5. 4.]
          [7. 6. 7. 8. 7.]]]
    """

    def __init__(self, padding):
        if isinstance(padding, int):
            padding = (padding, padding, padding, padding)
        super(ReflectionPad2d, self).__init__(padding, 'ReflectionPad2d')


class ReflectionPad3d(_ReflectionPadNd):
    r"""
    Pad the given tensor in a reflecting way using the input boundaries as the axis of symmetry.

    Note:
        ReflectionPad3d has not supported 5D tensor yet.

    Args:
        padding (union[int, tuple]): The padding size to pad the input tensor.
            If padding is an integer: all directions will be padded with the same size.
            If padding is a tuple: uses :math:`(pad\_left, pad\_right, pad\_up, pad\_down, pad\_front, pad\_back)`
            to pad.

    Inputs:
        - **x** (Tensor) - 4D Tensor, shape: :math:`(N, D_{in}, H_{in}, W_{in})`.

    Outputs:
        Tensor, after padding. Shape: :math:`(N, D_{out}, H_{out}, W_{out})`,
        where :math:`D_{out} = D_{in} + pad\_front + pad\_back`, :math:`H_{out} = H_{in} + pad\_up + pad\_down`
        :math:`W_{out} = W_{in} + pad\_left + pad\_right`.

    Raises:
        TypeError: If 'padding' is not a tuple or int.
        TypeError: If there is an element in 'padding' that is not int.
        ValueError: If the length of 'padding' is not divisible by 2.
        ValueError: If there is an element in 'padding' that is negative.
        ValueError: If the there is a dimension mismatch between the padding and the tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> arr = np.arange(8).astype(np.float32).reshape((1, 2, 2, 2))
        >>> x = ms.Tensor(arr)
        >>> # x has shape (1, 2, 2, 2)
        >>> padding = (1, 1, 1, 0, 0, 1)
        >>> pad3d = ms.nn.ReflectionPad3d(padding)
        >>> out = pad3d(x)
        >>> # The first dimension of x remains the same.
        >>> # The second dimension of x: D_out = D_in + pad_front + pad_back = 2 + 0 + 1 = 3
        >>> # The third dimension of x: H_out = H_in + pad_up + pad_down = 2 + 1 + 0 = 3
        >>> # The last dimension of x: W_out = W_in + pad_left + pad_right = 2 + 1 + 1 = 4
        >>> # The shape of out is (1, 3, 3, 4)
        >>> print(out)
        [[[[3. 2. 3. 2.]
           [1. 0. 1. 0.]
           [3. 2. 3. 2.]]
          [[7. 6. 7. 6.]
           [5. 4. 5. 4.]
           [7. 6. 7. 6.]]
          [[3. 2. 3. 2.]
           [1. 0. 1. 0.]
           [3. 2. 3. 2.]]]]
    """

    def __init__(self, padding):
        if isinstance(padding, int):
            padding = (padding, padding, padding, padding, padding, padding)
        super(ReflectionPad3d, self).__init__(padding, 'ReflectionPad3d')


class ZeroPad2d(_ConstantPadNd):
    r"""
    Pads the last two dimensions of input tensor with zero.

    Args:
        padding (Union[int, tuple]): The padding size to pad the last two dimensions of input tensor.
            If is int, uses the same padding in boundaries of input's last two dimensions.
            If is tuple and length of padding is 4 uses (padding_0, padding_1, padding_2, padding_3) to pad.
            If the input is `x`, the size of last dimension of output is :math:`padding\_0 + x.shape[-1] + padding\_1`.
            The size of penultimate dimension of output is :math:`padding\_2 + x.shape[-2] + padding\_3`.
            The remaining dimensions of the output are consistent with those of the input.
            Only support non-negative value while running in Ascend.

    Returns:
        Tensor, the tensor after padding.

    Raises:
        TypeError: If `padding` is not a tuple or int.
        ValueError: If the length of `padding` is more than 4 or not a multiple of 2.
        ValueError: If the output shape after padding is not positive.
        ValueError: If the rank of 'x' is more than 5 while running in Ascend.
        ValueError: If `padding` contains negative value while running in Ascend.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> x = np.ones(shape=(1, 2, 3, 4)).astype(np.float32)
        >>> x = ms.Tensor(x)
        >>> padding = (-1, 1, 0, 1)
        >>> pad = ms.nn.ZeroPad2d(padding)
        >>> out = pad(x)
        >>> print(out)
        [[[[1. 1. 1. 0.]
           [1. 1. 1. 0.]
           [1. 1. 1. 0.]
           [0. 0. 0. 0.]]
          [[1. 1. 1. 0.]
           [1. 1. 1. 0.]
           [1. 1. 1. 0.]
           [0. 0. 0. 0.]]]]
        >>> print(out.shape)
        (1, 2, 4, 4)
    """

    def __init__(self, padding):
        value = 0
        super(ZeroPad2d, self).__init__(padding, value, name='ZeroPad2d')


class _ReplicationPadNd(Cell):
    r"""
    Using a given padding to do replication pad on the given tensor.
    Work as a parent class, and only accepts tuple as padding input.
    """

    def __init__(self, padding, name="ReplicationPadNd"):
        super(_ReplicationPadNd, self).__init__()
        self.name = name
        if not isinstance(padding, tuple):
            raise TypeError(f"For '{self.name}' the input 'padding' must be an integer or tuple, "
                            f"but got {type(padding).__name__}")
        if len(padding) % 2 != 0:
            raise ValueError(f"For '{self.name}' the length of input 'padding' must be divisible by 2, "
                             f"but got padding of length {len(padding)}. ")
        if not all(isinstance(i, int) for i in padding):
            raise TypeError(f"For '{self.name}' every element in 'padding' must be integer, "
                            f"but got {padding}. ")
        self.padding = padding
        self.padv3 = nn_ops.PadV3(mode="edge")

    @staticmethod
    @constexpr
    def _need_expend_dim(x):
        raise NotImplementedError

    def construct(self, x):
        need_expend_dims = self._need_expend_dim(x)
        if need_expend_dims:
            x = x.expand_dims(0)
            x = self.padv3(x, self.padding, None)
            x = x.squeeze(0)
        else:
            x = self.padv3(x, self.padding, None)
        return x


class ReplicationPad1d(_ReplicationPadNd):
    r"""
    Pad on W dimension of input `x` according to `padding`.

    Args:
        padding (union[int, tuple]): The padding size to pad the last dimension of `x` .

            - If `padding` is an integer, all directions will be padded with the same size.
            - If `padding` is a tuple, uses :math:`(pad_{left}, pad_{right})` to pad.

    Inputs:
        - **x** (Tensor) - 2D or 3D, shape: :math:`(C, W_{in})` or :math:`(N, C, W_{in})`.

    Outputs:
        Tensor, after padding. Shape: :math:`(C, W_{out})` or :math:`(N, C, W_{out})`,
        where :math:`W_{out} = W_{in} + pad_{left} + pad_{right}`

    Raises:
        TypeError: If `padding` is neither a tuple nor an int.
        TypeError: If there is an element in `padding` that is not int.
        ValueError: If `padding` is tuple and the length of `padding` is not divisible by 2.
        ValueError: If `padding` is tuple and there is a dimension mismatch between the padding and the tensor.

    Supported Platforms:
        ``GPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> pad1d = ms.nn.ReplicationPad1d(2)
        >>> input = ms.Tensor(np.arange(0, 8).reshape(1, 2, 4), ms.float32)
        >>> print(input)
        [[[0. 1. 2. 3.]
          [4. 5. 6. 7.]]]
        >>> out = pad1d(input)
        >>> print(out)
        [[[0. 0. 0. 1. 2. 3. 3. 3.]
          [4. 4. 4. 5. 6. 7. 7. 7.]]]
        >>> pad1d = ms.nn.ReplicationPad1d((3, 1))
        >>> out = pad1d(input)
        >>> print(out)
        [[[0. 0. 0. 0. 1. 2. 3. 3.]
          [4. 4. 4. 4. 5. 6. 7. 7.]]]
    """

    def __init__(self, padding):
        if isinstance(padding, int):
            padding = (padding, padding)
        super(ReplicationPad1d, self).__init__(padding, name="ReplicationPad1d")

    def _need_expend_dim(self, x):
        input_shape = x.shape
        return 1 if len(input_shape) == 2 else 0


class ReplicationPad2d(_ReplicationPadNd):
    r"""
    Pad on HW dimension of input `x` according to `padding`.

    Args:
        padding (union[int, tuple]): The padding size to pad the last two dimension of `x` .

            - If `padding` is an integer, all directions will be padded with the same size.
            - If `padding` is a tuple, uses :math:`(pad_{left}, pad_{right}, pad_{up}, pad_{down})` to pad.

    Inputs:
        - **x** (Tensor) - 3D or 4D, shape: :math:`(C, H_{in}, W_{in})` or :math:`(N, C, H_{in}, W_{in})`.

    Outputs:
        Tensor, after padding. Shape: :math:`(C, H_{out}, W_{out})` or :math:`(N, C, H_{out}, W_{out})`,
        where :math:`H_{out} = H_{in} + pad_{up} + pad_{down}`, :math:`W_{out} = W_{in} + pad_{left} + pad_{right}`.

    Raises:
        TypeError: If `padding` is neither a tuple nor an int.
        TypeError: If there is an element in `padding` that is not int.
        ValueError: If `padding` is tuple and the length of `padding` is not divisible by 2.
        ValueError: If `padding` is tuple and there is a dimension mismatch between the padding and the tensor.

    Supported Platforms:
        ``GPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> pad2d = ms.nn.ReplicationPad2d(2)
        >>> input = ms.Tensor(np.arange(0, 9).reshape(1, 1, 3, 3), ms.float32)
        >>> print(input)
        [[[[0. 1. 2.]
           [3. 4. 5.]
           [6. 7. 8.]]]]
        >>> out = pad2d(input)
        >>> print(out)
        [[[[0. 0. 0. 1. 2. 2. 2.]
           [0. 0. 0. 1. 2. 2. 2.]
           [0. 0. 0. 1. 2. 2. 2.]
           [3. 3. 3. 4. 5. 5. 5.]
           [6. 6. 6. 7. 8. 8. 8.]
           [6. 6. 6. 7. 8. 8. 8.]
           [6. 6. 6. 7. 8. 8. 8.]]]]
        >>> pad2d = ms.nn.ReplicationPad2d((1, 1, 2, 0))
        >>> out = pad2d(input)
        >>> print(out)
        [[[[0. 0. 1. 2. 2.]
           [0. 0. 1. 2. 2.]
           [0. 0. 1. 2. 2.]
           [3. 3. 4. 5. 5.]
           [6. 6. 7. 8. 8.]]]]
    """

    def __init__(self, padding):
        if isinstance(padding, int):
            padding = (padding, padding, padding, padding)
        super(ReplicationPad2d, self).__init__(padding, name="ReplicationPad2d")

    def _need_expend_dim(self, x):
        input_shape = x.shape
        return 1 if len(input_shape) == 3 else 0


class ReplicationPad3d(_ReplicationPadNd):
    r"""
    Pad on DHW dimension of input `x` according to `padding`.

    Args:
        padding (union[int, tuple]): The padding size to pad the last three dimension of `x` .

            - If `padding` is an integer, all directions will be padded with the same size.
            - If `padding` is a tuple, uses :math:`(pad_{left}, pad_{right}, pad_{up}, pad_{down},
              pad_{front}, pad_{back})` to pad.

    Inputs:
        - **x** (Tensor) - 4D or 5D,
          shape: :math:`(C, D_{in}, H_{in}, W_{in})` or :math:`(N, C, D_{in}, H_{in}, W_{in})`.

    Outputs:
        Tensor, after padding, shape: :math:`(C, D_{out}, H_{out}, W_{out})` or
        :math:`(N, C, D_{out}, H_{out}, W_{out})`, where
        :math:`D_{out} = D_{in} + pad_{front} + pad_{back}`,
        :math:`H_{out} = H_{in} + pad_{up} + pad_{down}`,
        :math:`W_{out} = W_{in} + pad_{left} + pad_{right}`.

    Raises:
        TypeError: If `padding` is neither a tuple nor an int.
        TypeError: If there is an element in `padding` that is not int.
        ValueError: If `padding` is tuple and the length of `padding` is not divisible by 2.
        ValueError: If `padding` is tuple and there is a dimension mismatch between the padding and the tensor.

    Supported Platforms:
        ``GPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> pad3d = ms.nn.ReplicationPad3d(1)
        >>> input = ms.Tensor(np.arange(0, 9).reshape(1, 1, 1, 3, 3), ms.float32)
        >>> out = pad3d(input)
        >>> print(out)
        [[[[[0. 0. 1. 2. 2.]
            [0. 0. 1. 2. 2.]
            [3. 3. 4. 5. 5.]
            [6. 6. 7. 8. 8.]
            [6. 6. 7. 8. 8.]]
           [[0. 0. 1. 2. 2.]
            [0. 0. 1. 2. 2.]
            [3. 3. 4. 5. 5.]
            [6. 6. 7. 8. 8.]
            [6. 6. 7. 8. 8.]]
           [[0. 0. 1. 2. 2.]
            [0. 0. 1. 2. 2.]
            [3. 3. 4. 5. 5.]
            [6. 6. 7. 8. 8.]
            [6. 6. 7. 8. 8.]]]]]
    """

    def __init__(self, padding):
        if isinstance(padding, int):
            padding = (padding, padding, padding, padding, padding, padding)
        super(ReplicationPad3d, self).__init__(padding, name="ReplicationPad3d")

    def _need_expend_dim(self, x):
        input_shape = x.shape
        return 1 if len(input_shape) == 4 else 0
