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

"""basic"""
from __future__ import absolute_import

import math
import numpy as np

import mindspore.common.dtype as mstype
from mindspore import context, log as logger
from mindspore.ops.composite.multitype_ops import _constexpr_utils as const_utils
from mindspore.common.seed import _get_graph_seed
from mindspore.common.tensor import Tensor
from mindspore.common.initializer import initializer, HeUniform, Uniform
from mindspore.ops import operations as P
from mindspore.ops import functional as F
from mindspore.ops.operations import _inner_ops as inner
from mindspore.ops.primitive import constexpr, Primitive, _primexpr
from mindspore.common.parameter import Parameter
from mindspore._extends import cell_attr_register
from mindspore import _checkparam as Validator
from mindspore.nn.cell import Cell
from mindspore.nn.layer.activation import get_activation
from mindspore.common._decorator import deprecated

__all__ = ['Dropout', 'Flatten', 'Dense', 'ClipByNorm', 'Norm', 'OneHot', 'Pad', 'Unfold', 'Tril', 'Triu',
           'MatrixDiag', 'MatrixDiagPart', 'MatrixSetDiag', 'L1Regularizer', 'Dropout1d',
           'Dropout2d', 'Dropout3d', 'Upsample', 'Roll', 'Identity', 'Unflatten']


class L1Regularizer(Cell):
    r"""
    Applies l1 regularization to weights.

    l1 regularization makes weights sparsity.

    .. math::
        \text{loss}=\lambda * \text{reduce_sum}(\text{abs}(\omega))

    where :math:`\lambda` is `scale` .

    Note:
        scale(regularization factor) should be a number which greater than 0.

    Args:
        scale (int, float): l1 regularization factor which greater than 0.

    Inputs:
        - **weights** (Tensor) - The input of L1Regularizer with data type of float16 or float32.
          The shape is :math:`(N,*)` where :math:`*` means, any number of additional dimensions.

    Outputs:
        Tensor, which dtype is higher precision data type between mindspore.float32 and weights dtype,
        and Tensor shape is ()

    Raises:
        TypeError: If `scale` is neither an int nor float.
        ValueError: If `scale` is not greater than 0.
        ValueError: If `scale` is math.inf or math.nan.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore as ms
        >>> import numpy as np
        >>> scale = 0.5
        >>> net = ms.nn.L1Regularizer(scale)
        >>> weights = ms.Tensor(np.array([[1.0, -2.0], [-3.0, 4.0]]).astype(np.float32))
        >>> output = net(weights)
        >>> print(output.asnumpy())
        5.0
    """

    def __init__(self, scale):
        """Initialize L1Regularizer."""
        super(L1Regularizer, self).__init__()
        Validator.check_value_type("scale", scale, [int, float], self.cls_name)
        if scale <= 0:
            raise ValueError(
                f"For '{self.cls_name}', the 'scale' must be greater than 0, but got {scale}.")
        if math.isinf(scale) or math.isnan(scale):
            raise ValueError(
                f"For '{self.cls_name}', the 'scale' can not be INF or NAN, but got {scale}.")
        self.abs = P.Abs()
        self.reduce_sum = P.ReduceSum()
        self.scale = Tensor(scale, dtype=mstype.float32)

    def construct(self, weights):
        const_utils.check_type_valid(
            F.dtype(weights), mstype.number_type, 'weights')
        l1_regularization = self.scale * self.reduce_sum(self.abs(weights))
        return l1_regularization


class Dropout(Cell):
    r"""
    Dropout layer for the input.

    Dropout is a means of regularization that reduces overfitting by preventing correlations between neuronal nodes.
    The operator randomly sets some neurons output to 0 according to `p`, which means the probability of discarding
    during training. And the return will be multiplied by :math:`\frac{1}{1-p}` during training.
    During the reasoning, this layer returns the same Tensor as the `x`.

    This technique is proposed in paper `Dropout: A Simple Way to Prevent Neural Networks from Overfitting
    <http://www.cs.toronto.edu/~rsalakhu/papers/srivastava14a.pdf>`_ and proved to be effective to reduce
    over-fitting and prevents neurons from co-adaptation. See more details in `Improving neural networks by
    preventing co-adaptation of feature detectors
    <https://arxiv.org/pdf/1207.0580.pdf>`_.

    Note:
        - Each channel will be zeroed out independently on every construct call.
        - Parameter `keep_prob` will be removed in a future version, please use parameter `p` instead.
          Parameter `p` means the probability of the element of the input tensor to be zeroed.
        - Parameter `dtype` will be removed in a future version. It is not recommended to define this parameter.

    Args:
        keep_prob (float): Deprecated. The keep rate, greater than 0 and less equal than 1.
            E.g. rate=0.9, dropping out 10% of input neurons. Default: ``0.5`` .
        p (Union[float, int, None]): The dropout rate, greater than or equal to 0 and less than 1.
            E.g. rate=0.9, dropping out 90% of input neurons. Default: ``None`` .
        dtype (:class:`mindspore.dtype`): Data type of `input`. Default: ``mstype.float32`` .

    Inputs:
        - **x** (Tensor) - The input of Dropout with data type of float16 or float32.

    Outputs:
        Tensor, output tensor with the same shape as the `x`.

    Raises:
        TypeError: If `keep_prob` is not a float.
        TypeError: If the dtype of `p` is not float or int.
        TypeError: If dtype of `x` is not neither float16 nor float32.
        ValueError: If `keep_prob` is not in range (0, 1].
        ValueError: If `p` is not in range [0, 1).
        ValueError: If length of shape of `x` is less than 1.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, nn
        >>> import numpy as np
        >>> x = Tensor(np.ones([2, 2, 3]), mindspore.float32)
        >>> net = nn.Dropout(p=0.2)
        >>> net.set_train()
        >>> output = net(x)
        >>> print(output.shape)
        (2, 2, 3)
    """

    def __init__(self, keep_prob=0.5, p=None, dtype=mstype.float32):
        """Initialize Dropout."""
        super(Dropout, self).__init__()
        if dtype != mstype.float32:
            logger.warning(
                "This parameter `dtype` will be deleted or invisible in the future. Please don't use it.")
        if p is None:
            logger.warning("For Dropout, this parameter `keep_prob` will be deprecated, please use `p` instead.")
            Validator.check_value_type('keep_prob', keep_prob, [float], self.cls_name)
            if keep_prob <= 0 or keep_prob > 1:
                raise ValueError(f"For '{self.cls_name}', the 'keep_prob' must be a number in range (0, 1], "
                                 f"but got {keep_prob}.")
            seed0, seed1 = _get_graph_seed(0, "dropout")
            self.dropout = P.Dropout(keep_prob, seed0, seed1)
        else:
            Validator.check_value_type('p', p, [float, int], self.cls_name)
            if p < 0 or p >= 1:
                raise ValueError(f"For '{self.cls_name}', the 'p' must be a number in range [0, 1), "
                                 f"but got {p}.")
            seed0, seed1 = _get_graph_seed(0, "dropout")
            self.dropout = P.Dropout(1.0 - p, seed0, seed1)
        self.p = p
        self.keep_prob = keep_prob

    def construct(self, x):
        if not self.training or self.keep_prob == 1 or self.p == 0:
            return x

        out, _ = self.dropout(x)
        return out

    def extend_repr(self):
        if self.p is None:
            logger.warning("For Dropout, this parameter `keep_prob` will be deprecated, please use `p` instead.")
            return f'keep_prob={self.keep_prob}'
        return f'p={self.p}'


