# Copyright 2020-2022 Huawei Technologies Co., Ltd
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
"""Operators for random."""
from __future__ import absolute_import

from mindspore.common._decorator import deprecated
from mindspore import _checkparam as Validator
from mindspore.common import dtype as mstype
from mindspore.ops.primitive import PrimitiveWithInfer, prim_attr_register, Primitive
from mindspore.ops._utils import get_broadcast_shape
from ..auto_generate import RandpermV2


class NonDeterministicInts(Primitive):
    r"""
    Generates some integers that match the given type.

    Returns the tensor with the given shape, the random numbers in it drawn from the data range
    that a given type can represent.

    .. warning::
        The value of `shape` must be greater than zero.
        The number of elements of output can not exceed 1000000.

    Args:
        dtype (mindspore.dtype, optional): The date type of output. The supported values are: mstype.int32
            and mstype.int64. Default: mstype.int64.

    Inputs:
        - **shape** (Tensor) - The shape of random tensor to be generated. The supported values are:
          int32 and int64.

    Outputs:
        Tensor. Its shape is specified by the input `shape`. Its type is specified by `dtype`.

    Raises:
        TypeError: If `shape` is not a Tensor.
        TypeError: If `dtype` is not mstype.int32 or mstype.int64.
        ValueError: If `shape` has negative elements.
        ValueError: If `shape` has less than 2 elements.
        ValueError: If `shape` is not a 1-D tensor.
        ValueError: If the number of elements of output is more than 1000000.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> shape = Tensor((3,4), mstype.int32)
        >>> ndints = ops.NonDeterministicInts(dtype=mstype.int32)
        >>> output = ndints(shape)
        >>> print(output.shape)
        (3, 4)
    """

    @prim_attr_register
    def __init__(self, dtype=mstype.int64):
        """Initialize NonDeterministicInts"""
        self.dtype = dtype
        self.add_prim_attr("max_length", 1000000)
        self.init_prim_io_names(inputs=["shape"], outputs=["output"])
        self.add_prim_attr("side_effect_hidden", True)
        valid_values = (mstype.int32, mstype.int64, mstype.uint32, mstype.uint64)
        Validator.check_type_name("dtype", dtype, valid_values, self.name)


class TruncatedNormal(Primitive):
    """
    Returns a Tensor of the specified shape filled with truncated normal values.

    The generated values conform to a Gaussian distribution.

    Note:
        - The value of `shape` must be greater than zero. The output length can not exceed 1000000.
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .
        dtype (mindspore.dtype, optional): Specified output data type. Must be one of the following types:
            mindspore.float16, mindspore.float32 and mindspore.float64. Default: mindspore.float32.

    Inputs:
        - **shape** (Tensor) - The shape of random tensor to be generated. Its type must be one of the following types:
          mindspore.int32 and mindspore.int64.

    Outputs:
        Tensor. Its shape is specified by the input `shape`. Its type is specified by `dtype`.
        Its values are in [-2,2].

    Raises:
        TypeError: If `shape` is not a Tensor.
        TypeError: If data type of `dtype` and `shape` are not allowed.
        TypeError: If `seed` is not an integer.
        ValueError: If `shape` elements are not positive.
        ValueError: If `shape` is not a 1-D tensor.
        ValueError: If the number of elements of output is more than 1000000.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> shape = Tensor(np.array([2, 2]), mstype.int32)
        >>> seed = 0
        >>> seed2 = 0
        >>> truncated_normal = ops.TruncatedNormal(seed=seed, seed2=seed2)
        >>> output = truncated_normal(shape)
        >>> print(output)
        [[ -1.303105  0.641905 ]
         [ -0.917926  0.650655 ]]
    """

    @prim_attr_register
    def __init__(self, dtype=mstype.float32, seed=0, seed2=0):
        """Initialize TruncatedNormal"""
        self.dtype = dtype
        self.add_prim_attr("max_length", 1000000)
        self.add_prim_attr("side_effect_hidden", True)
        self.init_prim_io_names(inputs=["shape"], outputs=["output"])
        Validator.check_value_type('seed', seed, [int], self.name)
        Validator.check_value_type('seed2', seed2, [int], self.name)
        valid_values = (mstype.float16, mstype.float32, mstype.float64)
        Validator.check_type_name("dtype", dtype, valid_values, self.name)


class StandardNormal(Primitive):
    r"""
    Generates random numbers according to the standard Normal (or Gaussian) random number distribution.

    Refer to :func:`mindspore.ops.standard_normal` for more details.

    Note:
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .

    Inputs:
        - **shape** (tuple) - The shape of random tensor to be generated. Only constant value is allowed.
          Supported dtypes: int32, int64.

    Outputs:
        Tensor. The shape is the same as the input `shape`. The dtype is float32.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import ops
        >>> shape = (3, 4)
        >>> stdnormal = ops.StandardNormal(seed=2)
        >>> output = stdnormal(shape)
        >>> print(output)
        [[-1.3031056   0.64198005 -0.65207404 -1.767485  ]
         [-0.91792876  0.6508565  -0.9098478  -0.14092612]
         [ 0.7806437   1.1585592   1.9676613  -0.00440959]]
    """

    @prim_attr_register
    def __init__(self, seed=0, seed2=0):
        """Initialize StandardNormal"""
        self.init_prim_io_names(inputs=['shape'], outputs=['output'])
        self.add_prim_attr("side_effect_hidden", True)
        Validator.check_non_negative_int(seed, "seed", self.name)
        Validator.check_non_negative_int(seed2, "seed2", self.name)


