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

"""Operators for nn."""
from __future__ import absolute_import
from __future__ import division

import math
from functools import partial
from mindspore import log as logger
from mindspore._checkparam import _check_3d_int_or_tuple
from mindspore import context
from mindspore.ops import signature as sig
from mindspore import _checkparam as validator
from mindspore.common import dtype as mstype
from mindspore.common._decorator import deprecated
from mindspore.ops.primitive import Primitive
from mindspore.ops.primitive import PrimitiveWithInfer
from mindspore.ops.primitive import PrimitiveWithCheck
from mindspore.ops.primitive import prim_attr_register
from ..auto_generate import (CeLU, Flatten, LogSoftmax, ReLU, ReLU6,
                             Elu, Sigmoid, Softmax, HSwish, HSigmoid, AvgPool, BiasAdd,
                             NLLLoss, OneHot, GeLU, FastGeLU, PReLU,
                             GridSampler3D, GridSampler2D, LayerNorm, HShrink, AdamWeightDecay, Dropout,
                             ApplyRotaryPosEmb, PagedAttention, PagedAttentionMask, ReshapeAndCache)
from .manually_defined import BatchNorm


def _check_positive_int_or_tuple(arg_name, arg_value, prim_name, allow_four=False,
                                 ret_four=False, strict_positive=True):
    """
    Checks whether an argument is a positive int or tuple with 2 or 4(when allow_four is True) positive int elements.
    """

    def _raise_message():
        raise ValueError(f"For '{prim_name}' attr '{arg_name}' must be an positive int number or a tuple of two "
                         f"{'or four ' if allow_four else ''}positive int numbers, but got {arg_value}")

    def _get_return_value():
        if isinstance(arg_value, int):
            ret = (1, 1, arg_value, arg_value) if ret_four else (arg_value, arg_value)
        elif len(arg_value) == 2:
            ret = (1, 1, arg_value[0], arg_value[1]) if ret_four else arg_value
        elif len(arg_value) == 4:
            if not allow_four:
                _raise_message()
            ret = arg_value if ret_four else (arg_value[2], arg_value[3])
        else:
            _raise_message()
        return ret

    validator.check_value_type(arg_name, arg_value, (int, tuple), prim_name)
    ret_value = _get_return_value()
    for item in ret_value:
        if isinstance(item, int) and not isinstance(item, bool):
            if item > 0:
                continue
            if not strict_positive and item == 0:
                continue
        _raise_message()
    return ret_value


def _check_shape(arg_name, arg_value, prim_name):
    """
    Checks whether an shape dims is a positive int elements.
    """

    def _raise_message():
        raise ValueError(f"For '{prim_name}' attr '{arg_name}' dims elements must be positive int numbers, "
                         f"but got {arg_value}")

    validator.check_value_type(arg_name, arg_value, (list, tuple), prim_name)
    for item in arg_value:
        if isinstance(item, int) and item > 0:
            continue
        _raise_message()
    return arg_value


def _update_attr_by_format(arg_value, arg_format):
    """
    If the format is NHWC, should modify the strides or dilation shape.
    """
    ret = arg_value
    if len(arg_value) == 4 and arg_format == "NHWC":
        ret = arg_value[1:] + (1,)

    return ret


class AdaptiveAvgPool3D(Primitive):
    r"""
    AdaptiveAvgPool3D operation.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.adaptive_avg_pool3d` for more details.

    Args:
        output_size (Union[int, tuple]): Specify the size of output tensor. It
            can be a single int or a tuple of three ints.

    Inputs:
        - **x** (Tensor) - The input of AdaptiveAvgPool3D, which is a 5D or 4D tensor.

    Outputs:
        Tensor, with the same type as the `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import nn, Tensor
        >>> from mindspore.ops import AdaptiveAvgPool3D
        >>> class AdaptiveAvgPool3DNet(nn.Cell):
        ...     def __init__(self, output_size):
        ...         super(AdaptiveAvgPool3DNet, self).__init__()
        ...         self.output_size_ = output_size
        ...         self.adaptive_avg_pool_3d = AdaptiveAvgPool3D(self.output_size_)
        ...     def construct(self, x_):
        ...         return self.adaptive_avg_pool_3d(x_)
        ...
        >>> output_size=(1,1,1)
        >>> input_x_val = np.zeros((1,1,2,2,2))
        >>> input_x_val[:,:,0,:,:]  += 1
        >>> input_x = Tensor(input_x_val, mindspore.float32)
        >>> adaptive_avg_pool_3d = AdaptiveAvgPool3DNet(output_size)
        >>> output = adaptive_avg_pool_3d(input_x)
        >>> print(output)
        [[[[[0.5]]]]]
    """

    @prim_attr_register
    def __init__(self, output_size):
        validator.check_value_type("output_size", output_size, [int, tuple], self.name)
        self.output_size = (output_size,) * 3 if isinstance(self.output_size, int) else output_size
        for i, size in enumerate(self.output_size):
            validator.check_value_type(f"output_size[{i}]", size, [int, type(None)], self.name)
            if size is not None:
                validator.check_number(f"output_size[{i}]", size, 0, validator.GE, self.name)

        self.output_size = tuple(-1 if val is None else val for val in self.output_size)

        self.add_prim_attr('output_size', self.output_size)
        self.init_prim_io_names(inputs=['x'], outputs=['y'])


class AdaptiveAvgPool2D(Primitive):
    r"""
    AdaptiveAvgPool2D operation.

    Refer to :func:`mindspore.ops.adaptive_avg_pool2d` for more details.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        output_size (Union[int, tuple]): The target output size. `output_size` can be a tuple :math:`(H, W)`,
            or an int H for :math:`(H, H)`. :math:`H` and :math:`W` can be int or None.
            If it is None, it means the output size is the same as the input size.

    Inputs:
        - **input_x** (Tensor) - The input of AdaptiveAvgPool2D, which is a 3D or 4D tensor,
          with float16 ,float32 or float64 data type.

    Outputs:
        Tensor, with the same type as the `input_x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> # case 1: output_size=(None, 2)
        >>> input_x = Tensor(np.array([[[1.0, 2.0, 3.0], [4.0, 5.0, 6.0], [7.0, 8.0, 9.0]],
        ...                            [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0], [7.0, 8.0, 9.0]],
        ...                            [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0], [7.0, 8.0, 9.0]]]), mindspore.float32)
        >>> adaptive_avg_pool_2d = ops.AdaptiveAvgPool2D((None, 2))
        >>> output = adaptive_avg_pool_2d(input_x)
        >>> print(output)
        [[[1.5 2.5]
          [4.5 5.5]
          [7.5 8.5]]
         [[1.5 2.5]
          [4.5 5.5]
          [7.5 8.5]]
         [[1.5 2.5]
          [4.5 5.5]
          [7.5 8.5]]]
        >>> # case 2: output_size=2
        >>> adaptive_avg_pool_2d = ops.AdaptiveAvgPool2D(2)
        >>> output = adaptive_avg_pool_2d(input_x)
        >>> print(output)
        [[[3. 4.]
          [6. 7.]]
         [[3. 4.]
          [6. 7.]]
         [[3. 4.]
          [6. 7.]]]
        >>> # case 3: output_size=(1, 2)
        >>> adaptive_avg_pool_2d = ops.AdaptiveAvgPool2D((1, 2))
        >>> output = adaptive_avg_pool_2d(input_x)
        >>> print(output)
        [[[4.5 5.5]]
         [[4.5 5.5]]
         [[4.5 5.5]]]
    """

    @prim_attr_register
    def __init__(self, output_size):
        """Initialize AdaptiveAvgPool2D."""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])
        validator.check_value_type("output_size", output_size, [int, tuple], self.name)
        if isinstance(output_size, tuple):
            validator.check_int(len(output_size), 2, validator.EQ, 'length of output_size', self.name)
        self.output_size = (output_size, output_size) if isinstance(self.output_size, int) else output_size
        for i, size in enumerate(self.output_size):
            validator.check_value_type(f"output_size[{i}]", size, [int, type(None)], self.name)
            if size is not None:
                validator.check_number(f"output_size[{i}]", size, 0, validator.GE, self.name)

        self.output_size = tuple(-1 if val is None else val for val in self.output_size)
        self.add_prim_attr('output_size', self.output_size)


class AdaptiveMaxPool2D(Primitive):
    r"""
    Performs 2D adaptive max pooling on a multi-plane input signal.

    Refer to :func:`mindspore.ops.adaptive_max_pool2d` for more details.

    Args:
        output_size (Union[int, tuple]): The target output size. `output_size` can be a tuple :math:`(H, W)`,
            or an int H for :math:`(H, H)`. :math:`H` and :math:`W` can be int or None.
            If it is None, it means the output size is the same as the input size.

    Inputs:
        - **input_x** (Tensor) - The input of AdaptiveMaxPool2D, which is a 3D or 4D tensor,
          with float16, float32 or float64 data type.

    Outputs:
        Tensor, with the same type as the `input_x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> # case 1: output_size=(None, 2)
        >>> input_x = Tensor(np.array([[[[1.0, 2.0, 3.0], [4.0, 5.0, 6.0], [7.0, 8.0, 9.0]],
        ...                             [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0], [7.0, 8.0, 9.0]],
        ...                             [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0], [7.0, 8.0, 9.0]]]]), mindspore.float32)
        >>> adaptive_max_pool_2d = ops.AdaptiveMaxPool2D((None, 2))
        >>> output = adaptive_max_pool_2d(input_x)
        >>> print(output[0])
        [[[[2. 3.]
           [5. 6.]
           [8. 9.]]
          [[2. 3.]
           [5. 6.]
           [8. 9.]]
          [[2. 3.]
           [5. 6.]
           [8. 9.]]]]
        >>> # case 2: output_size=2
        >>> adaptive_max_pool_2d = ops.AdaptiveMaxPool2D(2)
        >>> output = adaptive_max_pool_2d(input_x)
        >>> print(output[0])
        [[[[5. 6.]
           [8. 9.]]
          [[5. 6.]
           [8. 9.]]
          [[5. 6.]
           [8. 9.]]]]
        >>> # case 3: output_size=(1, 2)
        >>> adaptive_max_pool_2d = ops.AdaptiveMaxPool2D((1, 2))
        >>> output = adaptive_max_pool_2d(input_x)
        >>> print(output[0])
        [[[[8. 9.]]
          [[8. 9.]]
          [[8. 9.]]]]
    """

    @prim_attr_register
    def __init__(self, output_size):
        """Initialize AdaptiveMaxPool2D."""
        validator.check_value_type("output_size", output_size, [int, tuple], self.name)
        if isinstance(output_size, tuple):
            validator.check_int(len(output_size), 2, validator.EQ,
                                'length of output_size', self.name)
        self.output_size = (output_size, output_size) if isinstance(self.output_size, int) else output_size
        self.output_size = (-1 if self.output_size[0] is None else self.output_size[0],
                            -1 if self.output_size[1] is None else self.output_size[1])
        for size in self.output_size:
            validator.check_number("output_size", size, -1, validator.GE, None)
        self.add_prim_attr('output_size', self.output_size)


class AdaptiveMaxPool3D(Primitive):
    r"""
    Performs 3D adaptive max pooling on a multi-plane input signal.

    Refer to :func:`mindspore.ops.adaptive_max_pool3d` for more details.

    Inputs:
        - **x** (Tensor) - Tensor, with shape :math:`(C, D, H, W)` or :math:`(N, C, D, H, W)`.
        - **output_size** (Union[int, tuple]) - The specified output size, which is an integer that represents depth,
          height and width, or a tuple of three int numbers that represent depth, height and width respectively.
          The value must be a positive integer. If it is None, the output size and input size of the corresponding
          dimension are the same.

    Outputs:
        - **y** (Tensor) - Tensor, with the same number of dims and data type as the `input`.
        - **argmax** (Tensor) - Tensor, the indices of max value, which has the same shape as the
          `y` and it's data type is int32.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> class AdaptiveMaxPool3DNet(nn.Cell):
        ...     def __init__(self):
        ...         super(AdaptiveMaxPool3DNet, self).__init__()
        ...         self.adaptive_max_pool_3d = ops.AdaptiveMaxPool3D()
        ...     def construct(self, x_, output_size_):
        ...         return self.adaptive_max_pool_3d(x_, output_size_)
        >>> x = np.arange(0,36).reshape((1, 3, 3, 4)).astype(np.float32)
        >>> output_size = np.array([1, 1, 2], dtype=np.int32)
        >>> net = AdaptiveMaxPool3DNet()
        >>> output = net(Tensor(x), Tensor(output_size))
        >>> print(output[0].asnumpy())
        [[[[33. 35.]]]]
        >>> print(output[1].asnumpy())
        [[[[33 35]]]]
    """

    @prim_attr_register
    def __init__(self):
        self.init_prim_io_names(inputs=['x', 'output_size'], outputs=['y', 'argmax'])


class Softplus(Primitive):
    r"""
    Softplus activation function.

    Softplus is a smooth approximation to the ReLU function.
    It can be used to constrain the output of a machine to always be positive.
    The function is shown as follows:

    .. math::

        \text{output} = \log(1 + \exp(\text{x}))

    Inputs:
        - **input_x** (Tensor) - Tensor of any dimension.
          Supported dtypes:

          - GPU/CPU: float16, float32, float64.
          - Ascend: float16, float32.

    Outputs:
        Tensor, with the same type and shape as the `input_x`.

    Raises:
        TypeError: If `input_x` is not a Tensor.
        TypeError: If the dtype of `input_x` is not float16, float32 or float64.

    Supported Platforms:
        ``Ascend``  ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor(np.array([1, 2, 3, 4, 5]), mindspore.float32)
        >>> softplus = ops.Softplus()
        >>> output = softplus(input_x)
        >>> print(output)
        [1.3132615 2.126928  3.0485873 4.01815   5.0067153]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Softplus"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class Softsign(Primitive):
    r"""
    Softsign activation function.

    Refer to :func:`mindspore.ops.softsign` for more details.

    Inputs:
        - **input_x** (Tensor) - Tensor of shape :math:`(N, *)`, where :math:`*` means, any number of
          additional dimensions, with float16 or float32 data type.

    Outputs:
        Tensor, with the same type and shape as the `input_x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor(np.array([0, -1, 2, 30, -30]), mindspore.float32)
        >>> softsign = ops.Softsign()
        >>> output = softsign(input_x)
        >>> print(output)
        [ 0.        -0.5         0.6666667  0.9677419 -0.9677419]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Softsign"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class ReLUV3(Primitive):
    r"""
    Computes ReLUV3 (Rectified Linear Unit activation function) of input tensors element-wise.

    It returns max(x, 0) element-wise. Specially, the neurons with the negative output
    will be suppressed and the active neurons will stay the same.

    .. math::

        ReLUV3(x) = (x)^+ = max(0, x)

    Inputs:
        - **input_x** (Tensor) - Tensor of shape :math:`(N, *)`, where :math:`*` means, any number of
          additional dimensions, data type is
          `number <https://www.mindspore.cn/docs/en/master/api_python/mindspore.html#mindspore.dtype>`_.

    Outputs:
        Tensor of shape :math:`(N, *)`, with the same type and shape as the `input_x`.

    Raises:
        TypeError: If `input_x` is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> input_x = Tensor(np.array([[-1.0, 4.0, -8.0], [2.0, -5.0, 9.0]]), mindspore.float32)
        >>> relu_v3 = ops.ReLUV3()
        >>> output = relu_v3(input_x)
        >>> print(output)
        [[0. 4. 0.]
         [2. 0. 9.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize ReLUV3"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class Mish(PrimitiveWithInfer):
    r"""
    Computes MISH(A Self Regularized Non-Monotonic Neural Activation Function) of input tensors element-wise.

    Refer to :func:`mindspore.ops.mish` for more details.

    Inputs:
        - **x** (Tensor) - The input Tensor.
          Supported dtypes:

          - GPU/CPU: float16, float32, float64.
          - Ascend: float16, float32.

    Outputs:
        Tensor, with the same type and shape as the `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([[-1.0, 4.0, -8.0], [2.0, -5.0, 9.0]]), mindspore.float32)
        >>> mish = ops.Mish()
        >>> output = mish(x)
        >>> print(output.shape)
        (2, 3)
        >>> x = Tensor(2.1, mindspore.float32)
        >>> output = mish(x)
        >>> print(output)
        2.050599
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Mish"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class SeLU(Primitive):
    r"""
    Activation function SeLU (Scaled exponential Linear Unit).

    The activation function is defined as:

    .. math::
        E_{i} =
        scale *
        \begin{cases}
        x_{i}, &\text{if } x_{i} \geq 0; \cr
        \text{alpha} * (\exp(x_i) - 1), &\text{otherwise.}
        \end{cases}

    where :math:`alpha` and :math:`scale` are pre-defined constants(:math:`alpha=1.67326324`
    and :math:`scale=1.05070098`).

    See more details in `Self-Normalizing Neural Networks <https://arxiv.org/abs/1706.02515>`_.

    Inputs:
        - **input_x** (Tensor) - Tensor of any dimension.
          The data type is int8, int32, float16, float32, float64(only CPU, GPU).

    Outputs:
        Tensor, with the same type and shape as the `input_x`.

    Raises:
        TypeError: If dtype of `input_x` is not int8, int32, float16, float32, float64.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor(np.array([[-1.0, 4.0, -8.0], [2.0, -5.0, 9.0]]), mindspore.float32)
        >>> selu = ops.SeLU()
        >>> output = selu(input_x)
        >>> print(output)
        [[-1.1113307 4.202804 -1.7575096]
        [ 2.101402 -1.7462534 9.456309 ]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SeLU"""
        self.init_prim_io_names(inputs=['input_x'], outputs=['output'])


class Tanh(Primitive):
    r"""
    Computes hyperbolic tangent of input element-wise.

    Refer to :func:`mindspore.ops.tanh` for more details.

    Inputs:
        - **input_x** (Tensor) - Input Tensor of any dimension.

    Outputs:
        Tensor, with the same type and shape as the `input_x`.

    Supported Platforms:
        ``Ascend`` ``GPU``  ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor(np.array([1, 2, 3, 4, 5]), mindspore.float32)
        >>> tanh = ops.Tanh()
        >>> output = tanh(input_x)
        >>> print(output)
        [0.7615941 0.9640276 0.9950547 0.9993293 0.9999092]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Tanh"""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])


class FusedBatchNorm(Primitive):
    r"""
    The FusedBatchNorm interface is deprecated, please use the BatchNorm interface.
    """

    def __init__(self, mode=0, epsilon=1e-5, momentum=0.1):
        raise TypeError("The FusedBatchNorm interface is deprecated, please use the BatchNorm interface.")


class FusedBatchNormEx(PrimitiveWithCheck):
    r"""
    The FusedBatchNormEx interface is deprecated, please use the BatchNorm interface.
    """

    def __init__(self, mode=0, epsilon=1e-5, momentum=0.1, data_format="NCHW"):
        raise TypeError("FusedBatchnormEx interface is deprecated, please use BatchNorm interface.")


class InstanceNorm(PrimitiveWithInfer):
    r"""
    Instance Normalization over a 4D input.

    This operator applies Instance Normalization over a 4D input (a mini-batch of 2D inputs with
    additional channel dimension) as described in the paper `Instance Normalization: The Missing Ingredient for
    Fast Stylization <https://arxiv.org/abs/1607.08022>`_. It rescales and recenters the feature using a mini-batch
    of data and the learned parameters which can be described in the following formula.

    .. math::

        y = \frac{x - mean}{\sqrt{variance + \epsilon}} * \gamma + \beta

    where :math:`\gamma` is scale, :math:`\beta` is bias, :math:`\epsilon` is epsilon.

    Args:
        epsilon (float): A small value added for numerical stability. Default: ``1e-5`` .
        momentum (float): The hyper parameter to compute moving average for running_mean and running_var
            (e.g. :math:`new\_running\_mean = momentum * running\_mean + (1 - momentum) * current\_mean`).
            Momentum value must be [0, 1]. Default: ``0.1`` .

    Inputs:
        - **input_x** (Tensor) - The input of InstanceNorm, Tensor of shape :math:`(N, C)`,
          data type: float16 or float32.
        - **gamma** (Parameter) - Scale, Tensor of shape :math:`(C,)`,
          data type: float32.
        - **beta** (Parameter) - Bias, Tensor of shape :math:`(C,)`,
          data type: float32.
        - **mean** (Parameter) - Mean value, Tensor of shape :math:`(C,)`, data type: float32.
        - **variance** (Parameter) - Variance value, Tensor of shape :math:`(C,)`, data type: float32.

    Outputs:
        Tuple of 3 Tensors, the normalized input, the updated parameters.

        - **output_x** (Tensor) - The output of InstanceNorm, same type and shape as the `input_x`.
        - **updated_moving_mean** (Tensor) - Updated mean value, Tensor of shape :math:`(NC,)`, data type: float32.
        - **updated_moving_variance** (Tensor) - Updated variance value, Tensor of shape :math:`(NC,)`,
          data type: float32.

    Supported Platforms:
        ``GPU``

    Raises:
        TypeError: If `epsilon` or `momentum` is not a float.
        TypeError: If dtype of `input_x` is neither float16 nor float32.
        TypeError: If dtype of `gamma`, `beta` or `mean` is not float32.
        ValueError: If `epsilon` is not in the range of [0, 1).
        ValueError: If `momentum` is not in the range of [0, 1].

    Examples:
        >>> class InstanceNormNet(nn.Cell):
        >>>     def __init__(self):
        >>>         super(InstanceNormNet, self).__init__()
        >>>         self.instance_norm = ops.InstanceNorm()
        >>>         self.gamma = Parameter(Tensor(np.ones([64]), mindspore.float32), name="gamma")
        >>>         self.beta = Parameter(Tensor(np.ones([64]), mindspore.float32), name="beta")
        >>>         self.mean = Parameter(Tensor(np.ones([64]), mindspore.float32), name="mean")
        >>>         self.variance = Parameter(Tensor(np.ones([64]), mindspore.float32), name="variance")
        >>>
        >>>     def construct(self, input_x):
        >>>         out = self.instance_norm(input_x, self.gamma, self.beta, self.mean, self.variance)
        >>>         return out
        >>>
        >>> input_x = Tensor(np.ones([128, 64, 32, 64]), mindspore.float32)
        >>> net = InstanceNormNet()
        >>> output = net(input_x)
        >>> result = output[0].shape
        >>> print(result)
        (128, 64, 32, 64)
    """
    __mindspore_signature__ = (
        sig.make_sig('input_x', dtype=sig.sig_dtype.T2),
        sig.make_sig('gamma', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('beta', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('mean', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('variance', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
    )

    @prim_attr_register
    def __init__(self, epsilon=1e-5, momentum=0.1):
        """Initialize InstanceNorm."""
        self.init_prim_io_names(inputs=['x', 'gamma', 'beta', 'mean', 'variance'],
                                outputs=['y', 'save_mean', 'save_variance'])
        self.epsilon = validator.check_float_range(epsilon, 0, 1, validator.INC_RIGHT, 'epsilon', self.name)
        self.momentum = validator.check_float_range(momentum, 0, 1, validator.INC_BOTH, 'momentum', self.name)
        self._update_parameter = True
        self.add_prim_attr('side_effect_mem', True)


class InstanceNormV2(Primitive):
    r"""
    Instance Normalization over a 4D or 5D input.

    This operator applies Instance Normalization over a 4D or 5D input (a mini-batch of 2D inputs with
    additional channel dimension) as described in the paper `Instance Normalization: The Missing Ingredient for
    Fast Stylization <https://arxiv.org/abs/1607.08022>`_. It rescales and recenters the feature using a mini-batch
    of data and the learned parameters which can be described in the following formula.

    .. math::

        y = \frac{x - mean}{\sqrt{variance + \epsilon}} * \gamma + \beta

    where :math:`\gamma` is scale(gamma), :math:`\beta` is bias(beta), :math:`\epsilon` is epsilon.

    Note:
        The format of input `x` support ``NCHW`` and ``NC1HWC0`` in platform ``CPU`` and ``Ascend``.
        When attr `is_training` is `False`, this module does not tracks the running mean and variance.
        The output `batch_mean` and `batch_variance` would be all zero.

    Args:
        is_training(bool): An optional boolean value. Default: ``True``.
            When set to ``True``, this module tracks the running mean and variance.
            When set to ``False``, this module does not track such statistics and always uses batch
            statistics in both training and eval modes.
        momentum (float): The hyper parameter to compute moving average for running_mean and running_var
            (e.g. :math:`new\_running\_mean = momentum * running\_mean + (1 - momentum) * current\_mean`).
            Momentum value must be [0, 1]. Default: ``0.1`` .
        epsilon (float): A small value added to the denominator for numerical stability.
            Epsilon value must be [0, 1). Default: ``1e-5`` .

    Inputs:
        - **x** (Tensor) - The input of InstanceNormV2, Tensor of shape :math:`(N, C, H, W)`
          or :math:`(N, C1, H, W, C0)`, data type: float16 or float32.
        - **gamma** (Tensor) - Scale, Shape depends on the shape of input `x`, data type: float32.
          If `x` shape is :math:`(N, C, H, W)`, shape of `gamma` is :math:`(N, C, 1, 1)`.
          If `x` shape is :math:`(N, C1, H, W, C0)`, shape of `gamma` is :math:`(N, C1, 1, 1, C0)`.
        - **beta** (Tensor) - Bias, has the same shape and data type as `gamma`.
        - **mean** (Tensor) - Mean value, has the same shape and data type as `gamma`.
        - **variance** (Tensor) - Variance value, has the same shape and data type as `gamma`.

    Outputs:
        Tuple of 3 Tensors, the normalized input, the mean and variance of batch input.

        - **y** (Tensor) - The output of InstanceNormV2, same type and shape as the `x`.
        - **batch_mean** (Tensor) - The mean value of batch input, same type and shape as the input `mean`.
        - **batch_variance** (Tensor) - The variance value of batch input, same type and shape as the input `variance`.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Raises:
        TypeError: If either item in the inputs is not Tensor.
        TypeError: If data type of `x` is neither float16 nor float32.
        TypeError: If data type of `gamma` is not a Tensor of float32.
        TypeError: If data type of `beta` is not a Tensor of float32.
        TypeError: If data type of `mean` is not a Tensor of float32.
        TypeError: If data type of `variance` is not a Tensor of float32.
        TypeError: If data type of attr `is_training` is not bool.
        TypeError: If data type of attr `momentum` is not float.
        TypeError: If data type of attr `epsilon` is not float.
        ValueError: If :math:`H * W <= 1` in input `x`.
        ValueError: If the shape of either item in the inputs is neither 4D nor 5D.
        ValueError: If `epsilon` is not in the range of [0, 1).
        ValueError: If `momentum` is not in the range of [0, 1].

    Examples:
        >>> x = Tensor(input_data=np.random.randn(128, 48, 32, 64, 12), dtype=mindspore.float32)
        >>> gamma = Tensor(input_data=np.random.randn(128, 48, 1, 1, 12), dtype=mstype.float32)
        >>> beta = Tensor(input_data=np.random.randn(128, 48, 1, 1, 12), dtype=mstype.float32)
        >>> mean = Tensor(input_data=np.random.randn(128, 48, 1, 1, 12), dtype=mstype.float32)
        >>> var = Tensor(input_data=np.random.randn(128, 48, 1, 1, 12), dtype=mstype.float32)
        >>> ops = P.InstanceNormV2()
        >>> output = ops(x, gamma, beta, mean, var)
        >>> y_shape = output[0].shape
        >>> print(y_shape)
        (128, 48, 32, 64, 12)
        >>> batch_mean_shape = output[1].shape
        >>> print(batch_mean_shape)
        (128, 48, 1, 1, 12)
        >>> batch_var_shape = output[2].shape
        >>> print(batch_var_shape)
        (128, 48, 1, 1, 12)
    """
    __mindspore_signature__ = (
        sig.make_sig('x', dtype=sig.sig_dtype.T1),
        sig.make_sig('gamma', dtype=sig.sig_dtype.T),
        sig.make_sig('beta', dtype=sig.sig_dtype.T),
        sig.make_sig('mean', dtype=sig.sig_dtype.T),
        sig.make_sig('variance', dtype=sig.sig_dtype.T),
    )

    @prim_attr_register
    def __init__(self, is_training=True, momentum=0.1, epsilon=1e-5):
        """Initialize InstanceNormV2."""
        self.init_prim_io_names(inputs=['x', 'gamma', 'beta', 'mean', 'variance'],
                                outputs=['y', 'batch_mean', 'batch_variance'])
        validator.check_is_float(epsilon, 'epsilon', self.name)
        validator.check_is_float(momentum, 'momentum', self.name)
        validator.check_float_range(epsilon, 0, 1, validator.INC_RIGHT, 'epsilon', self.name)
        validator.check_float_range(momentum, 0, 1, validator.INC_BOTH, 'momentum', self.name)
        validator.check_bool(is_training, "is_training", self.name)


class Conv2D(Primitive):
    r"""
    2D convolution layer.

    Applies a 2D convolution over an input tensor which is typically of shape :math:`(N, C_{in}, H_{in}, W_{in})`,
    where :math:`N` is batch size, :math:`C` is channel number, :math:`H` is feature height, :math:`W` is feature width.

    The output is calculated based on formula:

    .. math::

        \text{out}(N_i, C_{\text{out}_j}) = \text{bias}(C_{\text{out}_j}) +
        \sum_{k = 0}^{C_{in} - 1} \text{ccor}({\text{weight}(C_{\text{out}_j}, k), \text{X}(N_i, k)})

    where :math:`bias` is the output channel bias, :math:`ccor` is
    the `cross-correlation <https://en.wikipedia.org/wiki/Cross-correlation>`_,
    , :math:`weight` is the convolution kernel value and :math:`X` represents the input feature map.

    Here are the indices' meanings:

    - :math:`i` corresponds to the batch number, the range is :math:`[0, N-1]`,
      where :math:`N` is the batch size of the input.

    - :math:`j` corresponds to the output channel, the range is :math:`[0, C_{out}-1]`,
      where :math:`C_{out}` is the number of
      output channels, which is also equal to the number of kernels.

    - :math:`k` corresponds to the input channel, the range is :math:`[0, C_{in}-1]`,
      where :math:`C_{in}` is the number of
      input channels, which is also equal to the number of channels in the convolutional kernels.

    Therefore, in the above formula, :math:`{bias}(C_{\text{out}_j})` represents the bias of the :math:`j`-th
    output channel, :math:`{weight}(C_{\text{out}_j}, k)` represents the slice of the :math:`j`-th convolutional
    kernel in the :math:`k`-th channel, and :math:`{X}(N_i, k)` represents the slice of the :math:`k`-th input
    channel in the :math:`i`-th batch of the input feature map.

    The shape of the convolutional kernel is given by :math:`(\text{kernel_size[0]},\text{kernel_size[1]})`,
    where :math:`\text{kernel_size[0]}`
    and :math:`\text{kernel_size[1]}` are the height and width of the kernel, respectively.
    If we consider the input and output channels as well as the `group` parameter, the complete kernel shape
    will be :math:`(C_{out}, C_{in} / \text{group}, \text{kernel_size[0]}, \text{kernel_size[1]})`,
    where `group` is the number of groups dividing `x`'s input channel when applying group convolution.

    For more details about convolution layer, please refer to `Gradient Based Learning Applied to Document Recognition
    <http://vision.stanford.edu/cs598_spring07/papers/Lecun98.pdf>`_.

    Note:
        On Ascend platform, only group convolution in depthwise convolution scenarios is supported.
        That is, when `group>1`, condition `in\_channels` = `out\_channels` = `group` must be satisfied.

    Args:
        out_channel (int): Specifies output channel :math:`C_{out}`.
        kernel_size (Union[int, tuple[int]]): Specifies the height and width of the 2D convolution kernel.
            It can be a single int or a tuple of 2 integers. A single int means the value is for both the height
            and the width. A tuple of 2 ints means the first value is for the height and the other is for the width.
        mode (int, optional): Modes for different convolutions. The value is currently not used. Default: ``1`` .
        pad_mode (str, optional): Specifies the padding mode with a padding value of 0. It can be set to:
            ``"same"`` , ``"valid"`` or ``"pad"`` . Default: ``"valid"`` .

            - ``"same"``: Pad the input around its edges so that the shape of input and output
              are the same when `stride` is set to ``1``.
              The amount of padding to is calculated by the operator internally, If the amount is even, it is
              uniformly distributed around the input, if it is odd, the excess amount goes to the right/bottom side.
              If this mode is set, `pad` must be 0.
            - ``"valid"``: No padding is applied to the input, and the output returns the maximum
              possible height and width. Extra pixels that could not complete a full stride will
              be discarded. If this mode is set, `pad` must be 0.
            - ``"pad"``: Pad the input with a specified amount. In this mode, the amount of padding
              in the height and width directions is determined by the `pad` parameter.
              If this mode is set, `pad` must be greater than or equal to 0.

        pad (Union(int, tuple[int]), optional): Specifies the amount of padding to apply on input
            when `pad_mode` is set to ``"pad"``. It can be a single int or a tuple of 4 ints.
            If `pad` is one integer, the paddings of top, bottom, left and right are the same, equal to `pad`.
            If `pad` is a tuple with four integers, the paddings of top, bottom, left and right will be equal to pad[0],
            pad[1], pad[2], and pad[3] accordingly. Default: ``0`` .
        stride (Union(int, tuple[int]), optional): Specifies the stride of the convolution kernel's movement.
            It can be a single int or a tuple of two or four ints. A single int means the stride is the same in
            both the height and width directions. A tuple of two ints indicates the strides in the height and
            width directions, respectively. For a tuple of four ints, the two ints correspond to (N, C) dimension
            are treated as 1, and the two correspond to (H, W) dimensions is the step size in the height
            and width directions respectively. Default: ``1`` .
        dilation (Union(int, tuple[int]), optional): Specifies the dilation rate to use for dilated convolution.
            It can be a single int or a tuple of 2 or 4 integers. A single int means the dilation size is the same
            in both the height and width directions. A tuple of two ints represents the dilation size in
            the height and width directions, respectively. For a tuple of four ints, the two ints correspond
            to (N, C) dimension are treated as 1, and the two correspond to (H, W) dimensions is the
            dilation size in the height and width directions respectively.
            Assuming :math:`dilation=(d0, d1)`, the convolutional kernel samples the input with a
            spacing of :math:`d0-1` elements in the height direction and :math:`d1-1` elements in the width direction.
            The values in the height and width dimensions are in the ranges [1, H] and [1, W], respectively.
            Default: ``1`` .
        group (int, optional): Specifies the number of groups dividing `x`'s input channel when applying
            group convolution. Default: ``1`` .
        data_format (str, optional): The optional value for data format, is ``'NHWC'`` or ``'NCHW'`` .
            Default: ``"NCHW"``. (NHWC is only supported in GPU now.)

    Inputs:
        - **x** (Tensor) - Input tensor of shape :math:`(N, C_{in}, H_{in}, W_{in})` or
          :math:`(N, H_{in}, W_{in}, C_{in}, )` depending on `data_format` .
        - **weight** (Tensor) - The convolutional kernel value, it should has shape
          :math:`(C_{out}, C_{in} / \text{group}, \text{kernel_size[0]}, \text{kernel_size[1]})` .

    Outputs:
        Tensor, the value that applied 2D convolution. The shape is :math:`(N, C_{out}, H_{out}, W_{out})`
        or :math:`(N, H_{out}, W_{out}, C_{out}, )`.
        To see how different pad modes affect the output shape, please refer to
        :class:`mindspore.nn.Conv2d` for more details.

    Raises:
        TypeError: If `kernel_size`, `stride`, `pad` or `dilation` is neither an int nor a tuple.
        TypeError: If `out_channel` or `group` is not an int.
        ValueError: If `kernel_size`, `stride` or `dilation` is less than 1.
        ValueError: If `pad_mode` is not one of ``'same'``, ``'valid'`` or ``'pad'``.
        ValueError: If `pad` is a tuple whose length is not equal to 4.
        ValueError: If `pad_mode` it not equal to ``'pad'`` and `pad` is not equal to ``(0, 0, 0, 0)``.
        ValueError: If `data_format` is neither ``'NHWC'`` nor ``'NCHW'`` .

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> # case 1: All parameters use default values.
        >>> x = Tensor(np.ones([10, 32, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([32, 32, 3, 3]), mindspore.float32)
        >>> conv2d = ops.Conv2D(out_channel=32, kernel_size=3)
        >>> output = conv2d(x, weight)
        >>> print(output.shape)
        (10, 32, 30, 30)
        >>> # case 2: pad_mode="pad", other parameters being default.
        >>> x = Tensor(np.ones([10, 32, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([32, 32, 3, 3]), mindspore.float32)
        >>> conv2d = ops.Conv2D(out_channel=32, kernel_size=3, pad_mode="pad", pad=(4, 10, 4, 10))
        >>> output = conv2d(x, weight)
        >>> print(output.shape)
        (10, 32, 44, 44)
        >>> # case 3: stride=(2, 4), other parameters being default.
        >>> x = Tensor(np.ones([10, 32, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([32, 32, 3, 3]), mindspore.float32)
        >>> conv2d = ops.Conv2D(out_channel=32, kernel_size=3, stride=(2, 4))
        >>> output = conv2d(x, weight)
        >>> print(output.shape)
        (10, 32, 15, 8)
        >>> # case 4: dilation=2, other parameters being default.
        >>> x = Tensor(np.ones([10, 32, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([32, 32, 3, 3]), mindspore.float32)
        >>> conv2d = ops.Conv2D(out_channel=32, kernel_size=3, dilation=2)
        >>> output = conv2d(x, weight)
        >>> print(output.shape)
        (10, 32, 28, 28)
        >>> # case 5: group=2, other parameters being default.
        >>> x = Tensor(np.ones([10, 64, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([32, 32, 3, 3]), mindspore.float32)
        >>> conv2d = ops.Conv2D(out_channel=32, kernel_size=3, group=2)
        >>> output = conv2d(x, weight)
        >>> print(output.shape)
        (10, 32, 30, 30)
        >>> # case 6: All parameters are specified.
        >>> x = Tensor(np.ones([10, 64, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([32, 32, 3, 3]), mindspore.float32)
        >>> conv2d = ops.Conv2D(out_channel=32, kernel_size=3, pad_mode="pad",
        ...                     pad=(4, 10, 4, 10), stride=(2, 4), dilation=2,  group=2)
        >>> output = conv2d(x, weight)
        >>> print(output.shape)
        (10, 32, 21, 11)
    """

    @prim_attr_register
    def __init__(self,
                 out_channel,
                 kernel_size,
                 mode=1,
                 pad_mode="valid",
                 pad=0,
                 stride=1,
                 dilation=1,
                 group=1,
                 data_format="NCHW"):
        """Initialize Conv2D"""
        self.init_prim_io_names(inputs=['x', 'w'], outputs=['output'])
        self.kernel_size = _check_positive_int_or_tuple('kernel_size', kernel_size, self.name)
        self.stride = _check_positive_int_or_tuple('stride', stride, self.name, allow_four=True, ret_four=True)
        self.add_prim_attr('stride', self.stride)
        self.dilation = _check_positive_int_or_tuple('dilation', dilation, self.name, allow_four=True, ret_four=True)
        self.add_prim_attr('dilation', self.dilation)
        validator.check_value_type('pad', pad, (int, tuple), self.name)
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        if isinstance(pad, int):
            pad = (pad,) * 4
        else:
            validator.check_equal_int(len(pad), 4, 'pad size', self.name)
        self.pad_mode = validator.check_string(pad_mode, ['valid', 'same', 'pad'], 'pad_mode', self.name)

        if pad_mode != 'pad' and pad != (0, 0, 0, 0):
            raise ValueError(f"For '{self.name}', the 'pad' must be zero when 'pad_mode' is not 'pad', "
                             f"but got 'pad': {self.pad} and 'pad_mode': {self.pad_mode}.")
        self.add_prim_attr("pad", pad)
        self.padding = pad
        if self.pad_mode == 'pad':
            for item in pad:
                validator.check_non_negative_int(item, 'pad item', self.name)

        self.mode = validator.check_equal_int(mode, 1, 'mode', self.name)
        self.format = validator.check_string(data_format, ['NCHW', 'NHWC'], 'format', self.name)
        if context.get_context("device_target") != "GPU" and self.format == "NHWC":
            raise ValueError(f"For '{self.name}', the 'NHWC' format is only supported in GPU target, "
                             f"but got the 'data_format' is {self.format} "
                             f"and platform is {context.get_context('device_target')}.")
        self.add_prim_attr('data_format', self.format)
        self.out_channel = validator.check_positive_int(out_channel, 'out_channel', self.name)
        self.group = validator.check_positive_int(group, 'group', self.name)
        self.add_prim_attr('groups', self.group)


class DataFormatVecPermute(Primitive):
    r"""
    Converts the input tensor from the `src_format` to the `dst_format` by permuting its dimensions.

    Args:
        src_format (str, optional): the source data format, which can be ``'NHWC'`` and ``'NCHW'`` .
          Default: ``'NHWC'`` .
        dst_format (str, optional): the target data format, which can be ``'NHWC'`` and ``'NCHW'`` .
          Default: ``'NCHW'`` .

    Inputs:
        - **input_x** (Tensor) - A Tensor of shape :math:`(4, )` or :math:`(4, 2)` in source data format.
          Supports int32 and int64 datatype.

    Outputs:
        Tensor, has the same data type and shape as the `input_x`.

    Raises:
        TypeError: If `input_x` is not a Tensor.
        TypeError: If dtype of `input_x` is neither int32 nor int64.
        ValueError: If `src_format` or `dst_format` is not a str in ['NHWC', 'NCHW'].
        ValueError: If `input_x` shape is not :math:`(4, )` or :math:`(4, 2)`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> class Net(nn.Cell):
        ...     def __init__(self, src_format="NHWC", dst_format="NCHW"):
        ...         super().__init__()
        ...         self.op = ops.DataFormatVecPermute(src_format, dst_format)
        ...     def construct(self, x):
        ...         return self.op(x)
        ...
        >>> net = Net()
        >>> x = Tensor(np.array([1, 2, 3, 4]).astype(np.int32))
        >>> output = net(x)
        >>> print(output)
        [1 4 2 3]
    """

    @prim_attr_register
    def __init__(self, src_format='NHWC', dst_format='NCHW'):
        """Initialize DataFormatVecPermute."""
        valid_values = ['NHWC', 'NCHW']
        self.src_format = validator.check_string(src_format, valid_values, "src_format", self.name)
        self.dst_format = validator.check_string(dst_format, valid_values, "dst_format", self.name)
        self.init_prim_io_names(inputs=['input_x'], outputs=['output'])


class DepthwiseConv2dNative(PrimitiveWithInfer):
    r"""
    DepthwiseConv2dNative will be deprecated in the future. Please use :class:`mindspore.nn.Conv2d` instead.

    Supported Platforms:
        Deprecated
    """

    @prim_attr_register
    def __init__(self,
                 channel_multiplier,
                 kernel_size,
                 mode=3,
                 pad_mode="valid",
                 pad=0,
                 stride=1,
                 dilation=1,
                 group=1):
        """Initialize DepthwiseConv2dNative"""
        logger.warning("WARN_DEPRECATED: The usage of DepthwiseConv2dNative is deprecated."
                       " Please use nn.Conv2D.")
        self.init_prim_io_names(inputs=['x', 'w'], outputs=['output'])
        self.kernel_size = _check_positive_int_or_tuple('kernel_size', kernel_size, self.name)
        self.stride = _check_positive_int_or_tuple('stride', stride, self.name)
        if self.stride[0] != self.stride[1]:
            raise ValueError("The height and width of 'stride' must be equal,"
                             f"but got height:{self.stride[0]},  width:{self.stride[1]}")
        self.add_prim_attr('stride', (1, 1, self.stride[0], self.stride[1]))

        self.dilation = _check_positive_int_or_tuple('dilation', dilation, self.name)
        if self.dilation[0] != self.dilation[1]:
            raise ValueError("The height and width of 'dilation' must be equal,"
                             f"but got height:{self.dilation[0]},  width:{self.dilation[1]}")
        self.add_prim_attr('dilation', (1, 1, self.dilation[0], self.dilation[1]))
        validator.check_value_type('pad', pad, (int, tuple), self.name)
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        if isinstance(pad, int):
            pad = (pad,) * 4
        else:
            validator.check_equal_int(len(pad), 4, 'pad size', self.name)
        self.pad_mode = validator.check_string(pad_mode.lower(), ['valid', 'same', 'pad'], 'pad_mode', self.name)
        if pad_mode != 'pad' and pad != (0, 0, 0, 0):
            raise ValueError(f"For '{self.name}', the 'pad' must be zero or (0, 0, 0, 0) when 'pad_mode' "
                             f"is not \"pad\", but got 'pad' is {self.pad} and 'pad_mode' is {pad_mode}.")
        self.add_prim_attr("pad", pad)
        self.padding = pad
        if self.pad_mode == 'pad':
            for item in pad:
                validator.check_non_negative_int(item, 'pad item', self.name)
        self.mode = validator.check_equal_int(mode, 3, "mode", self.name)
        self.add_prim_attr('data_format', "NCHW")
        self.channel_multiplier = validator.check_positive_int(channel_multiplier, "channel_multiplier", self.name)
        self.group = validator.check_positive_int(group, "group", self.name)
        self.add_prim_attr('offset_a', 0)

    def infer_shape(self, x_shape, w_shape, b_shape=None):
        validator.check_equal_int(len(w_shape), 4, "weight rank", self.name)
        validator.check_equal_int(len(x_shape), 4, "x rank", self.name)
        validator.check("x_shape[1]", x_shape[1], "w_shape[1]", w_shape[1], validator.EQ, self.name)
        validator.check('kernel_size', self.kernel_size, 'w_shape[2:4]', tuple(w_shape[2:4]), validator.EQ, self.name)

        kernel_size_n, _, kernel_size_h, kernel_size_w = w_shape
        _, _, stride_h, stride_w = self.stride
        _, _, dilation_h, dilation_w = self.dilation
        if kernel_size_n != 1:
            raise ValueError(f"For '{self.name}', the batch of 'weight' must be 1, but got {kernel_size_n}")
        if self.pad_mode == "valid":
            h_out = math.ceil((x_shape[2] - dilation_h * (kernel_size_h - 1)) / stride_h)
            w_out = math.ceil((x_shape[3] - dilation_w * (kernel_size_w - 1)) / stride_w)
            pad_top, pad_bottom, pad_left, pad_right = 0, 0, 0, 0
        elif self.pad_mode == "same":
            h_out = math.ceil(x_shape[2] / stride_h)
            w_out = math.ceil(x_shape[3] / stride_w)

            pad_needed_h = max(0, (h_out - 1) * stride_h + dilation_h * (kernel_size_h - 1) + 1 - x_shape[2])
            pad_top = math.floor(pad_needed_h / 2)
            pad_bottom = pad_needed_h - pad_top

            pad_needed_w = max(0, (w_out - 1) * stride_w + dilation_w * (kernel_size_w - 1) + 1 - x_shape[3])
            pad_left = math.floor(pad_needed_w / 2)
            pad_right = pad_needed_w - pad_left
        elif self.pad_mode == 'pad':
            pad_top, pad_bottom, pad_left, pad_right = self.padding

            h_out = 1 + (x_shape[2] + pad_top + pad_bottom - kernel_size_h - (kernel_size_h - 1) * (dilation_h - 1)) \
                    / stride_h
            w_out = 1 + (x_shape[3] + pad_left + pad_right - kernel_size_w - (kernel_size_w - 1) * (dilation_w - 1)) \
                    / stride_w
            h_out = math.floor(h_out)
            w_out = math.floor(w_out)

        self.pad_list = (pad_top, pad_bottom, pad_left, pad_right)
        self.add_prim_attr('pad_list', self.pad_list)

        out_channel = self.channel_multiplier * x_shape[1]
        out_shape = [x_shape[0], out_channel, h_out, w_out]
        return out_shape

    def infer_dtype(self, x_dtype, w_dtype, b_dtype=None):
        args = {'x': x_dtype, 'w': w_dtype}
        validator.check_tensors_dtypes_same_and_valid(args, mstype.number_type, self.name)
        if x_dtype.element_type() == mstype.int8:
            return mstype.TensorType(mstype.int32)
        return x_dtype


class _Pool(PrimitiveWithInfer):
    r"""
    Performs max/avg pooling operation.

    Args:
        kernel_size (Union[int, tuple[int]]): The size of the kernel, that must be a tuple
           of two `int` for height and width. Default: ``1`` .
        strides (Union[int, tuple[int]]): The stride of the window, that must be
            a tuple of two `int` for height and width. Default: ``1`` .
        pad_mode (str): The optional value for pad mode, is ``"same"`` or ``"valid"`` .
            Default: ``"valid"`` .
        data_format (str): The optional value for data format, is ``'NHWC'`` or ``'NCHW'`` .
            Default: ``"NCHW"`` .
    """

    @prim_attr_register
    def __init__(self, kernel_size=1, strides=1, pad_mode="valid", data_format="NCHW"):
        """Initialize _Pool."""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])
        validator.check_value_type('kernel_size', kernel_size, [int, tuple], self.name)
        validator.check_value_type('strides', strides, [int, tuple], self.name)
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        self.pad_mode = validator.check_string(pad_mode.upper(), ['VALID', 'SAME'], 'pad_mode', self.name)
        self.add_prim_attr("pad_mode", self.pad_mode)
        self.is_maxpoolwithargmax = (self.name == "MaxPoolWithArgmax")
        self.format = validator.check_string(data_format, ['NCHW', 'NHWC'], 'format', self.name)
        if context.get_context("device_target") != "GPU" and self.format == "NHWC":
            raise ValueError(f"For '{self.name}', the 'NHWC' format is only supported in GPU target, "
                             f"but got the 'data_format' is {self.format} and "
                             f"the platform is {context.get_context('device_target')}.")
        if not self.is_maxpoolwithargmax:
            self.add_prim_attr('data_format', self.format)

        self.kernel_size = _check_positive_int_or_tuple(
            "kernel_size", kernel_size, self.name, allow_four=False, ret_four=True)
        if self.is_maxpoolwithargmax:
            self.kernel_size = (1, self.kernel_size[-2], self.kernel_size[-1], 1)
        self.add_prim_attr("kernel_size", self.kernel_size)

        self.strides = _check_positive_int_or_tuple("strides", strides, self.name, allow_four=False, ret_four=True)
        if self.is_maxpoolwithargmax:
            self.strides = (1, self.strides[-2], self.strides[-1], 1)
        self.add_prim_attr("strides", self.strides)

    def infer_shape(self, x_shape):
        x_shape_norm = x_shape if self.format == "NCHW" else [x_shape[0], x_shape[3], x_shape[1], x_shape[2]]
        validator.check_equal_int(len(x_shape_norm), 4, "x rank", self.name)
        batch, channel, input_h, input_w = x_shape_norm
        if self.is_maxpoolwithargmax:
            _, kernel_h, kernel_w, _ = self.kernel_size
            _, stride_h, stride_w, _ = self.strides
        else:
            _, _, kernel_h, kernel_w = self.kernel_size
            _, _, stride_h, stride_w = self.strides

        if self.pad_mode == "VALID":
            if input_h == -1:
                out_h = -1
            else:
                out_h = math.ceil((input_h - (kernel_h - 1)) / stride_h)
            if input_w == -1:
                out_w = -1
            else:
                out_w = math.ceil((input_w - (kernel_w - 1)) / stride_w)
        elif self.pad_mode == "SAME":
            if input_h == -1:
                out_h = -1
            else:
                out_h = math.ceil(input_h / stride_h)
            if input_w == -1:
                out_w = -1
            else:
                out_w = math.ceil(input_w / stride_w)
        out_shape = [batch, channel, out_h, out_w] if self.format == "NCHW" else [batch, out_h, out_w, channel]

        is_dynamic_shape = False
        for in_shape_val in x_shape_norm:
            if in_shape_val == -1:
                is_dynamic_shape = True

        for out_shape_val in out_shape:
            if out_shape_val <= 0 and not is_dynamic_shape:
                raise ValueError(f"For '{self.name}', the each element of the output shape must be larger than 0, "
                                 f"but got output shape: {out_shape}. The input shape: {x_shape}, "
                                 f"kernel size: {self.kernel_size}, strides: {self.strides}."
                                 f"Please check the official api documents for "
                                 f"more information about the output.")
        return out_shape

    def infer_dtype(self, x_dtype):
        validator.check_subclass("input", x_dtype, mstype.tensor_type, self.name)
        return x_dtype


class MaxPool(_Pool):
    r"""
    Max pooling operation.

    Applies a 2D max pooling over an input Tensor which can be regarded as a composition of 2D planes.

    Typically the input is of shape :math:`(N_{in}, C_{in}, H_{in}, W_{in})`, MaxPool outputs
    regional maximum in the :math:`(H_{in}, W_{in})`-dimension. Given kernel size
    :math:`ks = (h_{ker}, w_{ker})` and stride :math:`s = (s_0, s_1)`, the operation is as follows:

    .. math::
        \text{output}(N_i, C_j, h, w) = \max_{m=0, \ldots, h_{ker}-1} \max_{n=0, \ldots, w_{ker}-1}
        \text{input}(N_i, C_j, s_0 \times h + m, s_1 \times w + n)

    Args:
        kernel_size (Union[int, tuple[int]]): The size of kernel used to take the maximum value,
            is an int number that represents height and width of the kernel, or a tuple
            of two int numbers that represent height and width respectively. Default: ``1`` .
        strides (Union[int, tuple[int]]): The distance of kernel moving, an int number that represents
            not only the height of movement but also the width of movement, or a tuple of two int numbers that
            represent height and width of movement respectively. Default: ``1`` .
        pad_mode (str, optional): Specifies the padding mode with a padding value of 0. It can be set to:
            ``'same'`` or ``'valid'`` . Default: ``'valid'`` .

            - ``'same'``: Pad the input around its edges so that the shape of input and output
              are the same when `stride` is set to ``1``.
              The amount of padding to is calculated by the operator internally, If the amount is even, it is
              uniformly distributed around the input, if it is odd, the excess amount goes to the right/bottom side.
            - ``'valid'``: No padding is applied to the input, and the output returns the maximum
              possible height and width. Extra pixels that could not complete a full stride will
              be discarded.

        data_format (str) : The optional value for data format, is ``'NHWC'`` or ``'NCHW'`` .
            Default: ``'NCHW'`` .

    Inputs:
        - **x** (Tensor) - Tensor of shape :math:`(N, C_{in}, H_{in}, W_{in})`.
          Supported dtypes:

          - CPU: float16, float32, float64.
          - GPU/Ascend: float16, float32.

    Outputs:
        Tensor, with shape :math:`(N, C_{out}, H_{out}, W_{out})`.

    Raises:
        TypeError: If `kernel_size` or `strides` is neither int nor tuple.
        ValueError: If `pad_mode` is neither ``'valid'`` nor ``'same'`` with not case sensitive.
        ValueError: If `data_format` is neither ``'NCHW'`` nor ``'NHWC'``.
        ValueError: If `kernel_size` or `strides` is less than 1.
        ValueError: If length of shape of `input` is not equal to 4.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.arange(1 * 3 * 3 * 4).reshape((1, 3, 3, 4)), mindspore.float32)
        >>> maxpool_op = ops.MaxPool(pad_mode="VALID", kernel_size=2, strides=1)
        >>> output = maxpool_op(x)
        >>> print(output)
        [[[[ 5.  6.  7.]
           [ 9. 10. 11.]]
          [[17. 18. 19.]
           [21. 22. 23.]]
          [[29. 30. 31.]
           [33. 34. 35.]]]]
    """

    @prim_attr_register
    def __init__(self, kernel_size=1, strides=1, pad_mode="valid", data_format="NCHW"):
        """Initialize MaxPool."""
        super(MaxPool, self).__init__(kernel_size, strides, pad_mode, data_format)


class MaxPoolV1(Primitive):
    r"""
    Maxpooling operation.

    Applies a 2D maxpooling over an input Tensor which can be regarded as a composition of 2D planes.

    Typically, the input is of shape :math:`(N_{in}, C_{in}, H_{in}, W_{in})`, MaxPoolV1
    outputs regional maximum in the :math:`(H_{in}, W_{in})`-dimension. Given kernel size
    :math:`ks = (h_{ker}, w_{ker})` and stride :math:`s = (s_h, s_w)`, the operation is as follows.

    .. math::
        \text{output}(N_i, C_j, h, w) = \max_{m=0, \ldots, h_{ker}-1} \max_{n=0, \ldots, w_{ker}-1}
        \text{input}(N_i, C_j, s_h \times h + m, s_w \times w + n)

    Args:
        kernel_size (Union[int, tuple[int]]): The size of kernel used to take the max value,
            is an integer that represents height and width of the kernel, or a tuple
            of two integers that represent height and width respectively. Default: ``1`` .
        strides (Union[int, tuple[int]]): The distance of kernel moving, an integer that represents
            the height and width of movement are both strides, or a tuple of two integers that
            represent height and width of movement, respectively. Default: ``1`` .
        pad_mode (str, optional): Specifies the padding mode with a padding value of 0. It can be set to:
            ``"same"`` or ``"valid"`` . Default: ``"valid"`` .

            - ``"same"``: Pad the input around its edges so that the shape of input and output
              are the same when `stride` is set to ``1``.
              The amount of padding to is calculated by the operator internally, If the amount is even, it is
              uniformly distributed around the input, if it is odd, the excess amount goes to the right/bottom side.
            - ``"valid"``: No padding is applied to the input, and the output returns the maximum
              possible height and width. Extra pixels that could not complete a full stride will
              be discarded.

        data_format (str) : The optional value for data format, is ``'NCHW'`` or ``'NHWC'`` .
            Default: ``'NCHW'`` .

    Inputs:
        - **x** (Tensor) - Tensor of shape :math:`(N, C_{in}, H_{in}, W_{in})`.

    Outputs:
        Tensor, with shape :math:`(N, C_{out}, H_{out}, W_{out})`.

    Raises:
        TypeError: If `kernel_size` or `strides` is neither int nor tuple.
        ValueError: If `pad_mode` is neither 'valid' nor 'same' with not case sensitive.
        ValueError: If `data_format` is neither 'NHWC' nor 'NCHW'.
        ValueError: If `kernel_size` or `strides` is less than 1.
        ValueError: If the length of shape of `input` is not equal to 4.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> x = Tensor(np.arange(1 * 3 * 3 * 4).reshape((1, 3, 3, 4)), mindspore.float32)
        >>> maxpoolv1_op = ops.MaxPoolV1(pad_mode="VALID", kernel_size=2, strides=1)
        >>> output_ = maxpoolv1_op(x)
        >>> print(output_)
        [[[[ 5.  6.  7.]
           [ 9. 10. 11.]]
          [[17. 18. 19.]
           [21. 22. 23.]]
          [[29. 30. 31.]
           [33. 34. 35.]]]]
    """

    @prim_attr_register
    def __init__(self, kernel_size=1, strides=1, pad_mode="valid", data_format="NCHW"):
        """Initialize MaxPoolV1."""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])
        validator.check_value_type('kernel_size', kernel_size, [int, tuple], self.name)
        validator.check_value_type('strides', strides, [int, tuple], self.name)
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        self.pad_mode = validator.check_string(
            pad_mode.upper(), ['VALID', 'SAME'], 'pad_mode', self.name)
        self.add_prim_attr("pad_mode", self.pad_mode)
        self.format = validator.check_string(
            data_format, ['NCHW', 'NHWC'], 'format', self.name)
        self.add_prim_attr('data_format', self.format)

        self.kernel_size = _check_positive_int_or_tuple(
            "kernel_size", kernel_size, self.name, allow_four=False, ret_four=True)
        self.strides = _check_positive_int_or_tuple(
            "strides", strides, self.name, allow_four=False, ret_four=True)

        kernel_size_adapted = self.kernel_size if self.format == 'NCHW' else (
            self.kernel_size[0], self.kernel_size[2], self.kernel_size[3], self.kernel_size[1])
        strides_adapted = self.strides if self.format == 'NCHW' else (
            self.strides[0], self.strides[2], self.strides[3], self.strides[1])

        self.add_prim_attr("kernel_size", kernel_size_adapted)
        self.add_prim_attr("strides", strides_adapted)


class MaxPool3D(Primitive):
    r"""
    Applies a 3D max pooling over an input Tensor which can be regarded as a composition of 3D planes.

    Typically the input is of shape :math:`(N_{in}, C_{in}, D_{in}, H_{in}, W_{in})`, MaxPool outputs
    regional maximum in the :math:`(D_{in}, H_{in}, W_{in})`-dimension. Given kernel size
    :math:`ks = (d_{ker}, h_{ker}, w_{ker})` and stride :math:`s = (s_0, s_1, s_2)`, the operation is as follows:

    .. math::
        \text{output}(N_i, C_j, d, h, w) =
        \max_{l=0, \ldots, d_{ker}-1} \max_{m=0, \ldots, h_{ker}-1} \max_{n=0, \ldots, w_{ker}-1}
        \text{input}(N_i, C_j, s_0 \times d + l, s_1 \times h + m, s_2 \times w + n)

    Args:
        kernel_size (Union[int, tuple[int]]): The size of kernel used to take the maximum value,
            is an int number that represents depth, height and width of the kernel, or a tuple
            of three int numbers that represent depth, height and width respectively. Default: ``1`` .
        strides (Union[int, tuple[int]]): The distance of kernel moving, an int number that represents
            not only the depth, height of movement but also the width of movement,, or a tuple of three int numbers that
            represent depth, height and width of movement respectively. Default: ``1`` .
        pad_mode (str, optional): Specifies the padding mode with a padding value of 0. It can be set to:
            ``"SAME"`` , ``"VALID"`` or ``"PAD"`` . Default: ``"VALID"`` .

            - ``"SAME"``: Pad the input around its depth/height/width dimension so that the shape of input and output
              are the same when `stride` is set to ``1``.
              The amount of padding to is calculated by the operator internally.  If the amount is even,
              it isuniformly distributed around the input, if it is odd, the excess amount goes
              to the front/right/bottom side.
              If this mode is set, `pad_list` must be 0.
            - ``"VALID"``: No padding is applied to the input, and the output returns the maximum
              possible depth, height and width. Extra pixels that could not complete a full stride will
              be discarded. If this mode is set, `pad_list` must be 0.
            - ``"PAD"``: Pad the input with a specified amount. In this mode, the amount of padding
              in the depth, height and width dimension is determined by the `pad_list` parameter.
              If this mode is set, `pad_list` must be greater than or equal to 0.

        pad_list (Union(int, tuple[int])): The pad value to be filled. Default: ``0`` . If `pad` is an integer, the
            paddings of head, tail, top, bottom, left and right are the same, equal to pad. If `pad` is a tuple of six
            integers, the padding of head, tail, top, bottom, left and right equals to pad[0], pad[1], pad[2],
            pad[3], pad[4] and pad[5] correspondingly.
        ceil_mode (Union[bool, None]): Whether to use ceil instead of floor to calculate output shape.
            Only effective in "pad" mode.
            When `pad_mode` is ``"pad"`` and "ceil_mode" is ``None`` , `ceil_mode` will be set as ``False``.
            Default: ``None`` .
        data_format (str) : The optional value for data format. Currently only support ``"NCDHW"`` .
            Default: ``"NCDHW"`` .

    Inputs:
        - **x** (Tensor) - Tensor of shape :math:`(N, C, D_{in}, H_{in}, W_{in})`.
          Data type must be float16, float32 or float64.

    Outputs:
        Tensor, with shape :math:`(N, C, D_{out}, H_{out}, W_{out})`. Has the data type of `x`.

    Raises:
        TypeError: If `kernel_size` or `strides` is neither an int nor a tuple.
        TypeError: If `pad_mode` or `data_format` is not a string.
        ValueError: If numbers in `kernel_size` or `strides` are not positive.
        ValueError: If `pad_mode` is not one of ``"SAME"``, ``"VALID"`` or ``"PAD"``.
        ValueError: If `pad_mode` is ``"SAME"`` or ``"VALID"``, `ceil_mode` is not ``None``.
        ValueError: If `kernel_size` or `strides` is a tuple whose length is not equal to 3.
        ValueError: If `data_format` is not ``"NCDHW"``.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.arange(1 * 2 * 2 * 2 * 3).reshape((1, 2, 2, 2, 3)), mindspore.float32)
        >>> max_pool3d = ops.MaxPool3D(kernel_size=2, strides=1, pad_mode="VALID")
        >>> output = max_pool3d(x)
        >>> print(output)
        [[[[[10. 11.]]]
          [[[22. 23.]]]]]
    """

    @prim_attr_register
    def __init__(self, kernel_size=1, strides=1, pad_mode="VALID", pad_list=0, ceil_mode=None, data_format="NCDHW"):
        """Initialize MaxPool3D."""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])
        validator.check_value_type('kernel_size', kernel_size, [int, tuple], self.name)
        validator.check_value_type('strides', strides, [int, tuple], self.name)
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        self.pad_mode = validator.check_string(pad_mode.upper(), ['VALID', 'SAME', 'PAD'], 'pad_mode', self.name)
        if pad_mode.upper() == "PAD":
            self.pad_mode = "CALCULATED"
        self.add_prim_attr("pad_mode", self.pad_mode)
        self.data_format = validator.check_string(data_format, ['NCDHW'], 'data_format', self.name)
        self.kernel_size = _check_3d_int_or_tuple("kernel_size", kernel_size, self.name, ret_five=True)
        self.add_prim_attr("kernel_size", self.kernel_size)
        self.strides = _check_3d_int_or_tuple("strides", strides, self.name, ret_five=True)
        self.add_prim_attr("strides", self.strides)
        if ceil_mode is None:
            self.ceil_mode = False
        else:
            self.ceil_mode = validator.check_value_type('ceil_mode', ceil_mode, [bool], self.name)
            if self.pad_mode != "CALCULATED":
                raise ValueError("When the 'pad_mode' is 'same' or 'valid', the 'ceil_mode' only supports 'None'.")
        self.add_prim_attr("ceil_mode", int(self.ceil_mode))

        validator.check_value_type('pad_list', pad_list, (int, tuple), self.name)
        self.pad_list = pad_list
        if isinstance(self.pad_list, int):
            self.pad_list = (self.pad_list,) * 6
        if len(self.pad_list) == 3:
            self.pad_list = (pad_list[0], pad_list[0], pad_list[1], pad_list[1], pad_list[2], pad_list[2])
        if len(self.pad_list) != 3 and len(self.pad_list) != 6:
            raise ValueError(f"For '{self.name}', attr 'pad_list' must be an positive int number or a tuple of "
                             f"three or six positive int numbers, but got {len(self.pad_list)} numbers.")
        if self.pad_mode != 'CALCULATED' and self.pad_list != (0, 0, 0, 0, 0, 0):
            raise ValueError(f"For '{self.name}', the 'pad_list' must be zero or (0, 0, 0, 0, 0, 0) when 'pad_mode' "
                             f"is not \"pad\", but got 'pad_list' is {pad_list} and 'pad_mode' is {pad_mode}.")
        if self.pad_mode == 'CALCULATED':
            for item in self.pad_list:
                validator.check_non_negative_int(item, 'pad_list item', self.name)
        self.add_prim_attr("pad_list", self.pad_list)


class MaxUnpool2D(Primitive):
    r"""
    Calculates the partial inverse of MaxPool2D operation.

    Since MaxPool2D loses non-maximal values, it is not fully invertible.
    Therefore, MaxUnpool2D takes the output of MaxPool2D, including the indices of
    the maximal values, and computes a partial inverse where all non-maximal values are set to zero.
    Typically the input is of shape :math:`(N, C, H_{in}, W_{in})` ,
    the output is of shape :math:`(N, C, H_{out}, W_{out})` , the operation is as follows:

    .. math::
        \begin{array}{ll} \\
        H_{out} = (H{in} - 1) \times strides[0] - 2 \times pads[0] + ksize[0] \\
        W_{out} = (W{in} - 1) \times strides[1] - 2 \times pads[1] + ksize[1] \\
        \end{array}

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        ksize (Union[int, tuple[int]]): The size of kernel used to take the maximum value,
            is an int number that represents height and width of the kernel, or a tuple
            of two int numbers that represent height and width respectively.
        strides (Union[int, tuple[int]], optional): The strides of kernel moving.
            If `strides` is 0 or (0, 0), then `strides` equal to `ksize` . Default: ``0`` .

            - An int number that represents the height and width of movement are both `strides` .
            - A tuple of two int numbers that represent height and width of movement respectively.

        pads (Union[int, tuple[int]], optional): The pad value to be filled. Default: ``0`` .

            - If `pads` is an integer, the paddings of height and width are the same, equal to pads.
            - If `pads` is a tuple of two integers, the padding of height and width equal to pads[0]
              and pads[1] correspondingly.

        output_shape (tuple[int], optional): The target output size is an optional input. Default: ``()`` .

            - If :math:`output\_shape == ()` , then the shape of output computed by `kszie`, `strides` and `pads` .
            - If :math:`output\_shape != ()` , then `output_shape` must be :math:`(N, C, H, W)` or :math:`(N, H, W, C)`
              and `output_shape` must belong to :math:`[(N, C, H_{out} - strides[0], W_{out} - strides[1]),
              (N, C, H_{out} + strides[0], W_{out} + strides[1])]`.

        data_format (str, optional): The optional value for data format.
            Currently support ``"NCHW"`` and ``"NHWC"`` . Default: ``"NCHW"`` .

    Inputs:
        - **x** (Tensor) - The input Tensor to invert.
          Tensor of shape :math:`(N, C, H_{in}, W_{in})` or :math:`(N, H_{in}, W_{in}, C)`.
        - **argmax** (Tensor) - Max values' index represented by the `argmax`.
          Tensor of shape must be same with input `x`.
          Values of `argmax` must belong to :math:`[0, H_{in} \times W_{in} - 1]`.
          Data type must be in int32 or int64.

    Outputs:
        Tensor, with shape :math:`(N, C, H_{out}, W_{out})` or :math:`(N, H_{out}, W_{out}, C)`.
        Has the same data type with `x`.

    Raises:
        TypeError: If data type of `x` or `argmax` is not supported.
        TypeError: If `ksize`, `strides` or `pads` is neither int nor tuple.
        ValueError: If numbers in `strides` (also support 0 and (0, 0)) or `ksize` is not positive.
        ValueError: If numbers in `pads` is negative.
        ValueError: If `ksize`, `strides` or `pads` is a tuple whose length is not equal to 2.
        ValueError: If `data_format` is not a str or is neither `NCHW` nor `NHWC`.
        ValueError: If `output_shape` whose length is neither 0 or 4.
        ValueError: If `output_shape` is not close to output size
                    computed by attr `ksize`, `strides` and `pads`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([[[[0, 1], [8, 9]]]]).astype(np.float32))
        >>> argmax = Tensor(np.array([[[[0, 1], [2, 3]]]]).astype(np.int64))
        >>> maxunpool2d = ops.MaxUnpool2D(ksize=1, strides=1, pads=0)
        >>> output = maxunpool2d(x, argmax)
        >>> print(output.asnumpy())
        [[[[0. 1.]
            [8. 9.]]]]
    """

    @prim_attr_register
    def __init__(self, ksize, strides=0, pads=0, output_shape=(), data_format="NCHW"):
        """Initialize MaxUnpool2D."""
        self.init_prim_io_names(inputs=['x', 'argmax'], outputs=['y'])
        self.ksize = _check_positive_int_or_tuple('ksize', ksize, self.name, ret_four=True)
        if strides in (0, (0, 0)):
            strides = ksize
        self.strides = _check_positive_int_or_tuple('strides', strides, self.name, ret_four=True)
        self.pads = _check_positive_int_or_tuple('pads', pads, self.name, ret_four=True, strict_positive=False)
        self.data_format = validator.check_string(data_format, ['NCHW', 'NHWC'], 'data_format', self.name)

        if data_format == "NHWC":
            self.ksize = (self.ksize[0], self.ksize[2], self.ksize[3], self.ksize[1])
            self.strides = (self.strides[0], self.strides[2], self.strides[3], self.strides[1])
            self.pads = (self.pads[0], self.pads[2], self.pads[3], self.pads[1])

        self.add_prim_attr('ksize', self.ksize)
        self.add_prim_attr('strides', self.strides)
        self.add_prim_attr('pads', self.pads)

        validator.check_value_type("output_shape", output_shape, [tuple], self.name)
        self.output_shape = output_shape


class MaxUnpool3D(Primitive):
    r"""
    Computes the inverse of :class:`mindspore.ops.MaxPool3D`.

    MaxUnpool3D keeps the maximal value and set all position of non-maximal values to zero.
    Typically the input is of shape :math:`(N, C, D_{in}, H_{in}, W_{in})`, the output is of
    shape :math:`(N, C, D_{out}, H_{out}, W_{out})`, the operation is as follows.

    .. math::
        \begin{array}{ll} \\
        D_{out} = (D{in} - 1) \times strides[0] - 2 \times pads[0] + ksize[0] \\
        H_{out} = (H{in} - 1) \times strides[1] - 2 \times pads[1] + ksize[1] \\
        W_{out} = (W{in} - 1) \times strides[2] - 2 \times pads[2] + ksize[2] \\
        \end{array}

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        ksize (Union[int, tuple[int]]): The size of kernel used to take the maximum value,
            is an int number that represents depth, height and width of the kernel, or a tuple
            of three int numbers that represent depth, height and width respectively.
        strides (Union[int, tuple[int]], optional): The distance of kernel moving. Default: ``0`` .

            - If it is an int number, the depth, height and width of movement are all equal to `strides`.
            - If it is a tuple of three int numbers, they represent depth, height and width of movement respectively.
            - If strides is 0 or (0, 0, 0), then `strides` equal to `ksize`.

        pads (Union[int, tuple[int]], optional): The pad value to be filled. Default: ``0`` .

            - If `pads` is an integer, the paddings of depth, height and width are the same, equal to pads.
            - If `pads` is a tuple of three integers, the padding of depth, height and width equal to pads[0],
              pads[1] and pads[2] correspondingly.

        output_shape (tuple[int], optional) : The target output size. Default: ``()`` .
            If :math:`output\_shape == ()`, then the shape of output computed by kszie, strides and pads shown above.
            If :math:`output\_shape != ()`, then output_shape format must be :math:`(N, C, D, H, W)` or
            :math:`(N, D, H, W, C)` and output_shape must be in range
            :math:`[(N, C, D_{out} - strides[0], H_{out} - strides[1], W_{out} - strides[2]),
            (N, C, D_{out} + strides[0], H_{out} + strides[1], W_{out} + strides[2])]`.
        data_format (str, optional) : The optional value for data format. Currently
            support ``'NCDHW'`` and ``'NDHWC'`` . Default: ``'NCDHW'`` .

    Inputs:
        - **x** (Tensor) - The input Tensor to invert.
          Tensor of shape :math:`(N, C, D_{in}, H_{in}, W_{in})` or :math:`(N, D_{in}, H_{in}, W_{in}, C)`.
        - **argmax** (Tensor) - Max values' index. Tensor that has the same shape as `x`.
          Values of `argmax` must be in range :math:`[0, D_{in} \times H_{in} \times W_{in} - 1]`.
          Data type must be int32 or int64.

    Outputs:
        Tensor, with shape :math:`(N, C, D_{out}, H_{out}, W_{out})` or :math:`(N, D_{out}, H_{out}, W_{out}, C)`.
        Has the same data type with `x`.

    Raises:
        TypeError: If data type of `x` or `argmax` is Number.
        TypeError: If `ksize`, `strides` or `pads` is neither int nor tuple.
        ValueError: If numbers in `strides` or `ksize` is negative.
        ValueError: If numbers in `pads` is negative.
        ValueError: If `ksize`, `strides` or `pads` is a tuple whose length is not equal to 3.
        ValueError: If `data_format` is not a str or is neither ``'NCDHW'`` nor ``'NDHWC'``.
        ValueError: If `output_shape` whose length is neither 0 or 5.
        ValueError: If `output_shape` is not close to output size range
                    computed by attr `ksize, strides, pads`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([[[[[0, 1], [8, 9]]]]]).astype(np.float32))
        >>> argmax = Tensor(np.array([[[[[0, 1], [2, 3]]]]]).astype(np.int64))
        >>> maxunpool3d = ops.MaxUnpool3D(ksize=1, strides=1, pads=0)
        >>> output = maxunpool3d(x, argmax)
        >>> print(output.asnumpy())
        [[[[[0. 1.]
            [8. 9.]]]]]
    """

    @prim_attr_register
    def __init__(self, ksize, strides=0, pads=0, output_shape=(), data_format="NCDHW"):
        """Initialize MaxUnpool3D."""
        self.init_prim_io_names(inputs=['x', 'argmax'], outputs=['y'])
        self.ksize = _check_3d_int_or_tuple('ksize', ksize, self.name, ret_five=True)
        if strides in (0, (0, 0, 0)):
            strides = ksize
        self.strides = _check_3d_int_or_tuple('strides', strides, self.name, ret_five=True)
        self.pads = _check_3d_int_or_tuple('pads', pads, self.name, ret_five=True, greater_zero=False)
        self.data_format = validator.check_string(data_format, ['NCDHW', 'NDHWC'], 'data_format', self.name)
        if data_format == "NDHWC":
            self.ksize = (self.ksize[0], self.ksize[2], self.ksize[3], self.ksize[4], self.ksize[1])
            self.strides = (self.strides[0], self.strides[2], self.strides[3], self.strides[4], self.strides[1])
            self.pads = (self.pads[0], self.pads[2], self.pads[3], self.pads[4], self.pads[1])

        self.add_prim_attr('ksize', self.ksize)
        self.add_prim_attr('strides', self.strides)
        self.add_prim_attr('pads', self.pads)

        validator.check_value_type("output_shape", output_shape, [tuple], self.name)
        self.output_shape = output_shape


class AvgPoolV1(Primitive):
    r"""
    Average-pooling operation.

    Applies a 2D average pooling over an input Tensor which can be regarded as a composition of 2D planes.
    Typically the input is of shape :math:`(N_{in}, C_{in}, H_{in}, W_{in})`, AvgPoolV1 outputs
    regional average in the :math:`(H_{in}, W_{in})`-dimension. Given window size
    :math:`ks = (h_{ker}, w_{ker})` and strides :math:`s = (s_0, s_1)`, the operation is as follows.

    .. math::
        \text{output}(N_i, C_j, h, w) = \frac{1}{h_{ker} * w_{ker}} \sum_{m=0}^{h_{ker}-1} \sum_{n=0}^{w_{ker}-1}
        \text{input}(N_i, C_j, s_0 \times h + m, s_1 \times w + n)

    .. warning::
        - Only single input and single output are supported.
        - Global average pooling is supported.
        - The height of "kernel_size" and the weight of "kernel_size" are positive integers within the range [1, 255].
          ksize_h * ksize_w < 256.
        - Due to instruction restrictions, the values of "strides_h" and "strides_w" are
          positive integers within the range [1, 64).

    Args:
        kernel_size (Union[int, tuple[int]]): The size of the kernel used to take the average value,
            is an integer that represents height and width of the kernel, or a tuple
            of two integers that represent height and width respectively. Default: ``1`` .
        strides (Union[int, tuple[int]]): The distance of kernel moving, an integer that represents
            the height and width of movement are both strides, or a tuple of two integers that
            represent height and width of movement, respectively. Default: ``1`` .
        pad_mode (str, optional): Specifies the padding mode with a padding value of 0. It can be set to:
            ``"same"`` or ``"valid"`` . Default: ``"valid"`` .

            - ``"same"``: Pad the input around its edges so that the shape of input and output
              are the same when `stride` is set to ``1``.
              The amount of padding to is calculated by the operator internally, If the amount is even, it is
              uniformly distributed around the input, if it is odd, the excess amount goes to the right/bottom side.
            - ``"valid"``: No padding is applied to the input, and the output returns the maximum
              possible height and width. Extra pixels that could not complete a full stride will
              be discarded.

        data_format (str): The format of input and output data. Should be ``'NHWC'`` or ``'NCHW'`` .
            Default: ``'NCHW'`` .

    Inputs:
        - **x** (Tensor) - Tensor of shape :math:`(N, C_{in}, H_{in}, W_{in})`.

    Outputs:
        Tensor, with shape :math:`(N, C_{out}, H_{out}, W_{out})`.

    Raises:
        TypeError: If `kernel_size` or `strides` is neither int nor tuple.
        ValueError: If `pad_mode` is neither 'valid' nor 'same' with not case sensitive.
        ValueError: If `data_format` is neither 'NCHW' nor 'NHWC'.
        ValueError: If `kernel_size` or `strides` is less than 1.
        ValueError: If length of shape of `x` is not equal to 4.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> x = Tensor(np.arange(1 * 2 * 4 * 4).reshape((1, 2, 4, 4)), mindspore.float64)
        >>> avgpoolv1_op = ops.AvgPoolV1(pad_mode="VALID", kernel_size=3, strides=1)
        >>> _output = avgpoolv1_op(x)
        >>> print(_output)
        [[[[ 5.  6.]
           [ 9. 10.]]
          [[21. 22.]
           [25. 26.]]]]
    """

    @prim_attr_register
    def __init__(self, kernel_size=1, strides=1, pad_mode="valid", data_format="NCHW"):
        """Initialize AvgPoolV1."""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])
        validator.check_value_type('kernel_size', kernel_size, [int, tuple], self.name)
        validator.check_value_type('strides', strides, [int, tuple], self.name)
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        self.pad_mode = validator.check_string(
            pad_mode.upper(), ['VALID', 'SAME'], 'pad_mode', self.name)
        self.add_prim_attr("pad_mode", self.pad_mode)
        self.format = validator.check_string(
            data_format, ['NCHW', 'NHWC'], 'format', self.name)
        self.add_prim_attr('data_format', self.format)
        self.kernel_size = _check_positive_int_or_tuple(
            "kernel_size", kernel_size, self.name, allow_four=False, ret_four=True)
        self.strides = _check_positive_int_or_tuple(
            "strides", strides, self.name, allow_four=False, ret_four=True)

        # adapt data_format
        self.kernel_size_adapted = self.kernel_size if self.format == "NCHW" else (
            self.kernel_size[0], self.kernel_size[2], self.kernel_size[3], self.kernel_size[1])
        self.add_prim_attr("kernel_size", self.kernel_size_adapted)
        self.strides_adapted = self.strides if self.format == "NCHW" else (
            self.strides[0], self.strides[2], self.strides[3], self.strides[1])
        self.add_prim_attr("strides", self.strides_adapted)


class Conv2DBackpropInput(Primitive):
    r"""
    The Conv2DBackpropInput interface is deprecated, please refer to :class:`mindspore.ops.Conv2DTranspose` if you
    want to do unsampling.

    Supported Platforms:
        Deprecated
    """
    __mindspore_signature__ = (
        sig.make_sig('out_backprop', dtype=sig.sig_dtype.T),
        sig.make_sig('filter', dtype=sig.sig_dtype.T1),
        sig.make_sig('input_sizes', dtype=sig.sig_dtype.T2)
    )

    @prim_attr_register
    def __init__(self,
                 out_channel,
                 kernel_size,
                 pad_mode="valid",
                 pad=0,
                 pad_list=None,
                 mode=1,
                 stride=1,
                 dilation=1,
                 group=1,
                 data_format="NCHW"):
        """Initialize Conv2DBackpropInput"""
        self.init_prim_io_names(inputs=['out_backprop', 'filter', 'input_sizes'], outputs=['output'])
        self.out_channel = validator.check_positive_int(out_channel, 'out_channel', self.name)
        self.kernel_size = _check_positive_int_or_tuple('kernel_size', kernel_size, self.name)
        self.add_prim_attr('kernel_size', self.kernel_size)
        self.format = validator.check_string(data_format, ['NCHW', 'NHWC'], 'format', self.name)
        if context.get_context("device_target") != "GPU" and self.format == "NHWC":
            raise ValueError(f"For '{self.name}', the 'NHWC' format is only supported in GPU target, "
                             f"but got the 'data_format' is {self.format} and "
                             f"the platform is {context.get_context('device_target')}.")
        self.add_prim_attr('data_format', self.format)
        self.stride = _check_positive_int_or_tuple('stride', stride, self.name, allow_four=True, ret_four=True)
        self.stride = _update_attr_by_format(self.stride, self.format)
        self.add_prim_attr('stride', self.stride)
        self.dilation = _check_positive_int_or_tuple('dilation', dilation, self.name, allow_four=True, ret_four=True)
        self.dilation = _update_attr_by_format(self.dilation, self.format)
        self.add_prim_attr('dilation', self.dilation)
        validator.check_value_type('pad', pad, (int, tuple), self.name)
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        if isinstance(pad, int):
            pad = (pad,) * 4
        else:
            validator.check_equal_int(len(pad), 4, 'pad size', self.name)
        self.pad_mode = validator.check_string(pad_mode.lower(), ['valid', 'same', 'pad'], 'pad_mode', self.name)
        if pad_mode != 'pad' and pad != (0, 0, 0, 0):
            raise ValueError(f"For '{self.name}', the 'pad' must be zero or (0, 0, 0, 0) when 'pad_mode' "
                             f"is not \"pad\", but got 'pad' is {self.pad} and 'pad_mode' is {pad_mode}.")
        self.add_prim_attr("pad", pad)
        self.padding = pad
        if self.pad_mode == 'pad':
            for item in pad:
                validator.check_non_negative_int(item, 'pad item', self.name)

        pad_mode = pad_mode.upper()
        self.add_prim_attr('pad_mode', pad_mode)
        self.mode = validator.check_equal_int(mode, 1, 'mode', self.name)
        self.group = validator.check_positive_int(group, 'group', self.name)
        self.add_prim_attr('groups', self.group)
        if pad_list:
            for x in pad_list:
                if x != -1:
                    validator.check_non_negative_int(x, 'element of pad_list', self.name)
            self.pad_list = pad_list


class MaxPool3DWithArgmax(Primitive):
    r"""
    Performs a 3D max pooling on the input Tensor and returns both max values and indices.

    Typically the input is a Tensor with shape :math:`(N_{in}, C_{in}, D_{in}, H_{in}, W_{in})`, outputs
    regional maximum in the :math:`(D_{in}, H_{in}, W_{in})`-dimension. Given `ksize`
    :math:`ks = (d_{ker}, h_{ker}, w_{ker})` and `strides` :math:`s = (s_0, s_1, s_2)`, the operation is as follows.

    .. math::
        \text{output}(N_i, C_j, d, h, w) =
        \max_{l=0, \ldots, d_{ker}-1} \max_{m=0, \ldots, h_{ker}-1} \max_{n=0, \ldots, w_{ker}-1}
        \text{input}(N_i, C_j, s_0 \times d + l, s_1 \times h + m, s_2 \times w + n)

    The output is a Tensor with shape :math:`(N_{out}, C_{out}, D_{out}, H_{out}, W_{out})` and its depth, height and
    width are:

    .. math::
        \begin{array}{ll} \\
            D_{out} = \frac{D_{in} + 2 \times \text{pads}[0] - \text{dilation}[0] \times (\text{ksize}[0] - 1) - 1}
                {\text{stride}[0]} + 1 \\
            H_{out} = \frac{H_{in} + 2 \times \text{pads}[1] - \text{dilation}[1] \times (\text{ksize}[1] - 1) - 1}
                {\text{stride}[1]} + 1 \\
            W_{out} = \frac{W_{in} + 2 \times \text{pads}[2] - \text{dilation}[2] \times (\text{ksize}[2] - 1) - 1}
                {\text{stride}[2]} + 1 \\
        \end{array}

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        ksize (Union[int, tuple[int]]): The size of kernel used to take the maximum value and arg
            value, is an int number that represents depth, height and width of the kernel, or a tuple of
            three int numbers that represent depth, height and width respectively.
        strides (Union[int, tuple[int]]): The distance of kernel moving, an int number that represents the depth,
            height and width of movement are both strides, or a tuple of three int numbers that
            represent depth, height and width of movement respectively.
        pads (Union[int, tuple[int]]): An int number that represents the depth, height and width of movement are both
            strides, or a tuple of three int numbers that represent depth, height and width of movement respectively.
        dilation (Union[int, tuple[int]]): Default: ``(1, 1, 1)`` .
        ceil_mode (bool): Whether to use ceil instead of floor to calculate output shape. Default: ``False`` .
        data_format (str) : The optional value for data format. Currently only support ``'NCDHW'`` .
            Default: ``'NCDHW'`` .
        argmax_type (mindspore.dtype) : The dtype for argmax. Default: ``mstype.int64`` .

    Inputs:
        - **x** (Tensor) - Tensor of shape :math:`(N_{in}, C_{in}, D_{in}, H_{in}, W_{in})` with data type of int8,
          int16, int32, int64, uint8, uint16, uint32, uint64, float16, float32 or float64.

    Outputs:
        Tuple of 2 Tensors, representing the maxpool result and where the max values are generated.

        - **output** (Tensor) - Maxpooling result, with shape :math:`(N_{out}, C_{out}, D_{out}, H_{out}, W_{out})`.
          It has the same data type as `x`.
        - **argmax** (Tensor) - Index corresponding to the maximum value. Data type is int32 or int64.

    Raises:
        TypeError: If `x` is not a Tensor.
        ValueError: If length of shape of `x` is not equal to 5.
        TypeError: If `ksize` , `strides` , `pads` or `dilation` is not int or tuple.
        ValueError: If `ksize` or `strides` is less than 1.
        ValueError: If `pads` is less than 0.
        ValueError: If `data_format` is not ``'NCDHW'``.
        ValueError: If `argmax_type` is not mindspore.int64 or mindspore.int32.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.arange(2 * 1 * 2 * 2 * 2).reshape((2, 1, 2, 2, 2)), mindspore.float32)
        >>> max_pool3d_with_arg_op = ops.MaxPool3DWithArgmax(ksize=2, strides=1, pads=1)
        >>> output_tensor, argmax = max_pool3d_with_arg_op(x)
        >>> print(output_tensor.shape)
        (2, 1, 3, 3, 3)
        >>> print(argmax.shape)
        (2, 1, 3, 3, 3)
    """

    @prim_attr_register
    def __init__(self, ksize, strides, pads, dilation=(1, 1, 1), ceil_mode=False,
                 data_format='NCDHW', argmax_type=mstype.int64):
        """Initialize MaxPool3DWithArgmax."""
        self.init_prim_io_names(inputs=['x'], outputs=['y', 'argmax'])
        validator.check_value_type('ceil_mode', ceil_mode, bool, self.name)
        validator.check_value_type('data_format', data_format, str, self.name)
        validator.check_value_type("argmax_type", argmax_type, [mstype.Type], self.name)
        argmax_type_valid_values = (mstype.int32, mstype.int64)
        validator.check_type_name(
            "argmax_type", argmax_type, argmax_type_valid_values, self.name)
        self.data_format = validator.check_string(
            data_format, ['NCDHW'], 'data_format', self.name)
        if argmax_type == mstype.int32:
            self.add_prim_attr('argmax_type', 'int32')
        elif argmax_type == mstype.int64:
            self.add_prim_attr('argmax_type', 'int64')
        else:
            raise ValueError(f"For '{self.name}', the 'argmax_type' must be mstype.int32 or mstype.int64, "
                             f"but got {self.argmax_type}.")
        self.ksize = _check_3d_int_or_tuple("ksize", ksize, self.name, ret_five=False)
        self.add_prim_attr('ksize', self.ksize)
        self.strides = _check_3d_int_or_tuple("strides", strides, self.name, ret_five=False)
        self.add_prim_attr('strides', self.strides)
        self.pads = _check_3d_int_or_tuple("pads", pads, self.name, greater_zero=False, ret_five=False)
        self.add_prim_attr('pads', self.pads)
        self.dilation = _check_3d_int_or_tuple("dilation", dilation, self.name, allow_five=True, ret_five=False)
        self.add_prim_attr('dilation', self.dilation)


class Conv2DTranspose(Conv2DBackpropInput):
    """
    Calculates a 2D transposed convolution, which can be regarded as Conv2d for the gradient of the input,
    also called deconvolution, although it is not an actual deconvolution. Because it cannot restore
    the original input data completely, but it can restore the shape of the original input.

    Args:
        out_channel (int): The dimensionality of the output space.
        kernel_size (Union[int, tuple[int]]): The size of the convolution window.
        pad_mode (str, optional): Specifies the padding mode with a padding value of 0. It can be set to:
            ``"same"`` , ``"valid"`` or ``"pad"`` . Default: ``"valid"`` .

            - ``"same"``: Pad the input around its edges so that the shape of input and output
              are the same when `stride` is set to ``1``.
              The amount of padding to is calculated by the operator internally, If the amount is even, it is
              uniformly distributed around the input, if it is odd, the excess amount goes to the right/bottom side.
              If this mode is set, `pad` must be 0.
            - ``"valid"``: No padding is applied to the input, and the output returns the maximum
              possible height and width. Extra pixels that could not complete a full stride will
              be discarded. If this mode is set, `pad` must be 0.
            - ``"pad"``: Pad the input with a specified amount. In this mode, the amount of padding
              in the height and width directions is determined by the `pad` parameter.
              If this mode is set, `pad` must be greater than or equal to 0.

            Please refer to :class:`mindspore.nn.Conv2dTranspose` for more specifications about `pad_mode`.
        pad (Union[int, tuple[int]]): The pad value to be filled. Default: ``0`` . If `pad` is an integer, the paddings
                    of top, bottom, left and right are the same, equal to pad. If `pad` is a tuple of four integers,
                    the padding of top, bottom, left and right equal to pad[0], pad[1], pad[2], and pad[3]
                    correspondingly.
        pad_list (Union[str, None]): The pad list like (top, bottom, left, right). Default: ``None`` .
        mode (int): Modes for different convolutions. The value is currently not used. Default: ``1`` .
        stride (Union[int, tuple[int]]): The stride to be applied to the convolution filter. Default: ``1`` .
        dilation (Union[int, tuple[int]]): Specifies the dilation rate to be used for the dilated convolution.
            Default: ``1`` .
        group (int): Splits input into groups. Default: ``1`` .
        data_format (str): The format of input and output data. It should be ``'NHWC'`` or ``'NCHW'`` .
            Default is ``'NCHW'`` .

    Inputs:
        - **dout** (Tensor) - the gradients with respect to the output of the convolution.
          The shape conforms to the default data_format :math:`(N, C_{out}, H_{out}, W_{out})`.
        - **weight** (Tensor) - Set size of kernel is :math:`(K_1, K_2)`, then the shape is
          :math:`(C_{out}, C_{in}, K_1, K_2)`.
        - **input_size** (Tensor) - A tuple describes the shape of the input which conforms to the format
          :math:`(N, C_{in}, H_{in}, W_{in})`.

    Outputs:
        Tensor, the gradients with respect to the input of convolution. It has the same shape as the input.

    Raises:
        TypeError: If `kernel_size`, `stride`, `pad` or `dilation` is neither an int nor a tuple.
        TypeError: If `out_channel` or `group` is not an int.
        ValueError: If `kernel_size`, `stride` or `dilation` is less than 1.
        ValueError: If `pad_mode` is not one of ``'same'``, ``'valid'`` or ``'pad'``.
        ValueError: If `padding` is a tuple whose length is not equal to 4.
        ValueError: If `pad_mode` it not equal to ``'pad'`` and `pad` is not equal to (0, 0, 0, 0).
        ValueError: If `data_format` is neither ``'NCHW'`` nor ``'NHWC'``.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> dout = Tensor(np.ones([10, 32, 30, 30]), mindspore.float32)
        >>> weight = Tensor(np.ones([32, 32, 3, 3]), mindspore.float32)
        >>> x = Tensor(np.ones([10, 32, 32, 32]))
        >>> conv2d_transpose_input = ops.Conv2DTranspose(out_channel=32, kernel_size=3)
        >>> output = conv2d_transpose_input(dout, weight, ops.shape(x))
        >>> print(output.shape)
        (10, 32, 32, 32)
    """

    @prim_attr_register
    def __init__(self, out_channel, kernel_size, pad_mode="valid", pad=0,
                 pad_list=None, mode=1, stride=1, dilation=1, group=1, data_format="NCHW"):
        """Initialize Conv2DTranspose."""
        super(Conv2DTranspose, self).__init__(out_channel, kernel_size, pad_mode, pad,
                                              pad_list, mode, stride, dilation, group, data_format)


class SoftmaxCrossEntropyWithLogits(Primitive):
    r"""
    Gets the softmax cross-entropy value between logits and labels with one-hot encoding.

    The updating formulas of SoftmaxCrossEntropyWithLogits algorithm are as follows,

    .. math::
        \begin{array}{ll} \\
            p_{ij} = softmax(X_{ij}) = \frac{\exp(x_i)}{\sum_{j = 0}^{N-1}\exp(x_j)} \\
            loss_{ij} = -\sum_j{Y_{ij} * ln(p_{ij})}
        \end{array}

    where :math:`X` represents `logits`.
    :math:`Y` represents `label`.
    :math:`loss` represents `output`.

    Inputs:
        - **logits** (Tensor) - Input logits, with shape :math:`(N, C)`. Data type must be float16 or float32.
        - **labels** (Tensor) - Ground truth labels, with shape :math:`(N, C)`, has the same data type with `logits`.

    Outputs:
        Tuple of 2 tensors(loss, dlogits), the `loss` shape is :math:`(N,)`,
        and the `dlogits` with the same shape as `logits`.

    Raises:
        TypeError: If dtype of `logits` or `labels` is neither float16 nor float32.
        TypeError: If `logits` or `labels` is not a Tensor.
        ValueError: If shape of `logits` is not the same as `labels`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> logits = Tensor([[2, 4, 1, 4, 5], [2, 1, 2, 4, 3]], mindspore.float32)
        >>> labels = Tensor([[0, 0, 0, 0, 1], [0, 0, 0, 1, 0]], mindspore.float32)
        >>> softmax_cross = ops.SoftmaxCrossEntropyWithLogits()
        >>> loss, dlogits = softmax_cross(logits, labels)
        >>> print(loss)
        [0.5899297  0.52374405]
        >>> print(dlogits)
        [[ 0.02760027  0.20393994  0.01015357  0.20393994 -0.44563377]
         [ 0.08015892  0.02948882  0.08015892 -0.4077012   0.21789455]]
    """

    @prim_attr_register
    def __init__(self):
        pass


class SparseSoftmaxCrossEntropyWithLogits(Primitive):
    r"""
    Computes the softmax cross-entropy value between logits and sparse encoding labels.

    Sets input logits as `X`, input label as `Y`, output as `loss`. Then,

    .. math::
        \begin{array}{ll} \\
            p_{ij} = softmax(X_{ij}) = \frac{\exp(x_i)}{\sum_{j = 0}^{N-1}\exp(x_j)} \\
            loss_{ij} = \begin{cases} -ln(p_{ij}), &j = y_i \cr 0, & j \neq y_i \end{cases} \\
            loss = \sum_{ij} loss_{ij}
        \end{array}

    Args:
        is_grad (bool): If ``True`` , this operation returns the computed gradient. Default: ``False`` .

    Inputs:
        - **logits** (Tensor) - Input logits, with shape :math:`(N, C)`. Data type must be float16 or float32.
        - **labels** (Tensor) - Ground truth labels, with shape :math:`(N)`.
          Data type must be int32 or int64.

    Outputs:
        Tensor, if `is_grad` is False, the output tensor is the value of loss which is a scalar tensor;
        if `is_grad` is ``True`` , the output tensor is the gradient of input with the same shape as `logits`.

    Raises:
        TypeError: If `is_grad` is not a bool.
        TypeError: If dtype of `logits` is neither float16 nor float32.
        TypeError: If dtype of `labels` is neither int32 nor int64.
        ValueError: If :math:`logits.shape[0] != labels.shape[0]`.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> logits = Tensor([[2, 3, 1, 4, 5], [2, 1, 2, 4, 3]], mindspore.float32)
        >>> labels = Tensor([0, 1], mindspore.int32)
        >>> sparse_softmax_cross = ops.SparseSoftmaxCrossEntropyWithLogits()
        >>> loss = sparse_softmax_cross(logits, labels)
        >>> print(loss)
        3.4878292
        >>> sparse_softmax_cross_grad = ops.SparseSoftmaxCrossEntropyWithLogits(is_grad=True)
        >>> loss_grad = sparse_softmax_cross_grad(logits, labels)
        >>> print(loss_grad)
        [[-0.48415753  0.04306427  0.00582811  0.11706084  0.3182043 ]
         [ 0.04007946 -0.4852556   0.04007946  0.2961494   0.10894729]]
    """

    @prim_attr_register
    def __init__(self, is_grad=False):
        """Initialize SparseSoftmaxCrossEntropyWithLogits."""
        validator.check_value_type('is_grad', is_grad, [bool], self.name)
        self.init_prim_io_names(inputs=['features', 'labels'], outputs=['output'])
        self.is_grad = is_grad
        self.add_prim_attr('sens', 1.0)


class SparseSoftmaxCrossEntropyWithLogitsV2(Primitive):
    r"""
    Computes the softmax cross-entropy value between logits and sparse encoding labels.

    Sets input logits as `X`, input label as `Y`, output as `loss`. Then,

    .. math::
        \begin{array}{ll} \\
            p_{ij} = softmax(X_{ij}) = \frac{\exp(x_i)}{\sum_{j = 0}^{N-1}\exp(x_j)} \\
            loss_{ij} = \begin{cases} -ln(p_{ij}), &j = y_i \cr 0, & j \neq y_i \end{cases}
        \end{array}

    Inputs:
        - **logits** (Tensor) - Input logits, with shape :math:`(N, C)`. Data type must be float16 or float32.
        - **labels** (Tensor) - Ground truth labels, with shape :math:`(N)`.
          Data type must be int32 or int64.

    Outputs:
        - **loss** (Tensor) - With the same shape as `labels`, the same type as `logits`.
        - **backprop** (Tensor) - With the same shape and same type as `logits`.

    Raises:
        TypeError: If dtype of `logits` is neither float16 nor float32.
        TypeError: If dtype of `labels` is neither int32 nor int64.
        ValueError: If logits.shape is not [batch x classes] or labels.shape is not [batch].

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> logits = Tensor([[2, 3, 1, 4, 5], [2, 1, 2, 4, 3]], mindspore.float32)
        >>> labels = Tensor([0, 1], mindspore.int32)
        >>> sparse_softmax_cross = ops.SparseSoftmaxCrossEntropyWithLogitsV2()
        >>> loss, backprop = sparse_softmax_cross(logits, labels)
        >>> print(loss)
        [3.4519143 3.523744 ]
        >>> print(backprop)
        [[-0.96831506  0.08612854  0.01165623  0.23412165  0.6364086 ]
         [ 0.08015893 -0.9705112   0.08015893  0.5922988   0.21789455]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SparseSoftmaxCrossEntropyWithLogitsV2."""
        self.init_prim_io_names(inputs=['features', 'labels'], outputs=['loss', 'backprop'])


class ApplyMomentum(Primitive):
    r"""
    Optimizer that implements the Momentum algorithm.

    Refer to the paper `On the importance of initialization and momentum in deep
    learning <https://dl.acm.org/doi/10.5555/3042817.3043064>`_  for more details.

    Inputs of `variable`, `accumulation` and `gradient` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Refer to :class:`mindspore.nn.Momentum` for more details about the formula and usage.

    Args:
        use_locking (bool): Whether to enable a lock to protect the variable and accumulation tensors
                            from being updated. Default: ``False`` .
        use_nesterov (bool): Enable Nesterov momentum. Default: ``False`` .
        gradient_scale (float): The scale of the gradient. Default: ``1.0`` .

    Inputs:
        - **variable** (Parameter) - Weights to be updated. Data type must be float64, int64, float, float16,
          int16, int32, int8, uint16, uint32, uint64, uint8, complex64, complex128.
        - **accumulation** (Parameter) - Accumulated gradient value by moment weight,
          has the same data type with `variable`.
        - **learning_rate** (Union[Number, Tensor]) - The learning rate value, must be a float64, int64, float,
          float16, int16, int32, int8, uint16, uint32, uint64, uint8, complex64, complex128 number or
          a scalar tensor with float64, int64, float, float16, int16, int32, int8, uint16, uint32, uint64, uint8,
          complex64, complex128 data type.
        - **gradient** (Tensor) - Gradient, has the same data type as `variable`.
        - **momentum** (Union[Number, Tensor]) - Momentum, must be a float64, int64, float, float16, int16, int32,
          int8, uint16, uint32, uint64, uint8, complex64, complex128 number or
          a scalar tensor with float64, int64, float, float16, int16, int32, int8, uint16, uint32, uint64, uint8,
          complex64, complex128 data type.

    Outputs:
        Tensor, parameters to be updated.

    Raises:
        TypeError: If the `use_locking` or `use_nesterov` is not a bool or `gradient_scale` is not a float.
        TypeError: If the data type of `var`, `accum` and `grad` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...    def __init__(self):
        ...        super(Net, self).__init__()
        ...        self.apply_momentum = ops.ApplyMomentum()
        ...        self.variable = Parameter(Tensor(np.array([[0.6, 0.4],
        ...                                            [0.1, 0.5]]).astype(np.float32)), name="variable")
        ...        self.accumulate = Parameter(Tensor(np.array([[0.6, 0.5],
        ...                                            [0.2, 0.6]]).astype(np.float32)), name="accumulate")
        ...    def construct(self, lr, grad, moment):
        ...        out = self.apply_momentum(self.variable, self.accumulate, lr, grad, moment)
        ...        return out
        >>> net = Net()
        >>> lr = Tensor(0.1, mindspore.float32)
        >>> moment = Tensor(0.9, mindspore.float32)
        >>> grad = Tensor(np.array([[0.3, 0.7], [0.1, 0.8]]).astype(np.float32))
        >>> output = net(lr, grad, moment)
        >>> print(output)
        [[0.51600003 0.285     ]
        [0.072      0.366     ]]
    """
    __mindspore_signature__ = (
        sig.make_sig('variable', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accumulation', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('learning_rate', dtype=sig.sig_dtype.T1),
        sig.make_sig('gradient', dtype=sig.sig_dtype.T),
        sig.make_sig('momentum', dtype=sig.sig_dtype.T2)
    )

    @prim_attr_register
    def __init__(self, use_nesterov=False, use_locking=False, gradient_scale=1.0):
        """Initialize ApplyMomentum."""
        self.use_nesterov = validator.check_bool(use_nesterov, "use_nesterov", self.name)
        self.use_locking = validator.check_bool(use_locking, "use_locking", self.name)
        validator.check_value_type('gradient_scale', gradient_scale, [float], self.name)
        self.init_prim_io_names(inputs=['variable', 'accumulation', 'learning_rate', 'gradient', 'momentum'],
                                outputs=['output'])
        self.add_prim_attr('side_effect_mem', True)


class SmoothL1Loss(Primitive):
    r"""
    Calculate the smooth L1 loss, and the L1 loss function has robustness.

    Refer to :func:`mindspore.ops.smooth_l1_loss` for more details.

    Args:
        beta (float, optional): A parameter used to control the point where the function will change between
            L1 to L2 loss. The value should be greater than zero. Default: ``1.0`` .
        reduction (str, optional): Apply specific reduction method to the output: ``'none'`` , ``'mean'`` ,
            ``'sum'`` . Default: ``'none'`` .

            - ``'none'``: no reduction will be applied.
            - ``'mean'``: compute and return the mean of elements in the output.
            - ``'sum'``: the output elements will be summed.

    Inputs:
        - **logits** (Tensor) - Input Tensor of any dimension. Data type must be float16, float32 or float64.
        - **labels** (Tensor) - Ground truth data, has the same shape and dtype as the `logits`.

    Outputs:
        Tensor, loss float tensor, same shape and dtype as the `logits`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> loss = ops.SmoothL1Loss()
        >>> logits = Tensor(np.array([1, 2, 3]), mindspore.float32)
        >>> labels = Tensor(np.array([1, 2, 2]), mindspore.float32)
        >>> output = loss(logits, labels)
        >>> print(output)
        [0.  0.  0.5]
    """

    @prim_attr_register
    def __init__(self, beta=1.0, reduction='none'):
        """Initialize SmoothL1Loss."""
        validator.check_value_type('beta', beta, [float], self.name)
        validator.check('beta', beta, '', 0, validator.GT, self.name)
        validator.check_string(
            reduction, ['none', 'sum', 'mean'], 'reduction', self.name)
        self.add_prim_attr('sigma', self.beta)
        self.init_prim_io_names(inputs=['prediction', 'target'], outputs=['output'])


class MultiMarginLoss(Primitive):
    r"""
    Creates a loss function that minimizes the hinge loss
    for multi-class classification tasks.
    The loss is calculated by comparing the input and output of the function.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.multi_margin_loss` for more details.

    Args:
        p (int, optional): The norm degree for pairwise distance. Should be 1 or 2. Default: ``1`` .
        margin (int, optional): A parameter to change pairwise distance. Default: ``1.0`` .
        reduction (str, optional): Apply specific reduction method to the output: ``'none'`` , ``'mean'`` ,
            ``'sum'`` . Default: ``'mean'`` .

            - ``'none'``: no reduction will be applied.
            - ``'mean'``: compute and return the weighted mean of elements in the output.
            - ``'sum'``: the output elements will be summed.

    Inputs:
        - **inputs** (Tensor) - Input , with shape :math:`(N, C)`. Data type only support float32, float16
          or float64.
        - **target** (Tensor) - Ground truth labels, with shape :math:`(N,)`. Data type only support int64. The
          value of target should be non-negative, less than C.
        - **weight** (Tensor, optional) - The rescaling weight to each class with shape :math:`(C,)`. Data type only
          support float16, float32 or float64.

    Outputs:
        Tensor, When `reduction` is ``'none'``, the shape is :math:`(N,)`.
        Otherwise, it is a scalar. Has the same data type with `inputs`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.ones(shape=[3, 3]), mindspore.float32)
        >>> target = Tensor(np.array([1, 2, 1]), mindspore.int64)
        >>> weight = Tensor(np.array([1, 1, 1]), mindspore.float32)
        >>> loss = ops.MultiMarginLoss()
        >>> output = loss(x, target, weight)
        >>> print(output)
        0.6666667
    """
    __mindspore_signature__ = (
        sig.make_sig('x'),
        sig.make_sig('target'),
        sig.make_sig('weight', default=None)
    )

    @prim_attr_register
    def __init__(self, p=1, margin=1.0, reduction="mean"):
        """Initialize MultiMarginLoss"""
        self.p = validator.check_value_type('p', p, [int], self.name)
        validator.check_int(p, {1, 2}, validator.IN, 'p', self.name)
        self.margin = validator.check_value_type('margin', margin, [float], self.name)
        self.reduction = validator.check_string(reduction, ['none', 'sum', 'mean'], 'reduction', self.name)
        self.init_prim_io_names(inputs=['x', 'target', 'weight'], outputs=['y'])

    def __call__(self, x, target, weight=None):
        return super().__call__(x, target, weight)


class SoftMarginLoss(Primitive):
    r"""
    SoftMarginLoss operation.

    Creates a criterion that optimizes a two-class classification
    logistic loss between input tensor :math:`x` and target tensor :math:`y`
    (containing 1 or -1).

    .. math::
        \text{loss}(x, y) = \sum_i \frac{\log(1 + \exp(-y[i]*x[i]))}{\text{x.nelement}()}

    where :math:`x.nelement()` is the number of elements of x.

    Args:
        reduction (str, optional): Apply specific reduction method to the output: ``'none'`` , ``'mean'`` ,
            ``'sum'`` . Default: ``'mean'`` .

            - ``'none'``: no reduction will be applied.
            - ``'mean'``: compute and return the mean of elements in the output.
            - ``'sum'``: the output elements will be summed.

    Inputs:
        - **logits** (Tensor) - Predict data. Data type must be float16 or float32.
        - **labels** (Tensor) - Ground truth data, with the same type and shape as `logits`.

    Outputs:
        Tensor or Scalar, if `reduction` is ``"none"``, its shape is the same as `logits`.
        Otherwise, a scalar value will be returned.

    Raises:
        TypeError: If `logits` or `labels` is not a Tensor.
        TypeError: If dtype of `logits` or `labels` is neither float16 nor float32.
        ValueError: If shape of `logits` is not the same as `labels`.
        ValueError: If `reduction` is not one of ``"none"`` , ``"mean"`` or ``"sum"`` .

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> loss = ops.SoftMarginLoss()
        >>> logits = Tensor(np.array([[0.3, 0.7], [0.5, 0.5]]), mindspore.float32)
        >>> labels = Tensor(np.array([[-1, 1], [1, -1]]), mindspore.float32)
        >>> output = loss(logits, labels)
        >>> print(output)
        0.6764238
    """

    @prim_attr_register
    def __init__(self, reduction="mean"):
        """Initialize SoftMarginLoss"""
        self.init_prim_io_names(inputs=['predict', 'label'], outputs=['loss'])
        self.reduction = validator.check_string(reduction, ['none', 'sum', 'mean'], 'reduction', self.name)


class L2Loss(Primitive):
    r"""
    Calculates half of the L2 norm, but do not square the result.

    Set input as x and output as loss.

    .. math::
        loss = \frac{\sum x ^ 2}{2}

    Inputs:
        - **input_x** (Tensor) - Tensor for computing the L2 norm. Data type must be float16, float32 or float64.

    Outputs:
        Tensor, has a Scalar Tensor with the same data type as `input_x`.

    Raises:
        TypeError: If `input_x` is not a Tensor.
        TypeError: If dtype of `input_x` is not float16, float32 or float64.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor(np.array([1, 2, 3]), mindspore.float16)
        >>> l2_loss = ops.L2Loss()
        >>> output = l2_loss(input_x)
        >>> print(output)
        7.0
    """

    @prim_attr_register
    def __init__(self):
        """Initialize L2Loss"""


class DataFormatDimMap(Primitive):
    """
    Returns the dimension index in the destination data format given in the source data format.

    Args:
        src_format (str): An optional value for source data format. The format can be ``'NHWC'`` and ``'NCHW'`` .
            Default: ``'NHWC'`` .
        dst_format (str): An optional value for destination data format. The format can be ``'NHWC'`` and ``'NCHW'`` .
            Default: ``'NCHW'`` .

    Inputs:
        - **input_x** (Tensor) - A Tensor, each element is used as a dimension index of the source data format.
          The suggested values are in the range [-4, 4). Only supports int32.

    Outputs:
        Tensor, Return the dimension index in the given target data format,
        has the same data type and shape as the `input_x`.

    Raises:
        TypeError: If `src_format` or `dst_format` is not a str.
        TypeError: If `input_x` is not a Tensor whose dtype is not int32.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor([0, 1, 2, 3], mindspore.int32)
        >>> dfdm = ops.DataFormatDimMap()
        >>> output = dfdm(input_x)
        >>> print(output)
        [0 3 1 2]
    """

    @prim_attr_register
    def __init__(self, src_format='NHWC', dst_format='NCHW'):
        """Initialize DataFormatDimMap."""
        valid_values = ['NHWC', 'NCHW']
        self.src_format = validator.check_string(src_format, valid_values, "src_format", self.name)
        self.dst_format = validator.check_string(dst_format, valid_values, "dst_format", self.name)
        self.init_prim_io_names(inputs=['input_x'], outputs=['output'])


class RNNTLoss(PrimitiveWithInfer):
    """
    Computes the RNNTLoss and its gradient with respect to the softmax outputs.

    Args:
        blank_label (int): blank label. Default: ``0`` .

    Inputs:
        - **acts** (Tensor) - Tensor of shape :math:`(B, T, U, V)`, where :math:`B` is batch,
          :math:`T` is sequence length, :math:`U` is label length and :math:`V` is output dim.
          Data type must be float16 or float32.
        - **labels** (Tensor) - Tensor of shape :math:`(B, U-1)`. Data type is int32.
        - **input_lengths** (Tensor) - Tensor of shape :math:`(B,)`. Data type is int32.
        - **label_lengths** (Tensor) - Tensor of shape :math:`(B,)`. Data type is int32.

    Outputs:
        - **costs** (Tensor) - Tensor of shape :math:`(B,)`. Data type is int32.
        - **grads** (Tensor) - Has the same shape and dtype as `acts`.

    Raises:
        TypeError: If `acts`, `labels`, `input_lengths` or `label_lengths` is not a Tensor.
        TypeError: If dtype of `acts` is neither float16 nor float32.
        TypeError: If dtype of `labels`, `input_lengths` or `label_lengths` is not int32.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import numpy as np
        >>> from mindspore import ops, Tensor
        >>> B, T, U, V = 1, 2, 3, 5
        >>> blank = 0
        >>> acts = np.random.random((B, T, U, V)).astype(np.float32)
        >>> labels = np.array([[1, 2]]).astype(np.int32)
        >>> input_length = np.array([T] * B).astype(np.int32)
        >>> label_length = np.array([len(l) for l in labels]).astype(np.int32)
        >>> rnnt_loss = ops.RNNTLoss(blank_label=0)
        >>> costs, grads = rnnt_loss(Tensor(acts), Tensor(labels), Tensor(input_length), Tensor(label_length))
        >>> print(costs.shape)
        (1,)
        >>> print(grads.shape)
        (1, 2, 3, 5)
    """

    @prim_attr_register
    def __init__(self, blank_label=0):
        """Initialize RNNTLoss."""
        validator.check_value_type('blank_label', blank_label, [int], self.name)
        self.init_prim_io_names(inputs=['acts', 'labels', 'input_length', 'label_length'],
                                outputs=['costs', 'grads'])

    def infer_shape(self, acts_shape, labels_shape, input_length_shape, label_length_shape):
        validator.check_equal_int(len(acts_shape), 4, 'acts_rank', self.name)
        validator.check_equal_int(len(labels_shape), 2, 'labels_rank', self.name)
        validator.check_equal_int(len(input_length_shape), 1, 'input_length_rank', self.name)
        validator.check_equal_int(len(label_length_shape), 1, 'label_length_rank', self.name)
        validator.check('labels shape[0]', labels_shape[0], 'acts shape[0]', acts_shape[0], validator.EQ, self.name)
        validator.check('labels shape[1]', labels_shape[1], 'acts shape[2]-1',
                        acts_shape[2] - 1, validator.EQ, self.name)
        validator.check('input_length size', input_length_shape[0], 'acts shape[0]',
                        acts_shape[0], validator.EQ, self.name)
        validator.check('label_length size', label_length_shape[0], 'acts shape[0]',
                        acts_shape[0], validator.EQ, self.name)
        costs_shape = (acts_shape[0],)
        return costs_shape, acts_shape

    def infer_dtype(self, acts_type, labels_type, input_length_type, label_length_type):
        validator.check_tensor_dtype_valid("acts_type", acts_type, [mstype.float32, mstype.float16], self.name)
        tuple(map(partial(validator.check_tensor_dtype_valid,
                          valid_dtypes=(mstype.int32,), prim_name=self.name),
                  ("labels", "input_length", "label_length"),
                  (labels_type, input_length_type, label_length_type)))
        return acts_type, acts_type


class SGD(PrimitiveWithCheck):
    """
    Computes the stochastic gradient descent. Momentum is optional.

    Nesterov momentum is based on the formula from paper `On the importance of
    initialization and momentum in deep learning <http://proceedings.mlr.press/v28/sutskever13.html>`_.

    Note:
        If parameters are not grouped, the `weight_decay` in optimizer will be applied on the network parameters without
        'beta' or 'gamma' in their names. Users can group parameters to change the strategy of decaying weight. When
        parameters are grouped, each group can set `weight_decay`. If not, the `weight_decay` in optimizer will be
        applied.
        For more details, please refer to :class:`mindspore.nn.SGD`.

    Args:
        dampening (float): The dampening for momentum. Default: ``0.0`` .
        weight_decay (float): Weight decay (L2 penalty). Default: ``0.0`` .
        nesterov (bool): Enable Nesterov momentum. Default: ``False`` .

    Inputs:
        - **parameters** (Tensor) - Parameters to be updated. With float16 or float32 data type.
        - **gradient** (Tensor) - Gradient, with float16 or float32 data type.
        - **learning_rate** (Tensor) - Learning rate, a scalar tensor with float16 or float32 data type.
          e.g. Tensor(0.1, mindspore.float32)
        - **accum** (Tensor) - Accum(velocity) to be updated. With float16 or float32 data type.
        - **momentum** (Tensor) - Momentum, a scalar tensor with float16 or float32 data type.
          e.g. Tensor(0.1, mindspore.float32).
        - **stat** (Tensor) - States to be updated with the same shape as gradient, with float16 or float32 data type.

    Outputs:
        Tensor, parameters to be updated.

    Raises:
        TypeError: If `dampening` or `weight_decay` is not a float.
        TypeError: If `nesterov` is not a bool.
        TypeError: If `parameters`, `gradient`, `learning_rate`, `accum`, `momentum` or `stat` is not a Tensor.
        TypeError: If dtype of `parameters`, `gradient`, `learning_rate`, `accum`, `momentum` or `stat` is neither
                   float16 nor float32.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> sgd = ops.SGD()
        >>> parameters = Tensor(np.array([2, -0.5, 1.7, 4]), mindspore.float32)
        >>> gradient = Tensor(np.array([1, -1, 0.5, 2]), mindspore.float32)
        >>> learning_rate = Tensor(0.01, mindspore.float32)
        >>> accum = Tensor(np.array([0.1, 0.3, -0.2, -0.1]), mindspore.float32)
        >>> momentum = Tensor(0.1, mindspore.float32)
        >>> stat = Tensor(np.array([1.5, -0.3, 0.2, -0.7]), mindspore.float32)
        >>> output = sgd(parameters, gradient, learning_rate, accum, momentum, stat)
        >>> print(output.asnumpy())
        [1.99 -0.4903 1.695 3.9801]
    """

    @prim_attr_register
    def __init__(self, dampening=0.0, weight_decay=0.0, nesterov=False):
        """Initialize SGD."""
        validator.check_value_type("nesterov", nesterov, [bool], self.name)
        if nesterov and dampening != 0:
            raise ValueError(f"For '{self.name}', the 'dampening' must be 0 when 'nesterov' is True, "
                             f"but got 'dampening' is {dampening} and 'nesterov' is {nesterov}.")
        self.init_prim_io_names(inputs=['parameters', 'gradient', 'learning_rate', 'accum', 'momentum', 'stat'],
                                outputs=['output'])
        self.add_prim_attr('side_effect_mem', True)

    def check_shape(self, parameters_shape, gradient_shape, learning_rate_shape,
                    accum_shape, momentum_shape, stat_shape):
        validator.check_int(len(gradient_shape), 0, validator.GE, f'gradient rank', self.name)
        validator.check_int(len(learning_rate_shape), 0, validator.GE, f'learning rate rank', self.name)
        validator.check_int(len(momentum_shape), 0, validator.GE, f'momentum rank', self.name)
        validator.check_int(len(stat_shape), 0, validator.GE, f'stat rank', self.name)

    def check_dtype(self, parameters_dtype, gradient_dtype, learning_rate_dtype,
                    accum_dtype, momentum_dtype, stat_dtype):
        tuple(map(partial(validator.check_tensor_dtype_valid,
                          valid_dtypes=(mstype.float16, mstype.float32), prim_name=self.name),
                  ("parameters", "gradient", "learning_rate", "accum", "momentum", "stat"),
                  (parameters_dtype, gradient_dtype, learning_rate_dtype, accum_dtype, momentum_dtype, stat_dtype)))


class ApplyRMSProp(PrimitiveWithInfer):
    r"""
    Optimizer that implements the Root Mean Square prop(RMSProp) algorithm.
    Please refer to the usage in source code of :class:`mindspore.nn.RMSProp`.

    The updating formulas of ApplyRMSProp algorithm are as follows,

    .. math::
        \begin{array}{ll} \\
            s_{t+1} = \rho s_{t} + (1 - \rho)(\nabla Q_{i}(w))^2 \\
            m_{t+1} = \beta m_{t} + \frac{\eta} {\sqrt{s_{t+1} + \epsilon}} \nabla Q_{i}(w) \\
            w = w - m_{t+1}
        \end{array}

    where :math:`w` represents `var`, which will be updated.
    :math:`s_{t+1}` represents `mean_square`, :math:`s_{t}` is the last moment of :math:`s_{t+1}`,
    :math:`m_{t+1}` represents `moment`, :math:`m_{t}` is the last moment of :math:`m_{t+1}`.
    :math:`\rho` represents `decay`. :math:`\beta` is the momentum term, represents `momentum`.
    :math:`\epsilon` is a smoothing term to avoid division by zero, represents `epsilon`.
    :math:`\eta` represents `learning_rate`. :math:`\nabla Q_{i}(w)` represents `grad`.

    .. warning::
        Note that in dense implementation of this algorithm, "mean_square" and "moment" will update even if "grad" is 0,
        but in this sparse implementation, "mean_square" and "moment" will not update
        in iterations during which "grad" is 0.

    Args:
        use_locking (bool): Whether to enable a lock to protect the variable and accumulation tensors
                            from being updated. Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Weights to be updated.
        - **mean_square** (Tensor) - Mean square gradients, must be the same type as `var`.
        - **moment** (Tensor) - Delta of `var`, must be the same type as `var`.
        - **learning_rate** (Union[Number, Tensor]) - Learning rate. Must be a float number or
          a scalar tensor with float16 or float32 data type.
        - **grad** (Tensor) - Gradient, must be the same type as `var`.
        - **decay** (float) - Decay rate. Only constant value is allowed.
        - **momentum** (float) - Momentum. Only constant value is allowed.
        - **epsilon** (float) - Ridge term. Only constant value is allowed.

    Outputs:
        Tensor, parameters to be updated.

    Raises:
        TypeError: If `use_locking` is not a bool.
        TypeError: If `var`, `mean_square`, `moment` or `decay` is not a Tensor.
        TypeError: If `learning_rate` is neither a Number nor a Tensor.
        TypeError: If dtype of `decay`, `momentum` or `epsilon` is not float.
        TypeError: If dtype of `learning_rate` is neither float16 nor float32.
        ValueError: If `decay`, `momentum` or `epsilon` is not a constant value.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_rms_prop = ops.ApplyRMSProp()
        ...         self.var = Parameter(Tensor(np.ones([2, 2]).astype(np.float32)), name="var")
        ...
        ...     def construct(self, mean_square, moment, grad, decay, momentum, epsilon, lr):
        ...         out = self.apply_rms_prop(self.var, mean_square, moment, lr, grad, decay, momentum, epsilon)
        ...         return out
        ...
        >>> net = Net()
        >>> mean_square = Tensor(np.ones([2, 2]).astype(np.float32))
        >>> moment = Tensor(np.ones([2, 2]).astype(np.float32))
        >>> grad = Tensor(np.ones([2, 2]).astype(np.float32))
        >>> output = net(mean_square, moment, grad, 0.0, 1e-10, 0.001, 0.01)
        >>> print(net.var.asnumpy())
        [[0.990005  0.990005]
         [0.990005  0.990005]]
    """

    @prim_attr_register
    def __init__(self, use_locking=False):
        """Initialize ApplyRMSProp."""
        self.use_locking = validator.check_value_type("use_locking", use_locking, [bool], self.name)
        self.init_prim_io_names(inputs=['var', 'mean_square', 'moment', 'learning_rate', 'grad',
                                        'rho', 'momentum', 'epsilon'], outputs=['output'])
        self.add_prim_attr('side_effect_mem', True)


class ApplyCenteredRMSProp(Primitive):
    r"""
    Optimizer that implements the centered RMSProp algorithm.
    Please refer to the usage in source code of :class:`mindspore.nn.RMSProp`.

    The updating formulas of ApplyCenteredRMSProp algorithm are as follows,

    .. math::
        \begin{array}{ll} \\
            g_{t+1} = \rho g_{t} + (1 - \rho)\nabla Q_{i}(w) \\
            s_{t+1} = \rho s_{t} + (1 - \rho)(\nabla Q_{i}(w))^2 \\
            m_{t+1} = \beta m_{t} + \frac{\eta} {\sqrt{s_{t+1} - g_{t+1}^2 + \epsilon}} \nabla Q_{i}(w) \\
            w = w - m_{t+1}
        \end{array}

    where :math:`w` represents `var`, which will be updated.
    :math:`g_{t+1}` represents `mean_gradient`, :math:`g_{t}` is the last moment of :math:`g_{t+1}`.
    :math:`s_{t+1}` represents `mean_square`, :math:`s_{t}` is the last moment of :math:`s_{t+1}`,
    :math:`m_{t+1}` represents `moment`, :math:`m_{t}` is the last moment of :math:`m_{t+1}`.
    :math:`\rho` represents `decay`. :math:`\beta` is the momentum term, represents `momentum`.
    :math:`\epsilon` is a smoothing term to avoid division by zero, represents `epsilon`.
    :math:`\eta` represents `learning_rate`. :math:`\nabla Q_{i}(w)` represents `grad`.

    Note:
        The difference between `ApplyCenteredRMSProp` and `ApplyRMSProp` is that the former
        uses the centered RMSProp algorithm, and the centered RRMSProp algorithm uses an estimate of the centered second
        moment(i.e., the variance) for normalization, as opposed to regular RMSProp, which uses the (uncertained)
        second moment. This often helps with training, but is slightly more expensive in terms of computation and
        memory.

    .. warning::
        In dense implementation of this algorithm, `mean_gradient`, `mean_square`, and `moment` will update
        even if the `grad` is zero. But in this sparse implementation, `mean_gradient`, `mean_square`, and `moment`
        will not update in iterations during which the `grad` is zero.

    Args:
        use_locking (bool): Whether to enable a lock to protect the variable and accumulation tensors
                            from being updated. Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Weights to be updated.
        - **mean_gradient** (Tensor) - Mean gradients, must be the same type as `var`.
        - **mean_square** (Tensor) - Mean square gradients, must be the same type as `var`.
        - **moment** (Tensor) - Delta of `var`, must be the same type as `var`.
        - **grad** (Tensor) - Gradient, must be the same type as `var`.
        - **learning_rate** (Union[Number, Tensor]) - Learning rate. Must be a float number or
          a scalar tensor with float16 or float32 data type.
        - **decay** (float) - Decay rate.
        - **momentum** (float) - Momentum.
        - **epsilon** (float) - Ridge term.

    Outputs:
        Tensor, parameters to be updated.

    Raises:
        TypeError: If `use_locking` is not a bool.
        TypeError: If `var`, `mean_gradient`, `mean_square`, `moment` or `grad` is not a Tensor.
        TypeError: If `learing_rate` is neither a Number nor a Tensor.
        TypeError: If dtype of `learing_rate` is neither float16 nor float32.
        TypeError: If `decay`, `momentum` or `epsilon` is not a float.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_centerd_rms_prop = ops.ApplyCenteredRMSProp()
        ...         self.var = Parameter(Tensor(np.ones([2, 2]).astype(np.float32)), name="var")
        ...
        ...     def construct(self, mean_grad, mean_square, moment, grad, decay, momentum, epsilon, lr):
        ...         out = self.apply_centerd_rms_prop(self.var, mean_grad, mean_square, moment, grad,
        ...                                           lr, decay, momentum, epsilon)
        ...         return out
        ...
        >>> net = Net()
        >>> mean_grad = Tensor(np.ones([2, 2]).astype(np.float32))
        >>> mean_square = Tensor(np.ones([2, 2]).astype(np.float32))
        >>> moment = Tensor(np.ones([2, 2]).astype(np.float32))
        >>> grad = Tensor(np.ones([2, 2]).astype(np.float32))
        >>> output = net(mean_grad, mean_square, moment, grad, 0.0, 1e-10, 0.001, 0.01)
        >>> print(net.var.asnumpy())
        [[0.68377227  0.68377227]
         [0.68377227  0.68377227]]
    """

    @prim_attr_register
    def __init__(self, use_locking=False):
        """Initialize ApplyCenteredRMSProp."""
        self.use_locking = validator.check_value_type("use_locking", use_locking, [bool], self.name)
        self.add_prim_attr('side_effect_mem', True)


class L2Normalize(Primitive):
    r"""
    L2 Normalization Operator.

    This operator will normalize the input using the given axis. The function is shown as follows:

    .. math::
        \displaylines{{\text{output} = \frac{x}{\sqrt{\text{max}( \sum_{i}^{}\left | x_i  \right | ^2, \epsilon)}}}}

    where :math:`\epsilon` is epsilon and :math:`\sum_{i}^{}\left | x_i  \right | ^2` calculate the sum of squares of
    the input `x` along the dimension `axis`.

    Note:
        On Ascend, input data type of float64 is currently not supported.

    Args:
        axis (Union[list(int), tuple(int), int], optional): Specify the axis for calculating the L2 norm.
            Default: ``0`` .
        epsilon (float, optional): A small value added for numerical stability. Default: ``1e-4`` .

    Inputs:
        - **x** (Tensor) - Input to compute the normalization. Tensor of shape :math:`(N, *)`,
          where :math:`*` means any number of additional dimensions.
          Data type must be float16, float32 or float64.

    Outputs:
        Tensor, with the same type and shape as the `x`.

    Raises:
        TypeError: If `axis` is not one of the following: list, tuple or int.
        TypeError: If `epsilon` is not a float.
        TypeError: If `x` is not a Tensor.
        TypeError: If dtype of `x` is not in [float16, float32, float64].
        ValueError: If dimension of `x` is not greater than 0.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> l2_normalize = ops.L2Normalize()
        >>> x = Tensor(np.random.randint(-256, 256, (2, 3, 4)), mindspore.float32)
        >>> output = l2_normalize(x)
        >>> print(output.shape)
        (2, 3, 4)
    """

    @prim_attr_register
    def __init__(self, axis=0, epsilon=1e-4):
        """Initialize L2Normalize."""
        axis = [axis] if isinstance(axis, int) else axis
        validator.check_value_type('axis', axis, [list, tuple], self.name)
        validator.check_value_type('epsilon', epsilon, [int, float], self.name)
        self.add_prim_attr('axis', axis)
        self.init_attrs['axis'] = axis
        if len(axis) != 1:
            raise TypeError(f"For '{self.name}', the length of 'axis' must be 1, but got {len(axis)}, "
                            f"later will support multiple axis!")
        self.axis = axis


class UpsampleTrilinear3D(Primitive):
    r"""
    Performs upsampling with trilinear interpolation across 3dims for 5dim input Tensor.

    This operator scale up the volumetric input with specified `output_size` or `scales` factors,
    using trilinear upscaling algorithm.

    Note:
        One of `scales` and `output_size` must be specified. And it is an error if both are specified.

    Args:
        align_corners (bool, optional): An optional bool. Default: ``False``.
            If ``True``, the input and output tensors are aligned by the center points of their corner pixels,
            preserving the values at the corner pixels.
            If ``False`` , the input and output tensors are aligned by the corner points of their corner pixels,
            and the interpolation use edge value padding for out of boundary values.

    Inputs:
        - **x** (Tensor) - 5D tensor of shape :math:`(N, C, D_{in}, H_{in}, W_{in})`. Supporting types:
          [float16, float32, float64].
        - **output_size** (Union[tuple[int], list[int]]):  A tuple or list of 3 int elements
          :math:`(output\_depth, output\_height, output\_width)`. Default: ``None``.
        - **scales** (Union[tuple[float], list[float]]): A tuple or list of 3 float
          elements :math:`(scale\_depth, scale\_height, scale\_width)`. Default: ``None``.

    Outputs:
        - **y** (Tensor) - Upsampled output with the same data type as `x`, whose shape is
          :math:`(N, C, D_{out}, H_{out}, W_{out})`.

    Raises:
        TypeError: When `output_size` is not ``None`` and `output_size` is not list[int] or tuple[int].
        TypeError: When `scales` is not ``None`` and `scales` is not list[float] or tuple[float].
        TypeError: If dtype of `x` is not in [float16, float32, float64].
        TypeError: If type of `align_corners` is not bool.
        ValueError: If any value of `output_size` is negative or zero when `output_size` is not ``None``.
        ValueError: If any value of `scales` is negative or zero when `scales` is not ``None``.
        ValueError: If shape of `x` is not 5D.
        ValueError: If none of `scales` and `output_size` is specified or both specified.
        ValueError: If size of `scales` is not equal 3 when `scales` is specified.
        ValueError: If size of `output_size` is not equal 3 when `output_size` is specified.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> net = ops.UpsampleTrilinear3D()
        >>> in_x = Tensor(input_data=np.random.randn(2, 3, 4, 512, 256))
        >>> output_size=[4, 64, 48]
        >>> out = net(in_x, output_size, None)
        >>> print(out.shape)
        (2, 3, 4, 64, 48)
        >>>
        >>> net = ops.UpsampleTrilinear3D()
        >>> in_x = Tensor(np.arange(1, 5, dtype=np.float32).reshape((1, 1, 1, 2, 2)))
        >>> output_size=[2, 4, 4]
        >>> out = net(in_x, output_size, None)
        >>> print(out)
        [[[[[1.   1.25 1.75 2.  ]
            [1.5  1.75 2.25 2.5 ]
            [2.5  2.75 3.25 3.5 ]
            [3.   3.25 3.75 4.  ]]
           [[1.   1.25 1.75 2.  ]
            [1.5  1.75 2.25 2.5 ]
            [2.5  2.75 3.25 3.5 ]
            [3.   3.25 3.75 4.  ]]]]]
    """

    @prim_attr_register
    def __init__(self, align_corners=False):
        """Initialize UpsampleTrilinear3D."""
        self.init_prim_io_names(inputs=['x', 'output_size', 'scales'], outputs=['y'])
        self.align_corners = align_corners
        validator.check_bool(self.align_corners, "align_corners", self.name)
        self.add_prim_attr('align_corners', self.align_corners)


class GetNext(Primitive):
    """
    Returns the next element in the dataset queue.

    Note:
        The GetNext operation needs to be associated with network and it also depends
        on the 'dataset' interface, For example, please refer to :class:`mindspore.dataset.MnistDataset` .
        it can't be used directly as a single operation.
        For details, please refer to :class:`mindspore.connect_network_with_dataset` source code.

    Args:
        types (list[:class:`mindspore.dtype`]): The type of the outputs.
        shapes (list[tuple[int]]): The dimensionality of the outputs.
        output_num (int): The output number, length of `types` and `shapes`.
        shared_name (str): Queue name to fetch the data.

    Inputs:
        No inputs.

    Outputs:
        tuple[Tensor], the output of dataset. The shape is described in `shapes`
        and the type is described in `types`.

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import ops
        >>> from mindspore import dataset as ds
        >>> from mindspore import dtype as mstype
        >>> data_path = "/path/to/MNIST_Data/train/"
        >>> train_dataset = ds.MnistDataset(data_path, num_samples=10)
        >>> dataset_helper = mindspore.DatasetHelper(train_dataset, dataset_sink_mode=True)
        >>> dataset = dataset_helper.iter.dataset
        >>> dataset_types, dataset_shapes = dataset_helper.types_shapes()
        >>> queue_name = dataset.__transfer_dataset__.queue_name
        >>> get_next = ops.GetNext(dataset_types, dataset_shapes, len(dataset_types), queue_name)
        >>> data, label = get_next()
        >>> relu = ops.ReLU()
        >>> result = relu(data.astype(mstype.float32))
        >>> print(result.shape)
        (28, 28, 1)
    """

    @prim_attr_register
    def __init__(self, types, shapes, output_num, shared_name):
        """Initialize GetNext."""
        validator.check_value_type("types", types, [list, tuple], self.name)
        validator.check_value_type("shapes", shapes, [list, tuple], self.name)
        validator.check("types length", len(types), "shapes length", len(shapes), validator.EQ, self.name)
        validator.check_value_type("output_num", output_num, [int], self.name)


class LSTM(Primitive):
    r"""
    Performs the Long Short-Term Memory (LSTM) on the input.

    For more information, please refer to :class:`mindspore.nn.LSTM`.

    Args:
        input_size (int): Number of features of input.
        hidden_size (int):  Number of features of hidden layer.
        num_layers (int): Number of layers of stacked LSTM.
        has_bias (bool): Whether the cell has bias `b_ih` and `b_hh`.
        bidirectional (bool): Specifies whether it is a bidirectional LSTM.
        dropout (float): If not 0, append `Dropout` layer on the outputs of each
            LSTM layer except the last layer. The range of dropout is [0.0, 1.0].
        proj_size (int): If `proj_size` > 0, a projection of the corresponding size will be used,
            which is only supported on CPU now. Default: ``0`` .

    Inputs:
        - **input** (Tensor) - Tensor of shape :math:`(seq\_len, batch\_size, input\_size)` or
          :math:`(batch\_size, seq\_len, input\_size)`.
        - **h** (Tensor) - Tensor of shape :math:`(num\_directions * num\_layers, batch\_size, real\_hidden\_size)`.
        - **c** (Tensor) - Tensor of shape :math:`(num\_directions * num\_layers, batch\_size, hidden\_size)`.
        - **w** (Tensor) - A weight Tensor.

        If :math:`proj\_size > 0` , :math:`real\_hidden\_size = proj\_size` , otherwise
        :math:`real\_hidden\_size = hidden\_size` .

    Outputs:
        Tuple, a tuple contains `(output, h_n, c_n, reserve, state)`.

        - **output** (Tensor) - Tensor of shape :math:`(seq\_len, batch\_size, num\_directions * real\_hidden\_size)`.
        - **h_n** (Tensor) - Tensor of shape :math:`(num\_directions * num\_layers, batch\_size, real\_hidden\_size)`.
        - **c_n** (Tensor) - Tensor of shape :math:`(num\_directions * num\_layers, batch\_size, hidden\_size)`.
        - **reserve** (Tensor) - Tensor of shape :math:`(r, 1)`.
        - **state** (Tensor) - Random number generator state and its shape is :math:`(s, 1)`.

    Raises:
        TypeError: If `input_size`, `hidden_size` or `num_layers` is not an int.
        TypeError: If `has_bias` or `bidirectional` is not a bool.
        TypeError: If `dropout` is not a float.
        ValueError: If `dropout` is not in range [0.0, 1.0].
        ValueError: If `proj_size` is not in range [0, `hidden_size`).

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> input_size = 10
        >>> hidden_size = 2
        >>> num_layers = 1
        >>> seq_len = 5
        >>> batch_size = 2
        >>>
        >>> net = ops.LSTM(input_size, hidden_size, num_layers, True, False, 0.0)
        >>> input_tensor = Tensor(np.ones([seq_len, batch_size, input_size]).astype(np.float32))
        >>> h0 = Tensor(np.ones([num_layers, batch_size, hidden_size]).astype(np.float32))
        >>> c0 = Tensor(np.ones([num_layers, batch_size, hidden_size]).astype(np.float32))
        >>> w = Tensor(np.ones([112, 1, 1]).astype(np.float32))
        >>> output, hn, cn, _, _ = net(input_tensor, h0, c0, w)
        >>> print(output)
        [[[0.9640267  0.9640267 ]
          [0.9640267  0.9640267 ]]
         [[0.9950539  0.9950539 ]
          [0.9950539  0.9950539 ]]
         [[0.99932843 0.99932843]
          [0.99932843 0.99932843]]
         [[0.9999084  0.9999084 ]
          [0.9999084  0.9999084 ]]
         [[0.9999869  0.9999869 ]
          [0.9999869  0.9999869 ]]]
    """

    @prim_attr_register
    def __init__(self, input_size, hidden_size, num_layers, has_bias, bidirectional, dropout, proj_size=0):
        """Initialize LSTM."""
        self.input_size = validator.check_positive_int(input_size, "input_size", self.name)
        self.hidden_size = validator.check_positive_int(hidden_size, "hidden_size", self.name)
        self.proj_size = validator.check_int_range(proj_size, 0, hidden_size, validator.INC_LEFT,
                                                   'proj_size', self.name)
        self.num_layers = validator.check_positive_int(num_layers, "num_layers", self.name)
        self.has_bias = validator.check_value_type("has_bias", has_bias, (bool,), self.name)
        self.bidirectional = validator.check_value_type("bidirectional", bidirectional, (bool,), self.name)
        self.dropout = validator.check_value_type("dropout", dropout, [float], self.name)
        self.dropout = validator.check_float_range(dropout, 0, 1, validator.INC_BOTH, 'dropout', self.name)

        if bidirectional:
            self.num_directions = 2
        else:
            self.num_directions = 1


class SigmoidCrossEntropyWithLogits(Primitive):
    r"""
    Uses the given logits to compute sigmoid cross entropy between the logits and the label.

    Measures the distribution error in discrete classification tasks where each class is independent
    and not mutually exclusive using cross entropy loss.

    Sets input logits as :math:`X`, input label as :math:`Y`, output as :math:`loss`. Then,

    .. math::

        \begin{array}{ll} \\
            p_{ij} = sigmoid(X_{ij}) = \frac{1}{1 + e^{-X_{ij}}} \\
            loss_{ij} = -[Y_{ij} * ln(p_{ij}) + (1 - Y_{ij})ln(1 - p_{ij})]
        \end{array}

    Inputs:
        - **logits** (Tensor) - Input logits. Tensor of shape :math:`(N, *)` where :math:`*` means any number
          of additional dimensions.
        - **label** (Tensor) - Ground truth label. With the same shape and type as `logits`.

    Outputs:
        Tensor, with the same shape and type as input `logits`.

    Raises:
        TypeError: If `logits` or `label` is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> logits = Tensor(np.array([[-0.8, 1.2, 0.7], [-0.1, -0.4, 0.7]]).astype(np.float32))
        >>> labels = Tensor(np.array([[0.3, 0.8, 1.2], [-0.6, 0.1, 2.2]]).astype(np.float32))
        >>> sigmoid = ops.SigmoidCrossEntropyWithLogits()
        >>> output = sigmoid(logits, labels)
        >>> print(output)
        [[ 0.6111007   0.5032824   0.26318604]
         [ 0.58439666  0.5530153  -0.4368139 ]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SigmoidCrossEntropyWithLogits"""
        self.init_prim_io_names(inputs=['predict', 'target'], outputs=['loss'])


class BCEWithLogitsLoss(PrimitiveWithInfer):
    r"""
    Adds sigmoid activation function to input `logits`, and uses the given logits to compute binary cross entropy
    between the logits and the label.

    Sets input logits as :math:`X`, input label as :math:`Y`, input weight as :math:`W`, output as :math:`L`. Then,

    .. math::

        \begin{array}{ll} \\
            p_{ij} = sigmoid(X_{ij}) = \frac{1}{1 + e^{-X_{ij}}} \\
            L_{ij} = -[Y_{ij}log(p_{ij}) + (1 - Y_{ij})log(1 - p_{ij})]
        \end{array}

    :math:`i` indicates the :math:`i^{th}` sample, :math:`j` indicates the category. Then,

    .. math::
        \ell(x, y) = \begin{cases}
        L, & \text{if reduction} = \text{'none';}\\
        \operatorname{mean}(L), & \text{if reduction} = \text{'mean';}\\
        \operatorname{sum}(L),  & \text{if reduction} = \text{'sum'.}
        \end{cases}

    :math:`\ell` indicates the method of calculating the loss. There are three methods:
    the first method is to provide the loss value directly,
    the second method is to calculate the average value of all losses,
    and the third method is to calculate the sum of all losses.

    This operator will multiply the output by the corresponding weight.
    The tensor `weight` assigns different weights to each piece of data in the batch,
    and the tensor `pos_weight` adds corresponding weights to the positive examples of each category.

    In addition, it can trade off recall and precision by adding weights to positive examples.
    In the case of multi-label classification the loss can be described as:

    .. math::
        \begin{array}{ll} \\
            p_{ij,c} = sigmoid(X_{ij,c}) = \frac{1}{1 + e^{-X_{ij,c}}} \\
            L_{ij,c} = -[P_{c}Y_{ij,c} * log(p_{ij,c}) + (1 - Y_{ij,c})log(1 - p_{ij,c})]
        \end{array}

    where c is the class number (c>1 for multi-label binary classification, c=1 for single-label binary classification),
    n is the number of the sample in the batch and :math:`P_c` is the weight of the positive answer for the class c.
    :math:`P_c>1` increases the recall, :math:`P_c<1` increases the precision.

    Args:
        reduction (str, optional): Apply specific reduction method to the output: ``'none'`` , ``'mean'`` ,
            ``'sum'`` . Default: ``'mean'`` .

            - ``'none'``: no reduction will be applied.
            - ``'mean'``: compute and return the weighted mean of elements in the output.
            - ``'sum'``: the output elements will be summed.

    Inputs:
        - **logits** (Tensor) - Input logits. Data type must be float16 or float32.
          Tensor of shape :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **label** (Tensor) - Ground truth label, has the same shape as `logits`.
          Data type must be float16 or float32.
        - **weight** (Tensor) - A rescaling weight applied to the loss of each batch element. It can be
          broadcast to a tensor with shape of `logits`. Data type must be float16 or float32.
        - **pos_weight** (Tensor) - A weight of positive examples. Must be a vector with length equal to the
          number of classes. It can be broadcast to a tensor with shape of `logits`.
          Data type must be float16 or float32.

    Outputs:
        Tensor or Scalar, if `reduction` is ``'none'``, it's a tensor with the same shape and type as input `logits`.
        Otherwise, the output is a scalar.

    Raises:
        TypeError: If any input is not Tensor.
        TypeError: If data type of any input is neither float16 nor float32.
        TypeError: If data type of `reduction` is not string.
        ValueError: If `weight` or `pos_weight` can not be broadcast to a tensor with shape of `logits`.
        ValueError: If `reduction` is not one of ``'none'``, ``'mean'`` or ``'sum'``.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> logits = Tensor(np.array([[-0.8, 1.2, 0.7], [-0.1, -0.4, 0.7]]), mindspore.float32)
        >>> label = Tensor(np.array([[0.3, 0.8, 1.2], [-0.6, 0.1, 2.2]]), mindspore.float32)
        >>> weight = Tensor(np.array([1.0, 1.0, 1.0]), mindspore.float32)
        >>> pos_weight = Tensor(np.array([1.0, 1.0, 1.0]), mindspore.float32)
        >>> loss = ops.BCEWithLogitsLoss()
        >>> output = loss(logits, label, weight, pos_weight)
        >>> print(output)
        0.3463612
    """

    @prim_attr_register
    def __init__(self, reduction='mean'):
        """Initialize BCEWithLogitsLoss"""
        super().__init__("BCEWithLogitsLoss")
        self.reduction = validator.check_string(reduction, ['none', 'sum', 'mean'], 'reduction', self.name)


class Pad(Primitive):
    r"""
    Pads the input tensor according to the paddings.

    Refer to :func:`mindspore.ops.pad` for more details. Use :func:`mindspore.ops.pad` instead if `paddings` has
    negative values.

    Args:
        paddings (tuple): The shape of parameter `paddings` is (N, 2). N is the rank of input data. All elements of
            paddings are int type. For the input in `D` th dimension, paddings[D, 0] indicates how many sizes to be
            extended ahead of the input tensor in the `D` th dimension, and paddings[D, 1] indicates how many sizes to
            be extended behind the input tensor in the `D` th dimension.

    Inputs:
        - **input_x** (Tensor) - Tensor to be padded. It has shape :math:`(N, *)`, where :math:`*` means
          any number of additional dimensions.

    Outputs:
        Tensor, the tensor after padding.

    Raises:
        TypeError: If `paddings` is not a tuple.
        TypeError: If `input_x` is not a Tensor.
        ValueError: If shape of `paddings` is not :math:`(N, 2)`.
        ValueError: If paddings.size is not equal to 2 * len(input_x).

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor(np.array([[-0.1, 0.3, 3.6], [0.4, 0.5, -3.2]]), mindspore.float32)
        >>> pad_op = ops.Pad(((1, 2), (2, 1)))
        >>> output = pad_op(input_x)
        >>> print(output)
        [[ 0.   0.   0.   0.   0.   0. ]
         [ 0.   0.  -0.1  0.3  3.6  0. ]
         [ 0.   0.   0.4  0.5 -3.2  0. ]
         [ 0.   0.   0.   0.   0.   0. ]
         [ 0.   0.   0.   0.   0.   0. ]]
    """

    @prim_attr_register
    def __init__(self, paddings):
        """Initialize Pad"""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])
        validator.check_value_type("paddings", paddings, [tuple], self.name)
        self.paddings = paddings


class PadV3(Primitive):
    """
    Pads the input Tensor according to the `paddings`, `mode` and `paddings_contiguous`.

    Args:
        mode (str, optional): An optional string indicates padding mode,
            support ``"constant"`` , ``"reflect"`` , ``"edge"`` , ``"circular"`` . Default: ``"constant"`` .
            The effects of various padding modes are as follows:

            - ``"constant"``: Pads the input Tensor with value specified by `constant_value`.
            - ``"reflect"``: Pads the input Tensor by reflecting the values of the pixels at the
              boundary of the Tensor.
            - ``"edge"``: Pads the input Tensor with the values of the pixels on the border of the Tensor.
            - ``"circular"``: Circular padding mode. In this mode, the pixels from one edge of the image
              are wrapped around to the opposite edge, such that the pixel on the right edge of the
              image is replaced with the pixel on the left edge, and the pixel on the bottom edge
              is replaced with the pixel on the top edge.

        paddings_contiguous (bool, optional): An optional bool value indicates if the padding is paddings_contiguous.
            If ``True`` , paddings is arranged as [begin0, end0, begin1, end1, ...]
            If ``False`` , paddings is arranged as [begin0, begin1, ..., end1, end2, ...]
            Default: ``True`` .

    Inputs:
        - **x** (Tensor) - Tensor to be padded. It has shape :math:`(N, *)`, where :math:`*` means
          any number of additional dimensions.
        - **paddings** (Tensor) -  Specifies the number of zeros to be padded before and after each
          dimension of the input Tensor `x`. It's a 1D Tensor of type int32 or int64.
        - **constant_value** (Tensor, optional) - Padding value to use in 'constant' mode,
          if not specified, 0 is used instead. It has the same type as `x`.

    Outputs:
        Tensor, the tensor after padding.

    Raises:
        TypeError: If `x` or `paddings` is not a Tensor.
        TypeError: If `padding_contiguous` is not a bool.
        ValueError: If `mode` is not a str or not in support modes.
        ValueError: If `mode` is "constant", the element's number of `paddings` not be even.
        ValueError: If `mode` is "constant", the element's number of `paddings` large than input dim * 2.
        ValueError: If `mode` is "edge" "reflect" or "circular", the element's number of `paddings` is not 2, 4 or 6.
        ValueError: If `mode` is "edge" "reflect" or "circular", `x` dims equals 3,
            the element's number of `paddings` is not 2.
        ValueError: If `mode` is "edge" "reflect" or "circular", `x` dims equals 4,
            the element's number of `paddings` is not 4.
        ValueError: If `mode` is "circular", `x` dims equals 5, the element's number of `paddings` is not 6.
        ValueError: If `mode` is "edge", "reflect" or "circular", `x` dims smaller than 3.
        ValueError: If `mode` is "edge" or "circular", x dims bigger than 5.
        ValueError: If `mode` is "reflect", x dims bigger than 4.
        ValueError: If `mode` is "reflect", padding size bigger than the corresponding `x` dimension.
        ValueError: After padding, output's shape number is not greater than 0.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> # case1: mode="reflect", paddings_contiguous=True
        >>> class Net(nn.Cell):
        ...    def __init__(self, mode, paddings_contiguous):
        ...        super(Net, self).__init__()
        ...        self.pad = ops.PadV3(mode=mode, paddings_contiguous=paddings_contiguous)
        ...        self.paddings = Tensor([1, 1])
        ...    def construct(self, x):
        ...        return self.pad(x, self.paddings)
        ...
        >>> x = Tensor([[[0., 1.]]])
        >>> pad = Net(mode="reflect", paddings_contiguous=True)
        >>> output = pad(x)
        >>> print(output)
        [[[1. 0. 1. 0.]]]
        >>> # case2: mode="constant", padding_contigous=False
        >>> class Net(nn.Cell):
        ...    def __init__(self, mode, paddings_contiguous):
        ...        super(Net, self).__init__()
        ...        self.pad = ops.PadV3(mode=mode, paddings_contiguous=paddings_contiguous)
        ...        self.paddings = Tensor([1, 0, 1, 0])
        ...        self.value = Tensor(1.5)
        ...    def construct(self, x):
        ...        return self.pad(x, self.paddings, self.value)
        ...
        >>> x = Tensor([[0., 1., 2.]])
        >>> pad = Net(mode="constant", paddings_contiguous=False)
        >>> output = pad(x)
        >>> print(output)
        [[1.5 0. 1. 2. 1.5]]
    """

    @prim_attr_register
    def __init__(self, mode='constant', paddings_contiguous=True):
        """Initialize PadV3"""
        self.init_prim_io_names(inputs=['x', 'paddings', 'constant_value'], outputs=['y'])
        validator.check_string(mode, ['constant', 'reflect', 'edge', 'circular'], 'mode', self.name)
        validator.check_bool(paddings_contiguous, "paddings_contiguous", self.name)
        self.mode = mode
        self.paddings_contiguous = paddings_contiguous


class MirrorPad(Primitive):
    """
    Pads the input tensor according to the paddings and mode.

    Args:
        mode (str, optional): An optional string specifying the pad method.
            The optional values are ``'REFLECT'`` and ``'SYMMETRIC'`` .
            Default: ``'REFLECT'`` .

            - ``'REFLECT'``: Reflect the value on the edge while omitting the last one.
              For example, pad [1, 2, 3, 4] with 2 elements on both sides will result in [3, 2, 1, 2, 3, 4, 3, 2].
            - ``'SYMMETRIC'``: Reflect the value on the edge while repeating the last one.
              For example, pad [1, 2, 3, 4] with 2 elements on both sides will result in [2, 1, 1, 2, 3, 4, 4, 3].

    Inputs:
        - **input_x** (Tensor) - Tensor of shape :math:`(N, *)`, where :math:`*` means, any number of
          additional dimensions.
        - **paddings** (Tensor) - Paddings requires constant tensor. The value of `paddings` is a
          matrix(list), and its shape is :math:`(N, 2)`. N is the rank of input data. All elements of paddings
          are int type. For the input in the `D` th dimension, paddings[D, 0] indicates how many sizes
          to be extended ahead of the input tensor in the `D` th dimension, and paddings[D, 1]
          indicates how many sizes to be extended behind the input tensor in the `D` th dimension. Both
          paddings[D, 0] and paddings[D, 1] must be no greater than input_x.dim_size(D)
          (or input_x.dim_size(D) - 1) if mode is SYMMETRIC (if REFLECT, respectively).

    Outputs:
        Tensor, the tensor after padding.

        - If `mode` is ``'REFLECT'``, it uses a way of symmetrical copying through the axis of symmetry to fill in.
          If the `input_x` is [[1,2,3], [4,5,6], [7,8,9]] and `paddings` is [[1,1], [2,2]], then the
          `Outputs` is [[6,5,4,5,6,5,4], [3,2,1,2,3,2,1], [6,5,4,5,6,5,4], [9,8,7,8,9,8,7], [6,5,4,5,6,5,4]].
          For a more intuitive understanding, please see the example below.
        - If `mode` is ``'SYMMETRIC'``, the filling method is similar to the ``'REFLECT'``. It is also copied
          according to the symmetry axis, except that it includes the symmetry axis. If the `input_x`
          is [[1,2,3], [4,5,6], [7,8,9]] and `paddings` is [[1,1], [2,2]], then the `Outputs` is
          [[2,1,1,2,3,3,2], [2,1,1,2,3,3,2], [5,4,4,5,6,6,5], [8,7,7,8,9,9,8], [8,7,7,8,9,9,8]].
          For a more intuitive understanding, please see the example below.

    Raises:
        TypeError: If `input_x` or `paddings` is not a Tensor.
        TypeError: If `mode` is not a str.
        ValueError: If paddings.size is not equal to 2 * rank of input_x.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import Tensor, nn, ops
        >>> # case1: mode="REFLECT"
        >>> class Net(nn.Cell):
        ...    def __init__(self, mode):
        ...        super(Net, self).__init__()
        ...        self.pad = ops.MirrorPad(mode=mode)
        ...        self.paddings = Tensor([[1, 1], [2, 2]])
        ...    def construct(self, input_x):
        ...        return self.pad(input_x, self.paddings)
        ...
        >>> input_x = Tensor([[1,2,3], [4,5,6], [7,8,9]])
        >>> pad = Net("REFLECT")
        >>> output = pad(input_x)
        >>> print(output)
        [[6 5 4 5 6 5 4]
         [3 2 1 2 3 2 1]
         [6 5 4 5 6 5 4]
         [9 8 7 8 9 8 7]
         [6 5 4 5 6 5 4]]
        >>> # case2: mode="SYMMETRIC"
        >>> pad = Net("SYMMETRIC")
        >>> output = pad(input_x)
        >>> print(output)
        [[2 1 1 2 3 3 2]
         [2 1 1 2 3 3 2]
         [5 4 4 5 6 6 5]
         [8 7 7 8 9 9 8]
         [8 7 7 8 9 9 8]]
    """

    @prim_attr_register
    def __init__(self, mode='REFLECT'):
        """Initialize Pad"""
        self.init_prim_io_names(inputs=['x', 'paddings'], outputs=['y'])
        validator.check_string(mode, ['REFLECT', 'SYMMETRIC'], 'mode', self.name)
        self.mode = mode


class ComputeAccidentalHits(Primitive):
    r"""
    Compute accidental hits of sampled classes which match target classes.

    When a target class matches the sample class, we call it "accidental hit".
    The result of calculating accidental hits contain three parts (index, id, weight),
    where index represents the row number in true_classes, and id represents the position in sampled_candidates,
    the weight is FLOAT_MAX. FLOAT_MAX indicates the max value in the type of Float

    Args:
        num_true (int): The number of target classes per training example. Default: ``1`` .

    Inputs:
        - **true_classes** (Tensor) - The target classes. With data type of int64
          and shape :math:`(batch\_size, num\_true)`.
        - **sampled_candidates** (Tensor) - The Candidate sampling results of operators, types of training samples,
          with data type of int64 and shape :math:`(num\_sampled, )`.

    Outputs:
        Tuple of 3 Tensors.

        - **indices** (Tensor) - A Tensor with shape :math:`(num\_accidental\_hits, )`,
          with data type of int32.
        - **ids** (Tensor) - A Tensor with shape :math:`(num\_accidental\_hits, )`,
          with data type of int64.
        - **weights** (Tensor) - A Tensor with shape :math:`(num\_accidental\_hits, )`, with the type float32.

    Raises:
        TypeError: If dtype of `num_true` is not int.
        TypeError: If `true_classes` or `sampled_candidates` is not a Tensor.
        TypeError: If dtype of `true_classes` or `sampled_candidates` is neither int32 nor int64.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> true_classes = np.array([[1, 2], [0, 4], [3, 3]])
        >>> sampled_candidates = np.array([0, 1, 2, 3, 4])
        >>> sampler = ops.ComputeAccidentalHits(2)
        >>> indices, ids, weights = sampler(Tensor(true_classes), Tensor(sampled_candidates))
        >>> print(indices, ids, weights)
        [0 0 1 1 2 2]
        [1 2 0 4 3 3]
        [-3.4028235e+38 -3.4028235e+38 -3.4028235e+38 -3.4028235e+38 -3.4028235e+38 -3.4028235e+38]

    """

    @prim_attr_register
    def __init__(self, num_true=1):
        """Initialize ComputeAccidentalHits"""
        self.init_prim_io_names(inputs=['true_classes', 'sampled_candidates'],
                                outputs=['indices', 'ids', 'weights'])
        validator.check_value_type("num_true", num_true, [int], self.name)
        validator.check_number("num_true", num_true, 1, validator.GE, self.name)
        self.num_true = num_true


class ROIAlign(Primitive):
    r"""
    Computes the Region of Interest (RoI) Align operator.

    The operator computes the value of each sampling point by bilinear interpolation from the nearby grid points on the
    feature map. No quantization is performed on any coordinates involved in the RoI, its bins, or the sampling
    points. The details of (RoI) Align operator are described in `Mask R-CNN <https://arxiv.org/abs/1703.06870>`_.

    Args:
        pooled_height (int): The output features height.
        pooled_width (int): The output features width.
        spatial_scale (float): A scaling factor that maps the raw image coordinates to the input
            feature map coordinates. Suppose the height of a RoI is `ori_h` in the raw image and `fea_h` in the
            input feature map, the `spatial_scale` must be `fea_h / ori_h`.
        sample_num (int): Number of sampling points. Default: ``2`` .
        roi_end_mode (int): Number must be 0 or 1. If roi_end_mode=0, use the legacy implementation.
            If roi_end_mode=1, end pixel of the roi_box will be shifted by +1*spatial_scale. Default: ``1`` .


    Inputs:
        - **features** (Tensor) - The input features, whose shape must be :math:`(N, C, H, W)`.
        - **rois** (Tensor) - The shape is :math:`(rois\_n, 5)`. With data type of float16 or float32.
          `rois_n` represents the number of RoI. The size of the second dimension must be `5` and the `5` colunms
          are :math:`(image\_index, top\_left\_x, top\_left\_y, bottom\_right\_x, bottom\_right\_y)`.
          `image_index` represents the index of image. `top_left_x` and `top_left_y` represent the `x, y`
          coordinates of the top left corner of corresponding RoI, respectively. `bottom_right_x` and `bottom_right_y`
          represent the `x, y` coordinates of the bottom right corner of corresponding RoI, respectively.

    Outputs:
        Tensor, the shape is :math:`(rois\_n, C, pooled\_height, pooled\_width)`.

    Raises:
        TypeError: If `pooled_height`, `pooled_width`, `sample_num` or `roi_end_mode` is not an int.
        TypeError: If `spatial_scale` is not a float.
        TypeError: If `features` or `rois` is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> features = Tensor(np.array([[[[1., 2.], [3., 4.]]]]), mindspore.float32)
        >>> rois = Tensor(np.array([[0, 0.2, 0.3, 0.2, 0.3]]), mindspore.float32)
        >>> roi_align = ops.ROIAlign(2, 2, 0.5, 2)
        >>> output = roi_align(features, rois)
        >>> print(output)
        [[[[1.775 2.025]
           [2.275 2.525]]]]
    """

    @prim_attr_register
    def __init__(self, pooled_height, pooled_width, spatial_scale, sample_num=2, roi_end_mode=1):
        """Initialize ROIAlign"""
        validator.check_value_type("pooled_height", pooled_height, [int], self.name)
        validator.check_value_type("pooled_width", pooled_width, [int], self.name)
        validator.check_value_type("spatial_scale", spatial_scale, [float], self.name)
        validator.check_value_type("sample_num", sample_num, [int], self.name)
        validator.check_value_type("roi_end_mode", roi_end_mode, [int], self.name)
        validator.check_int_range(roi_end_mode, 0, 1, validator.INC_BOTH, "roi_end_mode", self.name)
        self.pooled_height = pooled_height
        self.pooled_width = pooled_width
        self.spatial_scale = spatial_scale
        self.sample_num = sample_num
        self.roi_end_mode = roi_end_mode


class Adam(Primitive):
    r"""
    Updates gradients by the Adaptive Moment Estimation (Adam) algorithm.

    The Adam algorithm is proposed in `Adam: A Method for Stochastic Optimization <https://arxiv.org/abs/1412.6980>`_.

    For more details, please refer to :class:`mindspore.nn.Adam`.

    The updating formulas are as follows,

    .. math::
        \begin{array}{ll} \\
            m = \beta_1 * m + (1 - \beta_1) * g \\
            v = \beta_2 * v + (1 - \beta_2) * g * g \\
            l = \alpha * \frac{\sqrt{1-\beta_2^t}}{1-\beta_1^t} \\
            w = w - l * \frac{m}{\sqrt{v} + \epsilon}
        \end{array}

    :math:`m` represents the 1st moment vector, :math:`v` represents the 2nd moment vector, :math:`g` represents
    `gradient`, :math:`l` represents scaling factor `lr`, :math:`\beta_1, \beta_2` represent `beta1` and `beta2`,
    :math:`t` represents updating step while :math:`beta_1^t(\beta_1^{t})` and :math:`beta_2^t(\beta_2^{t})`
    represent `beta1_power` and `beta2_power`, :math:`\alpha` represents `learning_rate`, :math:`w` represents `var`,
    :math:`\epsilon` represents
    `epsilon`.

    Inputs of `var`, `m`, `v` and `gradient`
    comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        use_locking (bool): Whether to enable a lock to protect variable tensors from being updated.
            If ``True`` , updates of the var, m, and v tensors will be protected by a lock.
            If ``False`` , the result is unpredictable. Default: ``False`` .
        use_nesterov (bool): Whether to use Nesterov Accelerated Gradient (NAG) algorithm to update the gradients.
            If ``True`` , update the gradients using NAG.
            If ``False`` , update the gradients without using NAG. Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Weights to be updated. The shape is :math:`(N, *)` where :math:`*` means,
          any number of additional dimensions. The data type can be float16 or float32.
        - **m** (Parameter) - The 1st moment vector in the updating formula,
          the shape should be the same as `var`.
        - **v** (Parameter) - the 2nd moment vector in the updating formula,
          the shape should be the same as `var`.
        - **beta1_power** (float) - :math:`beta_1^t(\beta_1^{t})` in the updating formula.
        - **beta2_power** (float) - :math:`beta_2^t(\beta_2^{t})` in the updating formula.
        - **lr** (float) - :math:`l` in the updating formula. The paper suggested value is :math:`10^{-8}`.
        - **beta1** (float) - The exponential decay rate for the 1st moment estimations.
          The paper suggested value is :math:`0.9`.
        - **beta2** (float) - The exponential decay rate for the 2nd moment estimations.
          The paper suggested value is :math:`0.999`.
        - **epsilon** (float) - Term added to the denominator to improve numerical stability.
        - **gradient** (Tensor) - Gradient, has the same shape and data type as `var`.

    Outputs:
        Tuple of 3 Tensor, the updated parameters.

        - **var** (Tensor) - The same shape and data type as Inputs `var`.
        - **m** (Tensor) - The same shape and data type as Inputs `m`.
        - **v** (Tensor) - The same shape and data type as Inputs `v`.

    Raises:
        TypeError: If neither `use_locking` nor `use_nesterov` is a bool.
        TypeError: If `var`, `m` or `v` is not a Parameter.
        TypeError: If `beta1_power`, `beta2_power1`, `lr`, `beta1`, `beta2`, `epsilon` or `gradient` is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops
        >>> from mindspore import Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_adam = ops.Adam()
        ...         self.var = Parameter(Tensor(np.ones([2, 2]).astype(np.float32)), name="var")
        ...         self.m = Parameter(Tensor(np.ones([2, 2]).astype(np.float32)), name="m")
        ...         self.v = Parameter(Tensor(np.ones([2, 2]).astype(np.float32)), name="v")
        ...     def construct(self, beta1_power, beta2_power, lr, beta1, beta2, epsilon, grad):
        ...         out = self.apply_adam(self.var, self.m, self.v, beta1_power, beta2_power, lr, beta1, beta2,
        ...                               epsilon, grad)
        ...         return out
        ...
        >>> net = Net()
        >>> gradient = Tensor(np.ones([2, 2]).astype(np.float32))
        >>> output = net(0.9, 0.999, 0.001, 0.9, 0.999, 1e-8, gradient)
        >>> print(net.var.asnumpy())
        [[0.9996838 0.9996838]
         [0.9996838 0.9996838]]
    """
    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('m', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T1),
        sig.make_sig('v', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T2),
        sig.make_sig('beta1_power', dtype=sig.sig_dtype.T3),
        sig.make_sig('beta2_power', dtype=sig.sig_dtype.T4),
        sig.make_sig('lr', dtype=sig.sig_dtype.T5),
        sig.make_sig('beta1', dtype=sig.sig_dtype.T6),
        sig.make_sig('beta2', dtype=sig.sig_dtype.T7),
        sig.make_sig('epsilon', dtype=sig.sig_dtype.T8),
        sig.make_sig('gradient', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self, use_locking=False, use_nesterov=False):
        """Initialize Adam."""
        validator.check_value_type("use_locking", use_locking, [bool], self.name)
        validator.check_value_type("use_nesterov", use_nesterov, [bool], self.name)
        self.add_prim_attr('side_effect_mem', True)


class AdamNoUpdateParam(Primitive):
    r"""
    Updates gradients by the Adaptive Moment Estimation (Adam) algorithm. This operator do not update the parameter, but
    calculate the value that should be added to the parameter instead.

    The Adam algorithm is proposed in `Adam: A Method for Stochastic Optimization <https://arxiv.org/abs/1412.6980>`_.

    The updating formulas are as follows,

    .. math::
        \begin{array}{ll} \\
            m = \beta_1 * m + (1 - \beta_1) * g \\
            v = \beta_2 * v + (1 - \beta_2) * g * g \\
            l = \alpha * \frac{\sqrt{1-\beta_2^t}}{1-\beta_1^t} \\
            \Delta{w} = - l * \frac{m}{\sqrt{v} + \epsilon}
        \end{array}

    :math:`m` represents the 1st moment vector, :math:`v` represents the 2nd moment vector, :math:`g` represents
    `gradient`, :math:`l` represents scaling factor `lr`, :math:`\beta_1, \beta_2` represent `beta1` and `beta2`,
    :math:`t` represents updating step while :math:`beta_1^t(\beta_1^{t})` and :math:`beta_2^t(\beta_2^{t})`
    represent `beta1_power` and `beta2_power`, :math:`\alpha` represents `learning_rate`,
    :math:`w` represents the parameter to be updated, :math:`\epsilon` represents `epsilon`.

    Args:
        use_locking (bool): Whether to enable a lock to protect variable tensors from being updated.
            If ``True`` , updates of the var, m, and v tensors will be protected by a lock.
            If ``False`` , the result is unpredictable. Default: ``False`` .
        use_nesterov (bool): Whether to use Nesterov Accelerated Gradient (NAG) algorithm to update the gradients.
            If ``True`` , update the gradients using NAG.
            If ``False`` , update the gradients without using NAG. Default: ``False`` .

    Inputs:
        - **m** (Tensor) - The 1st moment vector in the updating formula. The shape is :math:`(N, *)`
          where :math:`*` means, any number of additional dimensions. The data type must be float32.
        - **v** (Tensor) - the 2nd moment vector in the updating formula. The shape must be the same as `m`.
          The data type must be float32.
        - **beta1_power** (Tensor) - :math:`beta_1^t(\beta_1^{t})` in the updating formula.
          The shape is :math:`(1, )` and the data type must be float32.
        - **beta2_power** (Tensor) - :math:`beta_2^t(\beta_2^{t})` in the updating formula.
          The shape is :math:`(1, )` and the data type must be float32.
        - **lr** (Tensor) - :math:`l` in the updating formula.
          The shape is :math:`(1, )` and the data type must be float32.
        - **beta1** (Tensor) - The exponential decay rate for the 1st moment estimations.
          The shape is :math:`(1, )` and the data type must be float32.
        - **beta2** (Tensor) - The exponential decay rate for the 2nd moment estimations.
          The shape is :math:`(1, )` and the data type must be float32.
        - **epsilon** (Tensor) - Term added to the denominator to improve numerical stability.
          The shape is :math:`(1, )` and the data type must be float32.
        - **gradient** (Tensor) - Gradient, the shape must be the same as `m`, the data type must be float32.

    Outputs:
        Tensor, whose shape and data type are the same with Inputs `gradient`, is a value that should be added to the
        parameter to be updated.

    Raises:
        TypeError: If neither `use_locking` nor `use_nesterov` is a bool.
        TypeError: If `m`,  `v`, `beta1_power`, `beta2_power1`, `lr`, `beta1`, `beta2`, `epsilon` or `gradient`
                   is not a Tensor.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.adam = ops.AdamNoUpdateParam()
        ...         self.m = Parameter(Tensor(np.array([[0.1, 0.1, 0.1], [0.2, 0.2, 0.2]]).astype(np.float32)),
        ...                            name="m")
        ...         self.v = Parameter(Tensor(np.array([[0.1, 0.1, 0.1], [0.2, 0.2, 0.2]]).astype(np.float32)),
        ...                            name="v")
        ...     def construct(self, beta1_power, beta2_power, lr, beta1, beta2, epsilon, grad):
        ...         out = self.adam(self.m, self.v, beta1_power, beta2_power, lr, beta1, beta2, epsilon, grad)
        ...         return out
        >>> net = Net()
        >>> beta1_power = Tensor(0.9, ms.float32)
        >>> beta2_power = Tensor(0.999, ms.float32)
        >>> lr = Tensor(0.001, ms.float32)
        >>> beta1 = Tensor(0.9, ms.float32)
        >>> beta2 = Tensor(0.999, ms.float32)
        >>> epsilon = Tensor(1e-8, ms.float32)
        >>> gradient = Tensor(np.array([[0.1, 0.1, 0.1], [0.1, 0.1, 0.1]]).astype(np.float32))
        >>> result = net(beta1_power, beta2_power, lr, beta1, beta2, epsilon, gradient)
        >>> print(result)
        [[-0.00010004 -0.00010004 -0.00010004]
        [-0.00013441 -0.00013441 -0.00013441]]

    """

    @prim_attr_register
    def __init__(self, use_locking=False, use_nesterov=False):
        """Initialize AdamNoUpdateParam."""
        validator.check_value_type("use_locking", use_locking, [bool], self.name)
        validator.check_value_type("use_nesterov", use_nesterov, [bool], self.name)


class FusedSparseAdam(Primitive):
    r"""
    Merges the duplicate value of the gradient and then updates parameters by the Adaptive Moment Estimation (Adam)
    algorithm. This operator is used when the gradient is sparse.

    The Adam algorithm is proposed in `Adam: A Method for Stochastic Optimization <https://arxiv.org/abs/1412.6980>`_.

    The updating formulas are as follows,

    .. math::
        \begin{array}{ll} \\
            m = \beta_1 * m + (1 - \beta_1) * g \\
            v = \beta_2 * v + (1 - \beta_2) * g * g \\
            l = \alpha * \frac{\sqrt{1-\beta_2^t}}{1-\beta_1^t} \\
            w = w - l * \frac{m}{\sqrt{v} + \epsilon}
        \end{array}

    :math:`m` represents the 1st moment vector, :math:`v` represents the 2nd moment vector, :math:`g` represents
    `gradient`, :math:`l` represents scaling factor `lr`, :math:`\beta_1, \beta_2` represent `beta1` and `beta2`,
    :math:`t` represents updating step while :math:`\beta_1^t` and :math:`\beta_2^t` represent `beta1_power` and
    `beta2_power`, :math:`\alpha` represents `learning_rate`, :math:`w` represents `var`, :math:`\epsilon` represents
    `epsilon`.

    All of inputs except `indices` comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        use_locking (bool): Whether to enable a lock to protect variable tensors from being updated.
            If ``True`` , updates of the var, m, and v tensors will be protected by a lock.
            If ``False`` , the result is unpredictable. Default: ``False`` .
        use_nesterov (bool): Whether to use Nesterov Accelerated Gradient (NAG) algorithm to update the gradients.
            If ``True`` , update the gradients using NAG.
            If ``False`` , update the gradients without using NAG. Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Parameters to be updated with float32 data type. The shape is :math:`(N, *)`
          where :math:`*` means, any number of additional dimensions.
        - **m** (Parameter) - The 1st moment vector in the updating formula, has the same shape and data type as `var`.
        - **v** (Parameter) - The 2nd moment vector in the updating formula, has the same shape and data type as `var`.
          Mean square gradients, has the same type as `var` with float32 data type.
        - **beta1_power** (Tensor) - :math:`beta_1^t` in the updating formula with float32 data type.
          The shape is :math:`(1, )`.
        - **beta2_power** (Tensor) - :math:`beta_2^t` in the updating formula with float32 data type.
          The shape is :math:`(1, )`.
        - **lr** (Tensor) - :math:`l` in the updating formula. With float32 data type.
          The shape is :math:`(1, )`.
        - **beta1** (Tensor) - The exponential decay rate for the 1st moment estimations with float32 data type.
          The shape is :math:`(1, )`.
        - **beta2** (Tensor) - The exponential decay rate for the 2nd moment estimations with float32 data type.
          The shape is :math:`(1, )`.
        - **epsilon** (Tensor) - Term added to the denominator to improve numerical stability with float32 data type.
          The shape is :math:`(1, )`.
        - **gradient** (Tensor) - Gradient, has the same data type as `var` and
          gradient.shape[1:] = var.shape[1:] if var.shape > 1.
        - **indices** (Tensor) - Gradient indices with int32 data type and indices.shape[0] = gradient.shape[0].

    Outputs:
        Tuple of 3 Tensors, this operator will update the input parameters directly, the outputs are useless.

        - **var** (Tensor) - A Tensor with shape :math:`(N, *)`.
        - **m** (Tensor) - A Tensor with shape :math:`(1, )`.
        - **v** (Tensor) - A Tensor with shape :math:`(1, )`.

    Raises:
        TypeError: If neither `use_locking` nor `use_neserov` is a bool.
        TypeError: If dtype of `var`, `m`, `v`, `beta1_power`, `beta2_power`, `lr`, `beta1`, `beta2`, `epsilon`,
                   `gradient` or `indices` is not float32.
        RuntimeError: If the data type of all inputs except `indices` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.sparse_apply_adam = ops.FusedSparseAdam()
        ...         self.var = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="var")
        ...         self.m = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="m")
        ...         self.v = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="v")
        ...     def construct(self, beta1_power, beta2_power, lr, beta1, beta2, epsilon, grad, indices):
        ...         out = self.sparse_apply_adam(self.var, self.m, self.v, beta1_power, beta2_power, lr, beta1, beta2,
        ...                                      epsilon, grad, indices)
        ...         return out
        ...
        >>> net = Net()
        >>> beta1_power = Tensor(0.9, mindspore.float32)
        >>> beta2_power = Tensor(0.999, mindspore.float32)
        >>> lr = Tensor(0.001, mindspore.float32)
        >>> beta1 = Tensor(0.9, mindspore.float32)
        >>> beta2 = Tensor(0.999, mindspore.float32)
        >>> epsilon = Tensor(1e-8, mindspore.float32)
        >>> gradient = Tensor(np.array([[[0.1, 0.1]], [[0.1, 0.1]]]), mindspore.float32)
        >>> indices = Tensor([0, 1], mindspore.int32)
        >>> output = net(beta1_power, beta2_power, lr, beta1, beta2, epsilon, gradient, indices)
        >>> print(net.var.asnumpy())
        [[[0.9997121  0.9997121 ]]
         [[0.9997121  0.9997121 ]]
         [[0.99971527 0.99971527]]]
    """
    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('m', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('v', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('beta1_power', dtype=sig.sig_dtype.T),
        sig.make_sig('beta2_power', dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T),
        sig.make_sig('beta1', dtype=sig.sig_dtype.T),
        sig.make_sig('beta2', dtype=sig.sig_dtype.T),
        sig.make_sig('epsilon', dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1)
    )

    @prim_attr_register
    def __init__(self, use_locking=False, use_nesterov=False):
        """Initialize FusedSparseAdam."""
        validator.check_value_type("use_locking", use_locking, [bool], self.name)
        validator.check_value_type("use_nesterov", use_nesterov, [bool], self.name)
        self.init_prim_io_names(inputs=['var', 'm', 'v', 'beta1_power', 'beta2_power', 'lr', 'beta1', 'beta2',
                                        'epsilon', 'grad', 'indices'],
                                outputs=['var', 'm', 'v'])
        self.add_prim_attr('side_effect_mem', True)


class FusedSparseLazyAdam(Primitive):
    r"""
    Merges the duplicate value of the gradient and then updates parameters by the Adaptive Moment Estimation (Adam)
    algorithm. This operator is used when the gradient is sparse. The behavior is not equivalent to the
    original Adam algorithm, as only the current indices parameters will be updated.

    The Adam algorithm is proposed in `Adam: A Method for Stochastic Optimization <https://arxiv.org/abs/1412.6980>`_.

    The updating formulas are as follows,

    .. math::
        \begin{array}{ll} \\
            m = \beta_1 * m + (1 - \beta_1) * g \\
            v = \beta_2 * v + (1 - \beta_2) * g * g \\
            l = \alpha * \frac{\sqrt{1-\beta_2^t}}{1-\beta_1^t} \\
            w = w - l * \frac{m}{\sqrt{v} + \epsilon}
        \end{array}

    :math:`m` represents the 1st moment vector, :math:`v` represents the 2nd moment vector, :math:`g` represents
    `gradient`, :math:`l` represents scaling factor `lr`, :math:`\beta_1, \beta_2` represent `beta1` and `beta2`,
    :math:`t` represents updating step while :math:`\beta_1^t` and :math:`\beta_2^t` represent `beta1_power` and
    `beta2_power`, :math:`\alpha` represents `learning_rate`, :math:`w` represents `var`, :math:`\epsilon` represents
    `epsilon`.

    All of inputs except `indices` comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        use_locking (bool): Whether to enable a lock to protect variable tensors from being updated.
            If ``True`` , updates of the var, m, and v tensors will be protected by a lock.
            If ``False`` , the result is unpredictable. Default: ``False`` .
        use_nesterov (bool): Whether to use Nesterov Accelerated Gradient (NAG) algorithm to update the gradients.
            If ``True`` , update the gradients using NAG.
            If ``False`` , update the gradients without using NAG. Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Parameters to be updated with float32 data type. The shape is :math:`(N, *)`
          where :math:`*` means, any number of additional dimensions.
        - **m** (Parameter) - The 1st moment vector in the updating formula, has the same shape and data type as `var`.
        - **v** (Parameter) - The 2nd moment vector in the updating formula, has the same shape and data type as `var`.
          Mean square gradients, has the same type as `var` with float32 data type.
        - **beta1_power** (Tensor) - :math:`beta_1^t` in the updating formula with float32 data type.
          The shape is :math:`(1, )`.
        - **beta2_power** (Tensor) - :math:`beta_2^t` in the updating formula with float32 data type.
          The shape is :math:`(1, )`.
        - **lr** (Tensor) - :math:`l` in the updating formula with float32 data type.
          The shape is :math:`(1, )`.
        - **beta1** (Tensor) - The exponential decay rate for the 1st moment estimations with float32 data type.
          The shape is :math:`(1, )`.
        - **beta2** (Tensor) - The exponential decay rate for the 2nd moment estimations with float32 data type.
          The shape is :math:`(1, )`.
        - **epsilon** (Tensor) - Term added to the denominator to improve numerical stability with float32 data type.
          The shape is :math:`(1, )`.
        - **gradient** (Tensor) - Gradient value with float32 data type and
          gradient.shape[1:] = var.shape[1:] if var.shape > 1.
        - **indices** (Tensor) - Gradient indices with int32 data type and indices.shape[0] = gradient.shape[0].

    Outputs:
        Tuple of 3 Tensors, this operator will update the input parameters directly, the outputs are useless.

        - **var** (Tensor) - A Tensor with shape :math:`(N, *)`.
        - **m** (Tensor) - A Tensor with shape :math:`(1, )`.
        - **v** (Tensor) - A Tensor with shape :math:`(1, )`.

    Raises:
        TypeError: If neither `use_locking` nor `use_nestrov` is a bool.
        TypeError: If dtype of `var`, `m`, `v`, `beta1_power`, `beta2_power`, `lr`, `beta1`, `beta2`, `epsilon` or
                   gradient is not float32.
        TypeError: If dtype of `indices` is not int32.
        RuntimeError: If the data type of all inputs except `indices` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.sparse_apply_lazyadam = ops.FusedSparseLazyAdam()
        ...         self.var = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="var")
        ...         self.m = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="m")
        ...         self.v = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="v")
        ...     def construct(self, beta1_power, beta2_power, lr, beta1, beta2, epsilon, grad, indices):
        ...         out = self.sparse_apply_lazyadam(self.var, self.m, self.v, beta1_power, beta2_power, lr, beta1,
        ...                                          beta2, epsilon, grad, indices)
        ...         return out
        ...
        >>> net = Net()
        >>> beta1_power = Tensor(0.9, mindspore.float32)
        >>> beta2_power = Tensor(0.999, mindspore.float32)
        >>> lr = Tensor(0.001, mindspore.float32)
        >>> beta1 = Tensor(0.9, mindspore.float32)
        >>> beta2 = Tensor(0.999, mindspore.float32)
        >>> epsilon = Tensor(1e-8, mindspore.float32)
        >>> gradient = Tensor(np.array([[[0.1, 0.1]], [[0.1, 0.1]]]), mindspore.float32)
        >>> indices = Tensor([0, 1], mindspore.int32)
        >>> output = net(beta1_power, beta2_power, lr, beta1, beta2, epsilon, gradient, indices)
        >>> print(net.var.asnumpy())
        [[[0.9997121  0.9997121 ]]
         [[0.9997121  0.9997121 ]]
         [[1.         1.        ]]]
    """
    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('m', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('v', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('beta1_power', dtype=sig.sig_dtype.T),
        sig.make_sig('beta2_power', dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T),
        sig.make_sig('beta1', dtype=sig.sig_dtype.T),
        sig.make_sig('beta2', dtype=sig.sig_dtype.T),
        sig.make_sig('epsilon', dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1)
    )

    @prim_attr_register
    def __init__(self, use_locking=False, use_nesterov=False):
        """Initialize FusedSparseLazyAdam."""
        validator.check_value_type("use_locking", use_locking, [bool], self.name)
        validator.check_value_type("use_nesterov", use_nesterov, [bool], self.name)
        self.init_prim_io_names(inputs=['var', 'm', 'v', 'beta1_power', 'beta2_power', 'lr', 'beta1', 'beta2',
                                        'epsilon', 'grad', 'indices'],
                                outputs=['var', 'm', 'v'])
        self.add_prim_attr('side_effect_mem', True)


class FusedSparseFtrl(Primitive):
    """
    Merges the duplicate value of the gradient and then updates relevant entries according to the FTRL-proximal scheme.

    All inputs except `indices` comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        lr (float): The learning rate value, must be positive.
        l1 (float): l1 regularization strength, must be greater than or equal to zero.
        l2 (float): l2 regularization strength, must be greater than or equal to zero.
        lr_power (float): Learning rate power controls how the learning rate decreases during training,
            must be less than or equal to zero. Use fixed learning rate if `lr_power` is zero.
        use_locking (bool): Use locks for updating operation if True . Default: ``False`` .

    Inputs:
        - **var** (Parameter) - The variable to be updated. The data type must be float32. The shape is :math:`(N, *)`
          where :math:`*` means, any number of additional dimensions.
        - **accum** (Parameter) - The accumulation to be updated, must be same type and shape as `var`.
        - **linear** (Parameter) - the linear coefficient to be updated, must be same type and shape as `var`.
        - **grad** (Tensor) - A tensor of the same type as `var` and
          grad.shape[1:] = var.shape[1:] if var.shape > 1.
        - **indices** (Tensor) - A vector of indices into the first dimension of `var` and `accum`.
          The type must be int32 and indices.shape[0] = grad.shape[0].

    Outputs:
        Tuple of 3 Tensor, this operator will update the input parameters directly, the outputs are useless.

        - **var** (Tensor) - A Tensor with shape :math:`(N, *)`.
        - **accum** (Tensor) - A Tensor with shape :math:`(1, )`.
        - **linear** (Tensor) - A Tensor with shape :math:`(1, )`.

    Raises:
        TypeError: If `lr`, `l1`, `l2` or `lr_power` is not a float.
        ValueError: If shape of `lr_power` less than or equal to zero.
        TypeError: If dtype of `var` is not float32.
        TypeError: If dtype of `indices` is not int32.
        TypeError: If shape of `accum`, `linear` or `grad` is not same as `var`.
        TypeError: If shape of `indices` is not same as shape of first dimension of `grad`.
        RuntimeError: If the data type of all of inputs except `indices` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> class SparseApplyFtrlNet(nn.Cell):
        ...     def __init__(self):
        ...         super(SparseApplyFtrlNet, self).__init__()
        ...         self.sparse_apply_ftrl = ops.FusedSparseFtrl(lr=0.01, l1=0.0, l2=0.0, lr_power=-0.5)
        ...         self.var = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="accum")
        ...         self.linear = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="linear")
        ...
        ...     def construct(self, grad, indices):
        ...         out = self.sparse_apply_ftrl(self.var, self.accum, self.linear, grad, indices)
        ...         return out
        ...
        >>> net = SparseApplyFtrlNet()
        >>> grad = Tensor(np.array([[[0.1, 0.1]], [[0.1, 0.1]]]).astype(np.float32))
        >>> indices = Tensor(np.array([0, 1]).astype(np.int32))
        >>> output = net(grad, indices)
        >>> print(net.var.asnumpy())
        [[[-0.00598256 -0.00598256]]
         [[-0.00598256 -0.00598256]]
         [[ 1.          1.        ]]]
    """
    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('linear', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1)
    )

    @prim_attr_register
    def __init__(self, lr, l1, l2, lr_power, use_locking=False):
        """Initialize FusedSparseFtrl."""
        self.init_prim_io_names(inputs=['var', 'accum', 'linear', 'grad', 'indices'],
                                outputs=['output'])
        self.add_prim_attr('side_effect_mem', True)

        validator.check_value_type("lr", lr, [float], self.name)
        validator.check_value_type("l1", l1, [float], self.name)
        validator.check_value_type("l2", l2, [float], self.name)
        validator.check_value_type("lr_power", lr_power, [float], self.name)
        self.lr = validator.check_positive_float(lr, "lr", self.name)
        self.l1 = validator.check_non_negative_float(l1, "l1", self.name)
        self.l2 = validator.check_non_negative_float(l2, "l2", self.name)
        self.lr_power = validator.check_number("lr_power", lr_power, 0, validator.LE, self.name)
        self.use_locking = validator.check_value_type("use_locking", use_locking, [bool], self.name)


class FusedSparseProximalAdagrad(Primitive):
    r"""
    Merges the duplicate value of the gradient and then updates relevant entries according to the proximal adagrad
    algorithm.

    .. math::
        \begin{array}{ll} \\
            accum += grad * grad \\
            \text{prox_v} = var - lr * grad * \frac{1}{\sqrt{accum}} \\
            var = \frac{sign(\text{prox_v})}{1 + lr * l2} * \max(\left| \text{prox_v} \right| - lr * l1, 0)
        \end{array}

    All of inputs except `indices` comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        use_locking (bool): If ``True`` , the variable and accumulation tensors will be protected from being updated.
            Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable tensor to be updated. The data type must be float32.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **accum** (Parameter) - Variable tensor to be updated, has the same shape and data type as `var`.
        - **lr** (Tensor) - The learning rate value. The data type must be float32. The shape is :math:`(1, )`.
        - **l1** (Tensor) - l1 regularization strength. The data type must be float32. The shape is :math:`(1, )`.
        - **l2** (Tensor) - l2 regularization strength. The data type must be float32. The shape is :math:`(1, )`.
        - **grad** (Tensor) - A tensor of the same data type as `var` and
          grad.shape[1:] = var.shape[1:] if var.shape > 1.
        - **indices** (Tensor) - A vector of indices into the first dimension of `var` and `accum`.
          The type must be int32 and indices.shape[0] = grad.shape[0].

    Outputs:
        Tuple of 2 Tensors, this operator will update the input parameters directly, the outputs are useless.

        - **var** (Tensor) - A Tensor with shape :math:`(N, *)`.
        - **accum** (Tensor) - A Tensor with shape :math:`(1, )`.

    Raises:
        TypeError: If `use_locking` is not a bool.
        TypeError: If dtype of `var`, `accum`, `lr`, `l1`, `l2` or `grad` is not float32.
        TypeError: If dtype of `indices` is not int32.
        RuntimeError: If the data type of all inputs except `indices` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.sparse_apply_proximal_adagrad = ops.FusedSparseProximalAdagrad()
        ...         self.var = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.ones([3, 1, 2]).astype(np.float32)), name="accum")
        ...         self.lr = Tensor(0.01, mindspore.float32)
        ...         self.l1 = Tensor(0.0, mindspore.float32)
        ...         self.l2 = Tensor(0.0, mindspore.float32)
        ...     def construct(self, grad, indices):
        ...         out = self.sparse_apply_proximal_adagrad(self.var, self.accum, self.lr, self.l1,
        ...                                                  self.l2, grad, indices)
        ...         return out
        ...
        >>> net = Net()
        >>> grad = Tensor(np.array([[[0.1, 0.1]], [[0.1, 0.1]]]).astype(np.float32))
        >>> indices = Tensor(np.array([0, 1]).astype(np.int32))
        >>> output = net(grad, indices)
        >>> print(net.var.asnumpy())
        [[[0.99900496 0.99900496]]
         [[0.99900496 0.99900496]]
         [[1.         1.        ]]]
    """
    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T),
        sig.make_sig('l1', dtype=sig.sig_dtype.T),
        sig.make_sig('l2', dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1)
    )

    @prim_attr_register
    def __init__(self, use_locking=False):
        """Initialize FusedSparseProximalAdagrad"""
        self.init_prim_io_names(inputs=['var', 'accum', 'lr', 'l1', 'l2', 'grad', 'indices'],
                                outputs=['output'])
        self.add_prim_attr('side_effect_mem', True)
        self.use_locking = validator.check_value_type("use_locking", use_locking, [bool], self.name)


class KLDivLoss(Primitive):
    r"""
    Computes the Kullback-Leibler divergence between the logits and the labels.

    For tensors of the same shape :math:`x` and :math:`target`,
    the updating formulas of KLDivLoss algorithm are as follows,

    .. math::
        L(x, target) = target \cdot (\log target - x)

    Then,

    .. math::
        \ell(x, target) = \begin{cases}
        L(x, target), & \text{if reduction} = \text{'none';}\\
        \operatorname{mean}(L(x, target)), & \text{if reduction} = \text{'mean';}\\
        \operatorname{sum}(L(x, target)) / x.\operatorname{shape}[0], & \text{if reduction} = \text{'batchmean';}\\
        \operatorname{sum}(L(x, target)),  & \text{if reduction} = \text{'sum'.}
        \end{cases}

    where :math:`x` represents `logits`,
    :math:`target` represents `labels`, and
    :math:`\ell(x, target)` represents `output`.

    Note:
        - On Ascend, float64 dtype is not currently supported.
        - The output aligns with the mathematical definition of Kullback-Leibler divergence
          only when `reduction` is set to ``'batchmean'``.
        - On Ascend, the value of `reduction` must be one of ``'batchmean'``, ``'none'`` or ``'sum'``.
        - On GPU, the value of `reduction` must be one of ``'mean'``, ``'none'`` or ``'sum'``.
        - On CPU, the value of `reduction` must be one of ``'mean'``, ``'batchmean'``, ``'none'``
          or ``'sum'``.

    Args:
        reduction (str): Specifies the reduction to be applied to the output.
            Default: ``'mean'`` .

            - ``'none'``: no reduction will be applied.
            - ``'mean'``: compute and return the mean of elements in the output.
            - ``'sum'``: the output elements will be summed.
            - ``'batchmean'``: average loss is taken over the batch, similar to the mean mode.

    Inputs:
        - **logits** (Tensor) - The input Tensor. The data type must be float16, float32 or float64.
        - **labels** (Tensor) - The label Tensor which has the same shape and data type as `logits`.

    Outputs:
        Tensor or Scalar, if `reduction` is ``'none'``, then output is a tensor and has the same shape as `logits`.
        Otherwise it is a scalar.

    Raises:
        TypeError: If `reduction` is not a str.
        TypeError: If neither `logits` nor `labels` is a Tensor.
        TypeError: If dtype of `logits` or `labels` is not currently supported.
        ValueError: If shape of `logits` is not the same as `labels`.
        RuntimeError: If `logits` or `labels` is a scalar when `reduction` is 'batchmean'.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.kldiv_loss = ops.KLDivLoss(reduction='sum')
        ...     def construct(self, logits, labels):
        ...         result = self.kldiv_loss(logits, labels)
        ...         return result
        ...
        >>> net = Net()
        >>> logits = Tensor(np.array([0.2, 0.7, 0.1]), mindspore.float32)
        >>> labels = Tensor(np.array([0., 1., 0.]), mindspore.float32)
        >>> output = net(logits, labels)
        >>> print(output)
        -0.7
    """

    @prim_attr_register
    def __init__(self, reduction='mean'):
        """Initialize KLDivLoss."""
        device_target = context.get_context("device_target")
        if device_target == "CPU":
            support_mode = ['none', 'mean', 'batchmean', 'sum']
        elif device_target == "GPU":
            support_mode = ['none', 'mean', 'sum']
        elif device_target == "Ascend":
            support_mode = ['none', 'batchmean', 'sum', 'mean']
        else:
            raise ValueError(f"'{self.name}' unknown device target: '{device_target}'")

        self.reduction = validator.check_string(reduction, support_mode, 'reduction', self.name)


class BinaryCrossEntropy(Primitive):
    r"""
    Computes the binary cross entropy between the logits and the labels.

    Sets logits as :math:`x`, labels as :math:`y`, output as :math:`\ell(x, y)`.
    Let,

    .. math::
        L = \{l_1,\dots,l_N\}^\top, \quad
        l_n = - w_n \left[ y_n \cdot \log x_n + (1 - y_n) \cdot \log (1 - x_n) \right]

    In which, :math:`L` indicates the loss of all batch_sizes, :math:`l` indicates the loss of one batch_size,
    and n indicates one batch_size in the 1-N range, :math:`w_n` indicates the
    weight of :math:`n`-th batch of binary cross entropy. Then,

    .. math::
        \ell(x, y) = \begin{cases}
        L, & \text{if reduction} = \text{'none';}\\
        \operatorname{mean}(L), & \text{if reduction} = \text{'mean';}\\
        \operatorname{sum}(L),  & \text{if reduction} = \text{'sum'.}
        \end{cases}

    .. warning::
        - The value of :math:`x` must range from 0 to 1.

    Args:
        reduction (str, optional): Apply specific reduction method to the output: ``'none'`` , ``'mean'`` ,
            ``'sum'`` . Default: ``'mean'`` .

            - ``'none'``: no reduction will be applied.
            - ``'mean'``: compute and return the weighted mean of elements in the output.
            - ``'sum'``: the output elements will be summed.

    Inputs:
        - **logits** (Tensor) - The predictive value whose data type must be float16 or float32,
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **labels** (Tensor) - The target value which has the same shape and data type as `logits`.
        - **weight** (Tensor, optional) - A rescaling weight applied to the loss of each batch element.
          And it must have the same shape and data type as `logits`. Default: ``None`` .

    Outputs:
        Tensor or Scalar. Returns Tensor that has the same dtype and shape as `logits` if `reduction` is 'none'.
        Otherwise, returns a scalar Tensor.

    Raises:
        TypeError: If dtype of `logits`, `labels` or `weight` (if given) is neither float16 nor float32.
        ValueError: If `reduction` is not one of ``'none'``, ``'mean'`` or ``'sum'``.
        ValueError: If shape of `labels` is not the same as `logits` or `weight` (if given).
        TypeError: If `logits`, `labels` or `weight` is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.binary_cross_entropy = ops.BinaryCrossEntropy()
        ...     def construct(self, logits, labels, weight):
        ...         result = self.binary_cross_entropy(logits, labels, weight)
        ...         return result
        ...
        >>> net = Net()
        >>> logits = Tensor(np.array([0.2, 0.7, 0.1]), mindspore.float32)
        >>> labels = Tensor(np.array([0., 1., 0.]), mindspore.float32)
        >>> weight = Tensor(np.array([1, 2, 2]), mindspore.float32)
        >>> output = net(logits, labels, weight)
        >>> print(output)
        0.38240486
    """

    @prim_attr_register
    def __init__(self, reduction='mean'):
        """Initialize BinaryCrossEntropy."""
        self.reduction = validator.check_string(reduction, ['none', 'mean', 'sum'], 'reduction', self.name)


class ApplyAdaMax(Primitive):
    r"""
    Updates relevant entries according to the adamax scheme.

    The updating formulas are as follows,

    .. math::
        \begin{array}{ll} \\
            m_{t+1} = \beta_1 * m_{t} + (1 - \beta_1) * g \\
            v_{t+1} = \max(\beta_2 * v_{t}, \left| g \right|) \\
            var = var - \frac{l}{1 - \beta_1^{t+1}} * \frac{m_{t+1}}{v_{t+1} + \epsilon}
        \end{array}

    :math:`t` represents updating step while :math:`m` represents the 1st moment vector, :math:`m_{t}`
    is the last moment of :math:`m_{t+1}`, :math:`v` represents the 2nd moment vector, :math:`v_{t}`
    is the last moment of :math:`v_{t+1}`, :math:`l` represents scaling factor `lr`,
    :math:`g` represents `grad`, :math:`\beta_1, \beta_2` represent `beta1` and `beta2`,
    :math:`\beta_1^{t+1}` represents `beta1_power`, :math:`var` represents the variable to be updated,
    :math:`\epsilon` represents `epsilon`.

    Inputs of `var`, `m`, `v` and `grad` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Inputs:
        - **var** (Parameter) - Variable to be updated. With float32 or float16 data type.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **m** (Parameter) - The 1st moment vector in the updating formula, has the same shape as `var`.
          With float32 or float16 data type.
        - **v** (Parameter) - The 2nd moment vector in the updating formula. Mean square gradients
          with the same shape as `var`. With float32 or float16 data type.
        - **beta1_power** (Union[Number, Tensor]) - :math:`beta_1^t` in the updating formula, must be a scalar.
          With float32 or float16 data type.
        - **lr** (Union[Number, Tensor]) - Learning rate, :math:`l` in the updating formula, must be a scalar.
          With float32 or float16 data type.
        - **beta1** (Union[Number, Tensor]) - The exponential decay rate for the 1st moment estimations,
          must be a scalar. With float32 or float16 data type.
        - **beta2** (Union[Number, Tensor]) - The exponential decay rate for the 2nd moment estimations,
          must be a scalar. With float32 or float16 data type.
        - **epsilon** (Union[Number, Tensor]) - A small value added for numerical stability, must be a scalar.
          With float32 or float16 data type.
        - **grad** (Tensor) - A tensor for gradient, has the same shape as `var`.
          With float32 or float16 data type.

    Outputs:
        Tuple of 3 Tensor, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **m** (Tensor) - The same shape and data type as `m`.
        - **v** (Tensor) - The same shape and data type as `v`.

    Raises:
        TypeError: If dtype of `var`, `m`, `v`, `beta_power`, `lr`, `beta1`, `beta2`, `epsilon` or `grad` is neither
                   float16 nor float32.
        TypeError: If `beta_power`, `lr`, `beta1`, `beta2` or `epsilon` is neither a Number nor a Tensor.
        TypeError: If `grad` is not a Tensor.
        TypeError: If the data type of `var`, `m`, `v` and `grad` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_ada_max = ops.ApplyAdaMax()
        ...         self.var = Parameter(Tensor(np.array([[0.6, 0.4],
        ...                                               [0.1, 0.5]]).astype(np.float32)), name="var")
        ...         self.m = Parameter(Tensor(np.array([[0.6, 0.5],
        ...                                             [0.2, 0.6]]).astype(np.float32)), name="m")
        ...         self.v = Parameter(Tensor(np.array([[0.9, 0.1],
        ...                                             [0.7, 0.8]]).astype(np.float32)), name="v")
        ...     def construct(self, beta1_power, lr, beta1, beta2, epsilon, grad):
        ...         out = self.apply_ada_max(self.var, self.m, self.v, beta1_power, lr, beta1, beta2, epsilon, grad)
        ...         return out
        ...
        >>> net = Net()
        >>> beta1_power =Tensor(0.9, mindspore.float32)
        >>> lr = Tensor(0.001, mindspore.float32)
        >>> beta1 = Tensor(0.9, mindspore.float32)
        >>> beta2 = Tensor(0.99, mindspore.float32)
        >>> epsilon = Tensor(1e-10, mindspore.float32)
        >>> grad = Tensor(np.array([[0.3, 0.7], [0.1, 0.8]]).astype(np.float32))
        >>> output = net(beta1_power, lr, beta1, beta2, epsilon, grad)
        >>> print(output)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 5.93602717e-01,  3.92571449e-01],
         [ 9.72582996e-02,  4.92249995e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 5.69999993e-01,  5.19999981e-01],
         [ 1.89999998e-01,  6.20000005e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 8.90999973e-01,  6.99999988e-01],
         [ 6.93000019e-01,  8.00000012e-01]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('m', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('v', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('beta1_power', dtype=sig.sig_dtype.T1),
        sig.make_sig('lr', dtype=sig.sig_dtype.T2),
        sig.make_sig('beta1', dtype=sig.sig_dtype.T3),
        sig.make_sig('beta2', dtype=sig.sig_dtype.T4),
        sig.make_sig('epsilon', dtype=sig.sig_dtype.T5),
        sig.make_sig('grad', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self):
        """Initialize ApplyAdaMax"""
        self.add_prim_attr('side_effect_mem', True)


class ApplyAdadelta(Primitive):
    r"""
    Updates relevant entries according to the adadelta scheme.

    The Adadelta algorithm is proposed in
    `ADADELTA: AN ADAPTIVE LEARNING RATE METHOD <https://arxiv.org/abs/1212.5701>`_.

    .. math::
        \begin{array}{ll} \\
            \text{accum} = \rho * \text{accum} + (1 - \rho) * \text{grad}^2 \\
            \text{update} = \sqrt{\text{accum_update} +
              \epsilon} * \frac{\text{grad}}{\sqrt{\text{accum} + \epsilon}} \\
            \text{accum_update} = \rho * \text{accum_update} + (1 - \rho) * \text{update}^2 \\
            \text{var} = \text{var} - \text{lr} * \text{update}
        \end{array}

    where :math:`\rho` represents `rho`, :math:`\epsilon` represents `epsilon`.

    Inputs of `var`, `accum`, `accum_update` and `grad` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Inputs:
        - **var** (Parameter) - Weights to be updated. With float32 or float16 data type.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **accum** (Parameter) - Accumulation to be updated, has the same shape and data type as `var`.
        - **accum_update** (Parameter) - Accum_update to be updated, has the same shape and data type as `var`.
        - **lr** (Union[Number, Tensor]) - Learning rate, must be a scalar. With float32 or float16 data type.
        - **rho** (Union[Number, Tensor]) - Decay rate, must be a scalar. With float32 or float16 data type.
        - **epsilon** (Union[Number, Tensor]) - A small value added for numerical stability, must be a scalar.
          With float32 or float16 data type.
        - **grad** (Tensor) - Gradients, has the same shape and data type as `var`.

    Outputs:
        Tuple of 3 Tensor, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **accum** (Tensor) - The same shape and data type as `accum`.
        - **accum_update** (Tensor) - The same shape and data type as `accum_update`.

    Raises:
        TypeError: If dtype of `var`, `accum`, `accum_update`, `lr`, `rho`, `epsilon` or `grad` is neither float16 nor
                   float32.
        TypeError: If `accum_update`, `lr`, `rho` or `epsilon` is neither a Number nor a Tensor.
        TypeError: If the data type of `var`, `accum`, `accum_update` and `grad` conversion of Parameter
                      is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore
        >>> from mindspore import nn, Tensor, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_adadelta = ops.ApplyAdadelta()
        ...         self.var = Parameter(Tensor(np.array([[0.6, 0.4],
        ...                                               [0.1, 0.5]]).astype(np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.array([[0.6, 0.5],
        ...                                                 [0.2, 0.6]]).astype(np.float32)), name="accum")
        ...         self.accum_update = Parameter(Tensor(np.array([[0.9, 0.1],
        ...                                                        [0.7, 0.8]]).astype(np.float32)),
        ...                                                             name="accum_update")
        ...     def construct(self, lr, rho, epsilon, grad):
        ...         out = self.apply_adadelta(self.var, self.accum, self.accum_update, lr, rho, epsilon, grad)
        ...         return out
        ...
        >>> net = Net()
        >>> lr = Tensor(0.001, mindspore.float32)
        >>> rho = Tensor(0.0, mindspore.float32)
        >>> epsilon = Tensor(1e-6, mindspore.float32)
        >>> grad = Tensor(np.array([[0.3, 0.7], [0.1, 0.8]]).astype(np.float32))
        >>> output = net(lr, rho, epsilon, grad)
        >>> print(output)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 5.99051356e-01,  3.99683774e-01],
         [ 9.91633832e-02,  4.99105573e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 9.00000036e-02,  4.89999980e-01],
         [ 1.00000007e-02,  6.40000045e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 8.99990857e-01,  1.00000791e-01],
         [ 6.99930906e-01,  7.99999774e-01]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum_update', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T1),
        sig.make_sig('rho', dtype=sig.sig_dtype.T2),
        sig.make_sig('epsilon', dtype=sig.sig_dtype.T3),
        sig.make_sig('grad', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self):
        """Initialize ApplyAdadelta"""
        self.add_prim_attr('side_effect_mem', True)


class ApplyAdagrad(Primitive):
    r"""
    Updates relevant entries according to the adagrad scheme.
    The Adagrad algorithm was proposed in
    `Adaptive Subgradient Methods for Online Learning and Stochastic Optimization
    <http://www.jmlr.org/papers/volume12/duchi11a/duchi11a.pdf>`_.
    This module can adaptively assign different learning rates for each parameter in view of the uneven number
    of samples for different parameters.

    .. math::
        \begin{array}{ll} \\
            accum += grad * grad \\
            var -= lr * grad * \frac{1}{\sqrt{accum}}
        \end{array}

    Inputs of `var`, `accum` and `grad`  comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        update_slots (bool): If ``True`` , `accum` will be updated. Default: ``True`` .

    Inputs:
        - **var** (Parameter) - Variable to be updated. With float or complex data type.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **accum** (Parameter) - Accumulation to be updated. The shape must be the same as `var`.
        - **lr** (Union[Number, Tensor]) - The learning rate value, must be a scalar. With float or complex data type.
        - **grad** (Tensor) - A tensor for gradient. The shape must be the same as `var`.

    Outputs:
        Tuple of 2 Tensors, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **accum** (Tensor) - The same shape and data type as `accum`.

    Raises:
        TypeError: If dtype of `var`, `accum`, `lr` or `grad` is neither float nor complex.
        TypeError: If `lr` is neither a Number nor a Tensor.
        TypeError: If the data type of `var`, `accum` and `grad` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_adagrad = ops.ApplyAdagrad()
        ...         self.var = Parameter(Tensor(np.array([[0.6, 0.4],
        ...                                               [0.1, 0.5]]).astype(np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.array([[0.6, 0.5],
        ...                                                 [0.2, 0.6]]).astype(np.float32)), name="accum")
        ...     def construct(self, lr, grad):
        ...         out = self.apply_adagrad(self.var, self.accum, lr, grad)
        ...         return out
        ...
        >>> net = Net()
        >>> lr = Tensor(0.001, mindspore.float32)
        >>> grad = Tensor(np.array([[0.3, 0.7], [0.1, 0.8]]).astype(np.float32))
        >>> output = net(lr, grad)
        >>> print(output)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 5.99638879e-01,  3.99296492e-01],
         [ 9.97817814e-02,  4.99281585e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 6.90000057e-01,  9.90000010e-01],
         [ 2.10000008e-01,  1.24000001e+00]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T1),
        sig.make_sig('grad', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self, update_slots=True):
        """Initialize ApplyAdagrad."""
        validator.check_value_type("update_slots", update_slots, [bool], self.name)
        self.add_prim_attr('side_effect_mem', True)


class ApplyAdagradV2(Primitive):
    r"""
    Updates relevant entries according to the adagradv2 scheme.

    .. math::
        \begin{array}{ll} \\
            accum += grad * grad \\
            var -= lr * grad * \frac{1}{\sqrt{accum} + \epsilon}
        \end{array}

    where :math:`\epsilon` represents `epsilon`.

    Inputs of `var`, `accum` and `grad` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Note:
        The difference is that `ApplyAdagradV2` has one more small constant value :math:`\epsilon` than `ApplyAdagrad`.

    Args:
        epsilon (float): A small value added for numerical stability.
        update_slots (bool): If ``True`` , `accum` will be updated. Default: ``True`` .

    Inputs:
        - **var** (Parameter) - Variable to be updated. With float16 or float32 data type.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **accum** (Parameter) - Accumulation to be updated. The shape must be the same as `var`.
        - **lr** (Union[Number, Tensor]) - The learning rate value, must be a float number or
          a scalar tensor with float16 or float32 data type.
        - **grad** (Tensor) - A tensor for gradient. The shape must be the same as `var`.

    Outputs:
        Tuple of 2 Tensors, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **accum** (Tensor) - The same shape and data type as `accum`.

    Raises:
        TypeError: If dtype of `var`, `accum`, `lr` or `grad` is neither float16 nor float32.
        TypeError: If `lr` is neither a Number nor a Tensor.
        TypeError: If the data type of `var`, `accum` and `grad` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_adagrad_v2 = ops.ApplyAdagradV2(epsilon=1e-6)
        ...         self.var = Parameter(Tensor(np.array([[0.6, 0.4],
        ...                                               [0.1, 0.5]]).astype(np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.array([[0.6, 0.5],
        ...                                                 [0.2, 0.6]]).astype(np.float32)), name="accum")
        ...     def construct(self, lr, grad):
        ...         out = self.apply_adagrad_v2(self.var, self.accum, lr, grad)
        ...         return out
        ...
        >>> net = Net()
        >>> lr = Tensor(0.001, mindspore.float32)
        >>> grad = Tensor(np.array([[0.3, 0.7], [0.1, 0.8]]).astype(np.float32))
        >>> output = net(lr, grad)
        >>> print(output)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 5.99638879e-01,  3.99296492e-01],
         [ 9.97817814e-02,  4.99281585e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 6.90000057e-01,  9.90000010e-01],
         [ 2.10000008e-01,  1.24000001e+00]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T1),
        sig.make_sig('grad', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self, epsilon, update_slots=True):
        """Initialize ApplyAdagradV2."""
        validator.check_value_type("epsilon", epsilon, [float], self.name)
        validator.check_value_type("update_slots", update_slots, [bool], self.name)
        self.add_prim_attr('side_effect_mem', True)


class SparseApplyAdagrad(Primitive):
    """
    Deprecated
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1)
    )

    @deprecated("1.9", "SparseApplyAdagrad", False)
    @prim_attr_register
    def __init__(self, lr, update_slots=True, use_locking=False):
        """Initialize SparseApplyAdagrad."""
        validator.check_is_float(lr, "lr", self.name)
        validator.check_value_type("update_slots", update_slots, [bool], self.name)
        validator.check_value_type("use_locking", use_locking, [bool], self.name)
        self.add_prim_attr('side_effect_mem', True)


class SparseApplyAdagradV2(Primitive):
    r"""
    Updates relevant entries according to the adagrad scheme, one more epsilon attribute than SparseApplyAdagrad.

    .. math::
        \begin{array}{ll} \\
            accum += grad * grad \\
            var -= lr * grad * \frac{1}{\sqrt{accum} + \epsilon}
        \end{array}

    where :math:`\epsilon` represents `epsilon`.

    Inputs of `var`, `accum` and `grad` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        lr (float): Learning rate.
        epsilon (float): A small value added for numerical stability.
        use_locking (bool): If ``True`` , the `var` and `accum` tensors will be protected from being updated.
            Default: ``False`` .
        update_slots (bool): If ``True`` , the computation logic will be different to `False`. Default: ``True`` .

    Inputs:
        - **var** (Parameter) - Variable to be updated. The data type must be float16 or float32.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **accum** (Parameter) - Accumulation to be updated. The shape must be the same as `var`.
        - **grad** (Tensor) - Gradients has the same shape as `var` and
          :math:`grad.shape[1:] = var.shape[1:]` if var.shape > 1.
        - **indices** (Tensor) - A vector of indices into the first dimension of `var` and `accum`.
          The type must be int32 and :math:`indices.shape[0] = grad.shape[0]`.

    Outputs:
        Tuple of 2 tensors, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **accum** (Tensor) - The same shape and data type as `accum`.

    Raises:
        TypeError: If neither `lr` nor `epsilon` is a float.
        TypeError: If neither `update_slots` nor `use_locking` is a bool.
        TypeError: If dtype of `var`, `accum` or `grad` is neither float16 nor float32.
        TypeError: If dtype of `indices` is not int32.
        RuntimeError: If the data type of `var`, `accum` and `grad` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.sparse_apply_adagrad_v2 = ops.SparseApplyAdagradV2(lr=1e-8, epsilon=1e-6)
        ...         self.var = Parameter(Tensor(np.array([[0.2]]).astype(np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.array([[0.1]]).astype(np.float32)), name="accum")
        ...
        ...     def construct(self, grad, indices):
        ...         out = self.sparse_apply_adagrad_v2(self.var, self.accum, grad, indices)
        ...         return out
        ...
        >>> net = Net()
        >>> grad = Tensor(np.array([[0.7]]).astype(np.float32))
        >>> indices = Tensor(np.array([0]), mindspore.int32)
        >>> output = net(grad, indices)
        >>> print(output)
        (Tensor(shape=[1, 1], dtype=Float32, value=
        [[ 1.99999988e-01]]), Tensor(shape=[1, 1], dtype=Float32, value=
        [[ 5.89999974e-01]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1)
    )

    @prim_attr_register
    def __init__(self, lr, epsilon, use_locking=False, update_slots=True):
        """Initialize SparseApplyAdagradV2."""
        self.lr = validator.check_value_type("lr", lr, [float], self.name)
        self.epsilon = validator.check_value_type("epsilon", epsilon, [float], self.name)
        self.use_locking = validator.check_value_type("update_slots", update_slots, [bool], self.name)
        self.update_slots = validator.check_value_type("use_locking", use_locking, [bool], self.name)
        self.add_prim_attr('side_effect_mem', True)


class ApplyProximalAdagrad(Primitive):
    r"""
    Updates relevant entries according to the proximal adagrad algorithm.
    The proximal adagrad algorithm was proposed in `Efficient Learning using Forward-Backward Splitting
    <http://papers.nips.cc//paper/3793-efficient-learning-using-forward-backward-splitting.pdf>`_.

    .. math::
        \begin{array}{ll} \\
            accum += grad * grad \\
            \text{prox_v} = var - lr * grad * \frac{1}{\sqrt{accum}} \\
            var = \frac{sign(\text{prox_v})}{1 + lr * l2} * \max(\left| \text{prox_v} \right| - lr * l1, 0)
        \end{array}

    Inputs of `var`, `accum` and `grad` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        use_locking (bool): If ``True`` , the var and accumulation tensors will be protected from being updated.
            Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable to be updated. The data type must be float16 or float32.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **accum** (Parameter) - Accumulation to be updated, must have the same shape and dtype as `var`.
        - **lr** (Union[Number, Tensor]) - The learning rate value, must be a scalar. The data type must be
          float16 or float32.
        - **l1** (Union[Number, Tensor]) - l1 regularization strength, must be a scalar. The data type must be
          float16 or float32.
        - **l2** (Union[Number, Tensor]) - l2 regularization strength, must be a scalar. The data type must be
          float16 or float32.
        - **grad** (Tensor) - Gradient with the same shape and dtype as `var`.

    Outputs:
        Tuple of 2 Tensors, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **accum** (Tensor) - The same shape and data type as `accum`.

    Raises:
        TypeError: If `use_blocking` is not a bool.
        TypeError: If dtype of `var`, `lr`, `l1` or `l2` is neither float16 nor float32.
        TypeError: If `lr`, `l1` or `l2` is neither a Number nor a Tensor.
        TypeError: If `grad` is not a Tensor.
        TypeError: If the data type of `var`, `accum` and `grad` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_proximal_adagrad = ops.ApplyProximalAdagrad()
        ...         self.var = Parameter(Tensor(np.array([[0.6, 0.4],
        ...                                               [0.1, 0.5]]).astype(np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.array([[0.6, 0.5],
        ...                                                 [0.2, 0.6]]).astype(np.float32)), name="accum")
        ...         self.lr = 0.01
        ...         self.l1 = 0.0
        ...         self.l2 = 0.0
        ...     def construct(self, grad):
        ...         out = self.apply_proximal_adagrad(self.var, self.accum, self.lr, self.l1, self.l2, grad)
        ...         return out
        ...
        >>> net = Net()
        >>> grad = Tensor(np.array([[0.3, 0.7], [0.1, 0.8]]).astype(np.float32))
        >>> output = net(grad)
        >>> print(output)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 5.96388459e-01,  3.92964751e-01],
         [ 9.78178233e-02,  4.92815793e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 6.90000057e-01,  9.90000010e-01],
         [ 2.10000008e-01,  1.24000001e+00]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T1),
        sig.make_sig('l1', dtype=sig.sig_dtype.T2),
        sig.make_sig('l2', dtype=sig.sig_dtype.T3),
        sig.make_sig('grad', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self, use_locking=False):
        """Initialize ApplyProximalAdagrad."""
        self.init_prim_io_names(inputs=['var', 'accum', 'lr', 'l1', 'l2', 'grad'],
                                outputs=['var', 'accum'])
        self.add_prim_attr('side_effect_mem', True)
        self.use_locking = validator.check_value_type("use_locking", use_locking, [bool], self.name)


class SparseApplyProximalAdagrad(Primitive):
    r"""
    Updates relevant entries according to the proximal adagrad algorithm.
    Compared with :class:`mindspore.ops.ApplyProximalAdagrad`,
    an additional index tensor is input.

    .. math::
        \begin{array}{ll} \\
            accum += grad * grad \\
            \text{prox_v} = var - lr * grad * \frac{1}{\sqrt{accum}} \\
            var = \frac{sign(\text{prox_v})}{1 + lr * l2} * \max(\left| \text{prox_v} \right| - lr * l1, 0)
        \end{array}

    Inputs of `var`, `accum` and `grad` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        use_locking (bool): If ``True`` , the `var` and `accum` tensors will be protected from being updated.
            Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable tensor to be updated. The data type must be float16 or float32.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **accum** (Parameter) - Variable tensor to be updated, has the same shape as `var`.
        - **lr** (Union[Number, Tensor]) - The learning rate value, must be a float number or
          a scalar tensor with float16 or float32 data type. It must be positive.
        - **l1** (Union[Number, Tensor]) - l1 regularization strength, must be a float number or
          a scalar tensor with float16 or float32 data type. It must be non-negative.
        - **l2** (Union[Number, Tensor]) - l2 regularization strength, must be a float number or
          a scalar tensor with float16 or float32 data type. It must be non-negative.
        - **grad** (Tensor) - A tensor must meet with
          :math:`grad.shape[1:] = var.shape[1:]` if var.shape > 1.
        - **indices** (Tensor) - A tensor of indices in the first dimension of `var` and `accum`.
          If there are duplicates in `indices`, the behavior is undefined. Must be one of the
          following types: int32, int64 and :math:`indices.shape[0] = grad.shape[0]`.

    Outputs:
        Tuple of 2 tensors, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **accum** (Tensor) - The same shape and data type as `accum`.

    Raises:
        TypeError: If `use_locking` is not a bool.
        TypeError: If dtype of `var`, `accum`, `lr`, `l1`, `l2` or `grad` is neither float16 nor float32.
        TypeError: If dtype of `indices` is neither int32 nor int64.
        ValueError: If `lr` <= 0 or `l1` < 0 or `l2` < 0.
        RuntimeError: If the data type of `var`, `accum` and `grad` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.sparse_apply_proximal_adagrad = ops.SparseApplyProximalAdagrad()
        ...         self.var = Parameter(Tensor(np.array([[4.1, 7.2], [1.1, 3.0]], np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.array([[0, 0], [0, 0]], np.float32)), name="accum")
        ...         self.lr = 1.0
        ...         self.l1 = 1.0
        ...         self.l2 = 0.0
        ...     def construct(self, grad, indices):
        ...         out = self.sparse_apply_proximal_adagrad(self.var, self.accum, self.lr, self.l1,
        ...                                                  self.l2, grad, indices)
        ...         return out
        ...
        >>> net = Net()
        >>> grad = Tensor(np.array([[1, 1], [1, 1]], np.float32))
        >>> indices = Tensor(np.array([0, 1], np.int32))
        >>> output = net(grad, indices)
        >>> print(output)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 2.09999990e+00,  5.19999981e+00],
         [ 0.00000000e+00,  1.00000000e+00]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 1.00000000e+00,  1.00000000e+00],
         [ 1.00000000e+00,  1.00000000e+00]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T1),
        sig.make_sig('l1', dtype=sig.sig_dtype.T2),
        sig.make_sig('l2', dtype=sig.sig_dtype.T3),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T4)
    )

    @prim_attr_register
    def __init__(self, use_locking=False):
        """Initialize SparseApplyProximalAdagrad."""
        self.init_prim_io_names(inputs=['var', 'accum', 'lr', 'l1', 'l2', 'grad', 'indices'],
                                outputs=['var', 'accum'])
        self.add_prim_attr('side_effect_mem', True)
        self.use_locking = validator.check_value_type("use_locking", use_locking, [bool], self.name)


class ApplyAddSign(Primitive):
    r"""
    Updates relevant entries according to the AddSign algorithm.

    .. math::
        \begin{array}{ll} \\
            m_{t+1} = \beta * m_{t} + (1 - \beta) * g \\
            \text{update} = (\alpha + \text{sign_decay} * sign(g) * sign(m)) * g \\
            var = var - lr_{t+1} * \text{update}
        \end{array}

    :math:`t` represents updating step while :math:`m` represents the 1st moment vector, :math:`m_{t}`
    is the last moment of :math:`m_{t+1}`, :math:`lr` represents scaling factor `lr`, :math:`g` represents `grad`,
    :math:`\alpha` represents `alpha`, :math:`\beta` represents `beta`.

    The data type of all inputs must be float16 or float32 on Ascend and float16, float32 or float64 on CPU and GPU.

    Inputs of `var`, `accum` and `grad` , `sign_decay` and `beta` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Inputs:
        - **var** (Parameter) - Variable tensor to be updated.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **m** (Parameter) - Variable tensor to be updated, has the same data type as `var`.
        - **lr** (Union[Number, Tensor]) - The learning rate value, must be a scalar.
        - **alpha** (Union[Number, Tensor]) - Must be a scalar.
        - **sign_decay** (Union[Number, Tensor]) - Must be a scalar.
        - **beta** (Union[Number, Tensor]) - The exponential decay rate, must be a scalar.
        - **grad** (Tensor) - A tensor of the same shape as `var`, for the gradient.

    Outputs:
        Tuple of 2 Tensors, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **m** (Tensor) - The same shape and data type as `m`.

    Raises:
        TypeError: If dtype of `var`, `lr` and `alpha` is not float16, float32 or float64.
        TypeError: If dtype of `sign_decay` and `beta` are both not float16, float32 or float64.
        TypeError: If `lr`, `alpha` or `sign_decay` is neither a Number nor a Tensor.
        TypeError: If `grad` is not a Tensor.
        TypeError: If the data type of `var`, `accum` and `grad` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_add_sign = ops.ApplyAddSign()
        ...         self.var = Parameter(Tensor(np.array([[0.6, 0.4],
        ...                                               [0.1, 0.5]]).astype(np.float32)), name="var")
        ...         self.m = Parameter(Tensor(np.array([[0.6, 0.5],
        ...                                             [0.2, 0.6]]).astype(np.float32)), name="m")
        ...         self.lr = 0.001
        ...         self.alpha = 1.0
        ...         self.sign_decay = 0.99
        ...         self.beta = 0.9
        ...     def construct(self, grad):
        ...         out = self.apply_add_sign(self.var, self.m, self.lr, self.alpha, self.sign_decay, self.beta, grad)
        ...         return out
        ...
        >>> net = Net()
        >>> grad = Tensor(np.array([[0.3, 0.7], [0.1, 0.8]]).astype(np.float32))
        >>> output = net(grad)
        >>> print(output)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 5.99403024e-01,  3.98607016e-01],
         [ 9.98010039e-02,  4.98407990e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 5.70000052e-01,  5.19999981e-01],
         [ 1.89999998e-01,  6.20000064e-01]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('m', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T1),
        sig.make_sig('alpha', dtype=sig.sig_dtype.T2),
        sig.make_sig('sign_decay', dtype=sig.sig_dtype.T3),
        sig.make_sig('beta', dtype=sig.sig_dtype.T3),
        sig.make_sig('grad', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self):
        """Initialize ApplyAddSign."""
        self.add_prim_attr('side_effect_mem', True)


class ApplyPowerSign(Primitive):
    r"""
    Updates relevant entries according to the AddSign algorithm.

    The AddSign algorithm was proposed in `Neural Optimizer Search with Reinforcement Learning
    <https://arxiv.org/abs/1709.07417>`_.

    .. math::
        \begin{array}{ll} \\
            m_{t+1} = \beta * m_{t} + (1 - \beta) * g \\
            \text{update} = \exp(\text{logbase} * \text{sign_decay} * sign(g) * sign(m)) * g \\
            var = var - lr_{t+1} * \text{update}
        \end{array}

    :math:`t` represents updating step while :math:`m` represents the 1st moment vector, :math:`m_{t}`
    is the last moment of :math:`m_{t+1}`, :math:`lr` represents scaling factor `lr`, :math:`g` represents `grad`,
    :math:`\beta` represents `beta`.

    All of inputs comply with the implicit type conversion rules to make the data types consistent.
    If `lr`, `logbase`, `sign_decay` or `beta` is a number, the number is automatically converted to Tensor,
    and the data type is consistent with the Tensor data type involved in the operation.
    If inputs are tensors and have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Note:
        On Ascend, input data type of float64 is currently not supported.

    Inputs:
        - **var** (Parameter) - Variable tensor to be updated. With float64, float32 or float16 data type.
          If data type of `var` is float16, all inputs must have the same data type as `var`.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **m** (Parameter) - Variable tensor to be updated, has the same shape as `var`.
        - **lr** (Union[Number, Tensor]) - The learning rate value, should be a scalar or Tensor
          with float64, float32 or float16 data type.
        - **logbase** (Union[Number, Tensor]) - Should be a scalar or Tensor with float64, float32 or float16 data type.
        - **sign_decay** (Union[Number, Tensor]) - Should be a scalar or Tensor with float64, float32 or
          float16 data type.
        - **beta** (Union[Number, Tensor]) - The exponential decay rate, should be a scalar or Tensor
          with float64, float32 or float16 data type.
        - **grad** (Tensor) - A tensor of the same shape as `var`, for the gradient.

    Outputs:
        Tuple of 2 Tensors, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **m** (Tensor) - The same shape and data type as `m`.

    Raises:
        TypeError: If dtype of `var`, `lr`, `logbase`, `sign_decay`, `beta` or `grad` is not one of float16,
        float32 or float64.
        TypeError: If `lr`, `logbase`, `sign_decay` or `beta` is neither a Number nor a Tensor.
        TypeError: If `grad` is not a Tensor.
        TypeError: If the data type of `lr`, `logbase`, `sign_decay` and `grad` conversion of Parameter
                      is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_power_sign = ops.ApplyPowerSign()
        ...         self.var = Parameter(Tensor(np.array([[0.6, 0.4],
        ...                                               [0.1, 0.5]]).astype(np.float32)), name="var")
        ...         self.m = Parameter(Tensor(np.array([[0.6, 0.5],
        ...                                             [0.2, 0.6]]).astype(np.float32)), name="m")
        ...         self.lr = 0.001
        ...         self.logbase = np.e
        ...         self.sign_decay = 0.99
        ...         self.beta = 0.9
        ...     def construct(self, grad):
        ...         out = self.apply_power_sign(self.var, self.m, self.lr, self.logbase,
        ...                                        self.sign_decay, self.beta, grad)
        ...         return out
        ...
        >>> net = Net()
        >>> grad = Tensor(np.array([[0.3, 0.7], [0.1, 0.8]]).astype(np.float32))
        >>> output = net(grad)
        >>> print(output)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 5.95575690e-01,  3.89676481e-01],
         [ 9.85252112e-02,  4.88201708e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 5.70000052e-01,  5.19999981e-01],
         [ 1.89999998e-01,  6.20000064e-01]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('m', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T),
        sig.make_sig('logbase', dtype=sig.sig_dtype.T),
        sig.make_sig('sign_decay', dtype=sig.sig_dtype.T),
        sig.make_sig('beta', dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self):
        """Initialize ApplyPowerSign."""
        self.add_prim_attr('side_effect_mem', True)


class ApplyGradientDescent(Primitive):
    r"""
    Updates `var` by subtracting `alpha` * `delta` from it.

    .. math::
        var = var - \alpha * \delta

    where :math:`\alpha` represents `alpha`, :math:`\delta` represents `delta`.

    Inputs of `var` and `delta` comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Inputs:
        - **var** (Parameter) - Variable tensor to be updated. With float32 or float16 data type.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **alpha** (Union[Number, Tensor]) - Scaling factor, must be a scalar. With float32 or float16 data type.
        - **delta** (Tensor) - A tensor for the change, has the same shape as `var`.

    Outputs:
        Tensor, represents the updated `var`.

    Raises:
        TypeError: If dtype of `var` or `alpha` is neither float16 nor float32.
        TypeError: If `delta` is not a Tensor.
        TypeError: If `alpha` is neither a Number nor a Tensor.
        TypeError: If the data type of `var` and `delta` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_gradient_descent = ops.ApplyGradientDescent()
        ...         self.var = Parameter(Tensor(np.ones([2, 2]).astype(np.float32)), name="var")
        ...         self.alpha = 0.001
        ...     def construct(self, delta):
        ...         out = self.apply_gradient_descent(self.var, self.alpha, delta)
        ...         return out
        ...
        >>> net = Net()
        >>> delta = Tensor(np.array([[0.1, 0.1], [0.1, 0.1]]).astype(np.float32))
        >>> output = net(delta)
        >>> print(output)
        [[0.9999 0.9999]
         [0.9999 0.9999]]
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('alpha', dtype=sig.sig_dtype.T1),
        sig.make_sig('delta', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self):
        """Initialize ApplyGradientDescent."""
        self.add_prim_attr('side_effect_mem', True)


class ApplyProximalGradientDescent(Primitive):
    r"""
    Updates relevant entries according to the FOBOS(Forward Backward Splitting) algorithm.
    Refer to the paper `Efficient Learning using Forward-Backward Splitting
    <http://papers.nips.cc//paper/3793-efficient-learning-using-forward-backward-splitting.pdf>`_ for more details.

    .. math::
        \begin{array}{ll} \\
            \text{prox_v} = var - \alpha * \delta \\
            var = \frac{sign(\text{prox_v})}{1 + \alpha * l2} * \max(\left| \text{prox_v} \right| - \alpha * l1, 0)
        \end{array}

    where :math:`\alpha` represents `alpha`, :math:`\delta` represents `delta`.

    Inputs of `var` and `delta` comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Inputs:
        - **var** (Parameter) - Variable tensor to be updated. With float32 or float16 data type.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **alpha** (Union[Number, Tensor]) - Scaling factor, must be a scalar. With float32 or float16 data type.
        - **l1** (Union[Number, Tensor]) - l1 regularization strength, must be a scalar.
          With float32 or float16 data type.
        - **l2** (Union[Number, Tensor]) - l2 regularization strength, must be a scalar.
          With float32 or float16 data type.
        - **delta** (Tensor) - A tensor for the change.

    Outputs:
        Tensor, represents the updated `var`.

    Raises:
        TypeError: If dtype of `var`, `alpha`, `l1` or `l2` is neither float16 nor float32.
        TypeError: If `alpha`, `l1` or `l2` is neither a Number nor a Tensor.
        TypeError: If `delta` is not a Tensor.
        TypeError: If the data type of `var`, and `delta` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.apply_proximal_gradient_descent = ops.ApplyProximalGradientDescent()
        ...         self.var = Parameter(Tensor(np.ones([2, 2]).astype(np.float32)), name="var")
        ...         self.alpha = 0.001
        ...         self.l1 = 0.1
        ...         self.l2 = 0.1
        ...     def construct(self, delta):
        ...         out = self.apply_proximal_gradient_descent(self.var, self.alpha, self.l1, self.l2, delta)
        ...         return out
        ...
        >>> net = Net()
        >>> delta = Tensor(np.array([[0.1, 0.1], [0.1, 0.1]]).astype(np.float32))
        >>> output = net(delta)
        >>> print(output)
        [[0.99969995 0.99969995]
         [0.99969995 0.99969995]]
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('alpha', dtype=sig.sig_dtype.T1),
        sig.make_sig('l1', dtype=sig.sig_dtype.T2),
        sig.make_sig('l2', dtype=sig.sig_dtype.T3),
        sig.make_sig('delta', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self):
        """Initialize ApplyGradientDescent."""
        self.add_prim_attr('side_effect_mem', True)


class LARSUpdate(PrimitiveWithInfer):
    """
    Conducts LARS (layer-wise adaptive rate scaling) update on the sum of squares of gradient.

    For more details, please refer to :class:`mindspore.nn.LARS`.

    Args:
        epsilon (float, optional): Term added to the denominator to improve numerical stability.
            Default: ``1e-05`` .
        hyperpara (float, optional): Trust coefficient for calculating the local learning rate.
            Default: ``0.001`` .
        use_clip (bool, optional): Whether to use clip operation for calculating the local learning rate.
            Default: ``False`` .

    Inputs:
        - **weight** (Tensor) - A tensor, representing the weight.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **gradient** (Tensor) - The gradient of weight, which has the same shape and dtype with weight.
        - **norm_weight** (Tensor) - A scalar tensor, representing the sum of squares of weight.
        - **norm_gradient** (Tensor) - A scalar tensor, representing the sum of squares of gradient.
        - **weight_decay** (Union[Number, Tensor]) - Weight decay. It must be a scalar tensor or number.
        - **learning_rate** (Union[Number, Tensor]) - Learning rate. It must be a scalar tensor or number.

    Outputs:
        Tensor, represents the new gradient.

    Raises:
        TypeError: If neither `epsilon` nor `hyperpara` is a float.
        TypeError: If `use_clip` is not a bool.
        TypeError: If `weight`, `gradient`, `norm_weight` or `norm_gradient` is not a Tensor.
        TypeError: If `weight_decay` or `learning_rate` is neither a Number nor a Tensor.
        TypeError: If shape of `gradient` is not the same as `weight`.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.lars = ops.LARSUpdate()
        ...         self.reduce = ops.ReduceSum()
        ...         self.square = ops.Square()
        ...     def construct(self, weight, gradient):
        ...         w_square_sum = self.reduce(self.square(weight))
        ...         grad_square_sum = self.reduce(self.square(gradient))
        ...         grad_t = self.lars(weight, gradient, w_square_sum, grad_square_sum, 0.0, 1.0)
        ...         return grad_t
        ...
        >>> weight = Tensor(np.array([[0.5, 0.8, 0.2], [0.6, 0.4, 0.2]]).astype(np.float32))
        >>> gradient = Tensor(np.array([[0.4, 0.4, 0.5], [0.2, 0.4, 0.3]]).astype(np.float32))
        >>> net = Net()
        >>> output = net(Tensor(weight), Tensor(gradient))
        >>> print(output)
        [[0.0005265  0.0005265 0.00065813]
         [0.00026325 0.0005265 0.00039488]]
    """

    @prim_attr_register
    def __init__(self, epsilon=1e-05, hyperpara=0.001, use_clip=False):
        """Initialize LARSUpdate."""
        validator.check_value_type("epsilon", epsilon, [float], self.name)
        validator.check_value_type("hyperpara", hyperpara, [float], self.name)
        validator.check_value_type("use_clip", use_clip, [bool], self.name)


class ApplyFtrl(Primitive):
    """
    Updates relevant entries according to the FTRL scheme.

    For more details, please refer to :class:`mindspore.nn.FTRL`.

    Note:
        - Currently, only positive numbers are supported on the Ascend platform,
          and the calculation results for other scenarios are not defined.
        - Inputs of `var`, `accum`, `linear` and `grad` comply with the implicit type conversion rules
          to make the data types consistent.
          If they have different data types, the lower priority data type will be converted to
          the relatively highest priority data type.

    Args:
        use_locking (bool): Use locks for updating operation if ``True`` . Default: ``False`` .

    Inputs:
        - **var** (Parameter) - The variable to be updated. The data type must be float16 or float32.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **accum** (Parameter) - The accumulation to be updated, must be same shape as `var`.
        - **linear** (Parameter) - The linear coefficient to be updated, must be same shape as `var`.
        - **grad** (Tensor) - Gradient. The data type must be float16 or float32.
        - **lr** (Union[Number, Tensor]) - The learning rate value, must be positive. Default: ``0.001`` .
          It must be a float number or a scalar tensor with float16 or float32 data type.
        - **l1** (Union[Number, Tensor]) - l1 regularization strength, must be greater than or equal to zero.
          Default: ``0.0`` . It must be a float number or a scalar tensor with float16 or float32 data type.
        - **l2** (Union[Number, Tensor]) - l2 regularization strength, must be greater than or equal to zero.
          Default: ``0.0`` . It must be a float number or a scalar tensor with float16 or float32 data type.
        - **lr_power** (Union[Number, Tensor]) - Learning rate power controls how the learning rate decreases
          during training, must be less than or equal to zero. Use fixed learning rate if lr_power is zero.
          Default: ``-0.5`` . It must be a float number or a scalar tensor with float16 or float32 data type.

    Outputs:
        - **var** (Tensor) - Represents the updated `var`. As the input parameters has been updated in-place, this
          value is always zero when the platform is GPU.

    Raises:
        TypeError: If `use_locking` is not a bool.
        TypeError: If dtype of `var`, `grad`, `lr`, `l1`, `l2` or `lr_power` is neither float16 nor float32.
        TypeError: If `lr`, `l1`, `l2` or `lr_power` is neither a Number nor a Tensor.
        TypeError: If `grad` is not a Tensor.
        TypeError: If the parameter types of `var`, `accum` and `linear` are inconsistent.
        TypeError: If the parameter types of `grad`, `lr`, `l1`, `l2`, `lr_power` are inconsistent with `var`
            and the precision is greater than `var`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class ApplyFtrlNet(nn.Cell):
        ...     def __init__(self):
        ...         super(ApplyFtrlNet, self).__init__()
        ...         self.apply_ftrl = ops.ApplyFtrl()
        ...         self.lr = 0.001
        ...         self.l1 = 0.0
        ...         self.l2 = 0.0
        ...         self.lr_power = -0.5
        ...         self.var = Parameter(Tensor(np.array([[0.6, 0.4],
        ...                                               [0.1, 0.5]]).astype(np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.array([[0.6, 0.5],
        ...                                                 [0.2, 0.6]]).astype(np.float32)), name="accum")
        ...         self.linear = Parameter(Tensor(np.array([[0.9, 0.1],
        ...                                                  [0.7, 0.8]]).astype(np.float32)), name="linear")
        ...
        ...     def construct(self, grad):
        ...         out = self.apply_ftrl(self.var, self.accum, self.linear, grad, self.lr, self.l1, self.l2,
        ...                               self.lr_power)
        ...         return out
        ...
        >>> net = ApplyFtrlNet()
        >>> input_x = Tensor(np.array([[0.3, 0.7], [0.1, 0.8]]).astype(np.float32))
        >>> output = net(input_x)
        >>> print(net.var.asnumpy())
        [[ 0.0390525  0.11492836]
         [ 0.00066425 0.15075898]]
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('linear', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T),
        sig.make_sig('l1', dtype=sig.sig_dtype.T),
        sig.make_sig('l2', dtype=sig.sig_dtype.T),
        sig.make_sig('lr_power', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self, use_locking=False):
        """Initialize ApplyFtrl."""
        self.init_prim_io_names(inputs=['var', 'accum', 'linear', 'grad', 'lr', 'l1', 'l2', 'lr_power'],
                                outputs=['output'])
        self.add_prim_attr('side_effect_mem', True)
        self.use_locking = validator.check_value_type("use_locking", use_locking, [bool], self.name)


class SparseApplyFtrl(Primitive):
    """
    Updates relevant entries according to the FTRL-proximal scheme
    For more details, please refer to :class:`mindspore.nn.FTRL`.

    All of inputs except `indices` comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        lr (float): The learning rate value, must be positive.
        l1 (float): l1 regularization strength, must be greater than or equal to zero.
        l2 (float): l2 regularization strength, must be greater than or equal to zero.
        lr_power (float): Learning rate power controls how the learning rate decreases during training,
            must be less than or equal to zero. Use fixed learning rate if `lr_power` is zero.
        use_locking (bool, optional): Use locks for updating operation if ``True`` . Default: ``False`` .

    Inputs:
        - **var** (Parameter) - The variable to be updated. The data type must be float16 or float32.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **accum** (Parameter) - The accumulation to be updated, must be same shape as `var`.
        - **linear** (Parameter) - The linear coefficient to be updated, must be the same shape as `var`.
        - **grad** (Tensor) - A tensor must meet with :math:`grad.shape[1:] = var.shape[1:]`
          if var.shape > 1.
        - **indices** (Tensor) - A tensor of indices in the first dimension of `var` and `accum`.
          If there are duplicates in `indices`, the behavior is undefined.
          The type must be int32 or int64 and :math:`indices.shape[0] = grad.shape[0]`.

    Outputs:
        - **var** (Tensor) - Tensor, has the same shape and data type as `var`.
        - **accum** (Tensor) - Tensor, has the same shape and data type as `accum`.
        - **linear** (Tensor) - Tensor, has the same shape and data type as `linear`.

    Raises:
        TypeError: If `lr`, `l1`, `l2` or `lr_power` is not a float.
        TypeError: If `use_locking` is not a bool.
        TypeError: If dtype of `var`, `accum`, `linear` or `grad` is neither float16 nor float32.
        TypeError: If dtype of `indices` is neither int32 nor int64.
        RuntimeError: If the data type of all of inputs except `indices` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, Parameter, ops
        >>> class SparseApplyFtrlNet(nn.Cell):
        ...     def __init__(self):
        ...         super(SparseApplyFtrlNet, self).__init__()
        ...         self.sparse_apply_ftrl = ops.SparseApplyFtrl(lr=0.01, l1=0.0, l2=0.0, lr_power=-0.5)
        ...         self.var = Parameter(Tensor(np.array([[0.2]]).astype(np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.array([[0.1]]).astype(np.float32)), name="accum")
        ...         self.linear = Parameter(Tensor(np.array([[0.6]]).astype(np.float32)), name="linear")
        ...
        ...     def construct(self, grad, indices):
        ...         out = self.sparse_apply_ftrl(self.var, self.accum, self.linear, grad, indices)
        ...         return out
        ...
        >>> net = SparseApplyFtrlNet()
        >>> grad = Tensor(np.array([[0.7]]).astype(np.float32))
        >>> indices = Tensor(np.ones([1]), mindspore.int32)
        >>> output = net(grad, indices)
        >>> print(output)
        (Tensor(shape=[1, 1], dtype=Float32, value=
        [[2.00000003e-01]]), Tensor(shape=[1, 1], dtype=Float32, value=
        [[1.00000001e-01]]), Tensor(shape=[1, 1], dtype=Float32, value=
        [[6.00000024e-01]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('linear', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1)
    )

    @prim_attr_register
    def __init__(self, lr, l1, l2, lr_power, use_locking=False):
        """Initialize SparseApplyFtrl."""
        validator.check_value_type("lr", lr, [float], self.name)
        validator.check_value_type("l1", l1, [float], self.name)
        validator.check_value_type("l2", l2, [float], self.name)
        validator.check_value_type("lr_power", lr_power, [float], self.name)
        self.lr = validator.check_positive_float(lr, "lr", self.name)
        self.l1 = validator.check_non_negative_float(l1, "l1", self.name)
        self.l2 = validator.check_non_negative_float(l2, "l2", self.name)
        self.lr_power = validator.check_number("lr_power", lr_power, 0, validator.LE, self.name)
        self.use_locking = validator.check_value_type("use_locking", use_locking, [bool], self.name)
        self.init_prim_io_names(inputs=['var', 'accum', 'linear', 'grad', 'indices'],
                                outputs=['var', 'accum', 'linear'])
        self.add_prim_attr('side_effect_mem', True)


class SparseApplyFtrlV2(PrimitiveWithInfer):
    """
    The SparseApplyFtrlV2 interface is deprecated, please use the :class:`mindspore.ops.SparseApplyFtrl` instead.

    Supported Platforms:
        Deprecated
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('linear', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1)
    )

    @deprecated("2.1", "ops.SparseApplyFtrl", False)
    @prim_attr_register
    def __init__(self, lr, l1, l2, l2_shrinkage, lr_power, use_locking=False):
        """Initialize SparseApplyFtrlV2."""
        validator.check_value_type("lr", lr, [float], self.name)
        validator.check_value_type("l1", l1, [float], self.name)
        validator.check_value_type("l2", l2, [float], self.name)
        validator.check_value_type("lr_power", lr_power, [float], self.name)
        self.lr = validator.check_positive_float(lr, "lr", self.name)
        self.l1 = validator.check_non_negative_float(l1, "l1", self.name)
        self.l2 = validator.check_non_negative_float(l2, "l2", self.name)
        self.lr_power = validator.check_number("lr_power", lr_power, 0, validator.LE, self.name)
        self.l2_shrinkage = validator.check_value_type("l2_shrinkage", l2_shrinkage, [float], self.name)
        self.use_locking = validator.check_value_type("use_locking", use_locking, [bool], self.name)
        self.add_prim_attr('side_effect_mem', True)

    def infer_shape(self, var_shape, accum_shape, linear_shape, grad_shape, indices_shape):
        validator.check('var shape', var_shape, 'accum shape', accum_shape, validator.EQ, self.name)
        validator.check('var shape', var_shape, 'linear shape', linear_shape, validator.EQ, self.name)
        if len(var_shape) > 1:
            validator.check('var_shape[1:]', var_shape[1:], 'grad_shape[1:]', grad_shape[1:], validator.EQ, self.name)
        validator.check_int(len(indices_shape), 1, validator.EQ, "indices rank", self.name)
        validator.check('grad_shape[0]', grad_shape[0], 'indices_shape[0]', indices_shape[0], validator.EQ, self.name)
        return var_shape, accum_shape, linear_shape

    def infer_dtype(self, var_dtype, accum_dtype, linear_dtype, grad_dtype, indices_dtype):
        args = {"var_dtype": var_dtype, "accum_dtype": accum_dtype,
                "linear_dtype": linear_dtype, "grad_dtype": grad_dtype}
        validator.check_tensors_dtypes_same_and_valid(args, [mstype.float16, mstype.float32], self.name)
        validator.check_tensor_dtype_valid("indicese", indices_dtype, [mstype.int32], self.name)
        return var_dtype, accum_dtype, linear_dtype


class Dropout2D(PrimitiveWithInfer):
    r"""
    During training, randomly zeroes some channels of the input tensor with probability :math:`1-keep\_prob`
    from a Bernoulli distribution(For a 4-dimensional tensor with a shape of :math:`(N, C, H, W)`,
    the channel feature map refers
    to a 2-dimensional feature map with the shape of :math:`(H, W)`).

    Dropout2D can improve the independence between channel feature maps.

    Note:
        The keep probability :math:`keep\_prob` is equal to :math:`1 - p` in :func:`mindspore.ops.dropout2d`.

    Args:
        keep_prob (float, optional): The keep probability of a channel, between 0 and 1, e.g. `keep_prob` = 0.8,
            means dropping out 20% of channels. Default: ``0.5`` .

    Inputs:
        - **x** (Tensor) - A 4-D tensor with shape :math:`(N, C, H, W)`, where N is the batch size, C is the number
          of channels, H is the feature height, and W is the feature width.

    Outputs:
        - **output** (Tensor) - With the same shape and data type as `x`.
        - **mask** (Tensor) - With the same shape as `x` and the data type is bool.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If the data type of `keep_prob` is not float.
        ValueError: If `keep_prob` is out of the range `[0.0, 1.0]`.
        ValueError: If `x` shape is not `4D`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> dropout = ops.Dropout2D(keep_prob=0.5)
        >>> x = Tensor(np.ones([2, 1, 2, 3]), mindspore.float32)
        >>> output, mask = dropout(x)
        >>> print(output.shape)
        (2, 1, 2, 3)
    """

    @prim_attr_register
    def __init__(self, keep_prob=0.5):
        """Initialize Dropout2D."""
        super().__init__("Dropout2D")
        self.keep_prob = validator.check_value_type("keep_prob", keep_prob, [float], self.name)
        self.keep_prob = validator.check_float_range(keep_prob, 0.0, 1.0, validator.INC_BOTH, "keep_prob", self.name)


class Dropout3D(PrimitiveWithInfer):
    r"""
    During training, randomly zeroes some channels of the input tensor
    with probability :math:`1-keep\_prob` from a Bernoulli distribution(For a 5-dimensional
    tensor with a shape of NCDHW,
    the channel feature map refers to a 3-dimensional feature map with a shape of DHW).

    Note:
        The keep probability :math:`keep\_prob` is equal to :math:`1 - p` in :func:`mindspore.ops.dropout3d`.

    Dropout3D can improve the independence between channel feature maps.

    Args:
        keep_prob (float): The keep probability of a channel, between 0 and 1, e.g. `keep_prob` = 0.8,
            means dropping out 20% of channels. Default: ``0.5`` .

    Inputs:
        - **x** (Tensor) - A 5-D tensor with shape :math:`(N, C, D, H, W)`, where N is the batch size, C is the number
          of channels, D is the feature depth, H is the feature height, and W is the feature width.

    Outputs:
        - **output** (Tensor) - With the same shape and data type as `x`.
        - **mask** (Tensor) - With the same shape as `x` and the data type is bool.

    Raises:
        TypeError: If the data type of `keep_prob` is not float.
        ValueError: If `keep_prob` is out of the range [0.0, 1.0];
                    or if the dim of input is not 5-D.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> dropout = ops.Dropout3D(keep_prob=0.5)
        >>> x = Tensor(np.ones([2, 1, 2, 1, 2]), mindspore.float32)
        >>> output, mask = dropout(x)
        >>> print(output.shape)
        (2, 1, 2, 1, 2)
    """

    @prim_attr_register
    def __init__(self, keep_prob=0.5):
        """Initialize Dropout3D."""
        super().__init__("Dropout3D")
        self.keep_prob = validator.check_value_type("keep_prob", keep_prob, [float], self.name)
        self.keep_prob = validator.check_float_range(keep_prob, 0.0, 1.0, validator.INC_BOTH, "keep_prob", self.name)


class CTCLoss(Primitive):
    r"""
    Calculates the CTC (Connectionist Temporal Classification) loss and the gradient.

    The bottom layer of this interface calls the implementation of the third-party baidu-research::warp-ctc.
    The CTC algorithm is proposed in `Connectionist Temporal Classification: Labeling Unsegmented Sequence Data with
    Recurrent Neural Networks <http://www.cs.toronto.edu/~graves/icml_2006.pdf>`_.

    CTCLoss calculates loss between a continuous time series and a target sequence.
    CTCLoss sums over the probability of input to target, producing a loss value which is differentiable with
    respect to each input node. The alignment of input to target is assumed to be “many-to-one”,
    such that the length of target series must be less than or equal to the length of input.

    Args:
        preprocess_collapse_repeated (bool): If ``True`` , repeated labels will be collapsed prior to the CTC
                                             calculation. Default: ``False`` .
        ctc_merge_repeated (bool): If ``False`` , during CTC calculation, repeated non-blank labels will not be merged
                                   and these labels will be interpreted as individual ones. This is a simplified
                                   version of CTC. Default: ``True`` .
        ignore_longer_outputs_than_inputs (bool): If ``True`` , sequences with longer outputs than inputs will be
                                                  ignored. Default: ``False`` .

    Inputs:
        - **x** (Tensor) - The input Tensor must be a `3-D` tensor whose shape is
          :math:`(max\_time, batch\_size, num\_classes)`. `num_classes` must be `num_labels + 1` classes, `num_labels`
          indicates the number of actual labels. Blank labels are reserved. Default blank label is `num_classes - 1`.
          Data type must be float16, float32 or float64.
        - **labels_indices** (Tensor) - The indices of labels. `labels_indices[i, :] = [b, t]` means
          `labels_values[i]` stores the id for `(batch b, time t)`. The type must be int64 and rank must be 2.
        - **labels_values** (Tensor) - A `1-D` input tensor. The values are associated with the given batch and time.
          The type must be int32. `labels_values[i]` must be in the range of `[0, num_classes)`.
        - **sequence_length** (Tensor) - A tensor containing sequence lengths with the shape of :math:`(batch\_size, )`.
          The type must be int32. Each value in the tensor must not be greater than `max_time`.

    Outputs:
        - **loss** (Tensor) - A tensor containing log-probabilities, the shape is :math:`(batch\_size, )`.
          The tensor has the same data type as `x`.
        - **gradient** (Tensor) - The gradient of `loss`, has the same shape and data type as `x`.

    Raises:
        TypeError: If `preprocess_collapse_repeated`, `ctc_merge_repeated` or `ignore_longer_outputs_than_inputs`
                   is not a bool.
        TypeError: If `x`, `labels_indices`, `labels_values` or `sequence_length` is not a Tensor.
        ValueError: If rank of `labels_indices` is not equal to 2.
        TypeError: If dtype of `x` is not one of the following: float16, float32 nor float64.
        TypeError: If dtype of `labels_indices` is not int64.
        TypeError: If dtype of `labels_values` or `sequence_length` is not int32.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([[[0.3, 0.6, 0.6],
        ...                       [0.4, 0.3, 0.9]],
        ...
        ...                      [[0.9, 0.4, 0.2],
        ...                       [0.9, 0.9, 0.1]]]).astype(np.float32))
        >>> labels_indices = Tensor(np.array([[0, 0], [1, 0]]), mindspore.int64)
        >>> labels_values = Tensor(np.array([2, 2]), mindspore.int32)
        >>> sequence_length = Tensor(np.array([2, 2]), mindspore.int32)
        >>> ctc_loss = ops.CTCLoss()
        >>> loss, gradient = ctc_loss(x, labels_indices, labels_values, sequence_length)
        >>> print(loss)
        [ 0.79628  0.5995158 ]
        >>> print(gradient)
        [[[ 0.27029088  0.36485454  -0.6351454  ]
          [ 0.28140804  0.25462854  -0.5360366 ]]
         [[ 0.47548494  0.2883962    0.04510255 ]
          [ 0.4082751   0.4082751    0.02843709 ]]]
    """

    @prim_attr_register
    def __init__(self, preprocess_collapse_repeated=False, ctc_merge_repeated=True,
                 ignore_longer_outputs_than_inputs=False):
        """Initialize CTCLoss."""
        self.init_prim_io_names(inputs=["inputs", "labels_indices", "labels_values", "sequence_length"],
                                outputs=["loss", "gradient"])
        validator.check_value_type("preprocess_collapse_repeated", preprocess_collapse_repeated, [bool], self.name)
        self.preprocess_collapse_repeated_ = preprocess_collapse_repeated
        self.ctc_merge_repeated_ = validator.check_value_type("ctc_merge_repeated", ctc_merge_repeated,
                                                              [bool], self.name)
        validator.check_value_type("ignore_longer_outputs_than_inputs",
                                   ignore_longer_outputs_than_inputs, [bool], self.name)
        self.ignore_longer_outputs_than_inputs_ = ignore_longer_outputs_than_inputs


class CTCGreedyDecoder(Primitive):
    r"""
    Performs greedy decoding on the logits given in inputs.

    Refer to :func:`mindspore.ops.ctc_greedy_decoder` for more details.

    Note:
        On Ascend, 'merge_repeated' can not be set to false.

    Args:
        merge_repeated (bool, optional): If ``True`` , merge repeated classes in output. Default: ``True`` .

    Inputs:
        - **inputs** (Tensor) - The input Tensor must be a 3-D tensor whose shape is
          :math:`(max\_time, batch\_size, num\_classes)`. `num_classes` must be `num_labels + 1` classes,
          `num_labels` indicates the number of actual labels. Blank labels are reserved.
          Default blank label is `num_classes - 1`. Data type must be float32 or float64.
        - **sequence_length** (Tensor) - A tensor containing sequence lengths with the shape of :math:`(batch\_size, )`.
          The type must be int32. Each value in the tensor must be equal to or less than `max_time`.

    Outputs:
        - **decoded_indices** (Tensor) - A tensor with shape of :math:`(total\_decoded\_outputs, 2)`.
          Data type is int64.
        - **decoded_values** (Tensor) - A tensor with shape of :math:`(total\_decoded\_outputs, )`,
          it stores the decoded classes. Data type is int64.
        - **decoded_shape** (Tensor) - A tensor with shape of :math:`(batch\_size, max\_decoded\_length)`.
          Data type is int64.
        - **log_probability** (Tensor) - A tensor with shape of :math:`(batch\_size, 1)`,
          containing sequence log-probability, has the same type as `inputs`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> inputs = Tensor(np.array([[[0.6, 0.4, 0.2], [0.8, 0.6, 0.3]],
        ...                           [[0.0, 0.6, 0.0], [0.5, 0.4, 0.5]]]), mindspore.float32)
        >>> sequence_length = Tensor(np.array([2, 2]), mindspore.int32)
        >>> decoded_indices, decoded_values, decoded_shape, log_probability = ops.CTCGreedyDecoder()(inputs,
        ...                                                                                          sequence_length)
        >>> print(decoded_indices)
        [[0 0]
         [0 1]
         [1 0]]
        >>> print(decoded_values)
        [0 1 0]
        >>> print(decoded_shape)
        [2 2]
        >>> print(log_probability)
        [[-1.2]
         [-1.3]]
    """

    @prim_attr_register
    def __init__(self, merge_repeated=True):
        """Initialize CTCGreedyDecoder."""
        self.merge_repeated = validator.check_value_type("merge_repeated", merge_repeated, [bool], self.name)


class BasicLSTMCell(PrimitiveWithInfer):
    """
    It's similar to operator :class:`mindspore.ops.DynamicRNN`. BasicLSTMCell will be deprecated in the future.
    Please use DynamicRNN instead.

    Supported Platforms:
        Deprecated
    """

    @prim_attr_register
    def __init__(self, keep_prob=1.0, forget_bias=1.0, state_is_tuple=True, activation='tanh'):
        """Initialize BasicLSTMCell."""
        self.keep_prob = validator.check_value_type("keep_prob", keep_prob, [float], self.name)
        self.keep_prob = validator.check_float_range(keep_prob, 0.0, 1.0, validator.INC_BOTH, "keep_prob", self.name)
        self.forget_bias = validator.check_value_type("forget_bias", forget_bias, [float], self.name)
        self.state_is_tuple = validator.check_value_type("state_is_tuple", state_is_tuple, [bool], self.name)
        self.activation = validator.check_string(activation, ['tanh'], "activation", self.name)

    def infer_shape(self, x_shape, h_shape, c_shape, w_shape, b_shape):
        validator.check_int(len(x_shape), 2, validator.EQ, "x rank", self.name)
        validator.check_int(len(h_shape), 2, validator.EQ, "h rank", self.name)
        validator.check_int(len(c_shape), 2, validator.EQ, "c rank", self.name)
        validator.check_int(len(w_shape), 2, validator.EQ, "w rank", self.name)
        validator.check_int(len(b_shape), 1, validator.EQ, "b rank", self.name)
        validator.check("x_shape[0]", x_shape[0], "h_shape[0]", h_shape[0], validator.EQ, self.name)
        validator.check("c_shape[0]", c_shape[0], "h_shape[0]", h_shape[0], validator.EQ, self.name)
        validator.check("c_shape[1]", c_shape[1], "h_shape[1]", h_shape[1], validator.EQ, self.name)
        validator.check("w_shape[1]", w_shape[1], "4*h_shape[1]", 4 * h_shape[1], validator.EQ, self.name)
        validator.check("w_shape[0]", w_shape[0], "x_shape[1]+h_shape[1]", x_shape[1] + h_shape[1],
                        validator.EQ, self.name)
        validator.check("b_shape[0]", b_shape[0], "4*h_shape[1]", 4 * h_shape[1], validator.EQ, self.name)
        ct_shape = c_shape
        ht_shape = c_shape
        it_shape = c_shape
        jt_shape = c_shape
        ft_shape = c_shape
        ot_shape = c_shape
        tanhct_shape = c_shape

        return ct_shape, ht_shape, it_shape, jt_shape, ft_shape, ot_shape, tanhct_shape

    def infer_dtype(self, x_dtype, h_dtype, c_dtype, w_dtype, b_dtype):
        tuple(map(partial(validator.check_tensor_dtype_valid,
                          valid_dtypes=(mstype.float16, mstype.float32), prim_name=self.name),
                  ("x_dtype", "h_dtype", "w_dtype"),
                  (x_dtype, h_dtype, w_dtype)))
        args = {"c_dtype": c_dtype, "b_dtype": b_dtype}
        validator.check_tensors_dtypes_same_and_valid(args, [mstype.float16, mstype.float32], self.name)
        return c_dtype, mstype.float16, c_dtype, c_dtype, c_dtype, c_dtype, c_dtype


class DynamicRNN(Primitive):
    r"""
    Applies a recurrent neural network to the input.
    Only long short-term memory (LSTM) is supported currently.

    .. math::
        \begin{array}{ll} \\
            i_{t+1} = \sigma(W_{ix} x_{t+1} + b_{ix} + W_{ih} h_{(t)} + b_{ih}) \\
            f_{t+1} = \sigma(W_{fx} x_{t+1} + b_{fx} + W_{fh} h_{(t)} + b_{fh}) \\
            \tilde{c}_{t+1} = \tanh(W_{cx} x_{t+1} + b_{cx} + W_{ch} h_{(t)} + b_{ch}) \\
            o_{t+1} = \sigma(W_{ox} x_{t+1} + b_{ox} + W_{oh} h_{(t)} + b_{oh}) \\
            c_{t+1} = f_{t+1} * c_{(t)} + i_t * \tilde{c}_{t+1} \\
            h_{t+1} = o_{t+1} * \tanh(c_{t+1}) \\
        \end{array}

    :math:`h_{t+1}` is the hidden state at time `t+1`. :math:`x_{t+1}` is the input
    at time `t+1`. :math:`h_{t}` is the hidden state of the layer
    at time `t` or the initial hidden state at time `0`.
    :math:`\sigma` is the sigmoid function, and :math:`*` is the Hadamard product. :math:`W, b`
    are learnable weights between the output and the input in the formula. For instance,
    :math:`W_{ix}, b_{ix}` are the weight and bias used to transform from input :math:`x` to :math:`i`.

    Args:
        cell_type (str, optional): A string identifying the cell type in the operator. Default: ``'LSTM'`` .
            Only 'LSTM' is currently supported.
        direction (str, optional): A string identifying the direction in the operator. Default: ``'UNIDIRECTIONAL'`` .
            Only 'UNIDIRECTIONAL' is currently supported.
        cell_depth (int, optional): An integer identifying the cell depth in the operator. Default: ``1`` .
        use_peephole (bool, optional): A bool identifying if use peephole in the operator. Default: ``False`` .
        keep_prob (float, optional): A float identifying the keep prob in the operator. Default: ``1.0`` .
        cell_clip (float, optional): A float identifying the cell clip in the operator. Default: ``-1.0`` .
        num_proj (int, optional): An integer identifying the number projection in the operator. Default: ``0`` .
        time_major (bool, optional): A bool specify the data format of `x`. If it is set to ``True`` , the format is
            :math:`(num\_step, batch\_size, input\_size)`, if it is set to False, the format is
            :math:`(batch\_size, num\_step, input\_size)`.
            Default: ``True`` . Only supports ``True`` at present.
        activation (str, optional): A string identifying the type of activation function in the operator.
            Default: ``'tanh'`` . Only 'tanh' is currently supported.
        forget_bias (float, optional): A float identifying the forget bias in the operator. Default: ``0.0`` .
        is_training (bool, optional): A bool identifying is training in the operator. Default: ``True`` .

    Inputs:
        - **x** (Tensor) - Current words. Tensor of shape :math:`(num\_step, batch\_size, input\_size)`.
          The data type must be float16.
        - **w** (Tensor) - Weight. Tensor of shape :math:`(input\_size + hidden\_size, 4 * hidden\_size)`.
          The data type must be float16.
        - **b** (Tensor) - Bias. Tensor of shape :math:`(4 * hidden\_size)`.
          The data type must be float16.
        - **seq_length** (Tensor) - The length of each batch. Tensor of shape :math:`(batch\_size, )`.
          Only `None` is currently supported.
        - **init_h** (Tensor) - Hidden state of initial time. Tensor of shape :math:`(1, batch\_size, hidden\_size)`.
          The data type must be float16.
        - **init_c** (Tensor) - Cell state of initial time. Tensor of shape :math:`(1, batch\_size, hidden\_size)`.
          The data type must be float16.

    Outputs:
        - **y** (Tensor) - A Tensor of shape :math:`(num\_step, batch\_size, hidden\_size)`.
          Has the same type with input `b`.
        - **output_h** (Tensor) - A Tensor of shape :math:`(num\_step, batch\_size, hidden\_size)`.
          With data type of float16.
        - **output_c** (Tensor) - A Tensor of shape :math:`(num\_step, batch\_size, hidden\_size)`.
          Has the same type with input `b`.
        - **i** (Tensor) - A Tensor of shape :math:`(num\_step, batch\_size, hidden\_size)`.
          Has the same type with input `b`.
        - **j** (Tensor) - A Tensor of shape :math:`(num\_step, batch\_size, hidden\_size)`.
          Has the same type with input `b`.
        - **f** (Tensor) - A Tensor of shape :math:`(num\_step, batch\_size, hidden\_size)`.
          Has the same type with input `b`.
        - **o** (Tensor) - A Tensor of shape :math:`(num\_step, batch\_size, hidden\_size)`.
          Has the same type with input `b`.
        - **tanhct** (Tensor) - A Tensor of shape :math:`(num\_step, batch\_size, hidden\_size)`.
          Has the same type with input `b`.

    Raises:
        TypeError: If `cell_type`, `direction` or `activation` is not a str.
        TypeError: If `cell_depth` or `num_proj` is not an int.
        TypeError: If `keep_prob`, `cell_clip` or `forget_bias` is not a float.
        TypeError: If `use_peehpole`, `time_major` or `is_training` is not a bool.
        TypeError: If `x`, `w`, `b`, `seq_length`, `init_h` or `init_c` is not a Tensor.
        TypeError: If dtype of `x`, `w`, `init_h` or `init_c` is not float16.
        TypeError: If dtype of `b` is neither float16 nor float32.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.random.rand(2, 16, 64).astype(np.float16))
        >>> w = Tensor(np.random.rand(96, 128).astype(np.float16))
        >>> b = Tensor(np.random.rand(128).astype(np.float16))
        >>> init_h = Tensor(np.random.rand(1, 16, 32).astype(np.float16))
        >>> init_c = Tensor(np.random.rand(1, 16, 32).astype(np.float16))
        >>> dynamic_rnn = ops.DynamicRNN()
        >>> output = dynamic_rnn(x, w, b, None, init_h, init_c)
        >>> print(output[0].shape)
        (2, 16, 32)
    """

    @prim_attr_register
    def __init__(self,
                 cell_type='LSTM',
                 direction='UNIDIRECTIONAL',
                 cell_depth=1,
                 use_peephole=False,
                 keep_prob=1.0,
                 cell_clip=-1.0,
                 num_proj=0,
                 time_major=True,
                 activation='tanh',
                 forget_bias=0.0,
                 is_training=True):
        """Initialize DynamicRNN."""
        self.forget_bias = validator.check_value_type("forget_bias", forget_bias, [float], self.name)
        self.cell_depth = validator.check_value_type("cell_depth", cell_depth, [int], self.name)
        self.keep_prob = validator.check_value_type("keep_prob", keep_prob, [float], self.name)
        validator.check_number_range(keep_prob, 0.0, 1.0, validator.INC_BOTH, float, "keep_prob")
        self.cell_clip = validator.check_value_type("cell_clip", cell_clip, [float], self.name)
        self.num_proj = validator.check_non_negative_int(num_proj, "num_proj", self.name)
        self.forget_bias = validator.check_value_type("forget_bias", forget_bias, [float], self.name)
        self.use_peephole = validator.check_value_type("use_peephole", use_peephole, [bool], self.name)
        self.time_major = validator.check_value_type("time_major", time_major, [bool], self.name)
        self.is_training = validator.check_value_type("is_training", is_training, [bool], self.name)
        validator.check_value_type("cell_type", cell_type, [str], self.name)
        self.cell_type = validator.check_string(cell_type, ['LSTM'], "cell_type", self.name)
        validator.check_value_type("direction", direction, [str], self.name)
        self.direction = validator.check_string(direction, ['UNIDIRECTIONAL'], "direction", self.name)
        validator.check_value_type("activation", activation, [str], self.name)
        self.activation = validator.check_string(activation, ['tanh'], "activation", self.name)


class DynamicGRUV2(Primitive):
    r"""
    Applies a single-layer gated recurrent unit (GRU) to an input sequence.

    .. math::

        \begin{array}{ll}
            r_{t+1} = \sigma(W_{ir} x_{t+1} + b_{ir} + W_{hr} h_{(t)} + b_{hr}) \\
            z_{t+1} = \sigma(W_{iz} x_{t+1} + b_{iz} + W_{hz} h_{(t)} + b_{hz}) \\
            n_{t+1} = \tanh(W_{in} x_{t+1} + b_{in} + r_{t+1} * (W_{hn} h_{(t)}+ b_{hn})) \\
            h_{t+1} = (1 - z_{t+1}) * n_{t+1} + z_{t+1} * h_{(t)}
        \end{array}

    where :math:`h_{t+1}` is the hidden state at time `t+1`, :math:`x_{t+1}` is the input
    at time `t+1`, :math:`h_{t}` is the hidden state of the layer
    at time `t` or the initial hidden state at time `0`. :math:`r_{t+1}`,
    :math:`z_{t+1}`, :math:`n_{t+1}` are the reset, update, and new gates, respectively.
    :math:`W`, :math:`b` are the weight parameter and the deviation parameter respectively.
    :math:`\sigma` is the sigmoid function, and :math:`*` is the Hadamard product.

    Args:
        direction (str, optional): A string identifying the direction in the operator. Default: ``'UNIDIRECTIONAL'`` .
            Only ``'UNIDIRECTIONAL'`` is currently supported.
        cell_depth (int, optional): An integer identifying the cell depth in the operator. Default: ``1`` .
        keep_prob (float, optional): A float identifying the keep prob in the operator. Default: ``1.0`` .
        cell_clip (float, optional): A float identifying the cell clip in the operator. Default: ``-1.0`` .
        num_proj (int, optional): An integer identifying the number projection in the operator. Default: ``0`` .
        time_major (bool, optional): A bool identifying the time major in the operator. Default: ``True`` .
        activation (str, optional) : A string identifying the type of activation function in the operator.
            Default: ``'tanh'`` . Only ``'tanh'`` is currently supported.
        gate_order (str, optional): A string identifying the gate order in weight and bias. Default: ``'rzh'`` .
            ``'zrh'`` is another option. Here, ``'rzh'`` means the gate order is: reset gate, update gate, hidden gate.
            ``'zrh'`` means the gate order is: update gate, reset gate, hidden gate.
        reset_after (bool, optional): A bool identifying whether to apply reset gate after matrix multiplication.
            Default: ``True`` .
        is_training (bool, optional): A bool identifying is training in the operator. Default: ``True`` .

    Inputs:
        - **x** (Tensor) - Current words.
          Tensor of shape :math:`(\text{num_step}, \text{batch_size}, \text{input_size})`.
          The data type must be float16.
        - **weight_input** (Tensor) - Input-hidden weight :math:`W_{\{ir,iz,in\}}`.
          Tensor of shape :math:`(\text{input_size}, 3 \times \text{hidden_size})`.
          The data type must be float16.
        - **weight_hidden** (Tensor) - Hidden-hidden weight :math:`W_{\{hr,hz,hn\}}`.
          Tensor of shape :math:`(\text{hidden_size}, 3 \times \text{hidden_size})`.
          The data type must be float16.
        - **bias_input** (Tensor) - Input-hidden bias :math:`b_{\{ir,iz,in\}}`.
          Tensor of shape :math:`(3 \times \text{hidden_size})`, or None.
          Has the same data type with input `init_h`.
        - **bias_hidden** (Tensor) - Hidden-hidden bias :math:`b_{\{hr,hz,hn\}}`.
          Tensor of shape :math:`(3 \times \text{hidden_size})`,
          or None. Has the same data type with input `init_h`.
        - **seq_length** (Tensor) - The length of each batch. Tensor of shape :math:`(\text{batch_size})`.
          Only `None` is currently supported.
        - **init_h** (Tensor) - Hidden state of initial time.
          Tensor of shape :math:`(\text{batch_size}, \text{hidden_size})`.
          The data type must be float16 or float32.

    Outputs:
        - **y** (Tensor) - A Tensor of shape:

          - y_shape = :math:`(num\_step, batch\_size, min(hidden\_size, num\_proj))`: `If num_proj > 0`,
          - y_shape = :math:`(num\_step, batch\_size, hidden\_size)`: `If num_proj = 0`.

          Has the same data type with input `bias_type`.
        - **output_h** (Tensor) - A Tensor of shape :math:`(\text{num_step}, \text{batch_size}, \text{hidden_size})`.
          Has the same data type with input `bias_type`.
        - **update** (Tensor) - A Tensor of shape :math:`(\text{num_step}, \text{batch_size}, \text{hidden_size})`.
          Has the same data type with input `bias_type`.
        - **reset** (Tensor) - A Tensor of shape :math:`(\text{num_step}, \text{batch_size}, \text{hidden_size})`.
          Has the same data type with input `bias_type`.
        - **new** (Tensor) - A Tensor of shape :math:`(\text{num_step}, \text{batch_size}, \text{hidden_size})`.
          Has the same data type with input `bias_type`.
        - **hidden_new** (Tensor) - A Tensor of shape :math:`(\text{num_step}, \text{batch_size}, \text{hidden_size})`.
          Has the same data type with input `bias_type`.

        A note about the bias_type:

        - If `bias_input` and `bias_hidden` both are `None`, `bias_type` is the data type of `init_h`.
        - If `bias_input` is not `None`, `bias_type` is the data type of `bias_input`.
        - If `bias_input` is `None` and `bias_hidden` is not `None`, `bias_type` is the data type of `bias_hidden`.

    Raises:
        TypeError: If `direction`, `activation` or `gate_order` is not a str.
        TypeError: If `cell_depth` or `num_proj` is not an int.
        TypeError: If `keep_prob` or `cell_clip` is not a float.
        TypeError: If `time_major`, `reset_after` or `is_training` is not a bool.
        TypeError: If `x`, `weight_input`, `weight_hidden`, `bias_input`, `bias_hidden`, `seq_length` or `ini_h` is not
                   a Tensor.
        TypeError: If dtype of `x`, `weight_input` or `weight_hidden` is not float16.
        TypeError: If dtype of `init_h` is neither float16 nor float32.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.random.rand(2, 8, 64).astype(np.float16))
        >>> weight_i = Tensor(np.random.rand(64, 48).astype(np.float16))
        >>> weight_h = Tensor(np.random.rand(16, 48).astype(np.float16))
        >>> bias_i = Tensor(np.random.rand(48).astype(np.float16))
        >>> bias_h = Tensor(np.random.rand(48).astype(np.float16))
        >>> init_h = Tensor(np.random.rand(8, 16).astype(np.float16))
        >>> dynamic_gru_v2 = ops.DynamicGRUV2()
        >>> output = dynamic_gru_v2(x, weight_i, weight_h, bias_i, bias_h, None, init_h)
        >>> print(output[0].shape)
        (2, 8, 16)
    """

    @prim_attr_register
    def __init__(self,
                 direction='UNIDIRECTIONAL',
                 cell_depth=1,
                 keep_prob=1.0,
                 cell_clip=-1.0,
                 num_proj=0,
                 time_major=True,
                 activation="tanh",
                 gate_order="rzh",
                 reset_after=True,
                 is_training=True):
        """Initialize DynamicGRUV2."""
        self.cell_depth = validator.check_value_type("cell_depth", cell_depth, [int], self.name)
        self.keep_prob = validator.check_value_type("keep_prob", keep_prob, [float], self.name)
        self.cell_clip = validator.check_value_type("cell_clip", cell_clip, [float], self.name)
        self.num_proj = validator.check_non_negative_int(num_proj, "num_proj", self.name)
        self.time_major = validator.check_value_type("time_major", time_major, [bool], self.name)
        self.is_training = validator.check_value_type("is_training", is_training, [bool], self.name)
        self.direction = validator.check_string(direction, ['UNIDIRECTIONAL'], "direction", self.name)
        self.activation = validator.check_string(activation, ['tanh'], "activation", self.name)
        self.gate_order = validator.check_string(gate_order, ['zrh', 'rzh'], "gate_order", self.name)
        self.reset_after = validator.check_value_type("reset_after", reset_after, [bool], self.name)
        self.init_prim_io_names(
            inputs=[
                "x", "weight_input", "weight_hidden", "bias_input",
                "bias_hidden", "seq_length", "init_h"
            ],
            outputs=["y", "output_h", "update", "reset", "new", "hidden_new"])


class InTopK(Primitive):
    r"""
    Determines whether the targets are in the top `k` predictions.

    Refer to :func:`mindspore.ops.intopk` for more details.

    Args:
        k (int): Specifies the number of top elements to be used for computing precision along the last dimension.

    Inputs:
        - **x1** (Tensor) - A 2D Tensor defines the predictions of a batch of samples with float16 or float32
          data type.
        - **x2** (Tensor) - A 1D Tensor defines the labels of a batch of samples with int32 data type. The size of `x2`
          must be equal to the first dimension of `x1`. The values of `x2` can not be negative and
          must be equal to or less than index of x1's second dimension.

    Outputs:
        Tensor has 1 dimension of type bool and the same shape with `x2`. For labeling sample `i` in `x2`,
        if the label in the first `k` predictions for sample `i` is in `x1`, then the value is ``True`` ,
        otherwise ``False`` .

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x1 = Tensor(np.array([[1, 8, 5, 2, 7], [4, 9, 1, 3, 5]]), mindspore.float32)
        >>> x2 = Tensor(np.array([1, 3]), mindspore.int32)
        >>> in_top_k = ops.InTopK(3)
        >>> output = in_top_k(x1, x2)
        >>> print(output)
        [ True  False]
    """

    @prim_attr_register
    def __init__(self, k):
        """Initialize InTopK"""
        self.init_prim_io_names(inputs=['x1', 'x2', 'k'], outputs=['y'])
        validator.check_value_type("k", k, [int], self.name)


class LRN(Primitive):
    r"""
    Local Response Normalization.

    .. warning::
        LRN is deprecated on Ascend due to potential accuracy problem. It's recommended to use other
        normalization methods, e.g. :class:`mindspore.ops.BatchNorm`.

    .. math::

        b_{c} = a_{c}\left(k + \frac{\alpha}{n}
        \sum_{c'=\max(0, c-n/2)}^{\min(N-1,c+n/2)}a_{c'}^2\right)^{-\beta}

    where the :math:`a_{c}` indicates the specific value of the pixel corresponding to :math:`c` in feature map;
    where the :math:`n/2` indicates the `depth_radius`; where the :math:`k` indicates the `bias`;
    where the :math:`\alpha` indicates the `alpha`; where the :math:`\beta` indicates the `beta`.

    Args:
        depth_radius (int): Half-width of the 1-D normalization window with the shape of 0-D. Default: ``5`` .
        bias (float): An offset (usually positive to avoid dividing by 0). Default: ``1.0`` .
        alpha (float): A scale factor, usually positive. Default: ``1.0`` .
        beta (float): An exponent. Default: ``0.5`` .
        norm_region (str): Specifies normalization region. Options: ``"ACROSS_CHANNELS"`` .
            Default: ``"ACROSS_CHANNELS"`` .

    Inputs:
        - **x** (Tensor) - A 4-D Tensor with float16 or float32 data type.

    Outputs:
        Tensor, with the same shape and data type as `x`.

    Raises:
        TypeError: If `depth_radius` is not an int.
        TypeError: If `bias`, `alpha` or `beta` is not a float.
        TypeError: If `norm_region` is not a str.
        TypeError: If `x` is not a Tensor.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([[[[0.1], [0.2]],
        ...                       [[0.3], [0.4]]]]), mindspore.float32)
        >>> lrn = ops.LRN()
        >>> output = lrn(x)
        >>> print(output)
        [[[[0.09534626]
           [0.1825742 ]]
          [[0.2860388 ]
           [0.3651484 ]]]]
    """

    @prim_attr_register
    def __init__(self, depth_radius=5, bias=1.0, alpha=1.0, beta=0.5, norm_region="ACROSS_CHANNELS"):
        """Initialize LRN"""
        super().__init__("LRN")
        self.init_prim_io_names(inputs=['x'], outputs=['y'])
        validator.check_value_type("depth_radius", depth_radius, [int], self.name)
        validator.check_value_type("bias", bias, [float], self.name)
        validator.check_value_type("alpha", alpha, [float], self.name)
        validator.check_value_type("beta", beta, [float], self.name)
        validator.check_value_type("norm_region", norm_region, [str], self.name)
        validator.check_string(norm_region, ['ACROSS_CHANNELS'], 'norm_region', self.name)
        validator.check_non_negative_int(depth_radius, "depth_radius", self.name)


class AvgPool3D(Primitive):
    r"""
    3D Average pooling operation.

    Typically the input is of shape :math:`(N, C, D_{in}, H_{in}, W_{in})`, AvgPool3D outputs
    regional average in the :math:`(D_{in}, H_{in}, W_{in})`-dimension. Given kernel size
    :math:`ks = (d_{ker}, h_{ker}, w_{ker})` and stride :math:`s = (s_0, s_1, s_2)`, the operation is as follows.

    .. warning::
        "kernel_size" is in the range [1, 255]. "strides" is in the range [1, 63].

    .. math::
        \text{output}(N_i, C_j, d, h, w) =
        \frac{1}{d_{ker} * h_{ker} * w_{ker}} \sum_{l=0}^{d_{ker}-1} \sum_{m=0}^{h_{ker}-1} \sum_{n=0}^{w_{ker}-1}
        \text{input}(N_i, C_j, s_0 \times d + l, s_1 \times h + m, s_2 \times w + n)

    Args:
        kernel_size (Union[int, tuple[int]]): The size of kernel used to take the average value,
            is an int number that represents depth, height and width are both kernel_size, or a tuple
            of three int numbers that represent depth, height and width respectively. Default: ``1`` .
        strides (Union[int, tuple[int]]): The distance of kernel moving, an int number that represents
            the depth, height and width of movement are both strides, or a tuple of three int numbers that
            represent depth, height and width of movement respectively. Default: ``1`` .
        pad_mode (str, optional): Specifies the padding mode with a padding value of 0. It can be set to:
            ``"same"`` , ``"valid"`` or ``"pad"`` . Default: ``"valid"`` .

            - ``"same"``: Pad the input around its depth/height/width dimension so that the shape of input and output
              are the same when `stride` is set to ``1``.
              The amount of padding to is calculated by the operator internally.  If the amount is even,
              it isuniformly distributed around the input, if it is odd, the excess amount goes
              to the front/right/bottom side.
              If this mode is set, `pad` must be 0.
            - ``"valid"``: No padding is applied to the input, and the output returns the maximum
              possible depth, height and width. Extra pixels that could not complete a full stride will
              be discarded. If this mode is set, `pad` must be 0.
            - ``"pad"``: Pad the input with a specified amount. In this mode, the amount of padding
              in the depth, height and width dimension is determined by the `pad` parameter.
              If this mode is set, `pad` must be greater than or equal to 0.

        pad (Union(int, tuple[int], list[int])): The pad value to be filled. Default: ``0`` . If `pad` is an integer,
            the paddings of head, tail, top, bottom, left and right are the same, equal to pad.
            If `pad` is a tuple of six integers, the padding of head, tail, top, bottom, left and right equal to
            pad[0], pad[1], pad[2], pad[3], pad[4] and pad[5] correspondingly.
        ceil_mode (bool): If ``True`` , ceil instead of floor to compute the output shape. Default: ``False`` .
        count_include_pad (bool): If ``True`` , averaging calculation will include the zero-padding.
            Default: ``True`` .
        divisor_override (int): If specified, it will be used as divisor in the averaging calculation,
            otherwise kernel_size will be used. Default: ``0`` .
        data_format (str) : The optional value for data format. Currently only support ``'NCDHW'`` .
            Default: ``'NCDHW'`` .

    Inputs:
        - **x** (Tensor) - Tensor of shape :math:`(N, C, D_{in}, H_{in}, W_{in})`.
          Currently support float16, float32 and float64 data type.

    Outputs:
        Tensor, with shape :math:`(N, C, D_{out}, H_{out}, W_{out})`. Has the same data type with `x`.

    Raises:
        TypeError: If `kernel_size`, `strides` or `pad` is neither an int not a tuple.
        TypeError: If `ceil_mode` or `count_include_pad` is not a bool.
        TypeError: If `pad_mode` or `data_format` is not a string.
        TypeError: If `divisor_override` is not an int.
        ValueError: If numbers in `kernel_size` or `strides` are not positive.
        ValueError: If `kernel_size` or `strides` is a tuple whose length is not equal to 3.
        ValueError: If `pad_mode` is not one of 'same', 'valid' or 'pad'.
        ValueError: If `pad` is a tuple whose length is not equal to 6.
        ValueError: If element of `pad` is less than 0.
        ValueError: If `pad_mode` is not equal to 'pad' and `pad` is not equal to 0 or (0, 0, 0, 0, 0, 0).
        ValueError: If `data_format` is not 'NCDHW'.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> import numpy as np
        >>> x = Tensor(np.arange(1 * 2 * 2 * 2 * 3).reshape((1, 2, 2, 2, 3)), mindspore.float16)
        >>> avg_pool3d = ops.AvgPool3D(kernel_size=2, strides=1, pad_mode="valid")
        >>> output = avg_pool3d(x)
        >>> print(output)
        [[[[[ 5.  6.]]]
          [[[17. 18.]]]]]
    """

    @prim_attr_register
    def __init__(self, kernel_size=1, strides=1, pad_mode="valid", pad=0, ceil_mode=False,
                 count_include_pad=True, divisor_override=0, data_format="NCDHW"):
        """Initialize AvgPool3D"""
        self.init_prim_io_names(inputs=['input'], outputs=['output'])
        self.kernel_size = _check_3d_int_or_tuple('kernel_size', kernel_size, self.name, ret_five=True)
        self.add_prim_attr('kernel_size', self.kernel_size)
        self.strides = _check_3d_int_or_tuple('strides', strides, self.name, ret_five=True)
        self.add_prim_attr('strides', self.strides)
        validator.check_value_type('pad', pad, (int, tuple, list), self.name)
        if isinstance(pad, int):
            pad = (pad,) * 6
        if len(pad) != 6:
            raise ValueError(f"For '{self.name}', attr 'pad' must be an positive int number or a tuple of "
                             f"six positive int numbers, but got {self.pad}.")
        self.pad_list = pad
        self.add_prim_attr('pad_list', self.pad_list)
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        self.pad_mode = validator.check_string(pad_mode.upper(), ['VALID', 'SAME', 'PAD'], 'pad_mode', self.name)
        self.add_prim_attr('pad_mode', self.pad_mode)

        if self.pad_mode != 'PAD' and pad != (0, 0, 0, 0, 0, 0):
            raise ValueError(f"For '{self.name}', the 'pad' must be zero or (0, 0, 0, 0, 0, 0) when 'pad_mode' "
                             f"is not \"PAD\", but got 'pad' is {self.pad} and 'pad_mode' is {pad_mode}.")
        if self.pad_mode == 'PAD':
            for item in pad:
                validator.check_non_negative_int(item, 'pad or item of pad', self.name)
        self.ceil_mode = validator.check_value_type('ceil_mode', ceil_mode, bool, self.name)
        self.count_include_pad = validator.check_value_type('count_include_pad', count_include_pad, bool, self.name)
        self.divisor_override = validator.check_non_negative_int(divisor_override, 'divisor_override', self.name)
        self.format = validator.check_string(data_format, ['NCDHW'], 'format', self.name)


class Conv3D(Primitive):
    r"""
    3D convolution layer.

    Applies a 3D convolution over an input tensor which is typically of shape
    :math:`(N, C_{in}, D_{in}, H_{in}, W_{in})`,
    where :math:`N` is batch size, :math:`C` is channel number,
    :math:`D, H, W`
    are the depth, height and width of the feature map, respectively.

    The output is calculated based on formula:

    .. math::

        \text{out}(N_i, C_{\text{out}_j}) = \text{bias}(C_{\text{out}_j}) +
        \sum_{k = 0}^{C_{in} - 1} \text{ccor}({\text{weight}(C_{\text{out}_j}, k), \text{X}(N_i, k)})

    where :math:`bias` is the output channel bias, :math:`ccor` is
    the `cross-correlation <https://en.wikipedia.org/wiki/Cross-correlation>`_,
    :math:`weight` is the convolution kernel value and :math:`X` represents the input feature map.

    Here are the indices' meanings:

    - :math:`i` corresponds to the batch number, the range is :math:`[0, N-1]`,
      where :math:`N` is the batch size of the input.

    - :math:`j` corresponds to the output channel, the range is :math:`[0, C_{out}-1]`,
      where :math:`C_{out}` is the number of
      output channels, which is also equal to the number of kernels.

    - :math:`k` corresponds to the input channel, the range is :math:`[0, C_{in}-1]`,
      where :math:`C_{in}` is the number of
      input channels, which is also equal to the number of channels in the convolutional kernels.

    Therefore, in the above formula, :math:`{bias}(C_{\text{out}_j})` represents the bias of the :math:`j`-th
    output channel, :math:`{weight}(C_{\text{out}_j}, k)`represents the slice of the :math:`j`-th convolutional
    kernel in the :math:`k`-th channel, and :math:`{X}(N_i, k)` represents the slice of the :math:`k`-th input
    channel in the :math:`i`-th batch of the input feature map.

    The shape of the convolutional kernel is given by
    :math:`(\text{kernel_size[0]}, \text{kernel_size[1]}, \text{kernel_size[2]})`
    where :math:`\text{kernel_size[0]}` ,
    :math:`\text{kernel_size[1]}` and :math:`\text{kernel_size[2]}` are the depth,
    height and width of the kernel, respectively.
    If we consider the input and output channels as well as the `group` parameter, the complete kernel shape
    will be :math:`(C_{out}, C_{in} / \text{group}, \text{kernel_size[0]},
    \text{kernel_size[1]}, \text{kernel_size[2]})`,
    where `group` is the number of groups dividing `x`'s input channel when applying group convolution.

    For more details about convolution layer, please refer to `Gradient Based Learning Applied to Document Recognition
    <http://vision.stanford.edu/cs598_spring07/papers/Lecun98.pdf>`_.

    Note:
        1. On Ascend platform, :math:`groups=1` must be satisfied.
        2. On Ascend :math:`dilation` on depth only supports the case of 1.

    Args:
        out_channel (int): Specifies output channel :math:`C_{out}`.
        kernel_size (Union[int, tuple[int]]): Specifies the depth, height and width of the 3D convolution kernel.
            It can be a single int or a tuple of 3 integers. A single int means the value is for depth, height
            and the width. A tuple of 3 ints means the first value is for depth and
            the rest is for the height and width.
        mode (int, optional): Modes for different convolutions. It is currently not used. Default: ``1`` .
        stride (Union[int, tuple[int]], optional): The distance of kernel moving, it can be an int number
            that represents the depth, height and width of movement or a tuple of three int numbers that
            represent depth, height and width movement respectively. Default: ``1`` .
        pad_mode (str, optional): Specifies the padding mode with a padding value of 0. It can be set to:
            ``"same"`` , ``"valid"`` or ``"pad"`` . Default: ``"valid"`` .

            - ``"same"``: Pad the input around its depth/height/width dimension so that the shape of input and output
              are the same when `stride` is set to ``1``.
              The amount of padding to is calculated by the operator internally.  If the amount is even,
              it isuniformly distributed around the input, if it is odd, the excess amount goes
              to the front/right/bottom side.
              If this mode is set, `pad` must be 0.
            - ``"valid"``: No padding is applied to the input, and the output returns the maximum
              possible depth, height and width. Extra pixels that could not complete a full stride will
              be discarded. If this mode is set, `pad` must be 0.
            - ``"pad"``: Pad the input with a specified amount. In this mode, the amount of padding
              in the depth, height and width dimension is determined by the `pad` parameter.
              If this mode is set, `pad` must be greater than or equal to 0.

        pad (Union(int, tuple[int]), optional): Specifies the amount of padding to apply on input
            when `pad_mode` is set to ``"pad"``. It can be a single int or a tuple of 6 ints.
            If `pad` is one integer, the paddings of head, tail, top, bottom,
            left and right are the same, equal to `pad`. If `pad` is a tuple with 6 integers, the
            paddings of head, tail, top, bottom, left and right is equal to pad[0],
            pad[1], pad[2], pad[3], pad[4] and pad[5] accordingly. Default: ``0`` .
        dilation (Union[int, tuple[int]], optional): Specifies the dilation rate to use for dilated convolution.
            It can be a single int or a tuple of 3 integers. A single int means the dilation size is the same
            in the depth, height and width directions. A tuple of 3 ints represents the dilation size in
            the depth, height and width directions, respectively.
            Assuming :math:`dilation=(d0, d1, d2)`, the convolutional kernel samples the input with a
            spacing of :math:`d0-1` elements in the depth direction,
            :math:`d1-1` elements in the height direction, :math:`d2-1` elements in the
            width direction respectively. The values in the depth, height and width dimensions are in the
            ranges [1, D], [1, H] and [1, W], respectively.
            Default: ``1`` .
        group (int, optional): The number of groups into which the filter is divided. `in_channels`
            and `out_channels` must be divisible by `group`. Default: ``1`` .
        data_format (str, optional): The optional value for data format. Currently only support ``"NCDHW"`` .

    Inputs:
        - **x** (Tensor) - Tensor of shape :math:`(N, C_{in}, D_{in}, H_{in}, W_{in})`.
          Currently input data type only support float16 and float32.
        - **weight** (Tensor) - Set size of kernel is :math:`(k_d, K_h, K_w)`, then the shape is
          :math:`(C_{out}, C_{in}/groups, k_d, K_h, K_w)`.
          Currently weight data type only support float16 and float32.
        - **bias** (Tensor) - Tensor of shape :math:`(C_{out})`. When bias is None, zeros will be used.
          Default: ``None`` .

    Outputs:
        Tensor, the value that applied 3D convolution. The shape is :math:`(N, C_{out}, D_{out}, H_{out}, W_{out})`.

        `pad_mode` is ``"same"``:

        .. math::
            \begin{array}{ll} \\
                D_{out} = \left \lceil{\frac{D_{in}}{\text{stride[0]}}} \right \rceil \\
                H_{out} = \left \lceil{\frac{H_{in}}{\text{stride[1]}}} \right \rceil \\
                W_{out} = \left \lceil{\frac{W_{in}}{\text{stride[2]}}} \right \rceil \\
            \end{array}

        `pad_mode` is ``"valid"``:

        .. math::
            \begin{array}{ll} \\
                D_{out} = \left \lfloor{\frac{D_{in} - \text{dilation[0]} \times (\text{kernel_size[0]} - 1) }
                {\text{stride[0]}} + 1} \right \rfloor \\
                H_{out} = \left \lfloor{\frac{H_{in} - \text{dilation[1]} \times (\text{kernel_size[1]} - 1) }
                {\text{stride[1]}} + 1} \right \rfloor \\
                W_{out} = \left \lfloor{\frac{W_{in} - \text{dilation[2]} \times (\text{kernel_size[2]} - 1) }
                {\text{stride[2]}} + 1} \right \rfloor \\
            \end{array}

        `pad_mode` is ``"pad"``:

        .. math::
            \begin{array}{ll} \\
                D_{out} = \left \lfloor{\frac{D_{in} + pad[0] + pad[1] - (\text{dilation[0]} - 1) \times
                \text{kernel_size[0]} - 1 }{\text{stride[0]}} + 1} \right \rfloor \\
                H_{out} = \left \lfloor{\frac{H_{in} + pad[2] + pad[3] - (\text{dilation[1]} - 1) \times
                \text{kernel_size[1]} - 1 }{\text{stride[1]}} + 1} \right \rfloor \\
                W_{out} = \left \lfloor{\frac{W_{in} + pad[4] + pad[5] - (\text{dilation[2]} - 1) \times
                \text{kernel_size[2]} - 1 }{\text{stride[2]}} + 1} \right \rfloor \\
            \end{array}

    Raises:
        TypeError: If `out_channel` or `group` is not an int.
        TypeError: If `kernel_size`, `stride`, `pad` or `dilation` is neither an int nor a tuple.
        ValueError: If `out_channel`, `kernel_size`, `stride` or `dilation` is less than 1.
        ValueError: If `pad` is less than 0.
        ValueError: If `pad_mode` is not one of 'same', 'valid' or 'pad'.
        ValueError: If `pad` is a tuple whose length is not equal to 6.
        ValueError: If `pad_mode` is not equal to 'pad' and `pad` is not equal to (0, 0, 0, 0, 0, 0).
        ValueError: If `data_format` is not 'NCDHW'.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> # case 1: specify kernel_size with tuple, all parameters use default values.
        >>> x = Tensor(np.ones([16, 3, 10, 32, 32]), mindspore.float16)
        >>> weight = Tensor(np.ones([32, 3, 4, 3, 3]), mindspore.float16)
        >>> conv3d = ops.Conv3D(out_channel=32, kernel_size=(4, 3, 3))
        >>> output = conv3d(x, weight)
        >>> print(output.shape)
        (16, 32, 7, 30, 30)
        >>> # case 2: specify kernel_size with int, all parameters use default values.
        >>> x = Tensor(np.ones([10, 20, 32, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([40, 20, 3, 3, 3]), mindspore.float32)
        >>> conv3d = ops.Conv3D(out_channel=40, kernel_size=3)
        >>> output = conv3d(x, weight)
        >>> print(output.shape)
        (10, 40, 30, 30, 30)
         >>> # case 3: stride=(1, 2, 3), other parameters being default.
        >>> x = Tensor(np.ones([10, 20, 32, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([40, 20, 3, 3, 3]), mindspore.float32)
        >>> conv3d = ops.Conv3D(out_channel=40, kernel_size=3, stride=(1, 2, 3))
        >>> output = conv3d(x, weight)
        >>> print(output.shape)
        (10, 40, 30, 15, 10)
         >>> # case 4: pad_mode="pad", other parameters being default.
        >>> x = Tensor(np.ones([10, 20, 32, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([40, 20, 3, 3, 3]), mindspore.float32)
        >>> conv3d = ops.Conv3D(out_channel=40, kernel_size=3, pad_mode="pad", pad=2)
        >>> output = conv3d(x, weight)
        >>> print(output.shape)
        (10, 40, 34, 34, 34)
         >>> # case 5: dilation=(1, 1, 1), other parameters being default.
        >>> x = Tensor(np.ones([10, 20, 32, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([40, 20, 3, 3, 3]), mindspore.float32)
        >>> conv3d = ops.Conv3D(out_channel=40, kernel_size=3, dilation=(1, 1, 1))
        >>> output = conv3d(x, weight)
        >>> print(output.shape)
        (10, 40, 30, 30, 30)
        >>> # case 6: group=1, other parameters being default.
        >>> x = Tensor(np.ones([10, 20, 32, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([40, 20, 3, 3, 3]), mindspore.float32)
        >>> conv3d = ops.Conv3D(out_channel=40, kernel_size=3, group=1)
        >>> output = conv3d(x, weight)
        >>> print(output.shape)
        (10, 40, 30, 30, 30)
        >>> # case 7: All parameters are specified.
        >>> x = Tensor(np.ones([10, 20, 32, 32, 32]), mindspore.float32)
        >>> weight = Tensor(np.ones([40, 20, 3, 3, 3]), mindspore.float32)
        >>> conv3d = ops.Conv3D(out_channel=40, kernel_size=3, stride=(1, 2, 3), pad_mode="pad",
        ...                     pad=2, dilation=(1), group=1)
        >>> output = conv3d(x, weight)
        >>> print(output.shape)
        (10, 40, 34, 17, 12)
    """

    @prim_attr_register
    def __init__(self,
                 out_channel,
                 kernel_size,
                 mode=1,
                 pad_mode="valid",
                 pad=0,
                 stride=1,
                 dilation=1,
                 group=1,
                 data_format="NCDHW"):
        """Initialize Conv3D"""
        self.init_prim_io_names(inputs=['x', 'w'], outputs=['output'])
        self.kernel_size = _check_3d_int_or_tuple('kernel_size', kernel_size, self.name)
        if isinstance(kernel_size, int):
            self.kernel_size = (kernel_size,) * 3
        self.add_prim_attr('kernel_size', self.kernel_size)
        self.stride = _check_3d_int_or_tuple('stride', stride, self.name, allow_five=False, ret_five=True)
        self.add_prim_attr('strides', self.stride)
        target = context.get_context("device_target")
        if target.lower() == "ascend":
            self.dilation = _check_3d_int_or_tuple('dilation', dilation, self.name, allow_five=False,
                                                   ret_five=True, third_one=True)
        else:
            self.dilation = _check_3d_int_or_tuple('dilation', dilation, self.name, allow_five=False,
                                                   ret_five=True, third_one=False)
        self.add_prim_attr('dilations', self.dilation)
        validator.check_value_type('pad', pad, (int, tuple), self.name)
        if isinstance(pad, int):
            pad = (pad,) * 6
        if len(pad) != 6:
            raise ValueError(f"For '{self.name}', attr 'pad' must be an positive int number or a tuple of "
                             f"six positive int numbers, but got {self.pad}.")
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        self.pad_mode = validator.check_string(pad_mode.lower(), ['valid', 'same', 'pad'], 'pad_mode', self.name)
        self.add_prim_attr('pad_mode', self.pad_mode)

        if self.pad_mode != 'pad' and pad != (0, 0, 0, 0, 0, 0):
            raise ValueError(f"For '{self.name}', the 'pad' must be zero or (0, 0, 0, 0, 0, 0) when 'pad_mode' "
                             f"is not \"pad\", but got 'pad' is {self.pad} and 'pad_mode' is {pad_mode}.")
        self.add_prim_attr("pad", pad)
        self.padding = pad
        if self.pad_mode == 'pad':
            for item in pad:
                validator.check_non_negative_int(item, 'pad item', self.name)

        self.mode = validator.check_equal_int(mode, 1, 'mode', self.name)
        self.add_prim_attr('mode', self.mode)
        self.format = validator.check_string(data_format, ['NCDHW'], 'data_format', self.name)
        self.add_prim_attr('data_format', self.format)
        self.out_channel = validator.check_positive_int(out_channel, 'out_channel', self.name)
        validator.check_value_type("group", group, (int,), self.name)
        validator.check_int_range(group, 1, out_channel, validator.INC_BOTH, "group", self.name)
        device_target = context.get_context("device_target")
        if self.out_channel % group != 0:
            raise ValueError("The argument 'group' should be divisible by 'out_channel'")
        if device_target == "Ascend" and group != 1:
            raise ValueError("On Ascend platform, group = 1 must be satisfied.")

        self.group = group
        self.add_prim_attr('groups', self.group)
        self.add_prim_attr('offset_x', 0)


class Conv3DBackpropInput(Primitive):
    """
    Computes the gradients of convolution 3D with respect to the input.

    Args:
        out_channel (int): The dimension of the output.
        kernel_size (Union[int, tuple[int]]): The kernel size of the 3D convolution.
        mode (int): Modes for different convolutions. Not currently used.
        pad_mode (str, optional): Specifies the padding mode with a padding value of 0. It can be set to:
            ``"same"`` , ``"valid"`` or ``"pad"`` . Default: ``"valid"`` .

            - ``"same"``: Pad the input around its depth/height/width dimension so that the shape of input and output
              are the same when `stride` is set to ``1``.
              The amount of padding to is calculated by the operator internally.  If the amount is even,
              it isuniformly distributed around the input, if it is odd, the excess amount goes
              to the front/right/bottom side.
              If this mode is set, `pad` must be 0.
            - ``"valid"``: No padding is applied to the input, and the output returns the maximum
              possible depth, height and width. Extra pixels that could not complete a full stride will
              be discarded. If this mode is set, `pad` must be 0.
            - ``"pad"``: Pad the input with a specified amount. In this mode, the amount of padding
              in the depth, height and width dimension is determined by the `pad` parameter.
              If this mode is set, `pad` must be greater than or equal to 0.

        pad (Union(int, tuple[int])): The pad value to be filled. Default: ``0`` . If `pad` is an integer, the
                    paddings of head, tail, top, bottom, left and right are the same, equal to pad. If `pad` is a
                    tuple of four integers, the padding of head, tail, top, bottom, left and right equal to pad[0],
                    pad[1], pad[2], pad[3], pad[4] and pad[5] correspondingly.
        stride (Union(int, tuple[int])): The stride to be applied to the convolution filter. Default: ``1`` .
        dilation (Union(int, tuple[int])): Specifies the space to use between kernel elements. Default: ``1`` .
        group (int): Splits input into groups. Default: ``1`` .
        data_format (str): The optional value for data format. Currently only support ``'NCDHW'`` .

    Inputs:
        - **weight** (Tensor) - Set size of kernel is :math:`(D_{in}, K_h, K_w)`, then the shape is
          :math:`(C_{out}, C_{in}, D_{in}, K_h, K_w)`. Currently weight data type only support float16 and float32.
        - **dout** (Tensor) - the gradients with respect to the output of the convolution.
          The shape conforms to the default.
          data_format :math:`(N, C_{out}, D_{out}, H_{out}, W_{out})`. Currently dout data type only support float16
          and float32.
        - **input_size** (tuple(int)) - A tuple describes the shape of the input which conforms to the format
          :math:`(N, C_{in}, D_{in}, H_{in}, W_{in})`.

    Outputs:
        Tensor, the gradients with respect to the input of convolution 3D. It has the same shape as the input.

    Raises:
        TypeError: If `out_channel` or `group` is not an int.
        TypeError: If `kernel_size`, `stride`, `pad` or `dilation` is neither an int not a tuple.
        ValueError: If `out_channel`, `kernel_size`, `stride` or `dilation` is less than 1.
        ValueError: If `pad` is less than 0.
        ValueError: If `pad_mode` is not one of 'same', 'valid', 'pad'.
        ValueError: If `pad` is a tuple whose length is not equal to 6.
        ValueError: If `pad_mode` is not equal to 'pad' and `pad` is not equal to (0, 0, 0, 0, 0, 0).
        ValueError: If `data_format` is not 'NCDHW'.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import numpy as np
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> dout = Tensor(np.ones([16, 32, 10, 32, 32]), mindspore.float16)
        >>> weight = Tensor(np.ones([32, 32, 4, 6, 2]), mindspore.float16)
        >>> x = Tensor(np.ones([16, 32, 13, 37, 33]))
        >>> conv3d_backprop_input = ops.Conv3DBackpropInput(out_channel=4, kernel_size=(4, 6, 2))
        >>> output = conv3d_backprop_input(dout, weight, ops.shape(x))
        >>> print(output.shape)
        (16, 32, 13, 37, 33)
    """

    @prim_attr_register
    def __init__(self,
                 out_channel,
                 kernel_size,
                 mode=1,
                 pad_mode="valid",
                 pad=0,
                 stride=1,
                 dilation=1,
                 group=1,
                 data_format="NCDHW"):
        """Initialize Conv3DBackpropInput"""
        self.init_prim_io_names(inputs=['filter', 'out_backprop', 'input_size'], outputs=['y'])
        self.out_channel = validator.check_positive_int(out_channel, 'out_channel', self.name)
        self.kernel_size = _check_3d_int_or_tuple('kernel_size', kernel_size, self.name)
        self.stride = _check_3d_int_or_tuple('stride', stride, self.name, allow_five=True, ret_five=True)
        self.add_prim_attr('strides', self.stride)
        self.dilation = _check_3d_int_or_tuple('dilation', dilation, self.name, allow_five=True, ret_five=True)
        self.add_prim_attr('dilations', self.dilation)
        validator.check_value_type('pad', pad, (int, tuple), self.name)
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        if isinstance(pad, int):
            pad = (pad,) * 6
        validator.check_equal_int(len(pad), 6, 'pad size', self.name)
        self.add_prim_attr("pad", pad)
        self.pad_list = pad

        self.pad_mode = validator.check_string(pad_mode.lower(), ['valid', 'same', 'pad'], 'pad_mode', self.name)
        if self.pad_mode != 'pad' and self.pad_list != (0, 0, 0, 0, 0, 0):
            raise ValueError(f"For '{self.name}', the 'pad' must be (0, 0, 0, 0, 0, 0) "
                             f"when 'pad_mode' is not \"pad\", "
                             f"but got 'pad' is {self.pad_list} and 'pad_mode' is {self.pad_mode}.")
        if self.pad_mode == 'pad':
            for item in pad:
                validator.check_non_negative_int(item, 'pad item', self.name)
        self.add_prim_attr('pad_mode', self.pad_mode)

        self.mode = validator.check_equal_int(mode, 1, 'mode', self.name)
        self.add_prim_attr('mode', self.mode)
        self.group = validator.check_positive_int(group, 'group', self.name)
        self.add_prim_attr('groups', self.group)
        self.format = validator.check_string(data_format, ['NCDHW'], 'format', self.name)
        self.add_prim_attr('data_format', self.format)


def _deconv_output_length(input_length, kernel_size, stride_size, dilation_size):
    filter_size = kernel_size + (kernel_size - 1) * (dilation_size - 1)
    if filter_size - stride_size > 0:
        length = input_length * stride_size + filter_size - stride_size
    else:
        length = input_length * stride_size
    return length


class SparseApplyAdadelta(Primitive):
    r"""
    Updates relevant entries according to the adadelta scheme.

    .. math::
            \begin{array}{ll} \\
                accum = \rho * accum + (1 - \rho) * grad^2 \\
                \text{update} = \sqrt{\text{accum_update} + \epsilon} * \frac{grad}{\sqrt{accum + \epsilon}} \\
                var = var -  update * lr \\
                \text{accum_update} = \rho * \text{accum_update} + (1 - \rho) * update^2 \\
            \end{array}

    Inputs of 'var', 'accum', 'accum_update' and 'grad' comply with the implicit type conversion rules
    to make the data types consistent. Besides, inputs of 'lr' and 'rho' also support implicit type conversion.
    If they have different data types, the lower priority data type will be converted to
    relatively highest priority data type.
    RuntimeError exception will be thrown when the data type conversion of Parameter is required.

    Note:
        If there are negative values or values greater than or equal to var.shape[0] in `indices`,
        the behavior is undefined. Besides, this operator doesn't support duplicates in `indices`.

    Args:
        epsilon (float): A small value added for numerical stability. Its value must be greater or equal to 0.
        use_locking (bool): If ``True`` , the `var` and `accum` tensors will be protected from being updated.
            Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Weights to be updated. With float32 or float16 data type.
        - **accum** (Parameter) - Accumulation to be updated. Mush have the same shape and dtype as `var`.
          With float32 or float16 data type.
        - **accum_update** (Parameter) - Accum_update to be updated. Must have the same shape and dtype as `var`.
          With float32 or float16 data type.
        - **lr** (Union[float, Tensor]) - Learning rate, must be a scalar. With float32 or float16 data type.
        - **rho** (Union[float, Tensor]) - Decay rate, must be a scalar. With float32 or float16 data type.
        - **grad** (Tensor) - A tensor for gradient. Must have the same shape and dtype as `var`.
        - **indices** (Tensor) - A tensor of indices in the first dimension of `var` and `accum`.
          Must be one of the following types: int32, int64 and indices.shape[0] = grad.shape[0].

    Outputs:
        Tuple of 3 Tensor, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **accum** (Tensor) - The same shape and data type as `accum`.
        - **accum_update** (Tensor) - The same shape and data type as `accum_update`.

    Raises:
        TypeError: If `epsilon` is not a float.
        TypeError: If `use_locking` is not a bool.
        TypeError: If `var`, 'accum', 'accum_update' is not a Parameter.
        TypeError: If dtype of `accum`, `accum_updata`, `grad` is not same as `var`.
        TypeError: If dtype of `var`, `accum`, `accum_update`, `lr`, `rho` or `grad` is neither float16 nor
                   float32.
        TypeError: If dtype of `indices` is neither int32 nor int64.
        ValueError: If `epsilon` is less than 0.
        ValueError: If the shape of `accum`, `accum_updata`, `grad` is not same as `var`.
        ValueError: If the rank of `indices` is not equal to 1.
        ValueError: If shape of `indices` is not same as shape of first dimension of `grad`.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> class Net(nn.Cell):
        ...     def __init__(self,epsilon,use_locking = False):
        ...         super(Net, self).__init__()
        ...         self.sparse_apply_adadelta = P.SparseApplyAdadelta(epsilon,use_locking)
        ...         self.var = Parameter(Tensor(np.array([[1.0,2.0],[2.0,3.0]]).astype(np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.array([[1.5,2.5],[3.5,4.5]]).astype(np.float32)), name="accum")
        ...         self.accum_update = Parameter(Tensor(np.array([[1.2,2.4],[1.8,0.6]]).astype(np.float32)),
        ...                name="accum_update")
        ...     def construct(self, lr, rho, grad, indices):
        ...         out = self.sparse_apply_adadelta(self.var, self.accum, self.accum_update, lr, rho, grad, indices)
        ...         return out
        ...
        >>> epsilon = 1e-6
        >>> net = Net(epsilon)
        >>> lr = 0.01
        >>> rho = 0.2
        >>> grad = Tensor(np.array([[0.3, 0.7], [0.1, 0.8]]).astype(np.float32))
        >>> output = net(lr, rho, grad, Tensor(np.array([0,1],dtype=np.int32)))
        >>> print(output)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 9.94611859e-01,  1.98851788e+00],
         [ 1.99840558e+00,  2.99478507e+00]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 3.72000009e-01,  8.91999960e-01],
         [ 7.08000004e-01,  1.41200006e+00]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 4.72257614e-01,  1.53470778e+00],
         [ 3.80338937e-01,  3.37563992e-01]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum_updata', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T1),
        sig.make_sig('rho', dtype=sig.sig_dtype.T1),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T2),
    )

    @prim_attr_register
    def __init__(self, epsilon, use_locking=False):
        """Initialize SparseApplyAdadelta"""
        validator.check_value_type("epsilon", epsilon, [float], self.name)
        validator.check_number("epsilon", epsilon, 0.0, validator.GE, self.name)
        validator.check_value_type("use_locking", use_locking, [bool], self.name)


class CTCLossV2(Primitive):
    """
    Calculates the CTC (Connectionist Temporal Classification) loss and the gradient.

    The CTC algorithm is proposed in `Connectionist Temporal Classification: Labeling Unsegmented Sequence Data with
    Recurrent Neural Networks <http://www.cs.toronto.edu/~graves/icml_2006.pdf>`_.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        blank (int, optional): The blank label. Default: ``0`` .
        reduction (str, optional): Apply specific reduction method to the output. Currently only support ``'none'``.
            Default: ``'none'`` .

        zero_infinity (bool, optional): If loss is infinite, this parameter determines whether to set that loss
            and its correlated gradient to zero. Default: ``False`` .

    Inputs:
        - **log_probs** (Tensor) - A tensor of shape :math:`(T, N, C)`, where :math:`T` is input length, :math:`N` is
          batch size and :math:`C` is number of classes (including blank). Supported dtypes: float32, float64.
        - **targets** (Tensor) - A tensor of shape :math:`(N, S)`, where :math:`S` is max target length,
          means the target sequences. Supported dtypes: int32, int64.
        - **input_lengths** (Union(Tuple, Tensor)) - A tuple or Tensor of shape :math:`(N)`.
          It means the lengths of the input. Supported dtypes: int32, int64.
        - **target_lengths** (Union(Tuple, Tensor)) - A tuple or Tensor of shape :math:`(N)`.
          It means the lengths of the target. Supported dtypes: int32, int64.

    Outputs:
        - **neg_log_likelihood** (Tensor) - A loss value which is differentiable with respect to each input node.
        - **log_alpha** (Tensor) - The probability of possible trace of input to target.

    Raises:
        TypeError: If `zero_infinity` is not a bool.
        TypeError: If `reduction` is not string.
        TypeError: If the dtype of `log_probs` is not float or double.
        TypeError: If the dtype of `targets`, `input_lengths` or `target_lengths` is not int32 or int64.
        ValueError: If the rank of `log_probs` is not 3.
        ValueError: If the rank of `targets` is not 2.
        ValueError: If the shape of `input_lengths` does not match batch_size :math:`N`.
        ValueError: If the shape of `target_lengths` does not match batch_size :math:`N`.
        TypeError: If the types of `targets`, `input_lengths` or `target_lengths` are different.
        ValueError: If the value of `blank` is not in range [0, C).
        RuntimeError: If any value of `input_lengths` is larger than (num_labels|C).
        RuntimeError: If any `target_lengths[i]` is not in range [0, `input_length[i]`].

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> log_probs = Tensor(np.array([[[0.3, 0.6, 0.6]],
        ...                              [[0.9, 0.4, 0.2]]]).astype(np.float32))
        >>> targets = Tensor(np.array([[0, 1]]), mstype.int32)
        >>> input_lengths = Tensor(np.array([2]), mstype.int32)
        >>> target_lengths = Tensor(np.array([1]), mstype.int32)
        >>> CTCLossV2 = ops.CTCLossV2(blank=0, reduction='none', zero_infinity=False)
        >>> neg_log_hood, log_alpha = CTCLossV2(
        ...     log_probs, targets, input_lengths, target_lengths)
        >>> print(neg_log_hood)
        [-2.2986124]
        >>> print(log_alpha)
        [[[0.3       0.3            -inf      -inf      -inf]
          [1.2       1.8931472 1.2            -inf      -inf]]]
    """

    @prim_attr_register
    def __init__(self, blank=0, reduction="none", zero_infinity=False):
        """Initialize CTCLossV2"""
        self.init_prim_io_names(inputs=["log_probs", "targets", "input_lengths", "target_lengths"],
                                outputs=["neg_log_likelihood", "log_alpha"])
        validator.check_value_type("blank", blank, [int], self.name)
        self.add_prim_attr("blank", blank)
        validator.check_value_type("reduction", reduction, [str], self.name)
        self.reduction = reduction.lower()
        validator.check_string(self.reduction, ['none'], 'reduction', self.name)
        self.add_prim_attr("reduction", self.reduction)
        validator.check_value_type("zero_infinity", zero_infinity, [bool], self.name)
        self.add_prim_attr("zero_infinity", zero_infinity)


class CTCLossV2Grad(Primitive):
    """
    Calculates the gradient of CTC (Connectionist Temporal Classification) loss.

    The CTC algorithm is proposed in `Connectionist Temporal Classification: Labeling Unsegmented Sequence Data with
    Recurrent Neural Networks <http://www.cs.toronto.edu/~graves/icml_2006.pdf>`_.

    Args:
        blank (int): The blank label. Default: ``0`` .
        reduction (string): Apply specific reduction method to the output. Currently only support 'none'.
            Default: ``"none"`` .
        zero_infinity (bool): Whether to set infinite loss and correlation gradient to zero. Default: ``False`` .

    Inputs:
        - **grad_out** (Tenosr) - Gradient renewal codfficient, A tensor for shape (N), where N is batch size.
        - **log_probs** (Tensor) - A tensor of shape (T, N, C), where T is input length, N is batch size and C is number
          of classes (including blank).
        - **targets** (Tensor) - A tensor of shape (N, S), where S is max target length, means the target sequences.
        - **input_lengths** (Union(tuple, Tensor)) - A tuple or Tensor of shape(N). It means the lengths of the input.
        - **target_lengths** (Union(tuple, Tensor)) - A tuple or Tensor of shape(N). It means the lengths of the target.
        - **log_alpha** (Tensor) - The probability of possible trace of input to target.
        - **neg_log_likelihood** (Tensor) - A loss value which is differentiable with respect to each input node.

    Outputs:
        - **grad** (Tensor) - The grad of Connectionist Temporal Classification Loss.

    Raises:
        TypeError: If `zero_infinity` is not a bool, reduction is not string.
        TypeError: If the dtype of `log_probs` or `grad_out` is not float or double.
        TypeError: If the dtype of `targets`, `input_lengths` or `target_lengths` is not int32 or int64.
        RuntimeError: If the rank of `log_probs` is not 3.
        RuntimeError: If the rank of `targets` is not 2.
        RuntimeError: If the shape of `input_lengths` does not match {batch_size|N}.
        RuntimeError: If the shape of `target_lengths` does not match {batch_size|N}.
        RuntimeError: If the types of `targets`, `input_lengths`, `grad_out` or `target_lengths` are different.
        RuntimeError: If the value of `blank` is not in range [0, num_labels|C).
        RuntimeError: If any value of `input_lengths` is larger than (num_labels|C).
        RuntimeError: If any target_lengths[i] is not in range [0, input_length[i]].

    Supported Platforms:
        ``Ascend`` ``CPU``
    """

    @prim_attr_register
    def __init__(self, blank, reduction="none", zero_infinity=False):
        """Initialize CTCLossV2Grad"""
        self.init_prim_io_names(inputs=["grad_out", "log_probs", "targets", "input_lengths", "target_lengths",
                                        "neg_log_likelihood", "log_alpha"],
                                outputs=["grad"])
        validator.check_value_type("blank", blank, [int], self.name)
        self.add_prim_attr("blank", blank)
        validator.check_value_type("reduction", reduction, [str], self.name)
        self.add_prim_attr("reduction", reduction)
        validator.check_value_type("zero_infinity", zero_infinity, [bool], self.name)
        self.add_prim_attr("zero_infinity", zero_infinity)


class Conv3DTranspose(Primitive):
    r"""
    Computes a 3D transposed convolution, which is also known as a deconvolution
    (although it is not an actual deconvolution).

    Input is typically of shape :math:`(N, C, D, H, W)`, where :math:`N` is batch size, :math:`C` is channel number,
    :math:`D` is depth, :math:`H` is height, :math:`W` is width.

    If the 'pad_mode' is set to be "pad", the depth, height and width of output are defined as:

    .. math::
        D_{out} = (D_{in} - 1) \times \text{stride}[0] - 2 \times \text{pad}[0] + \text{dilation}[0]
        \times (\text{kernel_size}[0] - 1) + \text{output_padding}[0] + 1

        H_{out} = (H_{in} - 1) \times \text{stride}[1] - 2 \times \text{pad}[1] + \text{dilation}[1]
        \times (\text{kernel_size}[1] - 1) + \text{output_padding}[1] + 1

        W_{out} = (W_{in} - 1) \times \text{stride}[2] - 2 \times \text{pad}[2] + \text{dilation}[2]
        \times (\text{kernel_size}[2] - 1) + \text{output_padding}[2] + 1

    Note:
        In Ascend, only support :math:`group=1`.

    Args:
        in_channel (int): The channel of the input x.
        out_channel (int): The channel of the weight x.
        kernel_size (Union[int, tuple[int]]): The data type is int or a tuple of 3 integers.
            Specifies the depth, height and width of the 3D convolution window.
            Single int means the value is for the depth, height and width of the kernel.
            A tuple of 3 ints means the first value is for the depth, the second value is for the height and the
            other is for the width of the kernel.
        mode (int, optional): Modes for different convolutions. Default is ``1`` . It is currently not used.
        pad_mode (str, optional): Specifies the padding mode with a padding value of 0. It can be set to:
            ``"same"`` , ``"valid"`` or ``"pad"`` . Default: ``"valid"`` .

            - ``"same"``: Pad the input around its depth/height/width dimension so that the shape of input and output
              are the same when `stride` is set to ``1``.
              The amount of padding to is calculated by the operator internally.  If the amount is even,
              it isuniformly distributed around the input, if it is odd, the excess amount goes
              to the front/right/bottom side.
              If this mode is set, `pad` must be 0.
            - ``"valid"``: No padding is applied to the input, and the output returns the maximum
              possible depth, height and width. Extra pixels that could not complete a full stride will
              be discarded. If this mode is set, `pad` must be 0.
            - ``"pad"``: Pad the input with a specified amount. In this mode, the amount of padding
              in the depth, height and width dimension is determined by the `pad` parameter.
              If this mode is set, `pad` must be greater than or equal to 0.

        pad (Union(int, tuple[int]), optional): The pad value to be filled. Default: ``0`` . If `pad` is an integer,
            the paddings of head, tail, top, bottom, left and right are the same, equal to pad.
            If `pad` is a tuple of six integers, the padding of head, tail, top, bottom, left and right equal
            to pad[0], pad[1], pad[2], pad[3], pad[4] and pad[5] correspondingly.
        stride (Union(int, tuple[int]), optional): The distance of kernel moving, an int number that represents
            the depth, height and width of movement are both strides, or a tuple of three int numbers that
            represent depth, height and width of movement respectively. Default: ``1`` .
        dilation (Union(int, tuple[int]), optional): Specifies the space to use between kernel elements.
            Default: ``1`` .
        group (int, optional): The number of groups into which the filter is divided. `in_channels`
            and `out_channels` must be divisible by `group`. Default: ``1`` .
        output_padding (Union(int, tuple[int]), optional): Add extra size to each dimension of the output.
            Default: ``0`` .
        data_format (str, optional): The optional value for data format. Currently only ``'NCDHW'`` is supported.
            Default: ``'NCDHW'``.

    Inputs:
        - **dout** (Tensor) - The gradients with respect to the output of the convolution.
          The shape conforms to the default.
          data_format :math:`(N, C_{in}, D_{out}, H_{out}, W_{out})`. Currently dout data type only supports float16
          and float32.
        - **weight** (Tensor) - Set size of kernel is :math:`(K_d, K_h, K_w)`, then the shape is
          :math:`(C_{in}, C_{out}//group, K_d, K_h, K_w)`. Where :math:`group` is the Args parameter,
          :math:`//` is the symbol for integer division.
          Currently weight data type only supports float16 and float32.
        - **bias** (Tensor) - Tensor of shape :math:`C_{out}`. Currently, only support none. Default: ``None`` .

    Outputs:
        Tensor, the gradients with respect to the input of convolution 3D.
        Tensor of shape :math:`(N, C_{out}//group, D_{out}, H_{out}, W_{out})`,
        where :math:`group` is the Args parameter.

    Raises:
        TypeError: If `in_channel`, `out_channel` or `group` is not an int.
        TypeError: If `kernel_size`, `stride`, `pad` , `dilation` or `output_padding` is neither an int not a tuple.
        ValueError: If `in_channel`, `out_channel`, `kernel_size`, `stride` or `dilation` is less than 1.
        ValueError: If `pad` is less than 0.
        ValueError: If `pad_mode` is not one of 'same', 'valid' nor 'pad'.
        ValueError: If `pad` is a tuple whose length is not equal to 6.
        ValueError: If `pad_mode` is not equal to 'pad' and `pad` is not equal to (0, 0, 0, 0, 0, 0).
        ValueError: If `data_format` is not 'NCDHW'.
        TypeError: If data type of dout and weight is neither float16 nor float32.
        ValueError: If bias is not none. The rank of dout and weight is not 5.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> dout = Tensor(np.ones([32, 16, 10, 32, 32]), mindspore.float16)
        >>> weight = Tensor(np.ones([16, 3, 4, 6, 2]), mindspore.float16)
        >>> conv3d_transpose = ops.Conv3DTranspose(in_channel=16, out_channel=3, kernel_size=(4, 6, 2))
        >>> output = conv3d_transpose(dout, weight)
        >>> print(output.shape)
        (32, 3, 13, 37, 33)
    """

    @prim_attr_register
    def __init__(self,
                 in_channel,
                 out_channel,
                 kernel_size,
                 mode=1,
                 pad_mode='valid',
                 pad=0,
                 stride=1,
                 dilation=1,
                 group=1,
                 output_padding=0,
                 data_format="NCDHW"):
        """Initialize Conv3DTranspose"""
        self.init_prim_io_names(inputs=['x', 'filter'], outputs=['output'])
        self.in_channel = validator.check_positive_int(in_channel, 'in_channel', self.name)
        self.add_prim_attr('in_channel', self.in_channel)
        self.out_channel = validator.check_positive_int(out_channel, 'out_channel', self.name)
        self.add_prim_attr('out_channel', self.out_channel)
        self.kernel_size = _check_3d_int_or_tuple('kernel_size', kernel_size, self.name)
        if isinstance(kernel_size, int):
            self.kernel_size = (kernel_size,) * 3
        self.add_prim_attr('kernel_size', self.kernel_size)
        self.stride = _check_3d_int_or_tuple('stride', stride, self.name, allow_five=False,
                                             ret_five=True)
        self.add_prim_attr('strides', self.stride)
        self.dilation = _check_3d_int_or_tuple('dilation', dilation, self.name, allow_five=False,
                                               ret_five=True, third_one=True)
        self.add_prim_attr('dilations', self.dilation)
        validator.check_value_type('pad', pad, (int, tuple), self.name)
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        if isinstance(pad, int):
            pad = (pad,) * 6
        if len(pad) != 6:
            raise ValueError(f"For '{self.name}', attr 'pad' must be an positive int number or a tuple of "
                             f"six positive int numbers, but got {self.pad}.")
        self.pad_list = pad
        validator.check_value_type('pad_mode', pad_mode, [str], self.name)
        self.pad_mode = validator.check_string(pad_mode.lower(), ['valid', 'same', 'pad'], 'pad_mode', self.name)
        self.add_prim_attr('pad_mode', self.pad_mode)

        if self.pad_mode != 'pad' and pad != (0, 0, 0, 0, 0, 0):
            raise ValueError(f"For '{self.name}', the 'pad' must be zero or (0, 0, 0, 0, 0, 0) when 'pad_mode' "
                             f"is not \"pad\", but got 'pad' is {self.pad} and 'pad_mode' is {pad_mode}.")

        if self.pad_mode == 'pad':
            for item in self.pad_list:
                validator.check_non_negative_int(item, 'pad item', self.name)
        self.add_prim_attr('pad_list', self.pad_list)
        self.mode = validator.check_equal_int(mode, 1, 'mode', self.name)
        self.add_prim_attr('mode', self.mode)
        validator.check_value_type("group", group, (int,), self.name)
        validator.check_int_range(group, 1, out_channel, validator.INC_BOTH, "group", self.name)
        if self.out_channel % group != 0:
            raise ValueError("The argument 'group' should be divisible by 'out_channel'")
        device_target = context.get_context("device_target")
        if device_target == "Ascend" and group != 1:
            raise ValueError("On Ascend platform, group = 1 must be satisfied.")
        self.group = group
        self.add_prim_attr('groups', self.group)

        self.format = validator.check_string(data_format, ['NCDHW'], 'format', self.name)
        self.add_prim_attr('data_format', self.format)

        self.output_padding = _check_3d_int_or_tuple('output_padding', output_padding, self.name,
                                                     allow_five=False, ret_five=True, greater_zero=False)
        output_padding_ = (self.output_padding[2], self.output_padding[3], self.output_padding[4])
        if self.pad_mode != 'pad' and output_padding_ != (0, 0, 0):
            raise ValueError(f"For '{self.name}', the 'output_padding' must be zero or (0, 0, 0) "
                             f"when 'pad_mode' is not \"pad\", but got 'output_padding' is "
                             f"{output_padding} and 'pad_mode' is {pad_mode}.")
        self.add_prim_attr('output_padding', self.output_padding)
        validator.check_int_range(self.kernel_size[0] * self.kernel_size[1] * self.kernel_size[2],
                                  1, 343, validator.INC_BOTH,
                                  'The product of height, width and depth of kernel_size belonging [1, 343]',
                                  self.name)
        validator.check_int_range(self.stride[0] * self.stride[1] * self.stride[2], 1, 343, validator.INC_BOTH,
                                  'The product of height, width and depth of stride belonging [1, 343]', self.name)
        validator.check_int_range(self.stride[1] * self.stride[2], 1, 256, validator.INC_BOTH,
                                  'The product of height, width and depth of stride belonging [1, 256]', self.name)
        validator.check_int_range(self.output_padding[2], 0, max(self.dilation[2], self.stride[2]), validator.INC_LEFT,
                                  'output_padding_d belonging [0, max(stride_d, dilation_d))', self.name)
        validator.check_int_range(self.output_padding[3], 0, max(self.dilation[3], self.stride[3]), validator.INC_LEFT,
                                  'output_padding_h belonging [0, max(stride_h,dilation_h))', self.name)
        validator.check_int_range(self.output_padding[4], 0, max(self.dilation[4], self.stride[4]), validator.INC_LEFT,
                                  'output_padding_w belonging [0, max(stride_w,dilation_w))', self.name)


class Dilation2D(Primitive):
    r"""
    Computes the grayscale dilation of 4-D input and 3-D filters tensors.

    Applies a 2D dilation over an input tensor which is typically of shape :math:`(N, C_{in}, H_{in}, W_{in})`,
    where :math:`N` is batch size, :math:`H` is height, :math:`W` is width, :math:`C` is channel number.
    Given kernel size :math:`ks = (h_{ker}, w_{ker})`, stride :math:`s = (s_0, s_1)` and
    dilation :math:`d = (d_0, d_1)`, the operation is as follows:

    .. math::
        \text{output}(N_i, C_j, h, w) = \max_{m=0, \ldots, h_{ker}-1} \max_{n=0, \ldots, w_{ker}-1}
        \text{input}(N_i, C_j, s_0 \times h + d_0 \times m, s_1 \times w + d_1 \times n) + \text{filter}(C_j, m, n)

    .. warning::
        This is an experimental API that is subjected to change or deletion.

    Note:
        If the input data type is float32, this operator is still executed in float16 mode.

    Args:
        stride (Union(int, tuple[int])): The distance of kernel moving, an int number that represents
            the height and width of movement are both strides, or a tuple of two int numbers that
            represent height and width of movement respectively, or a tuple of four int numbers when
            data_format is 'NCHW' represents [1, 1, stride_height, stride_width].

        dilation (Union(int, tuple[int])): The data type is int or a tuple of 2 integers or a tuple of 4 integers.
                                      Specifies the dilation rate to use for dilated convolution.
                                      If set to be :math:`k > 1`, there will be :math:`k - 1` pixels skipped for
                                      each sampling location. Its value must be greater or equal to 1 and bounded by
                                      the height and width of the input `x`.

        pad_mode (str, optional): Specifies the padding mode with a padding value of 0. It can be set to:
            ``"same"`` or ``"valid"`` . Default: ``"valid"`` .

            - ``"same"``: Pad the input around its edges so that the shape of input and output
              are the same when `stride` is set to ``1``.
              The amount of padding to is calculated by the operator internally, If the amount is even, it is
              uniformly distributed around the input, if it is odd, the excess amount goes to the right/bottom side.
            - ``"valid"``: No padding is applied to the input, and the output returns the maximum
              possible height and width. Extra pixels that could not complete a full stride will
              be discarded.

        data_format (str, optional): The value for data format, only ``'NCHW'`` is supported at present.
            Default: ``"NCHW"`` .

    Inputs:
        - **x** (Tensor) - Input data. A 4-D Tensor, its shape must be
          :math:`(N, C_{in}, H_{in}, W_{in})`.
        - **filter** (Tensor) - A three dimension tensor with the same type as input. The shape must be
          :math:`(C_{in}, H_{filter}, W_{filter})`.

    Outputs:
        Tensor, the value that applied 2D dilation. The shape is :math:`(N, C_{out}, H_{out}, W_{out})` which
        is not necessarily the same as the input x, the type is the same as the input x.

    Raises:
        TypeError: If type of `x` or `filter` is not the type in [uint8, uint16, uint32, uint64, int8, int16,
                                  int32, int64, float16, float32, float64].
        TypeError: If `stride` or `dilation` is not an int number or a tuple of two or four int numbers.
        ValueError: If the length of `stride` or `dilation` is neither two nor four when they are tuple.
        ValueError: If `stride` or `dilation` shape is not (1, 1, height, width) when it is a tuple of four int numbers.
        ValueError: If `stride` is not in the range of [1, 255].
        ValueError: If `dilation` is less than 1.
        ValueError: If `pad_mode` is not a str of 'same', 'valid', 'SAME' or 'VALID'.
        ValueError: If `data_format` is not the str of 'NCHW'.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> x = Tensor(np.ones([10, 5, 32, 32]), mindspore.float16)
        >>> filter = Tensor(np.ones([5, 3, 3]), mindspore.float16)
        >>> dilation2d = ops.Dilation2D(stride=1, dilation=1, pad_mode='VALID')
        >>> output = dilation2d(x, filter)
        >>> print(output.shape)
        (10, 5, 30, 30)
    """

    @prim_attr_register
    def __init__(self, stride, dilation, pad_mode="SAME", data_format="NCHW"):
        """Initialize Dilation2D."""
        self.init_prim_io_names(inputs=['x', 'filter'], outputs=['y'])

        def _check_format_stride_or_dilation(arg_name, arg_value, prim_name, data_format):
            validator.check_value_type(arg_name, arg_value, (int, tuple), prim_name)
            if isinstance(arg_value, int):
                ret_value = (1, arg_value, arg_value, 1) if data_format == "NHWC" else (1, 1, arg_value, arg_value)
            elif len(arg_value) == 2:
                ret_value = (1, arg_value[0], arg_value[1], 1) if data_format == "NHWC" else \
                    (1, 1, arg_value[0], arg_value[1])
            elif len(arg_value) == 4:
                if data_format == "NHWC" and (arg_value[0] != 1 or arg_value[3] != 1):
                    raise ValueError(
                        f"For '{prim_name}' attr '{arg_name}' should be [1, {arg_name}_height, {arg_name}_weigth, 1]"
                        f"when data_format is 'NHWC', but got {arg_value}")
                if data_format == "NCHW" and (arg_value[0] != 1 or arg_value[1] != 1):
                    raise ValueError(
                        f"For '{prim_name}' attr '{arg_name}' should be [1, 1, {arg_name}_height, {arg_name}_weigth]"
                        f"when data_format is 'NCHW', but got {arg_value}")
                ret_value = arg_value
            else:
                raise ValueError(
                    f"For '{prim_name}' attr '{arg_name}' should be an positive int number or a tuple of two "
                    f"or four positive int numbers, but got {arg_value}")
            for item in ret_value:
                if isinstance(item, int) and not isinstance(item, bool) and item > 0:
                    continue
                raise ValueError(
                    f"For '{prim_name}' attr '{arg_name}' should be an positive int number or a tuple of two "
                    f"or four positive int numbers, but got {arg_value}")
            return ret_value

        if data_format == 'NHWC':
            raise ValueError(f"For '{self.name}', NHWC format is not supported at present.")
        self.data_format = validator.check_string(data_format, ['NCHW', 'NHWC'], 'data_format', self.name)
        self.add_prim_attr('data_format', self.data_format)
        self.pad_mode = validator.check_string(pad_mode, ['VALID', 'SAME', 'valid', 'same'], 'pad_mode', self.name)
        self.add_prim_attr('pad_mode', self.pad_mode.upper())
        self.stride = _check_format_stride_or_dilation("stride", stride, self.name, self.data_format)

        def is_in_range(x):
            return 1 <= x <= 255

        if not is_in_range(self.stride[2]) or not is_in_range(self.stride[3]):
            raise ValueError(f'For Dilation2D, size of stride is not supported, '
                             f'stride should be in the range of [1, 255], '
                             f'but got stride_h: `{self.stride[2]}`, stride_w: `{self.stride[3]}`.')
        self.add_prim_attr('stride', self.stride)
        self.dilation = _check_format_stride_or_dilation("dilation", dilation, self.name, self.data_format)
        self.add_prim_attr('dilation', self.dilation)


class SoftShrink(Primitive):
    r"""
    Applies the SoftShrink function element-wise.

    Refer to :func:`mindspore.ops.softshrink` for more details.

    Args:
        lambd(float, optional): The :math:`\lambda` must be no less than zero. Default: ``0.5`` .

    Inputs:
        - **input_x** (Tensor) - The input of soft shrink with data type of float16 or float32.

    Outputs:
        Tensor, has the same shape and data type as `input_x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor(np.array([[ 0.5297,  0.7871,  1.1754], [ 0.7836,  0.6218, -1.1542]]), mindspore.float16)
        >>> softshrink = ops.SoftShrink()
        >>> output = softshrink(input_x)
        >>> print(output)
        [[ 0.02979  0.287    0.676  ]
         [ 0.2837   0.1216  -0.6543 ]]
    """

    @prim_attr_register
    def __init__(self, lambd=0.5):
        """Initialize SoftShrink"""
        validator.check_value_type("lambd", lambd, [float], self.name)
        validator.check_number("lambd", lambd, 0, validator.GE, self.name)


class ApplyAdagradDA(Primitive):
    r"""
    Update `var` according to the proximal adagrad scheme.
    The Adagrad algorithm was proposed in
    `Adaptive Subgradient Methods for Online Learning and Stochastic Optimization
    <http://www.jmlr.org/papers/volume12/duchi11a/duchi11a.pdf>`_.

    .. math::
        \begin{array}{ll} \\
            grad\_accum += grad \\
            grad\_squared\_accum += grad * grad \\
            tmp\_val=
                \begin{cases}
                     sign(grad\_accum) * max\left \{|grad\_accum|-l1*global\_step, 0\right \} & \text{ if } l1>0 \\
                     grad\_accum & \text{ otherwise } \\
                 \end{cases} \\
            x\_value = -1 * lr * tmp\_val \\
            y\_value = l2 * global\_step * lr + \sqrt{grad\_squared\_accum} \\
            var = \frac{ x\_value }{ y\_value }
        \end{array}

    Inputs of `var`, `gradient_accumulator`, `gradient_squared_accumulator` and `grad`
    comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        use_locking (bool): If ``True`` , updating of the `var` and `accum` tensors will be protected by a lock.
                            Otherwise the behavior is undefined, but may exhibit less contention. Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable to be updated. The data type must be float16 or float32.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **gradient_accumulator** (Parameter) - The dict of mutable tensor :math:`grad\_accum`. Must have the same
          shape as `var`.
        - **gradient_squared_accumulator** (Parameter) - The dict of mutable tensor :math:`grad\_squared\_accum`.
          Must have the same shape as `var`.
        - **grad** (Tensor) - A tensor for gradient. Must have the same shape as `var`.
        - **lr** ([Number, Tensor]) - Scaling factor. Must be a scalar. With float32 or float16 data type.
        - **l1** ([Number, Tensor]) -  L1 regularization. Must be a scalar. With float32 or float16 data type.
        - **l2** ([Number, Tensor]) -  L2 regularization. Must be a scalar. With float32 or float16 data type.
        - **global_step** ([Number, Tensor]) - Training step number. Must be a scalar. With int32 or int64 data type.

    Outputs:
        Tuple of 1 Tensors, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.

    Raises:
        TypeError: If `var`, `gradient_accumulator` or `gradient_squared_accumulator` is not a Parameter.
        TypeError: If `grad` is not a Tensor.
        TypeError: If `lr`, `l1`, `l2` or `global_step` is neither a Number nor a Tensor.
        TypeError: If use_locking is not a bool.
        TypeError: If dtype of `var`, `gradient_accumulator`, `gradient_squared_accumulator`, `grad`,
                   `lr`, `l1` or `l2` is neither float16 nor float32.
        TypeError: If dtype of `gradient_accumulator`, `gradient_squared_accumulator` or `grad` is not same as `var`.
        TypeError: If dtype of `global_step` is not int32 nor int64.
        ValueError: If the shape size of `lr`, `l1`, `l2` and `global_step` is not 0.
        TypeError: If the data type of `var`, `gradient_accumulator`, `gradient_squared_accumulator` and `grad`
                      conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import dtype as mstype
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class ApplyAdagradDANet(nn.Cell):
        ...     def __init__(self, use_locking=False):
        ...         super(ApplyAdagradDANet, self).__init__()
        ...         self.apply_adagrad_d_a = ops.ApplyAdagradDA(use_locking)
        ...         self.var = Parameter(Tensor(np.array([[0.6, 0.4], [0.1, 0.5]]).astype(np.float32)), name="var")
        ...         self.gradient_accumulator = Parameter(Tensor(np.array([[0.1, 0.3],
        ...                                                                [0.1, 0.5]]).astype(np.float32)),
        ...                                               name="gradient_accumulator")
        ...         self.gradient_squared_accumulator = Parameter(Tensor(np.array([[0.2, 0.1],
        ...                                                                        [0.1, 0.2]]).astype(np.float32)),
        ...                                                       name="gradient_squared_accumulator")
        ...         self.gradient_accumulator = Parameter(Tensor(np.array([[0.1, 0.3],
        ...                                                                [0.1, 0.5]]).astype(np.float32)),
        ...                                               name="gradient_accumulator")
        ...     def construct(self, grad, lr, l1, l2, global_step):
        ...         out = self.apply_adagrad_d_a(self.var, self.gradient_accumulator,
        ...                                      self.gradient_squared_accumulator, grad, lr, l1, l2, global_step)
        ...         return out
        ...
        >>> net = ApplyAdagradDANet()
        >>> grad = Tensor(np.array([[0.3, 0.4], [0.1, 0.2]]).astype(np.float32))
        >>> lr = Tensor(0.001, mstype.float32)
        >>> l1 = Tensor(0.001, mstype.float32)
        >>> l2 = Tensor(0.001, mstype.float32)
        >>> global_step = Tensor(2, mstype.int32)
        >>> output = net(grad, lr, l1, l2, global_step)
        >>> print(output)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[-7.39064650e-04, -1.36888528e-03],
         [-5.96988888e-04, -1.42478070e-03]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('gradient_accumulator', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('gradient_squared_accumulator', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T1),
        sig.make_sig('l1', dtype=sig.sig_dtype.T2),
        sig.make_sig('l2', dtype=sig.sig_dtype.T3),
        sig.make_sig('global_step', dtype=sig.sig_dtype.T4)
    )

    @prim_attr_register
    def __init__(self, use_locking=False):
        """Initialize ApplyAdagradDA"""
        validator.check_value_type("use_locking", use_locking, [bool], self.name)
        self.add_prim_attr('side_effect_mem', True)


class SparseApplyRMSProp(Primitive):
    r"""
    Update relevant entries according to the rmsprop algorithm.

    .. math::
        \begin{array}{ll} \\
            ms = rho * ms_{t-1} + (1 - rho) * grad * grad \\
            mom = momentum * mom_{t-1} + lr * grad / sqrt(ms + epsilon) \\
            var = var - mom
        \end{array}

    Inputs of `var`, `ms`, `mom` and `grad` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        rho (float): Decay rate. The value should be between 0 and 1, otherwise the behavior is undefined.
        momentum (float): Momentum. The value should be greater or equal to 0, otherwise the behavior is undefined.
        epsilon (float): A small value added for numerical stability. The value should be greater than 0,
                         otherwise the behavior is undefined.
        use_locking (bool): If ``True`` , updating of the var, ms, and mom tensors are protected by a lock;
                            otherwise the behavior is undefined, but may exhibit less contention. Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable to be updated. The data type must be float16 or float32.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **ms** (Parameter) - The dict of mutable tensor ms. Must have the same shape and dtype as `var`.
        - **mom** (Parameter) - The dict of mutable tensor mom. Must have the same shape and dtype as `var`.
        - **lr** ([Number, Tensor]) - Learning rate. Must be a scalar. With float16 or float32 data type.
        - **grad** (Tensor) - A tensor for gradient. Must have the same shape and dtype as `var`.
        - **indices** (Tensor) - A tensor of indices in the first dimension of `var`, `ms` and `mom`.
          If there are duplicates in `indices`, the behavior is undefined. Must be one of the
          following types: int32, int64 and indices.shape[0] = var.shape[0].

    Outputs:
        Tuple of 3 Tensors, the updated parameters.

        - **var** (Tensor) -  The same shape and data type as `var`.
        - **ms** (Tensor) - The same shape and data type as `ms`.
        - **mom** (Tensor) - The same shape and data type as `mom`.

    Raises:
        TypeError: If `var`, `ms` or `mom` is not a Parameter.
        TypeError: If `grad` or `indices` is not a Tensor.
        TypeError: If dtype of `var`, `ms`, `mom`, `lr`, `grad` is neither float16 nor float32.
        TypeError: If dtype of `indices` is neither int32 nor int64.
        TypeError: If `lr` is neither a Number or a Tensor.
        TypeError: If `use_locking` is not a bool.
        TypeError: If dtype of `epsilon`, `rho`, `momentum` is not a float.
        ValueError: If shape of `ms`, `mom`, `grad` is not same as `var`.
        ValueError: If the shape size of `lr` is not 0.
        ValueError: If shape of `indices` is not same as shape of first dimension of `var`.
        ValueError: If `epsilon` is less than or equal to 0.
        ValueError: If `momentum` is less than 0.
        ValueError: If `rho` is less than 0 or greater than 1.
        ValueError: If dimension of `var` is less than 1.
        RuntimeError: If the data type of `var`, `ms`, `mom` and `grad` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend``  ``GPU`` ``CPU``

    Examples:
        >>> class SparseApplyRMSPropNet(nn.Cell):
        ...     def __init__(self, rho, momentum, epsilon, use_locking=False):
        ...         super(SparseApplyRMSPropNet, self).__init__()
        ...         self.sparse_apply_r_m_s_prop = P.SparseApplyRMSProp(rho, momentum, epsilon, use_locking)
        ...         self.var = Parameter(Tensor(np.array([[0.6, 0.3], [0.1, 0.5]]).astype(np.float32)), name="var")
        ...         self.ms = Parameter(Tensor(np.array([[0.2, 0.4], [0.1, 0.3]]).astype(np.float32)), name="ms")
        ...         self.mom = Parameter(Tensor(np.array([[0.3, 0.1], [0.3, 0.6]]).astype(np.float32)), name="mom")
        ...     def construct(self, lr, grad, indices):
        ...         out = self.sparse_apply_r_m_s_prop(self.var, self.ms, self.mom, lr, grad, indices)
        ...         return out
        ...
        >>> rho = 0.2
        >>> momentum = 0.01
        >>> epsilon = 1e-6
        >>> net = SparseApplyRMSPropNet(rho, momentum, epsilon)
        >>> lr = 0.01
        >>> grad = Tensor(np.array([[0.3, 0.7], [0.1, 0.8]]).astype(np.float32))
        >>> indices = Tensor(np.array([0, 1], dtype=np.int32))
        >>> out = net(lr, grad, indices)
        >>> print(out)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 5.88035822e-01,  2.88811117e-01],
         [ 9.10239667e-02,  4.83422279e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 1.12000003e-01,  4.72000003e-01],
         [ 2.80000009e-02,  5.72000027e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 1.19641740e-02,  1.11888833e-02],
         [ 8.97603668e-03,  1.65777095e-02]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('ms', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('mom', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T1),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T2)
    )

    @prim_attr_register
    def __init__(self, rho, momentum, epsilon, use_locking=False):
        """"Initialize SparseApplyRMSProp"""
        validator.check_value_type("rho", rho, [float], self.name)
        validator.check_value_type("momentum", momentum, [float], self.name)
        validator.check_value_type("epsilon", epsilon, [float], self.name)
        validator.check_value_type("use_locking", use_locking, [bool], self.name)
        self.epsilon = validator.check_number("epsilon", epsilon, 0.0, validator.GT, self.name)
        self.momentum = validator.check_number("momentum", momentum, 0.0, validator.GE, self.name)
        self.rho = validator.check_float_range(rho, 0.0, 1.0, validator.INC_BOTH, "rho", self.name)


class SparseApplyCenteredRMSProp(Primitive):
    r"""
    Update `var` according to the centered RMSProp algorithm.

    .. math::
        \begin{array}{l}
            \text { mean_square }=\text { decay } * \text { mean_square }+(1-\text { decay }) *
            \text { gradient }^{2} \\
            \text { mean_grad }=\text { decay } * \text { mean_grad }+(1-\text { decay }) *
            \text { gradient } \\
            \text { Delta }=l r * \frac{\text { gradient }}{\sqrt{\text { mean_square }+
            \text { epsilon-mean_grad }^{2}}} \\
            \text { ms }<-\text { rho } * \text { ms }_{t-1}+(1-\text { rho }) * \text { grad } * \text { grad } \\
            \text { mom }<-\text { momentum } * \text { mom }_{t-1}+\operatorname{lr} *
            \frac{\text { grad }}{\sqrt{\text { ms+epsilon }}} \\
            \text { var }<-\text { var }-\text { mom }
        \end{array}

    .. warning::
        In dense implementation of this algorithm, `mean_gradient`, `mean_square`, and `moment` will update
        even if the `grad` is zero. But in this sparse implementation, `mean_gradient`, `mean_square`, and `moment`
        will not update in iterations during which the `grad` is zero.

    Args:
        use_locking (bool): If ``True`` , updating of the `var`, `mg`, `ms`, and `mom` tensors will be protected by a
                            lock. Otherwise the behavior is undefined, but may exhibit less contention.
                            Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable tensor to be updated. The data type must be int8, int16, int32, int64,
          uint8, uint16, uint32, uint64, float16, float32 or float64.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **mg** (Parameter) - Mean gradients. Must have the same shape and dtype as `var`.
        - **ms** (Parameter) - Mean square gradients. Must have the same shape and dtype as `var`.
        - **mom** (Parameter) - Delta of `var`. Must have the same shape and dtype as `var`.
        - **lr** (Union[Number, Tensor]) - Learning rate. Must be a float number or a scalar tensor.
          Must have the same type as `var`.
        - **rho** (Union[Number, Tensor]) - Decay rate. Must be a float number or a scalar tensor.
          Must have the same type as `var`.
        - **momentum** (Union[Number, Tensor]) - Momentum. Must be a float number or a scalar tensor.
          Must have the same type as `var`.
        - **epsilon** (Union[Number, Tensor]) - Ridge term. Must be a float number or a scalar tensor.
          Must have the same type as `var`.
        - **grad** (Tensor) - A tensor of the same type as `var` and grad.shape[1:] = var.shape[1:] if rank(var) > 1.
        - **indices** (Tensor) - Gradient indices. Must be one of the following types: int32, int64.
          and indices.shape[0] = grad.shape[0].

    Outputs:
        - **var** (Tensor) - Tensor, has the same shape and data type as `var`.

    Raises:
        TypeError: If `use_locking` is not a bool.
        TypeError: If `var`, `mg`, `ms`, `mom`, `grad`, `indices` is not a Tensor.
        TypeError: If `lr`, `rho`, `momentum` or `epsilon` is neither a Number nor a Tensor.
        TypeError: If dtype of `var`, `mg`, `ms`, `mom`, `lr`, `rho`, `momentum`, `epsilon` or `grad`
                   is neither float16 nor float32.
        TypeError: If dtype of `mg`, `ms`, `mom`, `grad` is not same as `var`.
        TypeError: If dtype of `indices` is not int32 or int64.
        ValueError: If shape of `mg`, `ms` or `mom` is not same as `var`.
        ValueError: If the rank of `indices` is not equal to 1.
        ValueError: If dimension of `grad` is not equal or greater than 1.
        ValueError: If shape of `indices` is not same as shape of first dimension of `grad`.
        ValueError: If shape of `grad` is not same as shape of `var` except first dimension.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> var = Tensor(np.array([[0.6, 0.4], [0.1, 0.5]]).astype(np.float32))
        >>> mg = Tensor(np.array([[0.1, 0.3], [0.1, 0.5]]).astype(np.float32))
        >>> ms = Tensor(np.array([[0.2, 0.1], [0.1, 0.2]]).astype(np.float32))
        >>> mom = Tensor(np.array([[0.2, 0.1], [0.1, 0.2]]).astype(np.float32))
        >>> lr = Tensor(0.001, mstype.float32)
        >>> rho = Tensor(1e-10, mstype.float32)
        >>> momentum = Tensor(0.001, mstype.float32)
        >>> epsilon = Tensor(0.01, mstype.float32)
        >>> grad = Tensor(np.array([[0.3, 0.4], [0.1, 0.2]]).astype(np.float32))
        >>> indices = Tensor(np.array([0, 1]).astype(np.int32))
        >>> sparse_apply_centered_rms_prop = nn_ops.SparseApplyCenteredRMSProp()
        >>> output = sparse_apply_centered_rms_prop(var, mg, ms, mom, lr, rho, momentum, epsilon, grad, indices)
        >>> print(output)
        [[0.5968 0.3959]
         [0.0989 0.4978]]
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('mg', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('ms', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('mom', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T),
        sig.make_sig('rho', dtype=sig.sig_dtype.T),
        sig.make_sig('momentum', dtype=sig.sig_dtype.T),
        sig.make_sig('epsilon', dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1)
    )

    @prim_attr_register
    def __init__(self, use_locking=False):
        """Initialize SparseApplyCenteredRMSProp."""
        self.init_prim_io_names(inputs=['var', 'mg', 'ms', 'mom', 'lr', 'rho', 'momentum',
                                        'epsilon', 'grad', 'indices'],
                                outputs=['var'])
        validator.check_value_type("use_locking", use_locking, [bool], self.name)


class ApplyKerasMomentum(Primitive):
    r"""
    Update `var` according to the momentum scheme.

    .. math::
        \begin{array}{ll} \\
            accum = accum * momentum - grad * lr \\
            var =
            \begin{cases}
                var + accum * momentum - grad * lr, &\text{if use_nesterov} \\
                var + accum, &\text{else}
            \end{cases}
        \end{array}

    Refer to the paper `On the importance of initialization and momentum in deep
    learning <https://dl.acm.org/doi/10.5555/3042817.3043064>`_  for more details.

    Inputs of `var`, `accum` and `grad` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    relatively highest priority data type.
    RuntimeError exception will be thrown when the data type conversion of Parameter is required.

    Args:
        use_locking (bool): If ``True`` , updating of the `var` and `accum` tensors will be protected by a lock;
                            Otherwise the behavior is undefined, but may exhibit less contention. Default: ``False`` .
        use_nesterov (bool): If ``True`` , the tensor passed to compute grad will be var + momentum * accum,
                            so in the end, the var you get is actually var + momentum * accum. Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable to be updated. With float16 or float32 data type.
        - **accum** (Parameter) - Must have the same shape and type as `var`. With float16 or float32 data type.
        - **lr** (Union[Number, Tensor]) - Scaling factor. Must be a scalar. With float16 or float32 data type.
        - **grad** (Tensor) - The gradient. Must have the same shape and type as `var`.
          With float16 or float32 data type.
        - **momentum** (Union[Number, Tensor]) - Momentum. Must be a scalar. With float16 or float32 data type.

    Outputs:
        Tuple of 2 Tensors, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **accum** (Tensor) - The same shape and data type as `accum`.

    Raises:
        TypeError: If the use_locking or use_nesterov is not a bool.
        TypeError: If `var` or `accum` is not a Parameter.
        TypeError: If `lr` is neither a Number nor a Tensor.
        TypeError: If `grad` is not a Tensor.
        TypeError: If `momentum` is neither a Number nor a Tensor.
        TypeError: If dtype of `var`, `accum`, `lr`, `grad`, `momentum` is neither float16 nor float32.
        ValueError: If `accum` or `grad` doesn't have the same shape as `var`.
        ValueError: If the shape size of `lr`, `momentum` is not 0.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> class ApplyKerasMomentumNet(nn.Cell):
        ...     def __init__(self, use_locking=False, use_nesterov=False):
        ...         super(ApplyKerasMomentumNet, self).__init__()
        ...         self.apply_keras_momentum = P.ApplyKerasMomentum(use_locking, use_nesterov)
        ...         self.var = Parameter(Tensor(np.array([[0.2, 0.3], [0.1, 0.4]]).astype(np.float32)), name="var")
        ...         self.accum = Parameter(Tensor(np.array([[0.2, 0.3], [0.1, 0.4]]).astype(np.float32)), name="accum")
        ...     def construct(self, lr, grad, momentum):
        ...         out = self.apply_keras_momentum(self.var, self.accum, lr, grad, momentum)
        ...         return out
        ...
        >>> net = ApplyKerasMomentumNet()
        >>> lr = Tensor(0.001, mstype.float32)
        >>> grad = Tensor(np.array([[0.3, 0.2], [0.4, 0.1]]).astype(np.float32))
        >>> momentum = Tensor(0.99, mstype.float32)
        >>> output = net(lr, grad, momentum)
        >>> print(output)
        (Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 3.97700012e-01,  5.96800029e-01],
        [ 1.98599994e-01,  7.95899987e-01]]), Tensor(shape=[2, 2], dtype=Float32, value=
        [[ 1.97699994e-01,  2.96800017e-01],
        [ 9.86000001e-02,  3.95900011e-01]]))
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T1),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('momentum', dtype=sig.sig_dtype.T2)
    )

    @prim_attr_register
    def __init__(self, use_locking=False, use_nesterov=False):
        """Initialize ApplyKerasMomentum"""
        validator.check_value_type("use_locking", use_locking, [bool], self.name)
        validator.check_value_type("use_nesterov", use_nesterov, [bool], self.name)


class MultilabelMarginLoss(Primitive):
    r"""
    Creates a loss criterion that minimizes the hinge loss for multi-class
    classification tasks.
    It takes a 2D mini-batch Tensor :math:`x` as input and a 2D
    Tensor :math:`y` containing target class indices as output.

    Refer to :func:`mindspore.ops.multilabel_margin_loss` for more details.

    Args:
        reduction (str, optional): Apply specific reduction method to the output: ``'none'`` , ``'mean'`` ,
            ``'sum'`` . Default: ``'mean'`` .

            - ``'none'``: no reduction will be applied.
            - ``'mean'``: compute and return the mean of elements in the output.
            - ``'sum'``: the output elements will be summed.

    Inputs:
        - **x** (Tensor) - Predict data. Tensor of shape :math:`(C)` or :math:`(N, C)`, where :math:`N`
          is the batch size and :math:`C` is the number of classes. Data type must be float16 or float32.
        - **target** (Tensor) - Ground truth data, with the same shape as `input`, data type must be int32 and
          label targets padded by -1.

    Outputs:
        - **y** (Union[Tensor, Scalar]) - The loss of MultilabelMarginLoss. If `reduction` is ``"none"``, its shape
          is :math:`(N)`. Otherwise, a scalar value will be returned.
        - **is_target** (Tensor) - Output tensor for backward input, with the same shape as `target`,
          data type must be int32.

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
       >>> import mindspore
       >>> import numpy as np
       >>> from mindspore import Tensor, ops
       >>> loss = ops.MultilabelMarginLoss()
       >>> x = Tensor(np.array([[0.1, 0.2, 0.4, 0.8], [0.2, 0.3, 0.5, 0.7]]), mindspore.float32)
       >>> target = Tensor(np.array([[1, 2, 0, 3], [2, 3, -1, 1]]), mindspore.int32)
       >>> output = loss(x, target)
       >>> print(output)
       (Tensor(shape=[], dtype=Float32, value= 0.325), Tensor(shape=[2, 4], dtype=Int32, value=
       [[1, 1, 1, 1], [0, 0, 1, 1]]))
    """

    @prim_attr_register
    def __init__(self, reduction='mean'):
        """Initialize MultilabelMarginLoss"""
        self.init_prim_io_names(inputs=['x', 'target'], outputs=['y', 'is_target'])
        self.reduction = validator.check_string(reduction, ['none', 'sum', 'mean'], 'reduction', self.name)


class ApplyAdamWithAmsgrad(Primitive):
    r"""
    Update var according to the Adam algorithm.

    .. math::
        \begin{array}{l1} \\
            lr_t:=learning\_rate*\sqrt{1-\beta_2^t}/(1-\beta_1^t) \\
            m_t:=\beta_1*m_{t-1}+(1-\beta_1)*g \\
            v_t:=\beta_2*v_{t-1}+(1-\beta_2)*g*g \\
            \hat v_t:=max(\hat v_{t-1}, v_t) \\
            var:=var-lr_t*m_t/(\sqrt{\hat v_t}+\epsilon) \\
        \end{array}

    Inputs of `var`, `m`, `v`, `vhat` and `grad` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Inputs of `beta1_power`, `beta1`, `beta2` and `epsilon` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    However, note that there is no implicit type conversion rule between `var` and `beta1_power`;
    the two sets of rules are independent of each other.

    Args:
        beta1 (float): A Tensor. Must have the same type as beta1_power. Momentum factor. Must be a scalar.
        beta2 (float): A Tensor. Must have the same type as beta1_power. Momentum factor. Must be a scalar.
        epsilon (float): A Tensor. Must have the same type as beta1_power. Ridge term. Must be a scalar.
        use_locking (bool): use_locking: If ``True`` , updating of the `var`, `m`, and `v` tensors will
          be protected by a lock; Otherwise the behavior is undefined, but may exhibit less contention.
          Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable to be updated. The data type can be float16 or float32.
        - **m** (Parameter) - The 1st moment vector in the updating formula,
          the shape and data type value should be the same as `var`.
        - **v** (Parameter) - the 2nd moment vector in the updating formula,
          the shape and data type value should be the same as `var`.
        - **vhat** (Parameter) - :math:`\hat v_t` in the updating formula,
          the shape and data type value should be the same as `var`.
        - **beta1_power** (Union[float, Tensor]) - :math:`beta_1^t(\beta_1^{t})` in the updating formula,
          a scalar tensor with float16 or float32 data type.
        - **beta2_power** (Union[float, Tensor]) - :math:`beta_2^t(\beta_2^{t})` in the updating formula,
          a scalar tensor with float16 or float32 data type.
        - **lr** (Union[float, Tensor]) - Scaling factor, a scalar tensor with float16 or float32 data type.
        - **grad** (Tensor) - The gradient, has the same shape and data type as `var`.

    Outputs:
        Tuple of 4 Tensors, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **m** (Tensor) - The same shape and data type as `m`.
        - **v** (Tensor) - The same shape and data type as `v`.
        - **vhat** (Tensor) - The same shape and data type as `vhat`.

    Raises:
        TypeError: If `var`, `m`, `v`, `vhat` is not a Parameter.
        TypeError: If `beta1_power`, `beta2_power`, `lr` is neither a Number nor a Tensor.
        TypeError: If `grad` is not a Tensor.
        TypeError: If dtype of `var`, `m`, `v`, `vhat`, `beta1_power`, `beta2_power`,
          `lr`, `grad`, `momentum` is not float32 or float16.
        ValueError: If `m` or `v` or `vhat` or `grad` doesn't have the same shape of `var`.
        ValueError: If the shape of `beta1_power`, `beta2_power`, `lr` is not 0.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> class ApplyAdamWithAmsgradNet(nn.Cell):
        ...     def __init__(self, beta1=0.9, beta2=0.999, epsilon=1e-8, use_locking=False):
        ...         super(ApplyAdamWithAmsgradNet, self).__init__()
        ...         self.apply_adam_with_amsgrad = P.ApplyAdamWithAmsgrad(beta1, beta2, epsilon, use_locking)
        ...         self.var = Parameter(Tensor(np.array([[0.2, 0.2], [0.2, 0.2]]).astype(np.float32)), name="var")
        ...         self.m = Parameter(Tensor(np.array([[0.1, 0.2], [0.4, 0.3]]).astype(np.float32)), name="m")
        ...         self.v = Parameter(Tensor(np.array([[0.2, 0.1], [0.3, 0.4]]).astype(np.float32)), name="v")
        ...         self.vhat = Parameter(Tensor(np.array([[0.1, 0.2], [0.6, 0.2]]).astype(np.float32)), name="vhat")
        ...     def construct(self, beta1_power, beta2_power, lr, grad):
        ...         out = self.apply_adam_with_amsgrad(self.var, self.m, self.v, self.vhat,
        ...                                            beta1_power, beta2_power, lr, grad)
        ...         return out
        >>> net = ApplyAdamWithAmsgradNet()
        >>> grad = Tensor(np.array([[0.4, 0.2], [0.2, 0.3]]).astype(np.float32))
        >>> output = net(Tensor(0.9, mstype.float32), Tensor(0.999, mstype.float32), Tensor(0.01, mstype.float32), grad)
        >>> print(net.var.asnumpy())
        [[0.19908068 0.1985858 ]
        [0.19844866 0.19849943]]
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('m', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('v', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('vhat', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('beta1_power', dtype=sig.sig_dtype.T1),
        sig.make_sig('beta2_power', dtype=sig.sig_dtype.T2),
        sig.make_sig('lr', dtype=sig.sig_dtype.T3),
        sig.make_sig('grad', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self, beta1=0.9, beta2=0.999, epsilon=1e-8, use_locking=False):
        """Initialize ApplyAdamWithAmsgrad"""
        validator.check_value_type("beta1", beta1, [float], self.name)
        validator.check_value_type("beta2", beta2, [float], self.name)
        validator.check_value_type("epsilon", epsilon, [float], self.name)
        validator.check_value_type("use_locking", use_locking, [bool], self.name)
        self.add_prim_attr("side_effect_mem", True)


class ApplyAdamWithAmsgradV2(Primitive):
    r"""
    Update var according to the Adam algorithm.

    .. math::
        \begin{array}{l1} \\
            lr_t:=learning\_rate*\sqrt{1-\beta_2^t}/(1-\beta_1^t) \\
            m_t:=\beta_1*m_{t-1}+(1-\beta_1)*g \\
            v_t:=\beta_2*v_{t-1}+(1-\beta_2)*g*g \\
            \hat v_t:=\max(\hat v_{t-1}, v_t) \\
            var:=var-lr_t*m_t/(\sqrt{\hat v_t}+\epsilon) \\
        \end{array}

    :math:`t` represents updating step while :math:`m` represents the 1st moment vector,
    :math:`v` represents the 2nd moment vector,  :math:`\hat v_t` represents `vhat`,
    :math:`lr` represents learning rate,
    :math:`g` represents `grad`, :math:`\beta_1, \beta_2` represent `beta1` and `beta2`,
    :math:`\beta_1^{t}` represents `beta1_power`, :math:`\beta_2^{t}` represents `beta2_power`,
    :math:`var` represents the variable to be updated,
    :math:`\epsilon` represents `epsilon`.

    All of the inputs are consistent with implicit type conversion rules,
    which ensure that the data types are the same. If they have different data types, the lower precision data type
    will be converted to the data type with relatively higher precision.

    Args:
        use_locking (bool): If ``True`` , updating of the `var`, `m`, and `v` tensors will
            be protected by a lock; Otherwise the behavior is undefined, but may exhibit less contention.
            Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable to be updated. The data type can be float16, float32 or float64.
        - **m** (Parameter) - The 1st moment vector in the updating formula,
          the shape should be the same as `var`.
        - **v** (Parameter) - The 2nd moment vector in the updating formula,
          the shape should be the same as `var`.
        - **vhat** (Parameter) - :math:`\hat v_t` in the updating formula,
          the shape and data type value should be the same as `var`.
        - **beta1_power** (Union[float, Tensor]) - :math:`beta_1^t(\beta_1^{t})` in the updating formula,
          with float16, float32 or float64 data type.
        - **beta2_power** (Union[float, Tensor]) - :math:`beta_2^t(\beta_2^{t})` in the updating formula,
          with float16, float32 or float64 data type.
        - **lr** (Union[float, Tensor]) - Learning rate, with float16, float32 or float64 data type.
        - **beta1** (Union[float, Tensor]) - Exponential decay rate of the first moment.
          The data type can be float16, float32 or float64.
        - **beta2** (Union[float, Tensor]) - Exponential decay rate of the second moment.
          The data type can be float16, float32 or float64.
        - **epsilon** (Union[float, Tensor]) - A value added to the denominator to ensure numerical stability.
          The data type can be float16, float32 or float64.
        - **grad** (Tensor) - The gradient, has the same shape as `var`.

    Outputs:
        Tuple of 4 Tensors, the updated parameters.

        - **var** (Tensor) - The same shape and data type as `var`.
        - **m** (Tensor) - The same shape and data type as `m`.
        - **v** (Tensor) - The same shape and data type as `v`.
        - **vhat** (Tensor) - The same shape and data type as `vhat`.

    Raises:
        TypeError: If `var`, `m`, `v`, `vhat` is not a Parameter.
        TypeError: If dtype of `var`, `m`, `v`, `vhat`, `beta1_power`, `beta2_power`,
            `lr`, `beta1` , `beta2` , `epsilon` or `grad` is not float64, float32 or float16.
        RuntimeError: If the data type of `var`, `m`, `v` , `vhat` and `grad` conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore.ops as ops
        >>> import mindspore.nn as nn
        >>> from mindspore import Tensor, Parameter
        >>> import numpy as np
        >>> class ApplyAdamWithAmsgradNet(nn.Cell):
        ...     def __init__(self, use_locking=False):
        ...         super(ApplyAdamWithAmsgradNet, self).__init__()
        ...         self.apply_adam_with_amsgrad = ops.ApplyAdamWithAmsgradV2(use_locking)
        ...         self.var = Parameter(Tensor(np.array([[0.2, 0.2], [0.2, 0.2]]).astype(np.float32)), name="var")
        ...         self.m = Parameter(Tensor(np.array([[0.1, 0.2], [0.4, 0.3]]).astype(np.float32)), name="m")
        ...         self.v = Parameter(Tensor(np.array([[0.2, 0.1], [0.3, 0.4]]).astype(np.float32)), name="v")
        ...         self.vhat = Parameter(Tensor(np.array([[0.1, 0.2], [0.6, 0.2]]).astype(np.float32)), name="vhat")
        ...         self.beta1 = 0.8
        ...         self.beta2 = 0.999
        ...         self.epsilon = 1e-8
        ...         self.beta1_power = 0.9
        ...         self.beta2_power = 0.999
        ...         self.lr = 0.01
        ...
        ...     def construct(self, grad):
        ...         out = self.apply_adam_with_amsgrad(self.var, self.m, self.v, self.vhat,
        ...                                            self.beta1_power, self.beta2_power, self.lr,
        ...                                            self.beta1, self.beta2, self.epsilon, grad)
        ...         return out
        >>> net = ApplyAdamWithAmsgradNet()
        >>> grad = Tensor(np.array([[0.4, 0.2], [0.2, 0.3]]).astype(np.float32))
        >>> output = net(grad)
        >>> print(net.var.asnumpy())
        [[0.19886853 0.1985858 ]
        [0.19853032 0.19849943]]
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('m', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('v', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('vhat', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('beta1_power', dtype=sig.sig_dtype.T),
        sig.make_sig('beta2_power', dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T),
        sig.make_sig('beta1', dtype=sig.sig_dtype.T),
        sig.make_sig('beta2', dtype=sig.sig_dtype.T),
        sig.make_sig('epsilon', dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self, use_locking=False):
        """Initialize ApplyAdamWithAmsgradv2"""
        validator.check_value_type("use_locking", use_locking, [bool], self.name)
        self.add_prim_attr("side_effect_mem", True)


class FractionalMaxPool(Primitive):
    r"""
    Performs fractional max pooling on the input.

    Fractional max pooling is similar to regular max pooling, but with the added flexibility of
    allowing the overall reduction ratio `N` to be a non-integer value. In regular max pooling,
    an input set is reduced in size by taking the maximum value of  `N x N` (usually 2x2)
    subsections of the set, with the goal of reducing the set by a factor of `N`, where `N` is an integer.

    In contrast, fractional max pooling uses randomly generated pool sizes that are fairly uniform in size.

    .. warning::
        "pooling_ratio", currently only supports row and col dimension and should be >= 1.0, the first
        and last elements must be 1.0 because pooling on batch and channels dimensions is not allowed.

    Args:
        pooling_ratio (list(float)): Decide the shape of output, is a list of float numbers has length >= 4.
            Pooling ratio for each dimension of value should not be less than 0, currently only support
            for row and col dimension.
        pseudo_random(bool, optional): Generate the pooling sequence either randomly or pseudo-randomly.
            If the pseudo_random parameter is set to ``True`` , the sequence will be generated in a
            pseudo-random fashion, otherwise it will be generated randomly.
            Refer to `Fractional Max-Pooling  <https://arxiv.org/pdf/1412.6071>`_
            by Benjamin Graham to understand the distinction between the two.
            Default: ``False`` .
        overlapping(bool, optional): When set to ``True`` , the values at the boundary of adjacent pooling cells
            will be shared by both cells during pooling process. When set to ``False`` , the values are not reused.
            Default: ``False`` .
        deterministic(bool, optional): If deterministic is set to ``True`` , a fixed pooling region will be used
            in the computation graph, ensuring that the FractionalMaxPool is deterministic.
            This is often used in unit tests. When set to ``False`` , fixed pool regions will not be used.
            Default: ``False`` .
        seed(int, optional): If either seed or seed2 are set to a non-zero value, the random number
            generator will be seeded using the specified seed. If neither seed nor seed2 are set,
            the generator will be seeded by a random seed.
            Default: ``0`` .
        seed2(int, optional): The second seed to avoid seed collision.
            Default: ``0`` .

    Inputs:
        - **x** (Tensor) -The data type must be one of the following types: float32, float64, int32, int64.
          Tensor of shape :math:`(N, H_{in}, W_{in}, C_{in})`.

    Outputs:
        - **y** (Tensor) - the output of FractionalMaxPool, has the same data type with `x`.
          Tensor of shape :math:`(N, H_{out}, W_{out}, C_{out})`.

        - **row_pooling_sequence** (Tensor) - A tensor of type int64, the result list of pool boundary rows.

        - **col_pooling_sequence** (Tensor) - A tensor of type int64, the result list of pool boundary cols.

    Raises:
        TypeError: If data type of `x` is not float32, float64, int32, int64.
        TypeError: If `x` is not a 4D tensor.
        ValueError: If element of `x` equals 0 or is less than 0.
        ValueError: If `pooling_ratio` is a list whose length is not equal to 4.
        ValueError: If the first and last element of `pooling_ratio` is not equal to 1.0.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> x = np.array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]).reshape([1,4,4,1]).astype(np.int64)
        >>> pooling_ratio=[1.0,1.5,1.5,1.0]
        >>> fractionalmaxpool_op = ops.FractionalMaxPool(pooling_ratio=pooling_ratio)
        >>> output = fractionalmaxpool_op(Tensor(x))
        >>> print(output)
        (Tensor(shape=[1, 2, 2, 1], dtype=Int64, value=
        [[[[ 6],
           [ 8]],
          [[14],
           [16]]]]), Tensor(shape=[3], dtype=Int64, value= [0, 2, 4]), Tensor(shape=[3], dtype=Int64, value= [0, 2, 4]))
    """

    @prim_attr_register
    def __init__(self, pooling_ratio, pseudo_random=False, overlapping=False, deterministic=False, seed=0, seed2=0):
        """Initialize FractionalMaxPool."""
        self.init_prim_io_names(inputs=["x"], outputs=["y", "row_pooling_sequence", "col_pooling_sequence"])
        validator.check_value_type('pooling_ratio', pooling_ratio, [list], self.name)
        for item in pooling_ratio:
            validator.check_value_type("pooling_ratio_item", item, float, self.name)
        validator.check_value_type("pseudo_random", pseudo_random, [bool], self.name)
        validator.check_value_type("overlapping", overlapping, [bool], self.name)
        validator.check_value_type("deterministic", deterministic, [bool], self.name)
        validator.check_value_type("seed", seed, [int], self.name)
        validator.check_value_type("seed2", seed2, [int], self.name)


class FractionalMaxPool3DWithFixedKsize(Primitive):
    r"""
    Applies a 3D fractional max pooling to an input signal composed of multiple input planes.
    The max-pooling operation is applied in :math:`(kD, kH, kW)` regions by a stochastic step size determined by
    the target output size `output_shape`.

    The number of output features is equal to the number of input planes.

    Refer to the paper `Fractional MaxPooling by Ben Graham <https://arxiv.org/abs/1412.6071>`_  for more details.

    The input and output data format can be "NCDHW" and "NDHWC". N is the batch size, C is the number of channels,
    D the feature depth, H is the feature height, and W is the feature width.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        ksize (Union[float, tuple]): Size of the pooling window. `ksize` can be a tuple of three values specify a
            shape :math:`(k_D, k_H, k_W)`, or a single int `K` for :math:`(K, K, K)`.
        output_shape (Union[int, tuple]): The target output shape. `output_shape` can be a tuple of three values
            specify a shape :math:`(D_{out}, H_{out}, W_{out})`, or a single float `S` for :math:`(S, S, S)`.
        data_format (str, optional): The optional value for data format.
            Currently support ``'NCDHW'`` and ``'NHDWC'`` . Default: ``'NCDHW'`` .

    Inputs:
        - **x** (Tensor) - The input of FractionalMaxPool3DWithFixedKsize, which is a 4D or 5D tensor.
          Tensor of data type : float16, float32, double, int32, int64.
          Supported shape :math:`(N, C, D_{in}, H_{in}, W_{in})` or :math:`(N, D_{in}, H_{in}, W_{in}, C)`.
        - **random_samples** (Tensor) - The random step of FractionalMaxPool3DWithFixedKsize, which is a 3D tensor.
          Tensor of data type : float16, float32, double, and value is between (0, 1).
          Supported shape :math:`(N, C, 3)`

    Outputs:
        - **y** (Tensor) - A tensor, the output of FractionalMaxPool3DWithFixedKsize.
          Has the same data type with `x`.
          Tensor of shape :math:`(N, C, D_{out}, H_{out}, W_{out})` or :math:`(N, D_{out}, H_{out}, W_{out}, C)`.
        - **argmax** (Tensor) - A tensor, the indices along with the outputs.
          Has the same shape as the `y` and int32 or int64 data type.

    Raises:
        TypeError: If `input_x` is not a 4D or 5D tensor.
        TypeError: If `random_samples` is not a 3D tensor.
        TypeError: If data type of `x` is not float16, float32, double, int32, int64.
        TypeError: If dtype of `random_samples` is not float16, float32, double.
        TypeError: If dtype of `argmax` is not int32, int64.
        ValueError: If `output_shape` is a tuple and if `output_shape` length is not 3.
        ValueError: If `ksize` is a tuple and if `ksize` length is not 3.
        ValueError: If numbers in `output_shape` or `ksize` is not positive.
        ValueError: If `data_format` is neither 'NCDHW' nor 'NDHWC'.
        ValueError: If the first dimension size of `input_x` and `random_samples` is not equal.
        ValueError: If the second dimension size of `input_x` and `random_samples` is not equal.
        ValueError: If the third dimension size of `random_samples` is not 3.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> x = Tensor(np.array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])
        ...       .reshape([1, 1, 2, 2, 4]), mstype.float32)
        >>> random_samples = Tensor(np.array([0.7, 0.7, 0.7]).reshape([1, 1, 3]), mstype.float32)
        >>> ksize = (1, 1, 1)
        >>> output_shape = (1, 1, 2)
        >>> net = ops.FractionalMaxPool3DWithFixedKsize(ksize = ksize, output_shape = output_shape)
        >>> output, argmax = net(x, random_samples)
        >>> print(output)
        [[[[[13. 16.]]]]]
        >>> print(argmax)
        [[[[[12 15]]]]]
    """

    @prim_attr_register
    def __init__(self, ksize, output_shape, data_format="NCDHW"):
        """Initialize FractionalMaxPool3DWithFixedKsize."""
        self.init_prim_io_names(inputs=["x", "random_samples"], outputs=["y", "argmax"])
        validator.check_value_type("ksize", ksize, [int, tuple], self.name)
        self.ksize = ksize
        if isinstance(self.ksize, int):
            self.ksize = (ksize, ksize, ksize)
        if len(self.ksize) != 3:
            raise ValueError(f"For '{self.name}', attr 'ksize' must be an positive int number or a tuple of "
                             f"three positive int numbers, but got {len(self.ksize)} numbers.")
        for item in self.ksize:
            validator.check_positive_int(item, 'ksize item', self.name)
        self.output_shape = validator.check_value_type("output_shape", output_shape, [int, tuple], self.name)
        self.data_format = validator.check_string(data_format, ['NCDHW', 'NDHWC'], 'data_format', self.name)
        self.output_shape = _check_3d_int_or_tuple("output_shape", output_shape,
                                                   self.name, allow_five=False, ret_five=False)
        self.add_prim_attr("ksize", self.ksize)
        self.add_prim_attr("output_shape", self.output_shape)


class FractionalAvgPool(Primitive):
    r"""
    Performs fractional avg pooling on the input.

    Fractional avg pooling is similar to regular avg pooling, but with the added flexibility of
    allowing the overall reduction ratio `N` to be a non-integer value. In regular avg pooling,
    an input set is reduced in size by taking the average value of  `N x N` (usually 2x2)
    subsections of the set, with the goal of reducing the set by a factor of `N`, where `N` is an integer.

    .. warning::
        "pooling_ratio", currently only supports row and col dimension and should be >= 1.0, the first
        and last elements must be 1.0 because we don't allow pooling on batch and channels dimensions.

    Args:
        pooling_ratio (list(float)): Decide the shape of output, is a list of floats that has length >= 4.
            Pooling ratio for each dimension of value should be >=0, currently only support for row and col
            dimension. The first and last elements must be 1.0 because we don't allow pooling on batch and
            channels dimensions.
        pseudo_random(bool, optional): Generate the pooling sequence either randomly or pseudo-randomly.
            If the pseudo_random parameter is set to ``True`` , the sequence will be generated in a
            pseudo-random fashion, otherwise it will be generated randomly.
            Refer to `Fractional Max-Pooling  <https://arxiv.org/pdf/1412.6071>`_
            by Benjamin Graham to understand the distinction between the two.
            Default: ``False`` .
        overlapping(bool, optional): When set to ``True`` , the values at the boundary of adjacent pooling cells
            will be shared by both cells during pooling process. When set to ``False`` , the values are not reused.
            Default: ``False`` .
        deterministic(bool, optional): If deterministic is set to ``True`` , a fixed pooling region will be used
            in the computation graph, ensuring that the FractionalAvgPool is deterministic.
            This is often used in unit tests. When set to ``False`` , fixed pool regions will not be used.
            Default: ``False`` .
        seed(int, optional): If either seed or seed2 are set to a non-zero value, the random number
            generator will be seeded using the specified seed. If neither seed nor seed2 are set,
            the generator will be seeded by a random seed.
            Default: ``0`` .
        seed2(int, optional): The second seed to avoid seed collision.
            Default: ``0`` .

    Inputs:
        - **x** (Tensor) -The data type must be one of the following types: float32, float64, int32, int64.
          Tensor of shape :math:`(N, H_{in}, W_{in}, C_{in})`.

    Outputs:
        - **y** (Tensor) - A tensor, the output of FractionalAvgPool, has the same data type with `x`.
          Tensor of shape :math:`(N, H_{out}, W_{out}, C_{out})`.

        - **row_pooling_sequence** (Tensor) - A tensor of type int64, the result list of pool boundary rows.

        - **col_pooling_sequence** (Tensor) - A tensor of type int64, the result list of pool boundary cols.

    Raises:
        TypeError: If data type of `x` is not float32, float64, int32, int64.
        TypeError: If `x` is not a 4D tensor.
        ValueError: If element of `x` equals 0 or is less than 0.
        ValueError: If `pooling_ratio` is a list whose length is not equal to 4.
        ValueError: If the first and last element of `pooling_ratio` is not equal to 1.0.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> x = np.array([1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]).reshape([1,4,4,1]).astype(np.int64)
        >>> pooling_ratio=[1.0,1.5,1.5,1.0]
        >>> fractionalavgpool_op = ops.FractionalAvgPool(pooling_ratio=pooling_ratio)
        >>> output = fractionalavgpool_op(Tensor(x))
        >>> print(output)
        (Tensor(shape=[1, 2, 2, 1], dtype=Int64, value=
        [[[[ 3],
           [ 5]],
          [[11],
           [13]]]]), Tensor(shape=[3], dtype=Int64, value= [0, 2, 4]), Tensor(shape=[3], dtype=Int64, value= [0, 2, 4]))
    """

    @prim_attr_register
    def __init__(self, pooling_ratio, pseudo_random=False, overlapping=False, deterministic=False, seed=0, seed2=0):
        """Initialize FractionalAvgPool."""
        self.init_prim_io_names(inputs=["x"], outputs=["y", "row_pooling_sequence", "col_pooling_sequence"])
        validator.check_value_type('pooling_ratio', pooling_ratio, [list], self.name)
        for item in pooling_ratio:
            validator.check_value_type("pooling_ratio_item", item, float, self.name)
        validator.check_value_type("pseudo_random", pseudo_random, [bool], self.name)
        validator.check_value_type("overlapping", overlapping, [bool], self.name)
        validator.check_value_type("deterministic", deterministic, [bool], self.name)
        validator.check_value_type("seed", seed, [int], self.name)
        validator.check_value_type("seed2", seed2, [int], self.name)


class NthElement(Primitive):
    r"""
    Computes the n-th smallest values for the last dimension of the input Tensor.

    - When `input` is a 1-D Tensor (i.e. Vector), it finds the nth-smallest value in the vector
      and outputs its value as a scalar Tensor.
    - When `input` is matrices or has higher rank, it finds the nth-smallest value
      in each row (or vector along the last dimension) and outputs
      these values in a Tensor with shape of `values.shape = input.shape[:-1]`.

    Args:
        reverse (bool, optional): An optional bool. If set to ``True`` , it find the :math:`n`-th largest value
          in the vector instead of the nth-smallest. Default: ``False`` .

    Inputs:
        - **input** (Tensor) - Input Tensor with 1-D or higher dimension.
        - **n** (Union[int, Tensor]) -  If the `n` is a Tensor, it should be a 0-D Tensor, dtype is int32.
          Valid range of `n` is :math:`[0, input.shape[-1])` where :math:`input.shape[-1]` is
          last dimension size of `input`.

    Outputs:
        - **values** (Tensor) - Its shape satisfies:  `values`.shape = `input`.shape[:-1].
          The dtype is the same as `input`.

    Raises:
        TypeError**: If the type  of `input` is out of the valid list.
        TypeError**: If `n` is not int32 or not a Tensor.
        ValueError**: If n is out of :math:`[0, input.shape[-1])`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> input = Tensor(np.array([[1,2,3],[4,5,6]]) , mstype.int8)
        >>> n = 1
        >>> net = ops.NthElement()
        >>> out = net(input, n)
        >>> print(out)
        [2 5]
    """

    @prim_attr_register
    def __init__(self, reverse=False):
        """Initialize NthElement."""
        self.reverse = validator.check_value_type("reverse", reverse, [bool], self.name)
        self.add_prim_attr("reverse", self.reverse)
        self.init_prim_io_names(inputs=['input', 'n'],
                                outputs=['output'])


class PSROIPooling(Primitive):
    r"""
    Applies Position Sensitive ROI-Pooling on input Tensor.

    Args:
        spatial_scale (float): a scaling factor that maps the box coordinates to the input coordinates.
                               For example, if your boxes are defined on the scale of a 224x224 image and
                               your input is a 112x112 feature map (resulting from a 0.5x scaling of the original
                               image), you'll want to set this to 0.5.
        group_size (int): the size of the output (in pixels) after the pooling is performed, as (height, width).
        output_dim (int): the dim of the output after the pooling is performed.

    Inputs:
        - **features** (Tensor) - The input features, whose shape must be :math:`(N, C, H, W)`. With data type is
          float16 or float32. This formula should hold: :math:`(C == output\_dim * group\_size * group\_size)`.
        - **rois** (Tensor) - The shape is `(batch, 5, rois_n)`. With data type of float16 or float32.
          The size of first dimension `batch` is batch_size. The size of the second dimension must be `5`.
          The size of third dimension `rois_n` is the number of rois. The value of `rois` like:
          (index, x1, y1, x2, y2). The first element of `rois_n` is the index of the `rois`. And the box coordinates
          in (x1, y1, x2, y2) format where the regions will be taken from. The coordinate must satisfy
          0 <= x1 < x2 and 0 <= y1 < y2.

    Outputs:
        - **out** (Tensor) - The result after pooling. Its shape
          is :math:`(rois.shape[0] * rois.shape[2], output\_dim, group\_size, group\_size)`.

    Raises:
        TypeError: If `spatial_scale` is not a float.
        TypeError: If `group_size` or `output_dim` is not an int.
        TypeError: If `features` or `rois` is not a Tensor.
        TypeError: If dtype of `rois` is not float16 or float32.
        ValueError: If shape of `features` does not satisfy :math:`(C == output\_dim * group\_size * group\_size)`.
        ValueError: If `spatial_scale` is negative.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> features = np.random.randn(4, 3 * 7 * 7, 80, 48)
        >>> features = Tensor.from_numpy(features).astype(mindspore.float32)
        >>> rois = Tensor.from_numpy(
        ...     np.array([[[0.0000],
        ...                [150.3563],
        ...                [200.1320],
        ...                [579.3563],
        ...                [602.3452]],
        ...               [[1.0000],
        ...                [657.1263],
        ...                [302.8564],
        ...                [762.4214],
        ...                [567.9854]],
        ...               [[2.0000],
        ...                [321.3122],
        ...                [232.2410],
        ...                [679.0281],
        ...                [587.6346]],
        ...               [[3.0000],
        ...                [664.1630],
        ...                [387.4919],
        ...                [778.7322],
        ...                [562.7321]]])).astype(mindspore.float32)
        >>> psROIPooling = ops.PSROIPooling(spatial_scale=1.0/16, output_dim=3,
        ...                                       group_size=7)
        >>> out = psROIPooling(features, rois)
        >>> print(out.shape)
        (4, 3, 7, 7)
        >>> print(out.dtype)
        Float32
    """

    @prim_attr_register
    def __init__(self, spatial_scale, group_size, output_dim):
        """Initialize PSROIPooling"""
        validator.check_positive_float(spatial_scale, "spatial_scale", self.name)
        validator.check_positive_int(group_size, "group_size", self.name)
        validator.check_positive_int(output_dim, "output_dim", self.name)
        self.spatial_scale = spatial_scale
        self.group_size = group_size
        self.output_dim = output_dim

        self.add_prim_attr('spatial_scale', self.spatial_scale)
        self.add_prim_attr('group_size', self.group_size)
        self.add_prim_attr('output_dim', self.output_dim)


class TripletMarginLoss(Primitive):
    r"""
    TripletMarginLoss operation.

    Creates a criterion that measures the triplet loss given an input
    tensors :math:`x1`, :math:`x2`, :math:`x3` and a margin with a value greater than :math:`0`.
    This is used for measuring a relative similarity between samples. A triplet
    is composed by `a`, `p` and `n` (i.e., `anchor`, `positive examples` and `negative
    examples` respectively). The shapes of all input tensors should be
    :math:`(N, D)`.

    The distance swap is described in detail in the paper
    `Learning local feature descriptors with triplets and shallow convolutional neural
    networks <http://158.109.8.37/files/BRP2016.pdf>`_
    by V. Balntas, E. Riba et al.

    The loss function for each sample in the mini-batch is:

    .. math::
        L(a, p, n) = \max \{d(a_i, p_i) - d(a_i, n_i) + {\rm margin}, 0\}

    where

    .. math::
        d(x_i, y_i) = \left\lVert {\bf x}_i - {\bf y}_i \right\rVert_p

    Args:
        p (int, optional): The norm degree for pairwise distance. Default: ``2`` .
        eps (float, optional): Default: ``1e-6`` .
        swap (bool, optional): The distance swap. Default: ``False`` .
        reduction (str, optional): Apply specific reduction method to the output: ``'none'`` , ``'mean'`` ,
            ``'sum'`` . Default: ``'mean'`` .

            - ``'none'``: no reduction will be applied.
            - ``'mean'``: compute and return the mean of elements in the output.
            - ``'sum'``: the output elements will be summed.

    Inputs:
        - **x** (Tensor) - A sample randomly selected from the training set. Data type must be BasicType.
        - **positive** (Tensor) - A sample belonging to the same category as x,
          with the same type and shape as `x`.
        - **negative** (Tensor) - A sample belonging to the different class from x,
          with the same type and shape as `x`.
        - **margin** (Tensor) - Make a margin between the positive pair and the negative pair.

    Outputs:
        Union[Tensor, Scalar], if `reduction` is ``"none"``, its shape is :math:`(N)`.
        Otherwise, a scalar value will be returned.

    Raises:
        TypeError: If `x` or `positive` or `negative` or `margin` is not a Tensor.
        TypeError: If dtype of `x` or `positive` or `negative` is not BasicType.
        TypeError: If dtype of `x`, `positive` and `negative` is not the same.
        TypeError: If `margin` is not float32.
        TypeError: If `p` is not an int.
        TypeError: If `eps` is not a float.
        TypeError: If `swap` is not a bool.
        ValueError: If dimensions of input `x`, `positive` and `negative` are
          less than or equal to 1 at the same time.
        ValueError: If the dimension of input `x` or `positive` or `negative`
          is bigger than or equal to 8.
        ValueError: If length of shape of `margin` is not 0.
        ValueError: If shape of `x`, `positive` and `negative` cannot broadcast.
        ValueError: If `reduction` is not one of ``'none'``, ``'mean'``, ``'sum'``.

    Supported Platforms:
        ``GPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> loss = ops.TripletMarginLoss()
        >>> x = Tensor(np.array([[0.3, 0.7], [0.5, 0.5]]), mindspore.float32)
        >>> positive = Tensor(np.array([[0.4, 0.6], [0.4, 0.6]]), mindspore.float32)
        >>> negative = Tensor(np.array([[0.2, 0.9], [0.3, 0.7]]), mindspore.float32)
        >>> margin = Tensor(1.0, mindspore.float32)
        >>> output = loss(x, positive, negative, margin)
        >>> print(output)
        0.8881968
    """

    @prim_attr_register
    def __init__(self, p=2, swap=False, eps=1e-6, reduction="mean"):
        """Initialize TripletMarginLoss"""
        self.init_prim_io_names(inputs=['x', 'positive', 'negative', 'margin'], outputs=['y'])
        validator.check_value_type("p", p, [int], self.name)
        validator.check_value_type("swap", swap, [bool], self.name)
        validator.check_value_type("eps", eps, [float], self.name)
        self.reduction = validator.check_string(reduction, ['none', 'sum', 'mean'], 'reduction', self.name)


class DeformableOffsets(Primitive):
    r"""
    Computes the deformed convolution output with the expected input.

    Refer to :func:`mindspore.ops.deformable_conv2d` for more details.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``
    """

    @prim_attr_register
    def __init__(self,
                 strides,
                 pads,
                 ksize,
                 dilations=(1, 1, 1, 1),
                 data_format="NCHW",
                 deformable_groups=1,
                 modulated=True):
        """Initialize DeformableOffsets"""
        self.init_prim_io_names(inputs=['x', 'offsets'], outputs=['y'])

        self.format = validator.check_string(data_format, ['NCHW', 'NHWC'], 'data_format', self.name)
        pos_c = 1
        if self.format == "NHWC":
            pos_c = 3
        self.add_prim_attr('format', self.format)

        validator.check_size_and_element_type_of_tuple('strides', strides, 4, int, self.name)
        if strides[0] != 1 or strides[pos_c] != 1:
            raise ValueError(f"For '{self.name}', The N and C dimensions of 'strides' must be set to 1.")
        self.add_prim_attr('strides', strides)

        validator.check_size_and_element_type_of_tuple('pads', pads, 4, int, self.name)
        self.add_prim_attr('pads', pads)

        validator.check_size_and_element_type_of_tuple('kernel_size', ksize, 2, int, self.name)
        self.add_prim_attr('ksize', ksize)

        validator.check_size_and_element_type_of_tuple('dilations', dilations, 4, int, self.name)
        if dilations[0] != 1 or dilations[pos_c] != 1:
            raise ValueError(f"For '{self.name}', The N and C dimensions of 'dilations' must be set to 1.")
        self.add_prim_attr('dilations', dilations)

        self.deformable_groups = validator.check_positive_int(deformable_groups, 'deformable_groups', self.name)
        self.add_prim_attr('deformable_groups', self.deformable_groups)

        self.modulated = validator.check_bool(modulated, 'modulated', self.name)
        if self.modulated is not True:
            raise ValueError(f"For '{self.name}', The modulated must be set to True.")
        self.add_prim_attr('modulated', self.modulated)


class Pdist(Primitive):
    r"""
    Computes the p-norm distance between each pair of row vectors in the input.

    Refer to :func:`mindspore.ops.pdist` for more details.

    Note:
        The pdist operator involves exponentiation, the inf/nan calculation result may be generated
        when the float16 input is used. The float32 input is recommended.

    Args:
        p (float, optional): The order of norm distance, :math:`p∈[0, ∞)`. Default: ``2.0`` .

    Inputs:
        - **x** (Tensor) - Input tensor. Supported dtypes: float16, float32 or float64.

    Outputs:
        Tensor, has the same dtype as `x`.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import Tensor, ops
        >>> import numpy as np
        >>> x = Tensor(np.array([[1.0, 1.0], [2.0, 2.0], [3.0, 3.0]]).astype(np.float32))
        >>> op = ops.Pdist(p=2.0)
        >>> y = op(x)
        >>> print(y)
        [1.4142135 2.828427  1.4142135]
    """

    @prim_attr_register
    def __init__(self, p=2.0):
        """Initialize Pdist"""
        validator.check_value_type("p", p, [float], self.name)
        if p < 0:
            raise ValueError('Pdist p must be a non-negative value, but got `{}`.'.format(p))
        self.init_prim_io_names(inputs=['x'], outputs=['y'])


class UpsampleNearest3D(Primitive):
    r"""
    Performs nearest neighbor upsampling operation.

    This operator scale up the volumetric input with specified `output_size` or `scales` factors, using nearest
    neighbor algorithm.

    One of `output_size` or `scales` must be given, and can not specified both at the same time.

    Inputs:
        - **x** (Tensor) - 5D tensor of shape :math:`(N, C, D_{in}, H_{in}, W_{in})`.
          Supporting types: [float16, float32, float64].
        - **output_size** (Union[tuple[int], list[int]]): A tuple or list of int specifying the output volumetric size.
          Default: ``None``.
        - **scales** (Union[tuple[float], list[float]]): A tuple or list of float specifying the upsampling factors.
          Default: ``None``.

    Outputs:
        - **y** (Tensor) - Upsampled output with the same type as `x` , whose shape is
          :math:`(N, C, D_{out}, H_{out}, W_{out})`.

    Raises:
        TypeError: When `output_size` is not ``None`` and `output_size` is not list[int] or tuple[int].
        TypeError: When `scales` is not ``None`` and `scales` is not list[float] or tuple[float].
        TypeError: If dtype of `x` is not int [uint8, float16, float32, float64].
        ValueError: If any value of `output_size` is negative or zero when `output_size` is not ``None``.
        ValueError: If any value of `scales` is negative or zero when `scales` is not ``None``.
        ValueError: If shape of `x` is not 5D.
        ValueError: If none of `scales` and `output_size` is specified or both specified.
        ValueError: If size of `scales` is not equal 3 when `scales` is specified.
        ValueError: If size of `output_size` is not equal 3 when `output_size` is specified.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> x = Tensor(np.array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])
        ...       .reshape([1, 1, 2, 2, 4]), mstype.float32)
        >>> output_size = [3, 4, 5]
        >>> net = ops.UpsampleNearest3D()
        >>> output = net(x, output_size, None)
        >>> print(output)
        [[[[[ 1.  1.  2.  3.  4.]
            [ 1.  1.  2.  3.  4.]
            [ 5.  5.  6.  7.  8.]
            [ 5.  5.  6.  7.  8.]]
           [[ 1.  1.  2.  3.  4.]
            [ 1.  1.  2.  3.  4.]
            [ 5.  5.  6.  7.  8.]
            [ 5.  5.  6.  7.  8.]]
           [[ 9.  9. 10. 11. 12.]
            [ 9.  9. 10. 11. 12.]
            [13. 13. 14. 15. 16.]
            [13. 13. 14. 15. 16.]]]]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize UpsampleNearest3D."""
        self.init_prim_io_names(inputs=['x', 'output_size', 'scales'], outputs=['y'])


class SparseApplyAdagradDA(Primitive):
    r"""
    Update `var` according to the proximal adagrad scheme.

    .. math::
        \begin{array}{ll} \\
            grad_accum += grad \\
            grad_squared_accum += grad * grad \\
            tmp_val=sign(grad_accum) * max\left \{|grad_accum|-l1*global_step, 0\right \}
                    if l1>0 else grad_accum \\
            x_value = -1 * lr * tmp_val \\
            y_value = l2 * global_step * lr + \sqrt{grad_squared_accum} \\
            var = x_value / y_value
        \end{array}

    Inputs of `var`, `grad_accum`, `grad_square_accum` and `grad`
    comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, lower priority data type will be converted to the
    relatively highest priority data type.

    Args:
        use_locking (bool): If ``True`` , updating of the `var` and `accum` tensors will be protected by a lock.
                            Otherwise the behavior is undefined, but may exhibit less contention. Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable to be updated.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **grad_accum** (Parameter) - The dict of mutable tensor grad_accum. Must have the same
          shape and dtype as `var`.
        - **grad_square_accum** (Parameter) - The dict of mutable tensor grad_square_accum.
          Must have the same shape and dtype as `var`.
        - **grad** (Tensor) - A tensor of the same type as `var` and grad.shape[1:] = var.shape[1:] if rank(var) > 1.
        - **indices** (Tensor) - A tensor of indices in the first dimension of `var` and `accum`.
          If there are duplicates in `indices`, the behavior is undefined. Must be one of the
          following types: int32, int64 and indices.shape[0] = grad.shape[0].
        - **lr** (Union[Number, Tensor]) - Scaling factor. Must be a scalar. Must have the same type as `var`.
        - **l1** (Union[Number, Tensor]) -  L1 regularization. Must be a scalar. Must have the same type as `var`.
        - **l2** (Union[Number, Tensor]) -  L2 regularization. Must be a scalar. Must have the same type as `var`.
        - **global_step** (Union[Number, Tensor]) - Training step number. Must be a scalar.
          Must be one of the following types: int32, int64.

    Outputs:
        Tensor, with the same type and shape as 'var'.

    Raises:
        TypeError: If `var`, `grad_accum`, `grad_square_accum` is not a Parameter.
        TypeError: If `grad` is not a Tensor.
        TypeError: If `lr`, `l1`, `l2` or `global_step` is neither a Number nor a Tensor.
        TypeError: If use_locking is not a bool.
        TypeError: If dtype of `var`, `grad_accum`, `grad_square_accum`, `grad_accum` is not the same.
        TypeError: If dtype of `grad_accum`, `grad_square_accum`, `grad_accum`
                     is not same as `var`.
        TypeError: If dtype of `indices` is neither int32 nor int64.
        TypeError: If shape of `indices` is not same as shape of first dimension of `grad`.
        TypeError: If dtype of `global_step` is not int64.
        ValueError: If the shape size of `lr`, `l1`, `l2` and `global_step` is not 0.
        RuntimeError: If the data type of `var`, `grad_accum`, `grad_square_accum` and `grad`
                      conversion of Parameter is not supported.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> var = Parameter(Tensor(np.array([[1,2], [1,2]]).astype(np.float32)))
        >>> grad_accum = Parameter(Tensor(np.array([[2,1], [3,1]]).astype(np.float32)))
        >>> grad_square_accum = Parameter(Tensor(np.array([[4,1], [5,1]]).astype(np.float32)))
        >>> grad = Tensor(np.array([[5,1], [6,1]]).astype(np.float32))
        >>> indices = Tensor(np.array([0, 1], dtype=np.int32))
        >>> lr = Tensor(2, mstype.float32)
        >>> l1 = Tensor(-1, mstype.float32)
        >>> l2 = Tensor(1, mstype.float32)
        >>> global_step=Tensor(1, mstype.int64)
        >>> sparse_apply_adagrad_da = nn_ops.SparseApplyAdagradDA()
        >>> output = sparse_apply_adagrad_da(var, grad_accum, grad_square_accum,
        ...                                  grad, indices, lr, l1, l2, global_step)
        >>> print(output)
        [[-1.8956923 -1.1715728]
         [-2.1420605 -1.1715728]]
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('grad_accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('grad_square_accum', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1),
        sig.make_sig('lr', dtype=sig.sig_dtype.T),
        sig.make_sig('l1', dtype=sig.sig_dtype.T),
        sig.make_sig('l2', dtype=sig.sig_dtype.T),
        sig.make_sig('global_step', dtype=sig.sig_dtype.T2)
    )

    @prim_attr_register
    def __init__(self, use_locking=False):
        """Initialize SparseApplyAdagradDA"""
        self.init_prim_io_names(inputs=['var', 'grad_accum', 'grad_square_accum',
                                        'grad', 'indices', 'lr', 'l1', 'l2', 'global_step'],
                                outputs=['var'])
        validator.check_value_type("use_locking", use_locking, [bool], self.name)


class SparseApplyMomentum(Primitive):
    r"""
    Update relevant entries in '*var' and '*accum' according to the momentum scheme.

    .. math::
        \begin{array}{ll} \\
            accum = accum * momentum + grad \\
            var -= lr * accum
        \end{array}

    Inputs of `var`, `accum` and `grad` comply with the implicit type conversion rules
    to make the data types consistent.
    If they have different data types, lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        use_locking (bool): If ``True`` , the `var` and `accum` tensors will be protected from being updated.
            Default: ``False`` .
        use_nesterov (bool): If `True`, the tensor passed to compute grad will be var + momentum * accum,
            so in the end, the var you get is actually var + momentum * accum. Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable tensor to be updated. The data type must be int8, int16, int32, int64,
          uint8, uint16, uint32, uint64, float16, float32 or float64.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **accum** (Parameter) - Variable tensor to be updated, has the same shape and type as `var`.
        - **lr** (Union[Number, Tensor]) - The learning rate value. Must be a scalar with same type as `var`.
        - **grad** (Tensor) - A tensor for gradient, has the same type as `var`,
          and grad.shape[1:] = var.shape[1:] if rank(var) > 1.
        - **indices** (Tensor) - A tensor of indices in the first dimension of `var` and `accum`.
          If there are duplicates in `indices`, the behavior is undefined. Must be one of the
          following types: int32, int64 and indices.shape[0] = grad.shape[0].
        - **momentum** (Union[Number, Tensor]) - Momentum. Must be a scalar with same type as `var`.

    Outputs:
        - **var** (Tensor) - Tensor, has the same shape and type as 'var'.

    Raises:
        TypeError: If `var`, `accum`, `grad` or `indices` is not a Parameter.
        TypeError: If `lr`, `momentum` is neither a Number nor a Tensor.
        TypeError: If `use_locking` or `use_nesterov` is not a bool.
        TypeError: If dtype of `var`, `accum`, `lr`, `grad`, or `momentum` is not one of int8, int16,
                   int32, int64, uint8, uint16, uint32, uint64, float16, float32, float64.
        TypeError: If dtype of `indices` is neither int32 nor int64.
        ValueError: If the shape of `var`, `accum` or `grad` is rank 0.
        ValueError: If shape of `accum` or `grad` is not same as `var`.
        ValueError: If shape of `indices` is not same as the shape of first dimension of `grad`.
        ValueError: If the shape of `lr` or `momentum` is not rank 0.
        RuntimeError: If the data type of `var`, `accum`, `lr`, `grad` and 'momentum' conversion of Parameter
                      is not supported.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore.ops.operations.nn_ops as nn_ops
        >>> var = Tensor(np.array([[4.1, 7.2], [1.1, 3.0]]).astype(np.float32))
        >>> accum = Tensor(np.array([[2.2, 3.0], [3.1, 0.5]]).astype(np.float32))
        >>> lr = Tensor(0.01, mstype.float32)
        >>> grad = Tensor(np.array([[0.3, 0.2], [0.4, 0.1]]).astype(np.float32))
        >>> indices = Tensor(np.array([0, 1]), mstype.int32)
        >>> momentum = Tensor(0.99, mstype.float32)
        >>> sparse_apply_momentum = nn_ops.SparseApplyMomentum()
        >>> output = sparse_apply_momentum(var, accum, lr, grad, indices, momentum)
        >>> print(output)
        [[4.07522   7.1682997]
         [1.06531   2.99405  ]]
    """

    __mindspore_signature__ = (
        sig.make_sig('var', dtype=sig.sig_dtype.T),
        sig.make_sig('accum', dtype=sig.sig_dtype.T),
        sig.make_sig('lr', dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1),
        sig.make_sig('momentum', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self, use_locking=False, use_nesterov=False):
        """Initialize SparseApplyMomentum"""
        self.init_prim_io_names(inputs=['var', 'accum', 'lr', 'grad', 'indices', 'momentum'],
                                outputs=['var'])
        validator.check_value_type("use_locking", use_locking, [bool], self.name)
        validator.check_value_type("use_nesterov", use_nesterov, [bool], self.name)


class SparseApplyProximalGradientDescent(Primitive):
    r"""
    Sparse update '*var' as FOBOS algorithm with fixed learning rate.

    .. math::
        \begin{array}{ll} \\
            \text{prox_v} = var - alpha \\
            var = sign(\text{prox_v})/(1 + alpha * l2) * \max(\left| \text{prox_v} \right| - alpha * l1,0)
        \end{array}

    Inputs of `var` and `delta` comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, the lower priority data type will be converted to
    the relatively highest priority data type.

    Args:
        use_locking (bool): If ``True`` , the `var` tensors will be protected from being updated.
            Default: ``False`` .

    Inputs:
        - **var** (Parameter) - Variable tensor to be updated. The data type must be int8, int16, int32, int64,
          uint8, uint16, uint32, uint64, float16, float32 or float64.
          The shape is :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **alpha** (Union[Number, Tensor]) - Scaling factor. Must be a scalar with same type as `var`.
        - **l1** (Union[Number, Tensor]) - L1 regularization. Must be a scalar with same type as `var`.
        - **l2** (Union[Number, Tensor]) - l2 regularization. Must be a scalar with same type as `var`.
        - **grad** (Tensor) - A tensor for gradient, has the same type as `var`,
          and grad.shape[1:] = var.shape[1:] if rank(var) > 1.
        - **indices** (Tensor) - A tensor of indices in the first dimension of `var` and `accum`.
          If there are duplicates in `indices`, the behavior is undefined. Must be one of the
          following types: int32, int64 and indices.shape[0] = grad.shape[0].

    Outputs:
        - **var** (Tensor) - Tensor, has the same shape and type as 'var'.

    Raises:
        TypeError: If `var`, `grad` or `indices` is not a Parameter..
        TypeError: If `alpha`, `l1`, `l2` is neither a Number nor a Tensor.
        TypeError: If `use_locking` is not a bool.
        TypeError: If dtype of `var`, `alpha`, `l1`, `l2` or `grad` is not one of int8, int16,
                   int32, int64, uint8, uint16, uint32, uint64, float16, float32, float64.
        TypeError: If dtype of `indices` is neither int32 nor int64.
        ValueError: If the shape of `var` or `grad` is rank 0.
        ValueError: If shape of `grad` is not same as `var`.
        ValueError: If the shape of `alpha`, `l1` or `l2` is not rank 0.
        ValueError: If shape of `indices` is not same as the shape of first dimension of `grad`.
        RuntimeError: If the data type of `var`, `alpha`, `l1`, `l2`, `grad` conversion of Parameter
                      is not supported.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore.ops.operations.nn_ops as nn_ops
        >>> var = Tensor(np.array([[4.1, 7.2], [1.1, 3.0]]).astype(np.float32))
        >>> alpha = Tensor(1.0, mstype.float32)
        >>> l1 = Tensor(1.0, mstype.float32)
        >>> l2 = Tensor(0.0, mstype.float32)
        >>> grad = Tensor(np.array([[1, 1], [1, 1]]).astype(np.float32))
        >>> indices = Tensor(np.array([0, 1]).astype(np.int32))
        >>> sparse_apply_proximal_gradient_descent = nn_ops.SparseApplyProximalGradientDescent()
        >>> output = sparse_apply_proximal_gradient_descent(var, alpha, l1, l2, grad, indices)
        >>> print(output)
        [[2.1 5.2]
         [0.  1. ]]
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('alpha', dtype=sig.sig_dtype.T),
        sig.make_sig('l1', dtype=sig.sig_dtype.T),
        sig.make_sig('l2', dtype=sig.sig_dtype.T),
        sig.make_sig('grad', dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1)
    )

    @prim_attr_register
    def __init__(self, use_locking=False):
        """Initialize SparseApplyProximalGradientDescent."""
        self.init_prim_io_names(inputs=['var', 'alpha', 'l1', 'l2', 'grad', 'indices'],
                                outputs=['var'])
        validator.check_value_type("use_locking", use_locking, [bool], self.name)


class NuclearNorm(Primitive):
    r"""
    Returns the matrix nuclear norm of a given Tensor.

    Attr `dim` specifies which two dimensions of the input `x` to calculate the nuclear norm across. If `dim` is None,
    the nuclear norm will be calculated across all dimensions of input. Because the nuclear norm is the sum of the
    singular values of the matrix, the input at this time should be 2-dimensional. That is, if the input is
    2-dimensional, we compute the nuclear norm of the input matrix. At this point, `dim` should be None. If you set
    `dim`, it also needs to be in the proper range, otherwise it wonn't work. If the input is 3-dimensional and above,
    the attribute `dim` is required. It specifies which two dimensions of input to calculate the nuclear norm across.

    According to the `dim` list, the input Tensor is reordered by `dim`. The two dimensions pointed to by the attribute
    `dim` are placed at the end, and the order of the other dimensions is relatively unchanged. Perform the SVD of each
    slice of the adjusted Tensor to obtain the singular value. Sum all of the singular value of each slice/matrix to
    obtain the nuclear norm.

    Args:
        dim (Union[list(int), tuple(int)], optional): Specifies which two
            dimensions of `x` to calculate the matrix nuclear norm
            across. If `dim` is None, the nuclear norm will be calculated across all dimensions of `x`. The length of
            `dim` should be 2. The value in `dim` should be in this range:[-x_rank, x_rank). x_rank is the dimension of
            Tensor `x`. The value of `dim[0]` or `dim[1]` can not point to the same dimension. Default: ``None`` .
        keepdim (bool, optional): Whether the output Tensor have `dim` retained or not. Default: ``False`` .

    Inputs:
        - **x** (Tensor) - Input to compute the matrix nuclear norm. The dimension of `x` should be greater than or
          equal to 2. Data type must be float32 or float64.

    Outputs:
        Tensor, output Tensor with dimensions in `dim` reduced to 1 will be returned if `keepdim` is `True`;
        otherwise a Tensor with dimensions in `dim` removed is returned. The data type is same as `x`.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If dtype of `x` is neither float32 nor float64.
        TypeError: If dtype of `dim` is neither list(int) nor tuple(int).
        TypeError: If dtype of `keepdim` is not bool.
        ValueError: If dimension of Tensor `x` is less than 2.
        ValueError: If the length of `dim` is not 2 when `dim` is set.
        ValueError: If the dimension of Tensor `x` is not 2 when `dim` is not set.
        ValueError: If `dim[0]` or `dim[1]` point to the same dimension.
        ValueError: If `dim[0]` or `dim[1]` is not in this range:[-x_rank, x_rank).
                    x_rank is the dimension of Tensor `x`.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> input_x = Tensor(np.array([[[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]],
        ...                           [[7.0, 8.0, 9.0], [10.0, 11.0, 12.0]]]), ms.float32)
        >>> dim = [0, 2]
        >>> keepdim = True
        >>> nuclearnorm = nn_ops.NuclearNorm(dim = dim,keepdim = keepdim)
        >>> output = nuclearnorm(input_x)
        >>> print(output)
        [[[15.407588]
        [21.711605]]]
        >>> keepdim = False
        >>> nuclearnorm = nn_ops.NuclearNorm(dim = dim,keepdim = keepdim)
        >>> output = nuclearnorm(input_x)
        >>> print(output)
        [15.407588 21.711605]
        >>> dim = [0, 1]
        >>> keepdim = True
        >>> nuclearnorm = nn_ops.NuclearNorm(dim = dim,keepdim = keepdim)
        >>> output = nuclearnorm(input_x)
        >>> print(output)
        [[[14.212674 15.81139  17.492853]]]
        >>> keepdim = False
        >>> nuclearnorm = nn_ops.NuclearNorm(dim = dim,keepdim = keepdim)
        >>> output = nuclearnorm(input_x)
        >>> print(output)
        [14.212674 15.81139  17.492853]
    """

    @prim_attr_register
    def __init__(self, dim=None, keepdim=False):
        """Initialize NuclearNorm."""
        validator.check_value_type("dim", dim, [list, tuple, type(None)], self.name)
        if dim is not None:
            validator.check_int(len(dim), 2, validator.EQ, 'length of dim_size', self.name)
            validator.check_is_int(dim[0], "dim[0]", self.name)
            validator.check_is_int(dim[1], "dim[1]", self.name)
        else:
            self.add_prim_attr('dim', [1000])
        validator.check_value_type("keepdim", keepdim, [bool], self.name)


class GLU(Primitive):
    r"""
    Computes GLU (Gated Linear Unit activation function) of input tensors.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.glu` for more details.

    Args:
        axis (int, optional): Axis on which to split the input.
            The value of `axis` must be an int within range [-rank(`x`), rank(`x`)).
            Default: ``-1`` , specifying the last dimension.

    Inputs:
        - **x** (Tensor) - Input tensor. `x.shape[axis]` must be even.

    Outputs:
        Tensor, has the same data type with `x`.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> from mindspore import ops, Tensor
        >>> from mindspore import dtype as mstype
        >>> import numpy as np
        >>> axis = 0
        >>> x = Tensor(np.array([0.3220, 0.9545, 0.7879, 0.0975, 0.3698,
        ...                            0.5135, 0.5740, 0.3435, 0.1895, 0.8764,
        ...                            0.4980, 0.9673, 0.9879, 0.6988, 0.9022,
        ...                            0.9304, 0.1558, 0.0153, 0.1559, 0.9852]).reshape([2, 2, 5]), mstype.float32)
        >>> glu = ops.GLU(axis=axis)
        >>> y = glu(x)
        >>> print(y)
        [[[0.20028052 0.6916126  0.57412136 0.06512236 0.26307625]
          [0.3682598  0.3093122  0.17306386 0.10212085 0.63814086]]]
    """

    @prim_attr_register
    def __init__(self, axis=-1):
        """Initialize GLU"""
        validator.check_value_type("axis", axis, [int], self.name)


class FractionalMaxPoolWithFixedKsize(Primitive):
    r"""
    Applies a 2D fractional max pooling to an input signal composed of multiple input planes.
    The max-pooling operation is applied in :math:`(kH, kW)` regions by a stochastic step size determined by
    the target output size `output_shape`.

    The number of output features is equal to the number of input planes.

    Fractional MaxPooling is described in the paper `Fractional Max-Pooling <https://arxiv.org/pdf/1412.6071>`_.

    Args:
        ksize (Union[int, tuple[int]]): Size of the pooling window. `ksize` can be a tuple of two values
          specify a shape :math:`(k_H, k_W)`, or a single int `K` for :math:`(K, K)`.
        output_shape (Union[int, tuple[int]]): The target output shape. `output_shape` can be a
          tuple of two values specify a shape :math:`(H_{out}, W_{out})`, or a single float `S` for :math:`(S, S)`.
        data_format (str, optional): The optional value for data format, is ``'NCHW'`` .
            Default: ``"NCHW"`` .

    Inputs:
        - **input_x** (Tensor) - Tensor of shape :math:`(N, C, H_{in}, W_{in})`,
          with float16, float32, float64, int32, int64 data type.
        - **random_samples** (Tensor) - Tensor of shape :math:`(N, C, 2)`.
          with float16, float32, float64 data type.

    Outputs:
        - **y** (Tensor) - Has the same type as the `input_x`.
          Has the shape :math:`(N, C, H_{out}, W_{out})`.
        - **argmax** (Tensor) -A tensor whose data type must be int64. Has the same shape as the `y`.

    Raises:
        TypeError: If data type of `input_x` is not one of the following: float16, float32, float64, int32, int64.
        TypeError: If data type of `random_samples` is not one of the following: float16, float32, float64.
        ValueError: If `ksize` is not a number and `ksize` is not a tuple of length 2.
        ValueError: If `output_shape` is not a number and `output_shape` is not a tuple of length 2.
        ValueError: If the sum of `ksize` , `output_shape` and
          -1 is larger than the corresponding dimension of `input_x`.
        ValueError: If the dimension of `random_samples` is not 3.
        ValueError: If the first dimension size of `input_x` and `random_samples` is not equal.
        ValueError: If the second dimension size of `input_x` and `random_samples` is not equal.
        ValueError: If the third dimension size of `random_samples` is not 2.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> # the ksize is an int number and the output_shape is a tuple.
        >>> ksize = 2
        >>> output_shape = (2,2)
        >>> data_format = "NCHW"
        >>> input_x = Tensor(np.array([0.3220, 0.9545, 0.7879, 0.0975, 0.3698,
        ...                            0.5135, 0.5740, 0.3435, 0.1895, 0.8764,
        ...                            0.9581, 0.4760, 0.9014, 0.8522, 0.3664,
        ...                            0.4980, 0.9673, 0.9879, 0.6988, 0.9022,
        ...                            0.9304, 0.1558, 0.0153, 0.1559, 0.9852]).reshape([1, 1, 5, 5]), mstype.float32)
        >>> random_samples = Tensor(np.array([[[0.8, 0.8]]]), mstype.float32)
        >>> net = ops.FractionalMaxPoolWithFixedKsize(ksize, output_shape, data_format)
        >>> y, argmax = net(input_x, random_samples)
        >>> print(y)
        [[[[0.9545 0.8764]
           [0.9673 0.9852]]]]
        >>> print(argmax)
        [[[[ 1  9]
           [16 24]]]]
    """

    @prim_attr_register
    def __init__(self, ksize, output_shape, data_format="NCHW"):
        """Initialize FractionalMaxPoolWithFixedKsize."""
        validator.check_value_type('ksize', ksize, [int, tuple], self.name)
        self.ksize = _check_positive_int_or_tuple(
            "ksize", ksize, self.name, allow_four=False, ret_four=False)
        self.add_prim_attr("ksize", self.ksize)
        validator.check_value_type('output_shape', output_shape, [int, tuple], self.name)
        self.output_shape = _check_positive_int_or_tuple(
            "output_shape", output_shape, self.name, allow_four=False, ret_four=False)
        self.add_prim_attr("output_shape", self.output_shape)
        self.data_format = validator.check_string(data_format, ['NCHW'], 'data_format', self.name)
        self.init_prim_io_names(inputs=['input_x', 'random_samples'], outputs=['y', 'argmax'])


class ChannelShuffle(Primitive):
    r"""
    Divide the channels in a tensor of shape :math:`(*, C, H, W)` into :math:`g` group and
    rearrange them as :math:`(*, \frac C g, g, H*W)`, while keeping the original tensor shapes.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.channel_shuffle` for more detail.

    Args:
        group (int): Number of group to divide channels in.

    Inputs:
        - **x** (Tensor) - Tensor to be divided, it has shape :math:`(*, C, H, W)`,
          with float16, float32, int8, int16, int32, int64, uint8, uint16, uint32, uint64 data type.

    Outputs:
        A Tensor, has the same type as the `x`, and has the shape :math:`(*, C, H, W)`.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> group = 2
        >>> x = Tensor(np.arange(1 * 4 * 2 * 2).reshape(1, 4, 2, 2).astype(np.int16))
        >>> channel_shuffle_func = ops.ChannelShuffle(group)
        >>> y = channel_shuffle_func(x)
        >>> print(y)
        [[[[ 0  1]
           [ 2  3]]
           [[ 8  9]
           [10 11]]
           [[ 4  5]
           [ 6  7]]
           [[12 13]
           [14 15]]]]
    """

    @prim_attr_register
    def __init__(self, group):
        """Initialize ChannelShuffle"""
        if not isinstance(group, int):
            raise ValueError(f"For '{self.name}', attr 'group' must be an positive int number")
        self.init_prim_io_names(inputs=['x'], outputs=['y'])


class MaxPoolWithArgmaxV2(Primitive):
    r"""
    Performs max pooling on the input Tensor and returns both max values and indices.

    Typically the input is of shape :math:`(N_{in}, C_{in}, H_{in}, W_{in})`, MaxPool outputs
    regional maximum in the :math:`(H_{in}, W_{in})`-dimension. Given kernel size
    :math:`(h_{ker}, w_{ker})` and stride :math:`(s_0, s_1)`, the operation is as follows:

    .. math::
        \text{output}(N_i, C_j, h, w) = \max_{m=0, \ldots, h_{ker}-1} \max_{n=0, \ldots, w_{ker}-1}
        \text{input}(N_i, C_j, s_0 \times h + m, s_1 \times w + n)

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        kernel_size (Union[int, tuple[int]]): The size of kernel used to take the maximum value and argmax
            value, is an int number that represents height and width of the kernel, or a tuple of
            two int numbers that represent height and width respectively.
        strides (Union[int, tuple[int]], optional): The distance of kernel moving, an int number that represents
            not only the height of movement but also the width of movement, or a tuple of two int numbers that
            represent height and width of movement respectively. Default: ``None`` , meaning that
            `strides = kernel_size`.
        pads (Union[int, tuple[int]], optional): An int number that represents the depth,
            height and width of movement are both strides, or a tuple of two int numbers that represent
            depth, height and width of movement respectively.
            Default: 0.
        dilation (Union[int, tuple[int]], optional): Control the stride of elements in the kernel. Default: ``(1, 1)`` .
        ceil_mode (bool, optional): Whether to use ceil instead of floor to calculate output shape. Default: ``False`` .
        argmax_type (mindspore.dtype, optional) : The dtype for argmax.
            Default: ``mstype.int64`` . [Disabled in Ascend.]

    Inputs:
        - **x** (Tensor) - Tensor of shape :math:`(N_{in}, C_{in}, H_{in}, W_{in})` with data type of int8,
          int16, int32, int64, uint8, uint16, uint32, uint64, float16, float32 or float64 in CPU and GPU,
          with that of float16 in Ascend.

    Outputs:
        Tuple of 2 Tensors, representing the maxpool result and where the max values are generated.

        - **output** (Tensor) - Maxpooling result, with shape :math:`(N_{out}, C_{out}, H_{out}, W_{out})`.
          It has the same data type as `x`.

          .. math::
              H_{out} = \left\lfloor\frac{H_{in} + 2 * \text{pads[0]} - \text{dilation[0]}
               \times (\text{kernel_size[0]} - 1) - 1}{\text{strides[0]}} + 1\right\rfloor

          .. math::
              W_{out} = \left\lfloor\frac{W_{in} + 2 * \text{pads[1]} - \text{dilation[1]}
               \times (\text{kernel_size[1]} - 1) - 1}{\text{strides[1]}} + 1\right\rfloor

        - **argmax** (Tensor) - Index corresponding to the maximum value.
          Data type is int32 or int64 in GPU and CPU, is uint16 in Ascend.

    Raises:
        TypeError: If `x` is not a Tensor.
        ValueError: If length of shape of `x` is not equal to 4.
        TypeError: If `kernel_size` , `strides` , `pads` or `dilation` is not int or tuple.
        ValueError: If `kernel_size`, `strides` or `dilation` is less than 1.
        ValueError: If `pads` is less than 0.
        ValueError: If `pads` is more than half of `kernel_size`.
        ValueError: If `argmax_type` is not mindspore.int64 or mindspore.int32.
        TypeError: If `ceil_mode` is not bool.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.arange(20 * 16 * 50 * 32).reshape((20, 16, 50, 32)), mindspore.float32)
        >>> maxpool_arg_v2_op = ops.MaxPoolWithArgmaxV2(kernel_size=(3, 2), strides=(2, 1))
        >>> output_tensor, argmax = maxpool_arg_v2_op(x)
        >>> print(output_tensor.shape)
        (20, 16, 24, 31)
        >>> print(argmax.shape)
        (20, 16, 24, 31)
    """

    @prim_attr_register
    def __init__(self, kernel_size, strides=None, pads=0, dilation=(1, 1), ceil_mode=False, argmax_type=mstype.int64):
        """Initialize MaxPoolWithArgmaxV2."""
        self.init_prim_io_names(inputs=["x"], outputs=["output", "argmax"])
        validator.check_value_type("ceil_mode", ceil_mode, bool, self.name)
        self.ceil_mode = ceil_mode
        validator.check_value_type("argmax_type", argmax_type, [mstype.Type], self.name)
        argmax_type_valid_values = (mstype.int32, mstype.int64)
        validator.check_type_name("argmax_type", argmax_type, argmax_type_valid_values, self.name)
        if argmax_type == mstype.int32:
            self.add_prim_attr("argmax_type", 3)
        elif argmax_type == mstype.int64:
            self.add_prim_attr("argmax_type", 4)
        else:
            raise ValueError(
                f"For '{self.name}', the 'argmax_type' must be mstype.int32 or mstype.int64, but got {argmax_type}.")
        self.kernel_size = _check_positive_int_or_tuple("kernel_size", kernel_size, self.name, ret_four=True)
        if strides is None:
            strides = kernel_size
        self.strides = _check_positive_int_or_tuple("strides", strides, self.name, ret_four=True)
        self.pads = _check_positive_int_or_tuple("pads", pads, self.name, ret_four=True, strict_positive=False)
        self.dilation = _check_positive_int_or_tuple("dilation", dilation, self.name, ret_four=True)
        self.add_prim_attr("kernel_size", self.kernel_size)
        self.add_prim_attr("strides", self.strides)
        self.add_prim_attr("pads", self.pads)
        self.add_prim_attr("dilation", self.dilation)
        self.add_prim_attr("ceil_mode", self.ceil_mode)


class Dense(Primitive):
    r"""
    The dense connected fusion operator.

    Applies dense connected operator for the input. The implement of the operation is as:

    .. math::
        output = x @ w ^ T + b,

    where :math:`x` is the input tensor, :math:`w` is a weight matrix with the same data type as the :math:`x` ,
    and :math:`b` is a bias vector with the same data type as the :math:`x` (only if `b` is not ``None``).

    Inputs:
        - **x** (Tensor) - The shape must meet the following requirement: :math:`len(x.shape)>0`.
        - **w** (Tensor) - The shape must meet the following requirements:
          If :math:`len(x.shape)>1`, :math:`len(w.shape)=2`. If :math:`len(x.shape)=1`, :math:`len(w.shape)=1`.
          :math:`w.shape[-1]=x.shape[-1]`.
        - **b** (Union[Tensor, None]) - If `b` is not ``None``, the shape must meet the following requirements:
          If :math:`len(x.shape)>1`, :math:`len(b.shape)=0` or :math:`len(b.shape)=1` .
          If :math:`len(b.shape)=1`, :math:`b.shape[0]=w.shape[0]`.
          If :math:`len(x.shape)=1`, :math:`len(b.shape)=0`.

    Outputs:
        If :math:`len(x.shape)>1`, Tensor of shape :math:`(*x.shape[:-1], w.shape[0])`.
        If :math:`len(x.shape)=1`, Tensor of shape :math:`()`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.random.random((4, 5, 6, 7)).astype(np.float32))
        >>> weight = Tensor(np.random.random((6, 7)).astype(np.float32))
        >>> bias = Tensor(np.random.random((6,)).astype(np.float32))
        >>> dense = ops.Dense()
        >>> output = dense(x, weight, bias)
        >>> print(output.shape)
        (4, 5, 6, 6)
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Dense."""
        self.init_prim_io_names(inputs=['x', 'w', 'b'], outputs=["output"])
        self.add_prim_attr("has_bias", True)


class WKV(Primitive):
    r"""
    The WKV computation is similar to AFT(Zhai et al., 2021), but W is now a channel-wise vector multiplied
    by relative position rather than a pairwise matrix in AFT. We also introduce a vector U for separately
    attending to the current token in order to compensate for potential degeneration of W.

    Inputs:
        - **w** (Tensor) - The time_first tensor with data type of float32.
          Input tensor of shape :math:`(hidden\_size,)`.
        - **u** (Tensor]) - The time_decay tensor with data type of float32.
          Input tensor of shape :math:`(hidden\_size,)`.
        - **k** (Tensor) - The key tensor with data type of float32.
          Input tensor of shape :math:`(batch\_size, seq\_length, hidden\_size)`.
        - **v** (Tensor) - The value tensor with data type of float32.
          Input tensor of shape :math:`(batch\_size, seq\_length, hidden\_size)`.
        - **sp** (Tensor) - The states_p tensor with data type of float32.
          Input tensor of shape :math:`(batch\_size, seq\_length, hidden\_size)`.
        - **sq** (Tensor) - The states_q tensor with data type of float32.
          Input tensor of shape :math:`(batch\_size, hidden\_size)`.
        - **sm** (Tensor) - The states_m tensor with data type of float32.
          Input tensor of shape :math:`(batch\_size, hidden\_size)`.

    Outputs:
        Tensor of shape :math:`(batch\_size, seq\_length, hidden\_size)`.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> from mindspore.ops.operations import nn_ops
        >>> b = 32
        >>> t = 2
        >>> c = 128
        >>> w = Tensor(np.random.randn(c).astype(np.float32))
        >>> u = Tensor(np.random.randn(c).astype(np.float32))
        >>> k = Tensor(np.random.randn(b, t, c).astype(np.float32))
        >>> v = Tensor(np.random.randn(b, t, c).astype(np.float32))
        >>> sp = Tensor(np.random.randn(b, c).astype(np.float32))
        >>> sq = Tensor(np.random.randn(b, c).astype(np.float32))
        >>> sm = Tensor(np.random.randn(b, c).astype(np.float32))
        >>> dense = nn_ops.WKV()
        >>> output = dense(w, u, k, v, sp, sq, sm)
        >>> print(output[0].shape)
        (32, 2, 128)
    """

    @prim_attr_register
    def __init__(self):
        """Initialize WKV."""
        self.init_prim_io_names(inputs=["time_first", "time_decay", "key", "value", "sp", "sq", "sm"],
                                outputs=["output", "out_sp", "out_sq", "out_sm"])


class PromptFlashAttention(Primitive):
    r"""
    The interface for fully inference.
    B -- Batch size
    S -- Sequence length
    H -- Hidden size

    Note:
    experiment ops

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        num_heads (int): The number of heads.
        scale_value (float): The scale value indicating the scale coefficient, which is used as the scalar of
          Muls in the calculation. Default: 1.0.
        pre_tokens (int): Previous tokens. Default: 2147483547.
        next_tokens (int): next tokens.  Default: 0.
          indicate the upper triangle, Indicate the number of data blocks involved in the calculation. The value 0
          indicates that the data blocks in the upper triangle are not involved in the calculation
        input_layout (str): the data layout of the input qkv, support `(BSH)` and `(BNSD)`, Default `BSH`.
        num_key_value_heads (int): head numbers of key/value which are used in GQA algorithm.
          The value o indicates if the key and value have the same head nums, use numHeads.  Default: 0.
        sparse_mode (int): Default: 0
        inner_precise (int): 0, float16 high precision. 1, high performance. default 1

    Inputs:
        - **query** (Tensor) - The query tensor with data type of float16 or float32.
          Input tensor of shape :math:`(B, S, H)` / `(B, N, S, D)`.
        - **key** (Tensor) - The key tensor with data type of float16 or float32.
          Input tensor of shape :math:`(B, S, H)` / `(B, N, S, D)`.
        - **value** (Tensor) - The value tensor with data type of float16 or float32.
          Input tensor of shape :math:`(B, S, H)` / `(B, N, S, D)`.
        - **attn_mask** (Tensor) - The attention mask tensor with data type of float16 or float32.
          For each element, 0 indicates retention and 1 indicates discard. Input tensor of shape :math:`(B, 1, S, S)`.
        - **actual_seq_lengths** (Tensor): Describe actual sequence length of each input with data type of int64.
        - **actual_seq_lengths_kv** (Tensor): Describe actual sequence length of each input with data type of int64.
        - **pse_shift** (Tensor) - The position encoding tensor with data type of float16 or float32.
        - **dep_scale1** (Tensor)
        - **quant_scale1** (Tensor)
        - **deq_scale2** (Tensor)
        - **quant_scale2** (Tensor)
        - **quant_offset2** (Tensor)

    Outputs:
        - **attention_out** (Tensor) - Input tensor of shape :math:`(B, S, H)` / `(B, N, S, D)`.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import mindspore.ops.operations.nn_ops as P
        >>> from mindspore import Tensor
        >>> import numpy as np
        >>> B = 1
        >>> N = 16
        >>> S = 256
        >>> D = 16
        >>> query = Tensor(np.ones((B, N, S, D), dtype=np.float16))
        >>> key = Tensor(np.ones((B, N, S, D), dtype=np.float16))
        >>> value = Tensor(np.ones((B, N, S, D), dtype=np.float16))
        >>> attn_mask = Tensor(np.ones((B, 1, S, S), dtype=np.float16))
        >>> pfa = P.PromptFlashAttention(N, input_layout='BNSD')
        >>> out = pfa(query, key, value, attn_mask, None, None, None, None, None, None, None, None)
        >>> print(out.shape)
        (1, 16, 256, 16)
    """

    @prim_attr_register
    def __init__(self, num_heads, scale_value=1.0, pre_tokens=214748647, next_tokens=0, input_layout='BSH',
                 num_key_value_heads=0, sparse_mode=0, inner_precise=1):
        """Initialize PromptFlashAttention."""
        validator.check_value_type('num_heads', num_heads, [int], self.name)
        validator.check_value_type('scale_value', scale_value, [float], self.name)
        validator.check_value_type('pre_tokens', pre_tokens, [int], self.name)
        validator.check_value_type('next_tokens', next_tokens, [int], self.name)
        validator.check_value_type('input_layout', input_layout, [str], self.name)
        validator.check_value_type('num_key_value_heads', num_key_value_heads, [int], self.name)
        validator.check_value_type('sparse_mode', sparse_mode, [int], self.name)
        validator.check_value_type('inner_precise', inner_precise, [int], self.name)
        self.init_prim_io_names(inputs=["query", "key", "value", "attn_mask", "actual_seq_lengths",
                                        "actual_seq_lengths_kv", "pse_shift", "deq_scale1", "quant_scale1",
                                        "deq_scale2", "quant_scale2", "quant_offset2"],
                                outputs=["attention_out"])


class IncreFlashAttention(Primitive):
    r"""
    The interface for fully inference.

    B -- Batch size

    S -- Sequence length

    H -- Hidden size

    .. warning::
        This is an experimental API that is subject to change or deletion.
        If there is no input parameter and no default value, None needs to be passed.

    Args:
    - **num_heads**  (int) - The number of heads.
    - **input_layout** (str) - the data layout of the input qkv, support `(BSH)` and `(BNSD)`. Default `BSH`.
    - **scale_value** (double) - The scale value indicating the scale coefficient, which is used as the scalar of
        Muls in the calculation. Default: 1.0.
    - **num_key_value_heads** (int) - head numbers of key/value which are used in GQA algorithm.
        The value o indicates if the key and value have the same head nums, use numHeads.  Default: 0.
    - **block_size** (int) - Default: 0.
    - **inner_precise** (int) - Default: 1.

    Inputs:
        - **query** (Tensor) - The query tensor with data type of float16 or bfloat16.
          Input tensor of shape :math:`(B, 1, H)` / :math:`(B, N, 1, D)`.
        - **key** (TensorList) - The key tensor with data type of float16 or bfloat16.
          Input tensor of shape :math:`(B, S, H)` / :math:`(B, N, S, D)`.
        - **value** (TensorList) - The value tensor with data type of float16 or bfloat16.
          Input tensor of shape :math:`(B, S, H)` / :math:`(B, N, S, D)`.
        - **attn_mask** (Tensor) - The attention mask tensor with data type of float16 or bool.
          Input tensor of shape :math:`(B, S)` / :math:`(B, 1, S)` / :math:`(B, 1, 1, S)`.
        - **actual_seq_lengths** (Tensor) - Describe actual sequence length of each input with data type of int.
        - **pse_shift** (Tensor) - The position encoding tensor with data type of float16 or float32.
        - **dequant_scale1** (Tensor) - Quantitative parametor, the tensor with data type of uint64.
        - **quant_scale1** (Tensor) - Quantitative parametor, the tensor with data type of float.
        - **dequant_scale2** (Tensor) - Quantitative parametor, the tensor with data type of uint64.
        - **quant_scale2** (Tensor) - Quantitative parametor, the tensor with data type of float.
        - **quant_offset2** (Tensor) - Quantitative parametor, the tensor with data type of float.
        - **antiquant_scale** (Tensor) - Quantitative parametor, the tensor with data type of float.
        - **antiquant_offset** (Tensor) - Quantitative parametor, the tensor with data type of float.
        - **block_table** (Tensor) - The tensor with data type of float.

    Outputs:
        - **attention_out** (Tensor) - Input tensor of shape :math:`(B, 1, H)` / :math:`(B, N, 1, D)`.

    Supported Platforms:
        ``Ascend``
    """

    @prim_attr_register
    def __init__(self, num_heads, input_layout="BSH", scale_value=1.0, num_key_value_heads=0, block_size=0,
                 inner_precise=1):
        """Initialize IncreFlashAttention."""
        validator.check_value_type('num_heads', num_heads, [int], self.name)
        validator.check_value_type('input_layout', input_layout, [str], self.name)
        validator.check_value_type('scale_value', scale_value, [float], self.name)
        validator.check_value_type('num_key_value_heads', num_key_value_heads, [int], self.name)
        validator.check_value_type('block_size', block_size, [int], self.name)
        validator.check_value_type('inner_precise', inner_precise, [int], self.name)
        self.init_prim_io_names(inputs=["query", "key", "value", "attn_mask", "actual_seq_lengths", "pse_shift",
                                        "dequant_scale1", "quant_scale1", "dequant_scale2", "quant_scale2",
                                        "quant_offset2", "antiquant_scale", "antiquant_offset", "block_table"],
                                outputs=["attention_out"])


class FlashAttentionScore(Primitive):
    r"""
    FlashAttentionScore.
    .. math::
        \begin{array}{ll} \\
            y = Dropout(Softmax(Mask(scale_value \mul (real_shift + query * key), attn_mask), -1), keep_prob) \\
            \mul value \\
        \end{array}

    .. warning::
        This is an experimental API that is subject to change or deletion.
    B -- Batch size
    S1 -- Sequence length of query. The value ranges from 1 to 32768 and is a multiple of 16.
    S2 -- Sequence length of key and value. The value ranges from 1 to 32768 and is a multiple of 16.
    N1 -- Num heads of query
    N2 -- Num heads of key and value, and N2 must be a factor of N1
    D -- Head size. Support value: 64, 80, 96, 120, 128 and 256.
    H1 -- Hidden size of query, which equals to N1 * D
    H2 -- Hidden size of key and value, which equals to N2 * D
    Args:
        head_num (int): The head num of query. Default: 1.
        keep_prob (float): The keep probability of dropout. Default: 1.0.
        scale_value (float): The scale factor of score. Default: 1.0.
        pre_tokens (int): Parameter for sparse computation, represents how many tokens are counted forward.
        When sparse_mode is set to 1, 2, 3, or 5, this parameter does not take effect. Default: 2147483647.
        next_tokens (int): Parameter for sparse computation, represents how many tokens are counted backward.
        When sparse_mode is set to 1, 2, 3, or 5, this parameter does not take effect. Default: 2147483647.
        inner_precise (int): The parameter is reserved and not implemented yet. Default: 0.
        input_layout (str): Specifies the layout of input `query`, key and value. The value can be "BSH" or "BNSD".
        Default: "BSH".
        sparse_mode (int): Indicates sparse mode. Default 0.

            - 0: Indicates the defaultMask mode. If attn_mask is not passed, the mask operation is not performed,
              and preTokens and nextTokens(internally assigned as INT_MAX) are ignored. If passed in, the full attn_mask
              matrix (S1 * S2) needs to be passed in, indicating that the part between preTokens and nextTokens needs to
              be calculated.
            - 1: Represents allMask, that is, passing in the complete attn_mask matrix.
            - 2: Representing the leftUpCausal mode corresponds to the lower triangle scenario divided by the left
              vertex, and the optimized attn_mask matrix (2048*2048) is required.
            - 3: Representing the rightDownCausal model corresponds to the lower triangle scene divided by the lower
              right vertex, and the optimized attn_mask matrix (2048*2048) is required.
            - 4: Represents the band scenario, that is, the part between counting preTokens and nextTokens, and the
              optimized attn_mask matrix (2048*2048) is required..
            - 5: Represents the prefix scenario, that is, on the basis of rightDownCasual, a matrix with length S1 and
              width N is added to the left side. The value of N is obtained by the new input prefix, and the N value of
              each Batch axis is different. Not implemented yet.
            - 6: Represents the global scenario, not implemented yet.
            - 7: Represents the dilated scenario, not implemented yet.
            - 8: Represents the block_local scenario, not implemented yet.

    Inputs:
        - **query** (Tensor[float16, bfloat16]) - The query tensor.
          Input tensor of shape :math:`(B, S1, H1)` or `(B, N1, S1, D)`.
        - **key** (Tensor[float16, bfloat16]) - The key tensor.
          Input tensor of shape :math:`(B, S2, H2)` or `(B, N2, S2, D)`.
        - **value** (Tensor[float16, bfloat16]) - The value tensor.
          Input tensor of shape :math:`(B, S2, H2)` or `(B, N2, S2, D)`.
        - **real_shift** (Union[Tensor[float16, bfloat16], None]) - The position embedding code. If S is greater than
          1024 and the mask of the lower triangle is used, enter only the inverse 1024 lines of the lower triangle for
          memory optimization.
          Input tensor of shape :math: `(B, N1, S1, S2)`, `(1, N1, S1, S2)`, `(B, N1, 1024, S2)`, `(1, N1, 1024, S2)`
          or (1024, 1024).
        - **drop_mask** (Union[Tensor[uint8], None]) - The dropout mask tensor.
          Input tensor of shape :math:`(B, N1, S1, S2 // 8) or None`.
        - **padding_mask** (None) - Reserved parameter. Not implemented yet.
        - **attn_mask** (Union[Tensor[uint8], None]) - The attention mask tensor. For each element, 0 indicates
          retention and 1 indicates discard. Input tensor of shape :math:`(B, N1, S1, S2)`, `(B, 1, S1, S2)`, `(S1, S2)`
          or (2048, 2048).
        - **prefix** (Union[Tensor[int64], None]) - N value of each Batch in the prefix sparse calculation scenario.
          Input tensor of shape :math:`(B,)`.

    Outputs:
        - **softmax_max** (Tensor[float32]) - (B, N1, S1, 8)
        - **softmax_sum** (Tensor[float32]) - (B, N1, S1, 8)
        - **softmax_out** (Tensor[float16, bfloat16]) - Useless output, ignore it. Output tensor of shape : `()`
        - **attention_out** (Tensor[float16, bfloat16]) - The output of attention, its shape, and data type
          are the same as the query.

    Supported Platforms:
        ``Ascend910B``
    """

    @prim_attr_register
    def __init__(self, head_num=1, keep_prob=1.0, scale_value=1.0, pre_tokens=2147483647, next_tokens=2147483647,
                 inner_precise=0, input_layout="BSH", sparse_mode=0):
        """Initialize FlashAttentionScore"""
        validator.check_value_type('head_num', head_num, [int], self.name)
        validator.check_value_type('keep_prob', keep_prob, [int, float], self.name)
        validator.check_float(keep_prob, 0.0, validator.GE, "keep_prob", self.name)
        validator.check_float(keep_prob, 1.0, validator.LE, "keep_prob", self.name)
        validator.check_value_type('scale_value', scale_value, [float], self.name)
        validator.check_value_type('pre_tokens', pre_tokens, [int], self.name)
        validator.check_value_type('next_tokens', next_tokens, [int], self.name)
        validator.check_value_type('inner_precise', inner_precise, [int], self.name)
        validator.check_value_type('sparse_mode', sparse_mode, [int], self.name)
        valid_sparse_mode = [0, 1, 2, 3, 4]
        if sparse_mode not in valid_sparse_mode:
            raise ValueError(f"Attribute 'sparse_mode' must be one of {valid_sparse_mode}, but got {sparse_mode}")
        if inner_precise not in [0]:
            raise ValueError(f"Attribute 'inner_precise' must be 0, but got {inner_precise}")
        validator.check_value_type('input_layout', input_layout, [str], self.name)
        support_layout = ["BSH", "BNSD"]
        if input_layout not in support_layout:
            raise ValueError(f"Attribute 'input_layout' must be one of {support_layout}, but got {input_layout}")
        self.init_prim_io_names(
            inputs=['query', 'key', 'value', 'real_shift', 'drop_mask', 'padding_mask', 'attn_mask', 'prefix'],
            outputs=['softmax_max', 'softmax_sum', 'softmax_out', 'attention_out'])


class RmsNorm(Primitive):
    r"""
    The RmsNorm operator is a normalization operation, and its formula is:

    .. math::
        y=\frac{x_i}{\sqrt{\frac{1}{n}}\sum_{i=1}^{n}{ x_i^2}+\varepsilon  }\gamma_i

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        epsilon (float): prevent division by 0, default value is `1e-6`

    Inputs:
        - **input_x** (Tensor) - Input data of RmsNorm, support data type: float16, float32, bfloat16.
        - **gamma** (Tensor) - Support data type: float16, float32, bfloat16.

    Outputs:
        - **y** (Tensor) - Has the same type and shape with `input_x`.
        - **rstd** (Tensor) - Has the same type with `input_x`, used by gradient calculation.

    Raises:
        TypeError: If data type of `input_x` is not one of the following: float16, float32, bfloat16.
        TypeError: If data type of `gamma` is not one of the following: float16, float32, bfloat16.
        TypeError: If data type of "input_x" is not the same with the data type of "gamma"

    Supported Platforms:
        ``Ascend``
    """

    @prim_attr_register
    def __init__(self, epsilon=1e-6):
        """Initialize Dense."""
        validator.check_value_type("epsilon", epsilon, [float], self.name)
        self.init_prim_io_names(inputs=['x', 'gamma'], outputs=["y", "rstd"])