class Dropout1d(Cell):
    r"""
    During training, randomly zeroes entire channels of the input tensor with probability `p`
    from a Bernoulli distribution (For a 3-dimensional tensor with a shape of :math:`(N, C, L)`,
    the channel feature map refers to a 1-dimensional feature map with the shape of :math:`L`).

    For example, the :math:`j\_th` channel of the :math:`i\_th` sample in the batched input is a to-be-processed
    `1D` tensor input[i,j].
    Each channel will be zeroed out independently on every forward call with probability `p` using samples
    from a Bernoulli distribution.

    The paper `Dropout: A Simple Way to Prevent Neural Networks from Overfitting
    <http://www.cs.toronto.edu/~rsalakhu/papers/srivastava14a.pdf>`_ mentioned this technology, And it is proved that
    it can effectively reduce over fitting and prevent neuronal coadaptation.
    For more details, refer to `Improving neural networks by preventing co-adaptation of feature detectors
    <https://arxiv.org/pdf/1207.0580.pdf>`_ .

    `Dropout1d` can improve the independence between channel feature maps.

    Args:
        p (float, optional): The dropping probability of a channel, between 0 and 1, e.g. `p` = 0.8,
            which means an 80% chance of being set to 0. Default: ``0.5`` .

    Inputs:
        - **x** (Tensor) - A tensor with shape :math:`(N, C, L)` or :math:`(C, L)`, where `N` is the batch size,
          `C` is the number of channels, `L` is the feature length. The data type must be int8, int16, int32,
          int64, float16, float32 or float64.

    Outputs:
        Tensor, has the same shape and data type as `x`.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If the data type of `p` is not float.
        ValueError: If `p` is out of the range `[0.0, 1.0]`.
        ValueError: If the shape of `x` is not `2D` or `3D`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> op = ms.nn.Dropout1d(p=0.6)
        >>> op.training = True
        >>> a = ms.Tensor(np.ones((3, 3)), ms.float32)
        >>> output = op(a)
    """

    def __init__(self, p=0.5):
        """Initialize Dropout1d."""
        super(Dropout1d, self).__init__()
        Validator.check_value_type('p', p, [float], self.cls_name)
        if p < 0 or p > 1:
            raise ValueError(f"For '{self.cls_name}', the 'p' must be a number in range [0, 1], "
                             f"but got {p}.")
        self.prob = p

    def construct(self, x):
        if not self.training or self.prob == 0:
            return x

        out = F.dropout1d(x, self.prob)
        return out


class Dropout2d(Cell):
    r"""
    During training, randomly zeroes some channels of the input tensor with probability `p`
    from a Bernoulli distribution (For a 4-dimensional tensor with a shape of :math:`NCHW`,
    the channel feature map refers to a 2-dimensional feature map with the shape of :math:`HW`).

    For example, the :math:`j\_th` channel of the :math:`i\_th` sample in the batched input is a to-be-processed
    `2D` tensor input[i,j].
    Each channel will be zeroed out independently on every forward call with probability `p` using samples
    from a Bernoulli distribution.

    `Dropout2d` can improve the independence between channel feature maps.

    Refer to :func:`mindspore.ops.dropout2d` for more details.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, nn
        >>> import numpy as np
        >>> dropout = nn.Dropout2d(p=0.5)
        >>> x = Tensor(np.ones([2, 1, 2, 3]), mindspore.float32)
        >>> output = dropout(x)
        >>> print(output.shape)
        (2, 1, 2, 3)
    """

    def __init__(self, p=0.5):
        """Initialize Dropout2d."""
        super(Dropout2d, self).__init__()
        Validator.check_value_type('p', p, [float], self.cls_name)
        if p < 0 or p > 1:
            raise ValueError(f"For '{self.cls_name}', the 'p' must be a number in range [0, 1], "
                             f"but got {p}.")
        self.keep_prob = 1.0 - p
        self.dropout2d = P.Dropout2D(self.keep_prob)

    def construct(self, x):
        if not self.training or self.keep_prob == 1:
            return x

        out, _ = self.dropout2d(x)
        return out

    def extend_repr(self):
        return f"p={self.keep_prob}"


class Dropout3d(Cell):
    r"""
    During training, randomly zeroes some channels of the input tensor
    with probability `p` from a Bernoulli distribution (For a 5-dimensional tensor with
    a shape of :math:`NCDHW`, the channel feature map refers to a 3-dimensional feature
    map with a shape of :math:`DHW`).

    For example, the :math:`j\_th` channel of the :math:`i\_th` sample in the batched input is a to-be-processed
    `3D` tensor input[i,j].
    Each channel will be zeroed out independently on every forward call which based on Bernoulli distribution
    probability `p`.

    `Dropout3d` can improve the independence between channel feature maps.

    Refer to :func:`mindspore.ops.dropout3d` for more details.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, nn
        >>> import numpy as np
        >>> dropout = nn.Dropout3d(p=0.5)
        >>> x = Tensor(np.ones([2, 1, 2, 1, 2]), mindspore.float32)
        >>> output = dropout(x)
        >>> print(output.shape)
        (2, 1, 2, 1, 2)
    """

    def __init__(self, p=0.5):
        """Initialize Dropout3d."""
        super(Dropout3d, self).__init__()
        Validator.check_value_type('p', p, [float], self.cls_name)
        if p < 0 or p > 1:
            raise ValueError(f"For '{self.cls_name}', the 'p' must be a number in range [0, 1], "
                             f"but got {p}.")
        self.keep_prob = 1.0 - p
        self.dropout3d = P.Dropout3D(self.keep_prob)

    def construct(self, x):
        if not self.training or self.keep_prob == 1:
            return x

        out, _ = self.dropout3d(x)
        return out

    def extend_repr(self):
        return f'p={self.keep_prob}'