class StandardLaplace(Primitive):
    r"""
    Generates random numbers according to the Laplace random number distribution (mean=0, lambda=1).
    It is defined as:

    .. math::
        \text{f}(x) = \frac{1}{2}\exp(-|x|)

    Note:
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .

    Inputs:
        - **shape** (Union[tuple[int], Tensor[int]]) - The shape of random tensor to be generated.
          Only constant value is allowed
          when the input type is tuple. And the operator supports dynamic shape only when the input type is Tensor.

    Outputs:
        Tensor. The shape that the input 'shape' denotes. The dtype is float32.

    Raises:
        TypeError: If seed or seed2 is not an int.
        TypeError: If shape is neither a tuple nor a Tensor.
        ValueError: If seed or seed2 is not a non-negative int.
        ValueError: If shape is a tuple containing non-positive items.
        ValueError: If shape is a Tensor, and the rank of the Tensor is not equal to 1.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import ops
        >>> shape = (4, 16)
        >>> stdlaplace = ops.StandardLaplace(seed=2)
        >>> output = stdlaplace(shape)
        >>> result = output.shape
        >>> print(result)
        (4, 16)
    """

    @prim_attr_register
    def __init__(self, seed=0, seed2=0):
        """Initialize StandardLaplace"""
        self.init_prim_io_names(inputs=['shape'], outputs=['output'])
        self.add_prim_attr("side_effect_hidden", True)
        Validator.check_non_negative_int(seed, "seed", self.name)
        Validator.check_non_negative_int(seed2, "seed2", self.name)


class RandomGamma(Primitive):
    r"""
    Produces random positive floating-point values x, distributed according to probability density function:

    Note:
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .

    Inputs:
        - **shape** (Tensor) - The shape of random tensor to be generated. It must be constant value.
        - **alpha** (Tensor) - α is the shape parameter of RandomGamma distribution, it mainly determines the
          shape of the graph curve. It must be greater than 0 and have date type float32.

    Outputs:
        Tensor. The shape should be equal to the concat shape between the input `shape` and `alpha`.
        The dtype is the same type as alpha.

    Raises:
        TypeError: If data type of `seed` or `seed2` is not int.
        TypeError: If `shape` or `alpha` is not a Tensor.
        TypeError: If data type of `alpha` is not float32.
        ValueError: If `shape` is not a constant value.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> shape = Tensor(np.array([3, 1, 2]), mstype.int32)
        >>> alpha = Tensor(np.array([[3, 4], [5, 6]]), mstype.float32)
        >>> gamma = ops.RandomGamma(seed=3)
        >>> output = gamma(shape, alpha)
        >>> result = output.shape
        >>> print(result)
        (3, 1, 2, 2, 2)
    """

    @prim_attr_register
    def __init__(self, seed=0, seed2=0):
        """Initialize Gamma"""
        self.init_prim_io_names(inputs=['shape', 'alpha'], outputs=['output'])
        self.add_prim_attr("side_effect_hidden", True)
        Validator.check_non_negative_int(seed, "seed", self.name)
        Validator.check_non_negative_int(seed2, "seed2", self.name)


class LogNormalReverse(Primitive):
    r"""
    Fills the elements of the input tensor with log normal values initialized by
    given mean and std:

    .. math::
        \text{f}(x;1.0,2.0)=\frac{1}{x\delta \sqrt[]{2\pi} }e^{-\frac{(\ln x-\mu )^2}{2\delta ^2} }

    where \mu, \delta is mean and standard deviation of  lognormal distribution respectively.

    Args:
        mean (float, optional): the mean of normal distribution. With float data type.
            Default: ``1.0`` .
        std (float, optional): the std of normal distribution. With float data type.
            Default: ``2.0`` .

    Inputs:
        - **input** (Tensor) - The tensor to be generated with log-normal distribution.
          Must be one of the following types: float16, float32, float64.

    Outputs:
        Tensor. A Tensor with the same type and shape of input.

    Raises:
        TypeError: If `input` is not Tensor.
        ValueError: If `input` is NULL.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> x = Tensor(np.random.randn(3,4),mstype.float64)
        >>> mean = 2.0
        >>> std = 1.0
        >>> lognormalreverse = ops.LogNormalReverse(mean, std)
        >>> output = lognormalreverse(x)
        >>> result = output.shape
        >>> print(result)
        (3, 4)
    """

    @prim_attr_register
    def __init__(self, mean=1.0, std=2.0):
        """Initialize LogNormalReverse"""
        self.add_prim_attr("side_effect_hidden", True)
        Validator.check_value_type("mean", mean, [float], self.name)
        Validator.check_value_type("std", std, [float], self.name)


class Gamma(PrimitiveWithInfer):
    r"""
    Produces random positive floating-point values x, distributed according to probability density function:

    .. math::
        \text{P}(x|α,β) = \frac{\exp(-x/β)}{{β^α}\cdot{\Gamma(α)}}\cdot{x^{α-1}}

    Note:
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .

    Inputs:
        - **shape** (tuple) - The shape of random tensor to be generated. Only constant value is allowed.
        - **alpha** (Tensor) - α is the shape parameter of Gamma distribution, which mainly determines the shape of
          the curve. It must be greater than 0. The data type is float32.
        - **beta** (Tensor) - β is the inverse scale parameter of the Gamma distribution, which mainly determines how
          steep the curve is. It must be greater than 0. The data type is float32.

    Outputs:
        Tensor. The shape must be the broadcasted shape of Input "shape" and shapes of `alpha` and `beta`.
        The dtype is float32.

    Raises:
        TypeError: If data type of `seed` or `seed2` is not int.
        TypeError: If `alpha` or `beta` is not a Tensor.
        TypeError: If data type of `alpha` or `beta` is not float32.
        ValueError: If `shape` is not a constant value.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> shape = (3, 1, 2)
        >>> alpha = Tensor(np.array([[3, 4], [5, 6]]), mstype.float32)
        >>> beta = Tensor(np.array([1.0]), mstype.float32)
        >>> gamma = ops.Gamma(seed=3)
        >>> output = gamma(shape, alpha, beta)
        >>> result = output.shape
        >>> print(result)
        (3, 2, 2)
    """

    @prim_attr_register
    def __init__(self, seed=0, seed2=0):
        """Initialize RandomGamma"""
        self.init_prim_io_names(
            inputs=['shape', 'alpha', 'beta'], outputs=['output'])
        self.add_prim_attr("side_effect_hidden", True)
        Validator.check_non_negative_int(seed, "seed", self.name)
        Validator.check_non_negative_int(seed2, "seed2", self.name)

    def __infer__(self, shape, alpha, beta):
        shape_v = shape["value"]
        if shape_v is None:
            raise ValueError(f"For '{self.name}', the 'shape' cannot be None.")
        Validator.check_value_type("shape", shape_v, [tuple], self.name)
        for i, shape_i in enumerate(shape_v):
            Validator.check_positive_int(shape_i, f'shape[{i}]', self.name)
        Validator.check_tensor_dtype_valid("alpha", alpha["dtype"], [mstype.float32], self.name)
        Validator.check_tensor_dtype_valid("beta", beta["dtype"], [mstype.float32], self.name)
        broadcast_shape = get_broadcast_shape(alpha['shape'], beta['shape'], self.name,
                                              arg_name1="alpha", arg_name2="beta")
        broadcast_shape = get_broadcast_shape(broadcast_shape, shape_v, self.name,
                                              arg_name1="broadcast_alpha_beta", arg_name2="shape")
        out = {
            'shape': broadcast_shape,
            'dtype': mstype.float32,
            'value': None}
        return out


class ParameterizedTruncatedNormal(Primitive):
    r"""
    Returns a tensor of the specified shape filled with truncated normal values.
    When `shape` is :math:`(batch\_size, *)`, the shape of `mean`, `stdevs`,
    `min` and `max` should be :math:`()` or :math:`(batch\_size, )`.

    Note:
        - The value in tensor `min` must be strictly less than `max` at any position after broadcasting.
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .

    Inputs:
        - **shape** (Tensor) - The shape of random tensor to be generated.
          It has shape :math:`(batch\_size, *)` where :math:`*` is an additional
          dimension with a length of no less than 1.
          Its type must be one of the following types: int32 and int64.
        - **mean** (Tensor) - The parameter defines the mean of truncated normal distribution.
          It has shape :math:`()` or :math:`(batch\_size, )`.
          Its type must be one of the following types:float16, float32, float64.
        - **stdevs** (Tensor) - The parameter defines the standard deviation for truncation of
          the normal distribution.
          It must be greater than 0 and have the same shape and type as means.
        - **min** (Tensor) - The parameter defines the minimum of
          truncated normal distribution. It must have the same shape and type as means.
        - **max** (Tensor) - The parameter defines the maximum of
          truncated normal distribution. It must have the same shape and type as means.

    Outputs:
        Tensor. Its shape is specified by the input `shape` and it must have the same type as means.

    Raises:
        TypeError: If data type of `shape`, `mean`, `stdevs`, `min` and `max` are not allowed.
        TypeError: If `mean`, `stdevs`, `min`, `max` don't have the same type.
        TypeError: If any of `shape`, `mean`, `stdevs`, `min` and `max` is not Tensor.
        ValueError: When `shape` is :math:`(batch\_size, *)`, if the shape of `mean`, `stdevs`, `min` or `max`
                    is not :math:`()` or :math:`(batch\_size, )`.
        ValueError: If `shape` elements are not positive.
        ValueError: If `stdevs` elements are not positive.
        ValueError: If `shape` has less than 2 elements.
        ValueError: If `shape` is not a 1-D tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> shape = Tensor(np.array([2, 3]), mstype.int32)
        >>> mean = Tensor(np.array([0]), mstype.float32)
        >>> stdevs = Tensor(np.array([1]), mstype.float32)
        >>> min = Tensor(np.array([-100]), mstype.float32)
        >>> max = Tensor(np.array([100]),  mstype.float32)
        >>> seed = 1
        >>> seed2 = 2
        >>> parameterized_truncated_normal = ops.ParameterizedTruncatedNormal(seed=seed, seed2=seed2)
        >>> output = parameterized_truncated_normal(shape, mean, stdevs, min, max)
        >>> print(output)
        [[-0.54974616 -1.4028727   1.5827523 ]
         [ 0.25759354 -1.9593946  -1.5078077 ]]
    """

    @prim_attr_register
    def __init__(self, seed=0, seed2=0):
        """Initialize ParameterizedTruncatedNormal"""
        self.init_prim_io_names(
            inputs=['shape', 'mean', 'stdevs', 'min', 'max'], outputs=['y'])
        self.add_prim_attr("side_effect_hidden", True)
        Validator.check_value_type('seed', seed, [int], self.name)
        Validator.check_value_type('seed2', seed2, [int], self.name)