class Upsample(Cell):
    r"""
    For details, please refer to :func:`mindspore.ops.interpolate`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore as ms
        >>> x = ms.Tensor([[[[1.0, 2.0, 3.0, 4.0], [5.0, 6.0, 7.0, 8.0]]]])
        >>> upsample = ms.nn.Upsample(size=(5, 5))
        >>> out = upsample(x)
        >>> print(x.asnumpy())
        [[[[1. 2. 3. 4.]
           [5. 6. 7. 8.]]]]
        >>> print(out.asnumpy())
        [[[[1. 1. 2. 3. 4.]
           [1. 1. 2. 3. 4.]
           [1. 1. 2. 3. 4.]
           [5. 5. 6. 7. 8.]
           [5. 5. 6. 7. 8.]]]]
        >>> print(out.shape)
        (1, 1, 5, 5)
    """

    def __init__(self, size=None, scale_factor=None, mode="nearest", align_corners=None, recompute_scale_factor=None):
        """Initialize Upsample."""
        super(Upsample, self).__init__()
        self.size = size
        self.scale_factor = scale_factor
        self.mode = mode
        self.align_corners = align_corners
        self.recompute_scale_factor = recompute_scale_factor

    def construct(self, x):
        out = F.interpolate(x, self.size, self.scale_factor, self.mode,
                            self.align_corners, self.recompute_scale_factor)
        return out


class Flatten(Cell):
    r"""
    Flatten the input Tensor along dimensions from `start_dim` to `end_dim`.

    Args:
        start_dim (int, optional): The first dimension to flatten. Default: ``1`` .
        end_dim (int, optional): The last dimension to flatten. Default: ``-1`` .

    Inputs:
        - **x** (Tensor) - The input Tensor to be flattened.

    Outputs:
        Tensor. If no dimensions are flattened, returns the original `x`, otherwise return the flattened Tensor.
        If `x` is a 0-dimensional Tensor, a 1-dimensional Tensor will be returned.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If `start_dim` or `end_dim` is not int.
        ValueError: If `start_dim` is greater than `end_dim` after canonicalized.
        ValueError: If `start_dim` or `end_dim` is not in range of [-x.dim, x.dim-1].

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, nn
        >>> import numpy as np
        >>> x = Tensor(np.array([[[1.2, 1.2], [2.1, 2.1]], [[2.2, 2.2], [3.2, 3.2]]]), mindspore.float32)
        >>> net = nn.Flatten()
        >>> output = net(x)
        >>> print(output)
        [[1.2 1.2 2.1 2.1]
         [2.2 2.2 3.2 3.2]]
        >>> print(f"before flatten the x shape is {x.shape}")
        before flatten the x shape is  (2, 2, 2)
        >>> print(f"after flatten the output shape is {output.shape}")
        after flatten the output shape is (2, 4)
    """

    def __init__(self, start_dim=1, end_dim=-1):
        """Initialize Flatten."""
        super(Flatten, self).__init__()
        self.start_dim = start_dim
        self.end_dim = end_dim

    def check_axis_valid(self, axis, ndim):
        if axis < -ndim or axis >= ndim:
            raise ValueError("'start_dim' or 'end_dim' out of range.")

    def construct(self, x):
        x_rank = F.rank(x)
        ndim = x_rank if x_rank != 0 else 1
        self.check_axis_valid(self.start_dim, ndim)
        self.check_axis_valid(self.end_dim, ndim)
        return F.flatten(x, start_dim=self.start_dim, end_dim=self.end_dim)


class Identity(Cell):
    r"""
    A placeholder identity operator that returns the same as input.

    Inputs:
        - **x** (Any) - The input of Identity.

    Outputs:
        The same as `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, nn
        >>> import numpy as np
        >>> x = Tensor(np.array([1, 2, 3, 4]), mindspore.int64)
        >>> net = nn.Identity()
        >>> output = net(x)
        >>> print(output)
        [1 2 3 4]
    """

    def __init__(self):
        """Initialize Identity."""
        super(Identity, self).__init__()

    def construct(self, x):
        return x