class Poisson(PrimitiveWithInfer):
    r"""
    Produces random non-negative integer values i. Distributed according to discrete probability function:

    .. math::
        \text{P}(i|μ) = \frac{\exp(-μ)μ^{i}}{i!}

    Note:
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .

    Inputs:
        - **shape** (tuple) - The shape of random tensor to be generated. Only constant value is allowed.
        - **mean** (Tensor) - μ parameter the distribution was constructed with. The parameter defines mean number
          of occurrences of the event. It must be greater than 0. With float32 data type.

    Outputs:
        Tensor. Its shape must be the broadcasted shape of `shape` and the shape of `mean`.
        The dtype is int32.

    Raises:
        TypeError: If neither `seed` nor `seed2` is an int.
        TypeError: If `shape` is not a tuple.
        TypeError: If `mean` is not a Tensor whose dtype is not float32.

    Supported Platforms:
        deprecated

    Examples:
        >>> shape = (4, 1)
        >>> mean = Tensor(np.array([5.0, 10.0]), mstype.float32)
        >>> poisson = ops.Poisson(seed=5)
        >>> output = poisson(shape, mean)
        >>> result = output.shape
        >>> print(result)
        (4, 2)
    """

    @deprecated("2.0", "mindspore.ops.operations.Poisson", False)
    @prim_attr_register
    def __init__(self, seed=0, seed2=0):
        """Initialize Poisson"""
        self.init_prim_io_names(inputs=['shape', 'mean'], outputs=['output'])
        self.add_prim_attr("side_effect_hidden", True)
        Validator.check_non_negative_int(seed, "seed", self.name)
        Validator.check_non_negative_int(seed2, "seed2", self.name)

    def __infer__(self, shape, mean):
        shape_v = shape["value"]
        if shape_v is None:
            raise ValueError(f"For '{self.name}', the 'shape' cannot be None.")
        Validator.check_value_type("shape", shape_v, [tuple], self.name)
        for i, shape_i in enumerate(shape_v):
            Validator.check_positive_int(shape_i, f'shape[{i}]', self.name)
        Validator.check_tensor_dtype_valid(
            "mean", mean["dtype"], [mstype.float32], self.name)
        broadcast_shape = get_broadcast_shape(
            mean['shape'], shape_v, self.name, arg_name1="mean", arg_name2="shape")
        out = {
            'shape': broadcast_shape,
            'dtype': mstype.int32,
            'value': None}
        return out


class RandomPoisson(Primitive):
    r"""
    Produces random non-negative values i, distributed according to discrete probability function:

    .. math::
        \text{P}(i|μ) = \frac{\exp(-μ)μ^{i}}{i!}

    Note:
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .
        dtype (mindspore.dtype, optional): The type of output. Default: ``mstype.int64`` .

    Inputs:
        - **shape** (Tensor) - The shape of random tensor to be generated, 1-D Tensor, whose dtype must be in
          [int32, int64].
        - **rate** (Tensor) - μ parameter the distribution was constructed with. The parameter defines mean number
          of occurrences of the event. Its type must be in [float16, float32, float64, int32, int64].

    Outputs:
        Tensor. Its shape is :math:`(*shape, *rate.shape)`. Its type is specified by `dtype`.

    Raises:
        TypeError: If `shape` is not a Tensor or its dtype is not int32 or int64.
        TypeError: If `dtype` is not int32 or int64.
        ValueError: If `shape` is not a 1-D tensor.
        ValueError: If `shape` elements are negative.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> shape = Tensor(np.array([2, 3]), mstype.int32)
        >>> rate = Tensor(np.array([2, 2]), mstype.int32)
        >>> seed = 0
        >>> seed2 = 0
        >>> random_poisson = ops.RandomPoisson(seed=seed, seed2=seed2)
        >>> output = random_poisson(shape,rate)
        >>> print(output.shape)
        (2, 3, 2)
    """

    @prim_attr_register
    def __init__(self, seed=0, seed2=0, dtype=mstype.int64):
        """Initialize Poisson"""
        self.init_prim_io_names(inputs=['shape', 'rate'], outputs=['output'])
        Validator.check_value_type('seed', seed, [int], self.name)
        Validator.check_value_type('seed2', seed2, [int], self.name)
        self.add_prim_attr("side_effect_hidden", True)
        valid_values = (mstype.int64, mstype.int32,
                        mstype.float16, mstype.float32, mstype.float64)
        Validator.check_type_name("dtype", dtype, valid_values, self.name)


class UniformInt(Primitive):
    r"""
    Produces random integer values i, uniformly distributed on the closed interval [minval, maxval), that is,
    distributed according to the discrete probability function:

    .. math::
        \text{P}(i|a,b) = \frac{1}{b-a+1},

    where the :math:`a` indicates the min distribution parameter,
    the :math:`b` indicates the max distribution parameter.

    Note:
        - The number in tensor minval must be strictly less than maxval at any position after broadcasting.
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .

    Inputs:
        - **shape** (Union[tuple, Tensor]) - The shape of random tensor to be generated. Only constant value is allowed.
        - **minval** (Tensor) - The distribution parameter, :math:`a`.
          It defines the minimum possibly generated value, with int32 data type. Only one number is supported.
        - **maxval** (Tensor) - The distribution parameter, :math:`b`.
          It defines the maximum possibly generated value, with int32 data type. Only one number is supported.

    Outputs:
        Tensor. The shape is the same as the input 'shape', and the data type is int32.

    Raises:
        TypeError: If neither `seed` nor `seed2` is an int.
        TypeError: If `shape` is neither a tuple nor a Tensor.
        TypeError: If neither `minval` nor `maxval` is a Tensor.
        ValueError: If `shape` is not a constant value.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> shape = (2, 4)
        >>> minval = Tensor(1, mstype.int32)
        >>> maxval = Tensor(5, mstype.int32)
        >>> uniform_int = ops.UniformInt(seed=10)
        >>> output = uniform_int(shape, minval, maxval)
        >>> result = output.shape
        >>> print(result)
        (2, 4)
    """

    @prim_attr_register
    def __init__(self, seed=0, seed2=0):
        """Initialize UniformInt"""
        self.init_prim_io_names(
            inputs=['shape', 'minval', 'maxval'], outputs=['output'])
        self.add_prim_attr("side_effect_hidden", True)
        Validator.check_non_negative_int(seed, "seed", self.name)
        Validator.check_non_negative_int(seed2, "seed2", self.name)


class UniformReal(Primitive):
    r"""
    Produces random floating-point values, uniformly distributed to the interval [0, 1).

    Note:
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.
        - Currently, on the Ascend platform, `shape` as a Tensor is not supported.
          This is supported on CPU/GPU platforms. When the input is a Tensor,
          the supported data types are as follows:

          - GPU: int32, int64.
          - CPU: int16, int32, int64.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .

    Inputs:
        - **shape** (Union[tuple, Tensor]) - The shape of tensor to be generated. Only constant value is allowed.

    Outputs:
        Tensor. The shape that the input 'shape' denotes. The dtype is float32.

    Raises:
        TypeError: If `seed` or `seed2` is not an int.
        TypeError: If `shape` is neither a tuple nor a Tensor.
        ValueError: If `shape` is not a constant value.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import ops
        >>> shape = (2, 2)
        >>> uniformreal = ops.UniformReal(seed=2)
        >>> output = uniformreal(shape)
        >>> result = output.shape
        >>> print(result)
        (2, 2)
    """

    @prim_attr_register
    def __init__(self, seed=0, seed2=0):
        """Initialize UniformReal"""
        self.init_prim_io_names(inputs=['shape'], outputs=['output'])
        self.add_prim_attr("side_effect_hidden", True)
        Validator.check_non_negative_int(seed, "seed", self.name)
        Validator.check_non_negative_int(seed2, "seed2", self.name)


class RandomChoiceWithMask(Primitive):
    """
    Generates a random sample as index tensor with a mask tensor from a given tensor.

    Refer to :func:`mindspore.ops.choice_with_mask` for more details.

    Note:
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        count (int, optional): Number of items expected to get and the number must be greater than 0. Default: ``256`` .
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .

    Inputs:
        - **input_x** (Tensor[bool]) - The input tensor.
          The input tensor rank must be greater than or equal to 1 and less than or equal to 5.

    Outputs:
        Two tensors, the first one is the index tensor and the other one is the mask tensor.

        - **index** (Tensor) - The output shape is 2-D, its shape is :math:`(count, rank of input_x)`.
        - **mask** (Tensor) - The output shape is 1-D, its shape is :math:`(count)`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> rnd_choice_mask = ops.RandomChoiceWithMask()
        >>> input_x = Tensor(np.ones(shape=[240000, 4]).astype(np.bool))
        >>> output_y, output_mask = rnd_choice_mask(input_x)
        >>> result = output_y.shape
        >>> print(result)
        (256, 2)
        >>> result = output_mask.shape
        >>> print(result)
        (256,)
    """

    @prim_attr_register
    def __init__(self, count=256, seed=0, seed2=0):
        """Initialize RandomChoiceWithMask"""
        Validator.check_value_type("count", count, [int], self.name)
        Validator.check_positive_int(count, "count", self.name)
        Validator.check_value_type('seed', seed, [int], self.name)
        Validator.check_value_type('seed2', seed2, [int], self.name)
        self.add_prim_attr("side_effect_hidden", True)