class Dense(Cell):
    r"""
    The dense connected layer.

    Applies dense connected layer for the input. This layer implements the operation as:

    .. math::
        \text{outputs} = \text{activation}(\text{X} * \text{kernel} + \text{bias}),

    where :math:`X` is the input tensors, :math:`\text{activation}` is the activation function passed as the activation
    argument (if passed in), :math:`\text{kernel}` is a weight matrix with the same
    data type as the :math:`X` created by the layer, and :math:`\text{bias}` is a bias vector
    with the same data type as the :math:`X` created by the layer (only if has_bias is True).

    Args:
        in_channels (int): The number of channels in the input space.
        out_channels (int): The number of channels in the output space.
        weight_init (Union[Tensor, str, Initializer, numbers.Number]): The trainable weight_init parameter. The dtype
            is same as `x`. The values of str refer to the function `initializer`. Default: ``None`` ,
            weight will be initialized using HeUniform.
        bias_init (Union[Tensor, str, Initializer, numbers.Number]): The trainable bias_init parameter. The dtype is
            same as `x`. The values of str refer to the function `initializer`. Default: ``None`` ,
            bias will be initialized using Uniform.
        has_bias (bool): Specifies whether the layer uses a bias vector :math:`\text{bias}`. Default: ``True``.
        activation (Union[str, Cell, Primitive, None]): activate function applied to the output of the fully connected
            layer. Both activation name, e.g. 'relu', and mindspore activation function, e.g. mindspore.ops.ReLU(),
            are supported. Default: ``None`` .
        dtype (:class:`mindspore.dtype`): Data type of Parameter. Default: ``mstype.float32`` .

    Inputs:
        - **x** (Tensor) - Tensor of shape :math:`(*, in\_channels)`. The `in_channels` in `Args` should be equal
          to :math:`in\_channels` in `Inputs`.

    Outputs:
        Tensor of shape :math:`(*, out\_channels)`.

    Raises:
        TypeError: If `in_channels` or `out_channels` is not an int.
        TypeError: If `has_bias` is not a bool.
        TypeError: If `activation` is not one of str, Cell, Primitive, None.
        ValueError: If length of shape of `weight_init` is not equal to 2 or shape[0] of `weight_init`
                    is not equal to `out_channels` or shape[1] of `weight_init` is not equal to `in_channels`.
        ValueError: If length of shape of `bias_init` is not equal to 1
                    or shape[0] of `bias_init` is not equal to `out_channels`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, nn
        >>> import numpy as np
        >>> x = Tensor(np.array([[180, 234, 154], [244, 48, 247]]), mindspore.float32)
        >>> net = nn.Dense(3, 4)
        >>> output = net(x)
        >>> print(output.shape)
        (2, 4)
    """

    @cell_attr_register(attrs=['has_bias', 'activation'])
    def __init__(self,
                 in_channels,
                 out_channels,
                 weight_init=None,
                 bias_init=None,
                 has_bias=True,
                 activation=None,
                 dtype=mstype.float32):
        """Initialize Dense."""
        super(Dense, self).__init__()
        self.in_channels = Validator.check_positive_int(
            in_channels, "in_channels", self.cls_name)
        self.out_channels = Validator.check_positive_int(
            out_channels, "out_channels", self.cls_name)
        self.has_bias = Validator.check_bool(
            has_bias, "has_bias", self.cls_name)
        self.reshape = P.Reshape()
        self.shape_op = P.Shape()

        if isinstance(weight_init, Tensor):
            if weight_init.ndim != 2 or weight_init.shape[0] != out_channels or \
                    weight_init.shape[1] != in_channels:
                raise ValueError(f"For '{self.cls_name}', weight init shape error. The ndim of 'weight_init' must "
                                 f"be equal to 2, and the first dim must be equal to 'out_channels', and the "
                                 f"second dim must be equal to 'in_channels'. But got 'weight_init': {weight_init}, "
                                 f"'out_channels': {out_channels}, 'in_channels': {in_channels}.")
        if weight_init is None:
            weight_init = HeUniform(math.sqrt(5))
        self.weight = Parameter(initializer(
            weight_init, [out_channels, in_channels], dtype=dtype), name="weight")

        self.bias = None
        if self.has_bias:
            if isinstance(bias_init, Tensor):
                if bias_init.ndim != 1 or bias_init.shape[0] != out_channels:
                    raise ValueError(f"For '{self.cls_name}', bias init shape error. The ndim of 'bias_init' must "
                                     f"be equal to 1, and the first dim must be equal to 'out_channels'. But got "
                                     f"'bias_init': {bias_init}, 'out_channels': {out_channels}.")
            if bias_init is None:
                bound = 1 / math.sqrt(in_channels)
                bias_init = Uniform(scale=bound)
            self.bias = Parameter(initializer(
                bias_init, [out_channels], dtype=dtype), name="bias")
            self.bias_add = P.BiasAdd()

        self.matmul = P.MatMul(transpose_b=True)
        self.activation = get_activation(activation) if isinstance(
            activation, str) else activation
        if activation is not None and not isinstance(self.activation, (Cell, Primitive)):
            raise TypeError(f"For '{self.cls_name}', the 'activation' must be str or Cell or Primitive, but got "
                            f"{type(activation).__name__}.")
        self.activation_flag = self.activation is not None

    def construct(self, x):
        x_shape = self.shape_op(x)
        if len(x_shape) != 2:
            x = self.reshape(x, (-1, x_shape[-1]))
        x = self.matmul(x, self.weight)
        if self.has_bias:
            x = self.bias_add(x, self.bias)
        if self.activation_flag:
            x = self.activation(x)
        if len(x_shape) != 2:
            out_shape = x_shape[:-1] + (F.shape(x)[-1],)
            x = self.reshape(x, out_shape)
        return x

    def extend_repr(self):
        s = f'input_channels={self.in_channels}, output_channels={self.out_channels}'
        if self.has_bias:
            s += f', has_bias={self.has_bias}'
        if self.activation_flag:
            s += f', activation={self.activation}'
        return s


@constexpr
def _is_equal_one(x):
    if x is None:
        return False
    return F.equal(F.reduce_mean(x), 1.0)


@constexpr
def _dtype_check(x_dtype, prim_name=None):
    msg_prefix = f"For '{prim_name}', the" if prim_name else "The"
    if x_dtype not in [mstype.float32, mstype.float16]:
        raise TypeError(
            f"{msg_prefix} x_dtype must be float32 or float16, but got {x_dtype}.")


@constexpr
def _is_float_dtype(dtype):
    if dtype in [mstype.float32, mstype.float16]:
        return True
    return False


@constexpr
def _need_reduce_all(axis):
    if axis == ():
        return True
    return False


class ClipByNorm(Cell):
    r"""
    Clips tensor values to a maximum :math:`L_2`-norm.

    The output of this layer remains the same if the :math:`L_2`-norm of the input tensor
    is not greater than the argument clip_norm. Otherwise the tensor will be normalized as:

    .. math::
        \text{output}(X) = \frac{\text{clip_norm} * X}{L_2(X)},

    where :math:`L_2(X)` is the :math:`L_2`-norm of :math:`X`.

    Args:
        axis (Union[None, int, tuple(int)]): Compute the L2-norm along the Specific dimension.
                                            Default: ``None`` , all dimensions to calculate.

    Inputs:
        - **x** (Tensor) - Tensor of shape N-D. The type must be float32 or float16.
        - **clip_norm** (Tensor) - A scalar Tensor of shape :math:`()` or :math:`(1)`.
          Or a tensor shape can be broadcast to input `x` shape.

    Outputs:
        Tensor, clipped tensor with the same shape as the `x`, whose type is float32.

    Raises:
        TypeError: If `axis` is not one of None, int, tuple.
        TypeError: If dtype of `x` is neither float32 nor float16.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, nn
        >>> import numpy as np
        >>> net = nn.ClipByNorm()
        >>> x = Tensor(np.random.randint(0, 10, [4, 16]), mindspore.float32)
        >>> clip_norm = Tensor(np.array([100]).astype(np.float32))
        >>> output = net(x, clip_norm)
        >>> print(output.shape)
        (4, 16)

    """

    def __init__(self, axis=None):
        """Initialize ClipByNorm."""
        super(ClipByNorm, self).__init__()
        self.clip_by_norm = inner.ClipByNorm(axis)

    def construct(self, x, clip_norm):
        values_clip = self.clip_by_norm(x, clip_norm)
        return values_clip


class Norm(Cell):
    r"""
    The Norm class will be deprecated in the future,
    this function can be replaced by :func:`ops.norm`
    """

    @deprecated("2.0", "ops.norm", False)
    def __init__(self, axis=(), keep_dims=False):
        """Initialize Norm."""
        super(Norm, self).__init__()
        Validator.check_value_type(
            "keep_dims", keep_dims, [bool], self.cls_name)
        self.axis = axis
        self.keep_dims = keep_dims
        self.reduce_sum = P.ReduceSum(True)
        self.sqrt = P.Sqrt()
        self.squeeze = P.Squeeze(self.axis)

    def construct(self, x):
        x = self.sqrt(self.reduce_sum(F.square(x), self.axis))

        if not self.keep_dims:
            x = self.squeeze(x)
        return x

    def extend_repr(self):
        return f'axis={self.axis}, keep_dims={self.keep_dims}'