class RandomCategorical(PrimitiveWithInfer):
    r"""
    Generates random samples from a given categorical distribution tensor.

    Args:
        dtype (mindspore.dtype): The type of output. Its value must be one of mstype.int16,
            mstype.int32 and mstype.int64. Default: ``mstype.int64`` .

    Inputs:
        - **logits** (Tensor) - The input tensor. 2-D Tensor with shape :math:`(batch\_size, num\_classes)`.
        - **num_sample** (int) - Number of sample to be drawn. Only constant values is allowed.
        - **seed** (int) - Random seed. Default: ``0`` . Only constant values is allowed.

    Outputs:
        - **output** (Tensor) - The output Tensor with shape :math:`(batch\_size, num\_samples)`.

    Raises:
        TypeError: If `dtype` is not one of the following: mstype.int16, mstype.int32, mstype.int64.
        TypeError: If `logits` is not a Tensor.
        TypeError: If neither `num_sample` nor `seed` is an int.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import nn, ops, Tensor
        >>> class Net(nn.Cell):
        ...   def __init__(self, num_sample):
        ...     super(Net, self).__init__()
        ...     self.random_categorical = ops.RandomCategorical(mindspore.int64)
        ...     self.num_sample = num_sample
        ...   def construct(self, logits, seed=0):
        ...     return self.random_categorical(logits, self.num_sample, seed)
        ...
        >>> x = np.random.random((10, 5)).astype(np.float32)
        >>> net = Net(8)
        >>> output = net(Tensor(x))
        >>> result = output.shape
        >>> print(result)
        (10, 8)
    """

    @prim_attr_register
    def __init__(self, dtype=mstype.int64):
        """Initialize RandomCategorical"""
        self.dtype = dtype

        valid_values = (mstype.int32, mstype.int16, mstype.int64)
        Validator.check_type_name("dtype", dtype, valid_values, self.name)
        self.init_prim_io_names(inputs=['logits', 'num_samples', 'seed'],
                                outputs=['output'])
        self.add_prim_attr("side_effect_hidden", True)


class Multinomial(Primitive):
    r"""
    Returns a tensor sampled from the multinomial probability distribution located in the corresponding
    row of tensor input.

    Note:
        - The rows of input do not need to sum to one (in which case we use the values as weights),
          but must be non-negative, finite and have a non-zero sum.
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .
        dtype(mindspore.dtype, optional): The type of output, must be ``mstype.int32`` or ``mstype.int64``.
            Default: ``mstype.int32``.

    Inputs:
        - **x** (Tensor) - the input tensor containing the cumsum of probabilities, must be 1 or 2
          dimensions.
        - **num_samples** (int) - number of samples to draw, must be a nonnegative number.

    Outputs:
        Tensor with the same rows as `x`, each row has num_samples sampled indices.

    Raises:
        TypeError: If neither `seed` nor `seed2` is an int.
        TypeError: If dtype of `num_samples` is not int.
        TypeError: If `dtype` is not mstype.int32 or mstype.int64.
        ValueError: If `seed` or `seed2` is less than 0.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> x = Tensor([[0., 9., 4., 0.]], mstype.float32)
        >>> multinomial = ops.Multinomial(seed=10)
        >>> output = multinomial(x, 2)
        >>> print(output)
        [[1 1]]
    """

    @prim_attr_register
    def __init__(self, seed=0, seed2=0, dtype=mstype.int32):
        """Initialize Multinomial."""
        Validator.check_non_negative_int(seed, "seed", self.name)
        Validator.check_non_negative_int(seed2, "seed2", self.name)
        self.init_prim_io_names(
            inputs=['x', 'num_samples'], outputs=['output'])
        Validator.check_value_type("dtype", dtype, [mstype.Type], self.name)
        valid_values = (mstype.int64, mstype.int32)
        Validator.check_type_name("dtype", dtype, valid_values, self.name)
        self.add_prim_attr("side_effect_hidden", True)


class MultinomialWithReplacement(Primitive):
    r"""
    Returns a tensor where each row contains `numsamples` indices sampled from the multinomial distribution
    with replacement. It diffs from `Multinomial` in that it allows the same outcome to be chosen multiple times.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.multinomial_with_replacement` for more details.

    Note:
        The rows of input do not need to sum to one (in which case we use the values as weights),
        but must be non-negative, finite and have a non-zero sum.

    Args:
        numsamples (int): number of samples to draw, must be a nonnegative number.
        replacement (bool, optional): Whether to draw with replacement or not. Default: ``False`` .

    Inputs:
        - **x** (Tensor) - the input tensor containing the cumsum of probabilities, must be 1 or 2
          dimensions.
        - **seed** (Tensor) - If `seed` and 'offset' are both set to 0, the random number generator
          is seeded by a random seed. Otherwise, it is seeded by the given seed and offset.
          Supported dtype: int64.
        - **offset** (Tensor) - Offset used to avoid seed collision. Supported dtype: int64.

    Outputs:
        Tensor with the same rows as `x`, each row has `numsamples` sampled indices.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> x = Tensor([[0., 9., 4., 0.]], mstype.float32)
        >>> seed = Tensor(2, mstype.int64)
        >>> offset = Tensor(5, mstype.int64)
        >>> multinomialwithreplacement = ops.MultinomialWithReplacement(numsamples=2,replacement=True)
        >>> output = multinomialwithreplacement(x, seed, offset)
        >>> print(output)
        [[1 1]]
    """

    @prim_attr_register
    def __init__(self, numsamples, replacement=False):
        """Initialize MultinomialWithReplacement."""
        Validator.check_non_negative_int(numsamples, "numsamples", self.name)
        Validator.check_value_type("replacement", replacement, [bool], self.name)
        self.init_prim_io_names(inputs=['x', 'seed', 'offset'], outputs=['y'])
        self.add_prim_attr("side_effect_hidden", True)


class UniformCandidateSampler(Primitive):
    r"""
    Uniform candidate sampler.

    This function samples a set of classes(sampled_candidates) from [0, range_max-1] based on uniform distribution.

    Refer to :func:`mindspore.ops.uniform_candidate_sampler` for more details.

    Args:
        num_true (int): The number of target classes in each training example.
        num_sampled (int): The number of classes to randomly sample. The sampled_candidates will have a shape
            of num_sampled. If unique=True, num_sampled must be less than or equal to range_max.
        unique (bool): Whether all sampled classes in a batch are unique.
        range_max (int): The number of possible classes, must be non-negative.
        seed (int, optional): Used for random number generation, must be non-negative. If seed has a value of 0,
            the seed will be replaced with a randomly generated value. Default: ``0`` .
        remove_accidental_hits (bool, optional): Whether accidental hit is removed.
            Accidental hit is when one of the true classes matches one of the sample classes.
            Set ``True`` to remove which accidentally sampling the true class as sample class. Default: ``False`` .

    Inputs:
        - **true_classes** (Tensor) - A Tensor. The target classes with a Tensor shape of
          :math:`(batch\_size, num\_true)`.

    Outputs:
        - **sampled_candidates** (Tensor) - The sampled_candidates is independent of the true classes.
          Shape: :math:`(num\_sampled, )`.
        - **true_expected_count** (Tensor) - The expected counts under the sampling distribution of each
          of true_classes. Shape: :math:`(batch\_size, num\_true)`.
        - **sampled_expected_count** (Tensor) - The expected counts under the sampling distribution of
          each of sampled_candidates. Shape: :math:`(num\_sampled, )`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> sampler = ops.UniformCandidateSampler(1, 3, False, 4, 1)
        >>> output1, output2, output3 = sampler(Tensor(np.array([[1], [3], [4], [6], [3]], dtype=np.int64)))
        >>> print(output1.shape)
        (3,)
        >>> print(output2.shape)
        (5, 1)
        >>> print(output3.shape)
        (3,)
    """

    @prim_attr_register
    def __init__(self, num_true, num_sampled, unique, range_max, seed=0, remove_accidental_hits=False):
        """Initialize UniformCandidateSampler"""
        Validator.check_value_type("num_true", num_true, [int], self.name)
        Validator.check_value_type(
            "num_sampled", num_sampled, [int], self.name)
        Validator.check_value_type("unique", unique, [bool], self.name)
        Validator.check_value_type("range_max", range_max, [int], self.name)
        Validator.check_value_type("seed", seed, [int], self.name)
        Validator.check_value_type(
            "remove_accidental_hits", remove_accidental_hits, [bool], self.name)
        Validator.check("value of num_true", num_true,
                        '', 0, Validator.GT, self.name)
        Validator.check("value of num_sampled", num_sampled,
                        '', 0, Validator.GT, self.name)
        Validator.check("value of range_max", range_max,
                        '', 0, Validator.GT, self.name)
        self.num_true = num_true
        if unique:
            Validator.check('value of num_sampled', num_sampled,
                            "value of range_max", range_max, Validator.LE, self.name)
        Validator.check("value of seed", seed, '', 0, Validator.GE, self.name)
        self.num_sampled = num_sampled
        self.add_prim_attr("side_effect_hidden", True)