class OneHot(Cell):
    """
    The OneHot class will be deprecated in the future,
    this function can be replaced by :func:`ops.one_hot`
    """

    @deprecated("2.0", "ops.one_hot", False)
    def __init__(self, axis=-1, depth=1, on_value=1.0, off_value=0.0, dtype=mstype.float32):
        """Initialize OneHot."""
        super(OneHot, self).__init__()
        self.onehot = P.OneHot(axis)
        self.depth = depth
        self.dtype = dtype
        self.on_value = on_value
        self.off_value = off_value

    def construct(self, indices):
        return self.onehot(indices, self.depth, F.cast(self.on_value, self.dtype), F.cast(self.off_value, self.dtype))


class Pad(Cell):
    r"""
    Pads the input tensor according to the paddings and mode.

    Args:
        paddings (tuple): The shape of parameter `paddings` is :math:`(N, 2)` . N is the rank of input data. All
            elements of paddings are int type. For `D` th dimension of the `x`, paddings[D, 0] indicates how many
            sizes to be extended ahead of the `D` th dimension of the input tensor, and paddings[D, 1] indicates how
            many sizes to be extended behind of the `D` th dimension of the input tensor. The padded size of each
            dimension D of the output is: :math:`paddings[D, 0] + input\_x.dim\_size(D) + paddings[D, 1]`,
            e.g.:

            .. code-block::

                mode = "CONSTANT".
                paddings = [[1,1], [2,2]].
                x = [[1,2,3], [4,5,6], [7,8,9]].
                # The above can be seen: 1st dimension of `x` is 3, 2nd dimension of `x` is 3.
                # Substitute into the formula to get:
                # 1st dimension of output is paddings[0][0] + 3 + paddings[0][1] = 1 + 3 + 1 = 5.
                # 2nd dimension of output is paddings[1][0] + 3 + paddings[1][1] = 2 + 3 + 2 = 7.
                # So the shape of output is (5, 7).

        mode (str): Specifies padding mode. The optional values are ``"CONSTANT"`` , ``"REFLECT"`` , ``"SYMMETRIC"`` .
            Default: ``"CONSTANT"`` .

    Inputs:
        - **x** (Tensor) - The input tensor.

    Outputs:
        Tensor, the tensor after padding.

        - If `mode` is "CONSTANT", it fills the edge with 0, regardless of the values of the `x`.
          If the `x` is [[1,2,3], [4,5,6], [7,8,9]] and `paddings` is [[1,1], [2,2]], then the
          Outputs is [[0,0,0,0,0,0,0], [0,0,1,2,3,0,0], [0,0,4,5,6,0,0], [0,0,7,8,9,0,0], [0,0,0,0,0,0,0]].
        - If `mode` is "REFLECT", it uses a way of symmetrical copying through the axis of symmetry to fill in.
          If the `x` is [[1,2,3], [4,5,6], [7,8,9]] and `paddings` is [[1,1], [2,2]], then the
          Outputs is [[6,5,4,5,6,5,4], [3,2,1,2,3,2,1], [6,5,4,5,6,5,4], [9,8,7,8,9,8,7], [6,5,4,5,6,5,4]].
        - If `mode` is "SYMMETRIC", the filling method is similar to the "REFLECT". It is also copied
          according to the symmetry axis, except that it includes the symmetry axis. If the `x`
          is [[1,2,3], [4,5,6], [7,8,9]] and `paddings` is [[1,1], [2,2]], then the Outputs is
          [[2,1,1,2,3,3,2], [2,1,1,2,3,3,2], [5,4,4,5,6,6,5], [8,7,7,8,9,9,8], [8,7,7,8,9,9,8]].

    Raises:
        TypeError: If `paddings` is not a tuple.
        ValueError: If length of `paddings` is more than 4 or its shape is not :math:`(N, 2)` .
        ValueError: If `mode` is not one of ``"CONSTANT"``, ``"REFLECT"``, ``"SYMMETRIC"``.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, nn, ops
        >>> import numpy as np
        >>> # If `mode` is "CONSTANT"
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.pad = nn.Pad(paddings=((1, 1), (2, 2)), mode="CONSTANT")
        ...     def construct(self, x):
        ...         return self.pad(x)
        >>> x = Tensor(np.array([[1, 2, 3], [4, 5, 6]]), mindspore.float32)
        >>> pad = Net()
        >>> output = pad(x)
        >>> print(output)
        [[0. 0. 0. 0. 0. 0. 0.]
         [0. 0. 1. 2. 3. 0. 0.]
         [0. 0. 4. 5. 6. 0. 0.]
         [0. 0. 0. 0. 0. 0. 0.]]
        >>> # Another way to call
        >>> pad = ops.Pad(paddings=((1, 1), (2, 2)))
        >>> # From the above code, we can see following:
        >>> # "paddings=((1, 1), (2, 2))",
        >>> # paddings[0][0] = 1, indicates a row of values is filled top of the input data in the 1st dimension.
        >>> # Shown as follows:
        >>> # [[0. 0. 0.]
        >>> #  [1. 2. 3.]
        >>> #  [4. 5. 6.]]
        >>> # paddings[0][1] = 1 indicates a row of values is filled below input data in the 1st dimension.
        >>> # Shown as follows:
        >>> # [[0. 0. 0.]
        >>> #  [1. 2. 3.]
        >>> #  [4. 5. 6.]
        >>> #  [0. 0. 0.]]
        >>> # paddings[1][0] = 2, indicates 2 rows of values is filled in front of input data in the 2nd dimension.
        >>> # Shown as follows:
        >>> # [[0. 0. 0. 0. 0.]
        >>> #  [0. 0. 1. 2. 3.]
        >>> #  [0. 0. 4. 5. 6.]
        >>> #  [0. 0. 0. 0. 0.]]
        >>> # paddings[1][1] = 2, indicates 2 rows of values is filled in front of input data in the 2nd dimension.
        >>> # Shown as follows:
        >>> # [[0. 0. 0. 0. 0. 0. 0.]
        >>> #  [0. 0. 1. 2. 3. 0. 0.]
        >>> #  [0. 0. 4. 5. 6. 0. 0.]
        >>> #  [0. 0. 0. 0. 0. 0. 0.]]
        >>> output = pad(x)
        >>> print(output)
        [[0. 0. 0. 0. 0. 0. 0.]
         [0. 0. 1. 2. 3. 0. 0.]
         [0. 0. 4. 5. 6. 0. 0.]
         [0. 0. 0. 0. 0. 0. 0.]]
        >>> # if mode is "REFLECT"
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.pad = nn.Pad(paddings=((1, 1), (2, 2)), mode="REFLECT")
        ...     def construct(self, x):
        ...         return self.pad(x)
        >>> x = Tensor(np.array([[1, 2, 3], [4, 5, 6]]), mindspore.float32)
        >>> pad = Net()
        >>> output = pad(x)
        >>> print(output)
        [[6. 5. 4. 5. 6. 5. 4.]
         [3. 2. 1. 2. 3. 2. 1.]
         [6. 5. 4. 5. 6. 5. 4.]
         [3. 2. 1. 2. 3. 2. 1.]]
        >>> # if mode is "SYMMETRIC"
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.pad = nn.Pad(paddings=((1, 1), (2, 2)), mode="SYMMETRIC")
        ...     def construct(self, x):
        ...         return self.pad(x)
        >>> x = Tensor(np.array([[1, 2, 3], [4, 5, 6]]), mindspore.float32)
        >>> pad = Net()
        >>> output = pad(x)
        >>> print(output)
        [[2. 1. 1. 2. 3. 3. 2.]
         [2. 1. 1. 2. 3. 3. 2.]
         [5. 4. 4. 5. 6. 6. 5.]
         [5. 4. 4. 5. 6. 6. 5.]]
    """

    def __init__(self, paddings, mode="CONSTANT"):
        """Initialize Pad."""
        super(Pad, self).__init__()
        self.mode = mode
        self.paddings = paddings
        Validator.check_string(
            self.mode, ["CONSTANT", "REFLECT", "SYMMETRIC"], 'mode', self.cls_name)
        if not isinstance(paddings, tuple):
            raise TypeError(f"For '{self.cls_name}', the type of 'paddings' must be tuple, "
                            f"but got {type(paddings).__name__}.")
        for item in paddings:
            if len(item) != 2:
                raise ValueError(f"For '{self.cls_name}', the dimension of 'paddings' must be (n, 2), "
                                 f"but got {paddings}.")
        if len(paddings) > 4:
            raise ValueError(f"For '{self.cls_name}', only 'paddings' up to 4 dims is supported, but got "
                             f"{len(paddings)}.")
        if mode == "CONSTANT":
            self.pad = P.Pad(self.paddings)
        else:
            self.paddings = Tensor(np.array(self.paddings), dtype=mstype.int64)
            self.pad = P.MirrorPad(mode=mode)

    def construct(self, x):
        if self.mode == "CONSTANT":
            x = self.pad(x)
        else:
            x = self.pad(x, self.paddings)
        return x