class LogUniformCandidateSampler(Primitive):
    r"""
    Generates random labels with a log-uniform distribution for sampled_candidates.

    Randomly samples a tensor of sampled classes from the range of integers [0, range_max).

    Refer to :func:`mindspore.ops.log_uniform_candidate_sampler` for more details.

    Args:
        num_true (int, optional): The number of target classes per training example. Default: ``1`` .
        num_sampled (int, optional): The number of classes to randomly sample. Default: ``5`` .
        unique (bool, optional): Determines whether sample with rejection. If `unique` is ``True`` ,
          all sampled classes in a batch are unique. Default: ``True`` .
        range_max (int, optional): The number of possible classes. When `unique` is ``True`` ,
          `range_max` must be greater than or equal to `num_sampled`. Default: ``5`` .
        seed (int, optional): Random seed, must be non-negative. Default: ``0`` .

    Inputs:
        - **true_classes** (Tensor) - The target classes. With data type of int64 and
          shape :math:`(batch\_size, num\_true)` .

    Outputs:
        Tuple of 3 Tensors.

        - **sampled_candidates** (Tensor) - A Tensor with shape :math:`(num\_sampled,)`
          and the same type as `true_classes`.
        - **true_expected_count** (Tensor) - A Tensor with the same shape as `true_classes and` type float32.
        - **sampled_expected_count** (Tensor) - A Tensor with the same shape as `sampled_candidates` and type float32.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> sampler = ops.LogUniformCandidateSampler(2, 5, True, 5)
        >>> output1, output2, output3 = sampler(Tensor(np.array([[1, 7], [0, 4], [3, 3]])))
        >>> print(output1, output2, output3)
        [3 2 0 4 1]
        [[0.92312991 0.49336370]
         [0.99248987 0.65806371]
         [0.73553443 0.73553443]]
        [0.73553443 0.82625800 0.99248987 0.65806371 0.92312991]

    """

    @prim_attr_register
    def __init__(self, num_true=1, num_sampled=5, unique=True, range_max=5, seed=0):
        """Initialize LogUniformCandidateSampler"""
        self.init_prim_io_names(inputs=['true_classes'],
                                outputs=['sampled_candidates', 'true_expected_count', 'sampled_expected_count'])
        Validator.check_value_type("num_true", num_true, [int], self.name)
        Validator.check_value_type(
            "num_sampled", num_sampled, [int], self.name)
        Validator.check_value_type("unique", unique, [bool], self.name)
        Validator.check_value_type("range_max", range_max, [int], self.name)
        Validator.check_value_type("seed", seed, [int], self.name)
        self.num_true = Validator.check_number(
            "num_true", num_true, 1, Validator.GE, self.name)
        self.num_sampled = Validator.check_number(
            "num_sampled", num_sampled, 1, Validator.GE, self.name)
        Validator.check_number("range_max", range_max, 1, Validator.GE, self.name)
        if unique:
            Validator.check("range_max", range_max, "num_sampled",
                            num_sampled, Validator.GE, self.name)
        self.range_max = range_max
        self.unique = unique
        self.seed = Validator.check_number("seed", seed, 0, Validator.GE, self.name)
        self.add_prim_attr("side_effect_hidden", True)


class RandomShuffle(Primitive):
    r"""
    Randomly shuffles a Tensor along its first dimension.

    Note:
        - Random seed: a set of regular random numbers can be obtained through some complex mathematical algorithms,
          and the random seed determines the initial value of this random number. If the random seed is the same in two
          separate calls, the random number generated will not change.
        - Using the Philox algorithm to scramble seed and seed2 to obtain random seed so that the user doesn't need
          to worry about which seed is more important.

    Args:
        seed (int, optional): The operator-level random seed, used to generate random numbers,
            must be non-negative. Default: ``0`` .
        seed2 (int, optional): The global random seed, which combines with the operator-level
            random seed to determine the final generated random number, must be non-negative. Default: ``0`` .

    Inputs:
        - **x** (Tensor) - The Tensor need be shuffled.

    Outputs:
        Tensor. The shape and type are the same as the input `x`.

    Raises:
        TypeError: If data type of `seed` or `seed2` is not int.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> x = Tensor(np.array([1, 2, 3, 4]), mstype.float32)
        >>> shuffle = ops.RandomShuffle(seed=1, seed2=1)
        >>> output = shuffle(x)
        >>> print(output.shape)
        (4,)
    """

    @prim_attr_register
    def __init__(self, seed=0, seed2=0):
        """Initialize RandomShuffle"""
        self.init_prim_io_names(inputs=['input_x'], outputs=['output'])
        self.add_prim_attr("side_effect_hidden", True)
        Validator.check_non_negative_int(seed, "seed", self.name)
        Validator.check_non_negative_int(seed2, "seed2", self.name)


class Uniform(Primitive):
    r"""
    Generates random numbers according to the Uniform random number distribution.

    Args:
        minval(float):must be non-negative. Default: ``0.0`` .
        maxval(float):must be non-negative. Default: ``1.0`` .

    Inputs:
        - **x** (Tensor) - The x of random tensor to be generated.
          Only constant value is allowed, and the date type is float16, float32, float64.

    Raises:
        TypeError: If `minval` or `maxval` is not a float.
        TypeError: If `x`is not a Tensor.
        ValueError: If `minval` is larger than `maxval`.

    Outputs:
        - **output** (Tensor) - With the same type and shape as the 'x'.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> x = Tensor(np.random.randn(3,4), mstype.float64)
        >>> uniform = Uniform(minval=1.0, maxval=2.0)
        >>> y = uniform(x)
        >>> print(y.shape)
        (3, 4)
    """

    @prim_attr_register
    def __init__(self, minval=0., maxval=1., seed=0, offset=0):
        """Initialize Uniform"""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])
        self.add_prim_attr("from", minval)
        self.add_prim_attr("to", maxval)
        Validator.check_value_type('seed', seed, [int], self.name)
        Validator.check_value_type('offset', offset, [int], self.name)
        Validator.check('minval', minval, 'maxval', maxval, Validator.LE, self.name)
        Validator.check_non_negative_float(minval, "minval", self.name)
        Validator.check_non_negative_float(maxval, "maxval", self.name)
        self.add_prim_attr("side_effect_hidden", True)