class Unfold(Cell):
    r"""
    Extracts patches from images.
    The input tensor must be a 4-D tensor and the data format is NCHW.

    Args:
        ksizes (Union[tuple[int], list[int]]): The size of sliding window, must be a tuple or a list of integers,
            and the format is [1, ksize_row, ksize_col, 1].
        strides (Union[tuple[int], list[int]]): Distance between the centers of the two consecutive patches,
            must be a tuple or list of int, and the format is [1, stride_row, stride_col, 1].
        rates (Union[tuple[int], list[int]]): In each extracted patch, the gap between the corresponding dimension
            pixel positions, must be a tuple or a list of integers, and the format is [1, rate_row, rate_col, 1].
        padding (str): The type of padding algorithm, is a string whose value is ``"same"`` or ``"valid"`` , not case
            sensitive. Default: ``"valid"`` .

            - ``"same"``: Means that the patch can take the part beyond the original image, and this part is filled
              with 0.

            - ``"valid"``: Means that the taken patch area must be completely covered in the original image.

    Inputs:
        - **x** (Tensor) - A 4-D tensor whose shape is :math:`[in\_batch, in\_depth, in\_row, in\_col]`
          and data type is number.

    Outputs:
        Tensor, a 4-D tensor whose data type is same as `x`,
        and the shape is :math:`(out\_batch, out\_depth, out\_row, out\_col)`
        where `out_batch` is the same as the `in_batch`.

        - :math:`out\_depth = ksize\_row * ksize\_col * in\_depth`
        - :math:`out\_row = (in\_row - (ksize\_row + (ksize\_row - 1) * (rate\_row - 1))) // stride\_row + 1`
        - :math:`out\_col = (in\_col - (ksize\_col + (ksize\_col - 1) * (rate\_col - 1))) // stride\_col + 1`

    Raises:
        TypeError: If `ksizes`, `strides` or `rates` is neither a tuple nor list.
        ValueError: If shape of `ksizes`, `strides` or `rates` is not :math:`(1, x\_row, x\_col, 1)`.
        ValueError: If the second and third element of `ksizes`, `strides` or `rates` is less than 1.

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, nn
        >>> import numpy as np
        >>> net = nn.Unfold(ksizes=[1, 2, 2, 1], strides=[1, 2, 2, 1], rates=[1, 2, 2, 1])
        >>> # As stated in the above code:
        >>> # ksize_row = 2, ksize_col = 2, rate_row = 2, rate_col = 2, stride_row = 2, stride_col = 2.
        >>> image = Tensor(np.ones([2, 3, 6, 6]), dtype=mindspore.float16)
        >>> # in_batch = 2, in_depth = 3, in_row = 6, in_col = 6.
        >>> # Substituting the formula to get:
        >>> # out_batch = in_batch = 2
        >>> # out_depth = 2 * 2 * 3 = 12
        >>> # out_row = (6 - (2 + (2 - 1) * (2 - 1))) // 2 + 1 = 2
        >>> # out_col = (6 - (2 + (2 - 1) * (2 - 1))) // 2 + 1 = 2
        >>> output = net(image)
        >>> print(output.shape)
        (2, 12, 2, 2)
    """

    def __init__(self, ksizes, strides, rates, padding="valid"):
        """Initialize Unfold."""
        super(Unfold, self).__init__()

        def _check_tuple_or_list(arg_name, arg_val, prim_name):
            Validator.check_value_type(f"{arg_name}s", ksizes, [
                tuple, list], self.cls_name)
            if len(arg_val) != 4 or arg_val[0] != 1 or arg_val[3] != 1:
                raise ValueError(f"For '{prim_name}' the format of '{arg_name}s' must be [1, {arg_name}_row, "
                                 f"{arg_name}_col, 1], but got {arg_val}.")
            is_int = isinstance(arg_val[1], int) and isinstance(arg_val[2], int)
            if not is_int or arg_val[1] < 1 or arg_val[2] < 1:
                raise ValueError(f"For '{prim_name}' the {arg_name}_row and {arg_name}_col in '{arg_name}s' must be "
                                 f"an positive integer number, but got {arg_name}_row is {arg_val[1]}, "
                                 f"{arg_name}_col is {arg_val[2]}")

        _check_tuple_or_list("ksize", ksizes, self.cls_name)
        _check_tuple_or_list("stride", strides, self.cls_name)
        _check_tuple_or_list("rate", rates, self.cls_name)
        ksizes = ksizes[0], ksizes[3], ksizes[1], ksizes[2]
        strides = strides[0], strides[3], strides[1], strides[2]
        rates = rates[0], rates[3], rates[1], rates[2]
        self.extract_image_patches = inner.ExtractImagePatches(
            ksizes, strides, rates, padding)

    def construct(self, input_x):
        result = self.extract_image_patches(input_x)
        return result


@_primexpr
def tril(x_shape, x_dtype, k):
    Validator.check_int(len(x_shape), 1, Validator.GE, "x rank", "tril")
    Validator.check_is_int(k, "k value", "tril")
    value = F.cast(P.Tril(diagonal=k)(F.ones(x_shape, x_dtype)), x_dtype)
    return value


class Tril(Cell):
    """
    The Tril class will be deprecated in the future,
    this function can be replaced by :func:`ops.tril`
    """

    @deprecated("2.0", "ops.tril", False)
    def __init__(self):
        """Initialize Tril."""
        super(Tril, self).__init__()
        self.dtype = P.DType()
        self.mul = P.Mul()
        self.cast = P.Cast()

    def construct(self, x, k=0):
        assist = tril(x.shape, self.dtype(x), k)
        result = self.mul(self.cast(x, mstype.float32),
                          self.cast(assist, mstype.float32))
        return self.cast(result, self.dtype(x))


@_primexpr
def triu(x_shape, x_dtype, k):
    Validator.check_int(len(x_shape), 1, Validator.GE, "x rank", "triu")
    Validator.check_is_int(k, "k value", "triu")
    value = F.cast(P.Triu(k)(F.ones(x_shape, x_dtype)), x_dtype)
    return value


class Triu(Cell):
    """
    The Triu class will be deprecated in the future,
    this function can be replaced by :func:`ops.triu`
    """

    @deprecated("2.0", "ops.triu", False)
    def __init__(self):
        """Initialize Triu."""
        super(Triu, self).__init__()
        self.dtype = P.DType()
        self.mul = P.Mul()
        self.cast = P.Cast()

    def construct(self, x, k=0):
        assist = triu(x.shape, self.dtype(x), k)
        result = self.mul(self.cast(x, mstype.float32),
                          self.cast(assist, mstype.float32))
        return self.cast(result, self.dtype(x))


@_primexpr
def _get_matrix_diag_assist(x_shape, x_dtype):
    """Get matrix diag assist"""
    Validator.check_int(len(x_shape), 1, Validator.GE, "x rank", "_get_matrix_diag_assist")
    base_eye = F.reshape(
        F.eye(x_shape[-1], x_shape[-1], x_dtype), (x_shape[-1] * x_shape[-1],))
    if len(x_shape) == 1:
        assist = F.reshape(base_eye, x_shape + (x_shape[-1],))
    else:
        assist = F.reshape(
            F.tile(base_eye, x_shape[:-1]), x_shape + (x_shape[-1],))
    value = F.cast(assist, x_dtype)
    return value


@constexpr
def _get_matrix_diag_part_assist(x_shape, x_dtype):
    """Get matrix diag part assist"""
    Validator.check_int(len(x_shape), 2, Validator.GE, "x rank", "_get_matrix_diag_part_assist")
    base_eye = F.reshape(
        F.eye(x_shape[-2], x_shape[-1], x_dtype), (x_shape[-2] * x_shape[-1],))
    if len(x_shape) <= 2:
        assist = F.reshape(base_eye, x_shape)
    else:
        assist = F.reshape(F.tile(base_eye, x_shape[:-2]), x_shape)
    value = F.cast(assist, x_dtype)
    return value


class MatrixDiag(Cell):
    r"""
    The MatrixDiag class will be deprecated in the future,
    this function can be replaced by :func:`ops.diag`
    """

    @deprecated("2.0", "ops.diag", False)
    def __init__(self):
        """Initialize MatrixDiag."""
        super(MatrixDiag, self).__init__()
        self.matrix_diag = inner.MatrixDiag()
        self.dtype = P.DType()

    def construct(self, input_x):
        x_shape = F.shape(input_x)
        x_dtype = self.dtype(input_x)
        assist = _get_matrix_diag_assist(x_shape, x_dtype)
        out_matrix_diag = self.matrix_diag(input_x, assist)
        return out_matrix_diag


class MatrixDiagPart(Cell):
    r"""
    The MatrixDiagPart class will be deprecated in the future,
    this function can be replaced by :func:`ops.diagonal`
    """

    @deprecated("2.0", "ops.diagonal", False)
    def __init__(self):
        """Initialize MatrixDiagPart."""
        super(MatrixDiagPart, self).__init__()
        self.matrix_diag_part = inner.MatrixDiagPart()
        self.dtype = P.DType()

    def construct(self, input_x):
        x_shape = F.shape(input_x)
        x_dtype = self.dtype(input_x)
        assist = _get_matrix_diag_part_assist(x_shape, x_dtype)
        out_matrix_diag_part = self.matrix_diag_part(input_x, assist)
        return out_matrix_diag_part


class MatrixSetDiag(Cell):
    r"""
    Modifies the batched diagonal part of a batched tensor.

    Assume `x` has :math:`k+1` dimensions :math:`[I, J, K, ..., M, N]` and `diagonal` has :math:`k`
    dimensions :math:`[I, J, K, ..., min(M, N)]`, the output is a tensor of rank :math:`k+1` with dimensions
    :math:`[I, J, K, ..., M, N]`, where:

    .. math::
        output[i, j, k, ..., m, n] = diagonal[i, j, k, ..., n]\ for\ m == n

    .. math::
        output[i, j, k, ..., m, n] = x[i, j, k, ..., m, n]\ for\ m != n

    Inputs:
        - **x** (Tensor) - The batched tensor. Rank k+1, where k >= 1. It can be one of the following data types:
          float32, float16, int32, int8, and uint8.
        - **diagonal** (Tensor) - The diagonal values. Must have the same type as input `x`. Rank k, where k >= 1.

    Outputs:
        Tensor, has the same type and shape as input `x`.

    Raises:
        TypeError: If dtype of `x` or `diagonal` is not one of float32, float16, int32, int8 or uint8.
        ValueError: If length of shape of `x` is less than 2.
        ValueError: If x_shape[-2] < x_shape[-1] and x_shape[:-1] != diagonal_shape.
        ValueError: If x_shape[-2] >= x_shape[-1] and x_shape[:-2] + x_shape[-1:] != diagonal_shape.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, nn
        >>> x = Tensor([[[-1, 0], [0, 1]], [[-1, 0], [0, 1]], [[-1, 0], [0, 1]]], mindspore.float32)
        >>> diagonal = Tensor([[-1., 2.], [-1., 1.], [-1., 1.]], mindspore.float32)
        >>> matrix_set_diag = nn.MatrixSetDiag()
        >>> output = matrix_set_diag(x, diagonal)
        >>> print(output)
        [[[-1.  0.]
          [ 0.  2.]]
         [[-1.  0.]
          [ 0.  1.]]
         [[-1.  0.]
          [ 0.  1.]]]
    """

    def __init__(self):
        """Initialize MatrixSetDiag."""
        super(MatrixSetDiag, self).__init__()
        self.matrix_set_diag = inner.MatrixSetDiag()
        self.dtype = P.DType()

    def construct(self, input_x, diagonal):
        x_shape = F.shape(input_x)
        x_dtype = self.dtype(input_x)
        assist = _get_matrix_diag_part_assist(x_shape, x_dtype)
        out_matrix_set_diag = self.matrix_set_diag(input_x, diagonal, assist)
        return out_matrix_set_diag


@constexpr
def _check_input_dim(axis, dim, cls_name):
    Validator.check_int_range(axis, -dim, dim, Validator.INC_LEFT, 'axis', cls_name)


class Roll(Cell):
    """
    The Roll class will be deprecated in the future,
    this function can be replaced by :func:`ops.roll`
    """

    @deprecated("2.0", "ops.roll", False)
    def __init__(self, shift, axis):
        """Initialize Roll"""
        super(Roll, self).__init__()
        Validator.check_value_type(
            "shift", shift, [int, tuple, list], self.cls_name)
        Validator.check_value_type(
            "axis", axis, [int, tuple, list], self.cls_name)
        self.shape_op = P.Shape()
        self.shift = shift
        self.axis = axis
        self.op_list = []
        self.gpu = False

        if not isinstance(self.axis, (list, tuple)):
            self.axis = [self.axis]
        if not isinstance(self.shift, (list, tuple)):
            self.shift = [self.shift]
        if context.get_context("device_target") == "GPU":
            Validator.check_int(len(self.shift), 1, Validator.GE, "shift", "Roll")
            Validator.check_int(len(self.axis), 1, Validator.GE, "axis", "Roll")
            for s_axis in self.axis:
                Validator.check_is_int(s_axis, "axis", "Roll")
            for s_shift in self.shift:
                Validator.check_is_int(s_shift, "shift", "Roll")
            self.roll = P.Roll(self.shift, self.axis)
            self.gpu = True
            if len(self.shift) != len(self.axis):
                raise ValueError(f"For '{self.cls_name}', the shape of 'shift' and the shape of 'axis' must be "
                                 f"the same, but got the length of 'shift' {len(self.shift)} "
                                 f"and the length of 'axis' {len(self.axis)}.")
        else:
            if not isinstance(self.axis, (list, tuple)):
                self.op_list.append(
                    (P.Roll(shift=self.shift, axis=0), self.axis))
            else:
                if len(self.shift) != len(self.axis):
                    raise ValueError(f"For '{self.cls_name}', the shape of 'shift' and the shape of 'axis' must be "
                                     f"the same, but got the length of 'shift' {len(self.shift)} "
                                     f"and the length of 'axis' {len(self.axis)}.")
                for idx, _ in enumerate(self.axis):
                    self.op_list.append(
                        (P.Roll(shift=self.shift[idx], axis=0), self.axis[idx]))

    def construct(self, input_x):
        dim = len(self.shape_op(input_x))
        if self.gpu:
            output = self.roll(input_x)
        else:
            for single_op_roll, single_axis in self.op_list:
                _check_input_dim(single_axis, dim, self.cls_name)
                if single_axis < 0:
                    single_axis += dim
                transpose_perm = []
                for i in range(dim):
                    transpose_perm.append(i)
                transpose_perm[0], transpose_perm[single_axis] = single_axis, 0

                input_x = input_x.transpose(transpose_perm)
                input_x = single_op_roll(input_x)
                input_x = input_x.transpose(transpose_perm)
            output = input_x
        return output


class Unflatten(Cell):
    r"""
    Unflattens a Tensor dim according to `axis` and `unflattened_size`.

    Args:
        axis (int): specifies the dimension of the input Tensor to be unflattened.
        unflattened_size (Union(tuple[int], list[int])): the new shape of the unflattened dimension of
            the Tensor and it can be a tuple of ints or a list of ints. The product of `unflattened_size`
            must equal to input_shape[axis].

    Inputs:
        - **input** (Tensor) - The input Tensor to be unflattened.

    Outputs:
        Tensor that has been unflattend.

    Raises:
        TypeError: If `axis` is not int.
        TypeError: If `unflattened_size` is neither tuple of ints nor list of ints.
        TypeError: The product of `unflattened_size` does not equal to input_shape[axis].

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, nn
        >>> import numpy as np
        >>> input = Tensor(np.arange(0, 100).reshape(2, 10, 5), mindspore.float32)
        >>> net = nn.Unflatten(1, (2, 5))
        >>> output = net(input)
        >>> print(f"before unflatten the input shape is {input.shape}")
        before unflatten the input shape is  (2, 10, 5)
        >>> print(f"after unflatten the output shape is {output.shape}")
        after unflatten the output shape is (2, 2, 5, 5)
    """

    def __init__(self, axis, unflattened_size):
        """Initialize Unflatten."""
        super(Unflatten, self).__init__()
        self.shape = P.Shape()
        self.reshape = P.Reshape()
        Validator.check_is_int(axis, 'axis', 'Unflatten')
        Validator.check_value_type(
            'unflattended_size', unflattened_size, (list, tuple), 'Unflatten')
        self.axis = axis
        if isinstance(unflattened_size, list):
            unflattened_size = tuple(unflattened_size)
        self.unflattened_size = unflattened_size

    def construct(self, input_x):
        input_shape = self.shape(input_x)
        new_shape = tuple()
        new_shape += input_shape[: self.axis]
        new_shape += self.unflattened_size
        if self.axis != -1:
            new_shape += input_shape[self.axis + 1:]
        return self.reshape(input_x, new_shape)
