# Copyright 2023 Huawei Technologies Co., Ltd
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
"""Defines parameter operators with functional form."""

from __future__ import absolute_import
import numpy as np

from mindspore import context
from mindspore.ops import operations as P
from mindspore.ops import functional as F
from mindspore.ops.primitive import constexpr, _primexpr
from mindspore.ops.composite.multitype_ops import _constexpr_utils as const_utils
from mindspore.common import dtype as mstype
from mindspore.common.seed import _get_graph_seed
from mindspore.common.tensor import Tensor
from mindspore.ops.operations.random_ops import RandomShuffle, RandomChoiceWithMask
from mindspore.common.api import _function_forbid_reuse
from mindspore.ops.auto_generate import randperm


cast_ = P.Cast()
log_ = P.Log()
real_div_ = P.RealDiv()
reshape_ = P.Reshape()
shape_ = P.Shape()
top_k_ = P.TopK()

@constexpr
def _set_prim_op_user_data(prim, key, value):
    prim.add_prim_attr(key, value)
    return prim


@_function_forbid_reuse
def random_gamma(shape, alpha, seed=None):
    r"""
    Outputs random values from the Gamma distribution(s) described by alpha.


    Args:
        shape (Tensor): The shape of random tensor to be generated.
            Must be one of the following types: int32, int64. 1-D integer tensor.
        alpha (Tensor): The :math:`\alpha` distribution parameter.
            A Tensor. Must be one of the following types: half, float32, float64.
        seed (int, optional): Seed is used as entropy source for Random number engines generating pseudo-random numbers.
            Default: ``None`` , which will be treated as 0.

    Returns:
        Tensor. The shape should be equal to the concat shape between the input `shape` and the broadcast
        of `alpha`.
        The dtype is the same type as alpha.

    Raises:
        TypeError: If `shape` is not a Tensor.
        TypeError: If `alpha` is not a Tensor.
        TypeError: If `seed` is not an int.
        TypeError: If dtype of `alpha` is not half, float32 or float64.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> shape = Tensor(np.array([7, 5]), mindspore.int32)
        >>> alpha = Tensor(np.array([0.5, 1.5]), mindspore.float32)
        >>> output = ops.random_gamma(shape, alpha, seed=5)
        >>> result = output.shape
        >>> print(result)
        (7, 5, 2)
    """
    seed1, seed2 = _get_seed(seed, "random_gamma")
    random_gamma_op = P.RandomGamma(seed1, seed2)
    random_gamma_op = _set_prim_op_user_data(random_gamma_op, "random_cache", False)
    output = random_gamma_op(shape, alpha)
    return output


@constexpr(reuse_result=False)
def _get_seed(op_seed, kernel_name):
    """Get the graph-level seed."""
    return _get_graph_seed(op_seed, kernel_name)


@_function_forbid_reuse
def standard_laplace(shape, seed=None):
    r"""
    Generates random numbers according to the Laplace random number distribution (mean=0, lambda=1).
    It is defined as:

    .. math::
        \text{f}(x) = \frac{1}{2}\exp(-|x|)

    Args:
        shape (Union[tuple, Tensor]): The shape of random tensor to be generated. Only constant value is allowed
          when the input type is tuple. And the operator supports dynamic shape only when the input type is Tensor.
        seed (int, optional): Seed is used as entropy source for Random number engines generating pseudo-random numbers.
          Default: ``None`` , which will be treated as 0.

    Returns:
        Tensor. The shape that the input 'shape' denotes. The dtype is float32.

    Raises:
        TypeError: If shape is neither a tuple nor a Tensor.
        ValueError: If shape is a tuple containing non-positive items.
        ValueError: If shape is a Tensor, and the rank of the Tensor is not equal to 1.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import ops
        >>> shape = (4, 4)
        >>> output = ops.standard_laplace(shape)
        >>> result = output.shape
        >>> print(result)
        (4, 4)
    """
    seed1, seed2 = _get_seed(seed, "standard_laplace")
    standard_laplace_op = P.StandardLaplace(seed=seed1, seed2=seed2)
    standard_laplace_op = _set_prim_op_user_data(standard_laplace_op, "random_cache", False)
    return standard_laplace_op(shape)


@_function_forbid_reuse
def random_categorical(logits, num_sample, seed=0, dtype=mstype.int64):
    r"""
    Generates random samples from a given categorical distribution tensor.

    Args:
        logits (Tensor): The input tensor. 2-D Tensor with shape :math:`(batch\_size, num\_classes)`.
        num_sample (int):  Number of sample to be drawn. Only constant values is allowed.
        seed (int):  Random seed. Only constant values is allowed. Default: ``0`` .
        dtype (mindspore.dtype): The type of output. Its value must be one of mindspore.int16,
            mindspore.int32 and mindspore.int64. Default: ``mstype.int64`` .

    Returns:
        Tensor, The output Tensor with shape :math:`(batch\_size, num\_samples)`.

    Raises:
        TypeError: If `dtype` is not one of the following: mindspore.int16, mindspore.int32, mindspore.int64.
        TypeError: If `logits` is not a Tensor.
        TypeError: If neither `num_sample` nor `seed` is an int.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import ops
        >>> from mindspore import Tensor
        >>> import mindspore.common.dtype as mstype
        >>> import numpy as np
        >>> logits = Tensor(np.random.random((10, 5)).astype(np.float32), mstype.float32)
        >>> net = ops.random_categorical(logits, 8)
        >>> result = net.shape
        >>> print(result)
        (10, 8)
    """
    random_categorical_ = P.RandomCategorical(dtype)
    random_categorical_ = _set_prim_op_user_data(random_categorical_, "random_cache", False)
    return random_categorical_(logits, num_sample, seed)


@_function_forbid_reuse
def multinomial_with_replacement(x, seed, offset, numsamples, replacement=False):
    r"""
    Returns a tensor where each row contains numsamples indices sampled from the
    multinomial distribution with replacement. It is different from `multinomial` in that it allows
    the same outcome to be chosen multiple times.

    Note:
        The rows of input do not need to sum to one (in which case we use the values as weights),
        but must be non-negative, finite and have a non-zero sum.

    Args:
        x (Tensor): the input tensor containing the cumsum of probabilities, must be 1 or 2
          dimensions. Must be one of the following types: float16, float32, float64.
        seed (int): If seed is set to be -1, and offset is set to be 0, the random number
          generator is seeded by a random seed. Otherwise, it is seeded by the given seed.
        offset (int): Offset used to avoid seed collision.
        numsamples (int): the number of samples to draw.
        replacement (bool, optional): Whether to draw with replacement or not. Default: ``False`` .

    Returns:
        Tensor with the same rows as `x`, each row has `numsamples` sampled indices.

    Raises:
        TypeError: If `x`  is not a 1D or 2D Tensor.
        TypeError: If dtype of `x` is not float16, float32 or float64.
        TypeError: If `numsamples` is not an int.
        TypeError: If `replacement` is not a bool.
        ValueError: If the value of `numsamples` is not greater than x_shape[-1] when `replacement` is False.
        ValueError: If the sum of one row of `x` less than 0.
        ValueError: If one of the element of each row of `x` less than 0.
        ValueError: If `numsamples` equal or less than 0.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> x = Tensor([[0., 9., 4., 0.]], mstype.float32)
        >>> output = ops.multinomial_with_replacement(x, 2, 5, 2, True)
        >>> print(output)
        [[1 1]]
    """
    if not isinstance(seed, Tensor):
        if not isinstance(seed, int):
            raise TypeError(f"For multinomial_with_replacement,",
                            f"the input[seed] must be int, but got {type(seed)}.")
        seed = Tensor(seed, dtype=mstype.int64)
    if not isinstance(offset, Tensor):
        if not isinstance(offset, int):
            raise TypeError(f"For multinomial_with_replacement,",
                            f"the input[offset] must be int, but got {type(offset)}.")
        offset = Tensor(offset, dtype=mstype.int64)
    multinomial_with_replacement_ = P.MultinomialWithReplacement(numsamples=numsamples,
                                                                 replacement=replacement)
    multinomial_with_replacement_ = _set_prim_op_user_data(multinomial_with_replacement_, "random_cache", False)
    return multinomial_with_replacement_(x, seed, offset)


@_function_forbid_reuse
def uniform(shape, minval, maxval, seed=None, dtype=mstype.float32):
    """
    Generates random numbers according to the Uniform random number distribution.

    Note:
        The number in tensor minval should be strictly less than maxval at any position after broadcasting.

    Args:
        shape (Union[tuple, Tensor]): The shape of random tensor to be generated.
        minval (Tensor): The distribution parameter `a`.
          It defines the minimum possible generated value, with int32 or float32 data type.
          If dtype is int32, only one number is allowed.
        maxval (Tensor): The distribution parameter `b`.
          It defines the maximum possible generated value, with int32 or float32 data type.
          If dtype is int32, only one number is allowed.
        seed (int): Seed is used as entropy source for the random number engines to generate pseudo-random numbers,
          must be non-negative. Default: ``None`` , which will be treated as 0.
        dtype (mindspore.dtype): Type of the Uniform distribution. If it is int32, it generates numbers from discrete
          uniform distribution; if it is float32, it generates numbers from continuous uniform distribution. It only
          supports these two data types. Default: mstype.float32.

    Returns:
        Tensor. The shape should be equal to the broadcasted shape between the input `shape` and shapes
        of `minval` and `maxval`.
        The dtype is designated as the input `dtype`.

    Raises:
        TypeError: If `shape` is neither a tuple nor a Tensor.
        TypeError: If 'minval' or 'maxval' is neither int32 nor float32
            and dtype of 'minval' is not the same as 'maxval'.
        TypeError: If `seed` is not an int.
        TypeError: If 'dtype' is neither int32 nor float32.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import Tensor, ops
        >>> import mindspore
        >>> import numpy as np
        >>> # For discrete uniform distribution, only one number is allowed for both minval and maxval:
        >>> shape = (4, 2)
        >>> minval = Tensor(1, mindspore.int32)
        >>> maxval = Tensor(2, mindspore.int32)
        >>> output = ops.uniform(shape, minval, maxval, seed=5, dtype=mindspore.int32)
        >>>
        >>> # For continuous uniform distribution, minval and maxval can be multi-dimentional:
        >>> shape = (3, 1, 2)
        >>> minval = Tensor(np.array([[3, 4], [5, 6]]), mindspore.float32)
        >>> maxval = Tensor([8.0, 10.0], mindspore.float32)
        >>> output = ops.uniform(shape, minval, maxval, seed=5)
        >>> result = output.shape
        >>> print(result)
        (3, 2, 2)
    """
    if not isinstance(minval, Tensor) or not isinstance(maxval, Tensor):
        raise TypeError(f"For functional operator[uniform], the input[minval] and input[maxval] must be a Tensor.")

    minval_dtype = F.dtype(minval)
    maxval_dtype = F.dtype(maxval)
    const_utils.check_type_valid(dtype, [mstype.int32, mstype.float32], 'uniform')
    const_utils.check_tensors_dtype_same(minval_dtype, dtype, "uniform")
    const_utils.check_tensors_dtype_same(maxval_dtype, dtype, "uniform")
    seed1, seed2 = _get_seed(seed, "uniform")
    if const_utils.is_same_type(dtype, mstype.int32):
        random_uniform = P.UniformInt(seed1, seed2)
        random_uniform = _set_prim_op_user_data(random_uniform, "random_cache", False)
        value = random_uniform(shape, minval, maxval)
    else:
        uniform_real = P.UniformReal(seed1, seed2)
        uniform_real = _set_prim_op_user_data(uniform_real, "random_cache", False)
        uniform_real = uniform_real(shape)
        value = uniform_real * (maxval - minval) + minval
    return value


@_function_forbid_reuse
def standard_normal(shape, seed=None):
    r"""
    Generates random numbers according to the standard Normal (or Gaussian) random number distribution.

    Returns the tensor with the given shape, the random numbers in it drawn from normal distributions
    whose mean is 0 and standard deviation is 1.

    .. math::
        f(x)=\frac{1}{\sqrt{2 \pi}} e^{\left(-\frac{x^{2}}{2}\right)}

    Args:
        shape (Union[tuple, Tensor]): The shape of random tensor to be generated. Only constant value is allowed
          when the input type is tuple. And the operator supports dynamic shape only when the input type is Tensor.
        seed (int, optional): Seed is used as entropy source for Random number engines generating pseudo-random numbers.
          Default: ``None`` , which will be treated as 0.

    Returns:
        Tensor. The shape that the input 'shape' denotes. The dtype is float32.

    Raises:
        TypeError: If `shape` is neither a tuple nor a Tensor.
        ValueError: If `shape` is a tuple containing non-positive items.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import ops
        >>> shape = (4, 4)
        >>> output = ops.standard_normal(shape)
        >>> result = output.shape
        >>> print(result)
        (4, 4)
    """
    seed1, seed2 = _get_seed(seed, "standard_normal")
    standard_normal_op = P.StandardNormal(seed=seed1, seed2=seed2)
    standard_normal_op = _set_prim_op_user_data(standard_normal_op, "random_cache", False)
    return standard_normal_op(shape)


@_function_forbid_reuse
def uniform_candidate_sampler(true_classes,
                              num_true,
                              num_sampled,
                              unique,
                              range_max,
                              seed=0,
                              remove_accidental_hits=False):
    r"""
    Uniform candidate sampler.

    This function samples a set of classes(sampled_candidates) from [0, range_max-1] based on uniform distribution.
    If unique=True, candidates are drawn without replacement, else unique=False with replacement.

    Args:
        true_classes (Tensor): A Tensor. The target classes with a Tensor shape of :math:`(batch\_size, num\_true)` .
        num_true (int): The number of target classes in each training example.
        num_sampled (int): The number of classes to randomly sample. The sampled_candidates will have a shape
            of num_sampled. If unique=True, num_sampled must be less than or equal to range_max.
        unique (bool): Whether all sampled classes in a batch are unique.
        range_max (int): The number of possible classes, must be positive.
        seed (int): Used for random number generation, must be non-negative. If seed has a value of 0,
            the seed will be replaced with a randomly generated value. Default: ``0`` .
        remove_accidental_hits (bool): Whether accidental hit is removed.
            Accidental hit is when one of the true classes matches one of the sample classes.
            Set ``True`` to remove which accidentally sampling the true class as sample class. Default: ``False`` .

    Returns:
        - **sampled_candidates** (Tensor) - The sampled_candidates is independent of the true classes.
          shape: :math:`(num\_sampled, )` .
        - **true_expected_count** (Tensor) - The expected counts under the sampling distribution of each
          of true_classes. shape: :math:`(batch\_size, num\_true)` .
        - **sampled_expected_count** (Tensor) - The expected counts under the sampling distribution of
          each of sampled_candidates. shape: :math:`(num\_sampled, )` .

    Raises:
        TypeError: If neither `num_true` nor `num_sampled` is an int.
        TypeError: If neither `unique` nor `remove_accidental_hits` is a bool.
        TypeError: If neither `range_max` nor `seed` is an int.
        TypeError: If `true_classes` is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> data = Tensor(np.array([[1], [3], [4], [6], [3]], dtype=np.int64))
        >>> output1, output2, output3 = ops.uniform_candidate_sampler(data, 1, 3, False, 4, 1)
        >>> print(output1.shape)
        (3,)
        >>> print(output2.shape)
        (5, 1)
        >>> print(output3.shape)
        (3,)
    """
    sampler_op = P.UniformCandidateSampler(num_true,
                                           num_sampled,
                                           unique,
                                           range_max,
                                           seed=seed,
                                           remove_accidental_hits=remove_accidental_hits)
    sampler_op = _set_prim_op_user_data(sampler_op, "random_cache", False)
    sampled_candidates, true_expected_count, sampled_expected_count = sampler_op(true_classes)
    return sampled_candidates, true_expected_count, sampled_expected_count


@_function_forbid_reuse
def random_poisson(shape, rate, seed=None, dtype=mstype.float32):
    r"""
    Generates random number Tensor with shape `shape` according to a Poisson distribution with mean `rate`.


    .. math::

        \text{P}(i|μ) = \frac{\exp(-μ)μ^{i}}{i!}

    Args:
        shape (Tensor): The shape of random tensor to be sampled from each poisson distribution, 1-D `Tensor` whose
            dtype is mstype.int32 or mstype.int64.
        rate (Tensor): The :math:`μ` parameter the distribution is constructed with.
            It represents the mean of the distribution
            and also the variance of the distribution. It should be a `Tensor` whose dtype is mstype.int64,
            mstype.int32, mstype.float64, mstype.float32 or mstype.float16.
        seed (int, optional): Seed is used as entropy source for the random number engines to generate pseudo-random
            numbers and must be non-negative. Default: ``None`` , which will be treated as 0.
        dtype (mindspore.dtype): The data type of output: ``mstype.int64``, ``mstype.int32``,
            ``mstype.float64``, ``mstype.float32`` or ``mstype.float16``. Default: ``mstype.float32``.

    Returns:
        A Tensor whose shape is `mindspore.concat(['shape', mindspore.shape('rate')], axis=0)` and data type is equal to
        argument `dtype`.

    Raises:
        TypeError: If `shape` is not a Tensor.
        TypeError: If datatype of `shape` is not mstype.int64 nor mstype.int32.
        ValueError: If shape of `shape` is not 1-D.
        TypeError: If `rate` is not a Tensor nor a scalar.
        TypeError: If datatype of `rate` is not in [mstype.int64, mstype.int32,
            mstype.float64, mstype.float32 or mstype.float16].
        TypeError: If `seed` is not a non-negtive int.
        TypeError: If `dtype` is not in [mstype.int64, mstype.int32, mstype.float64,
            mstype.float32 nor mstype.float16].
        ValueError: If any element of input `shape` tensor is not positive.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> # case 1: 1-D shape, 2-D rate, float64 output
        >>> shape = Tensor(np.array([2, 2]), mindspore.int64)
        >>> rate = Tensor(np.array([[5.0, 10.0], [5.0, 1.0]]), mindspore.float32)
        >>> output = ops.random_poisson(shape, rate, seed=5, dtype=mindspore.float64)
        >>> print(output.shape, output.dtype)
        (2, 2, 2, 2) Float64
        >>> # case 2: 1-D shape, scalar rate, int64 output
        >>> shape = Tensor(np.array([2, 2]), mindspore.int64)
        >>> rate = Tensor(5.0, mindspore.float64)
        >>> output = ops.random_poisson(shape, rate, seed=5, dtype=mindspore.int64)
        >>> print(output.shape, output.dtype)
        (2, 2) Int64
    """
    seed1, seed2 = _get_seed(seed, "random_poisson")
    prim_random_poisson = P.RandomPoisson(seed1, seed2, dtype)
    prim_random_poisson = _set_prim_op_user_data(prim_random_poisson, "random_cache", False)
    value = prim_random_poisson(shape, rate)
    return value


@_function_forbid_reuse
def shuffle(x, seed=None):
    r"""
    Randomly shuffles a Tensor along its first dimension.

    Args:
        x (Tensor): The Tensor need be shuffled.
        seed (int, optional): Random seed used for random number generation, must be non-negative. If `seed` is 0,
            which will be replaced with a randomly generated value. Default: ``None`` , which will be treated as 0.

    Returns:
        Tensor. The shape and type are the same as the input `x`.

    Raises:
        TypeError: If data type of `seed` is not None or non-negative int.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> x = Tensor(np.array([1, 2, 3, 4]), mstype.float32)
        >>> output = ops.shuffle(x, seed=1)
        >>> print(output)
        [3. 4. 2. 1.]
    """
    seed, seed2 = _get_seed(seed, "shuffle")
    random_shuffle_ = RandomShuffle(seed=seed, seed2=seed2)
    random_shuffle_ = _set_prim_op_user_data(random_shuffle_, "random_cache", False)
    output = random_shuffle_(x)
    return output


@_function_forbid_reuse
def log_uniform_candidate_sampler(true_classes, num_true=1, num_sampled=5, unique=True, range_max=5, seed=0):
    r"""
    Generates random labels with a log-uniform distribution for sampled_candidates.

    Randomly samples a tensor of sampled classes from the range of integers [0, range_max).

    Args:
        true_classes (Tensor): The target classes. With data type of int64 and
          shape :math:`(batch\_size, num\_true)` .
        num_true (int): The number of target classes per training example. Default: ``1`` .
        num_sampled (int): The number of classes to randomly sample. Default: ``5`` .
        unique (bool): Determines whether sample with rejection. If `unique` is ``True`` ,
          all sampled classes in a batch are unique. Default: ``True`` .
        range_max (int): The number of possible classes. When `unique` is ``True`` ,
          `range_max` must be greater than or equal to `num_sampled`. Default: ``5`` .
        seed (int): Random seed, must be non-negative. Default: ``0`` .

    Returns:
        Tuple of 3 Tensors.

        - **sampled_candidates** (Tensor) - A Tensor with shape :math:`(num\_sampled,)`
          and the same type as `true_classes`.
        - **true_expected_count** (Tensor) - A Tensor with the same shape as `true_classes and` type float32.
        - **sampled_expected_count** (Tensor) - A Tensor with the same shape as `sampled_candidates` and type float32.

    Raises:
        TypeError: If neither `num_true` nor `num_sampled` is an int.
        TypeError: If `unique` is not a bool.
        TypeError: If neither `range_max` nor `seed` is an int.
        TypeError: If `true_classes` is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> output1, output2, output3 = ops.log_uniform_candidate_sampler(
        ... Tensor(np.array([[1, 7], [0, 4], [3, 3]])), 2, 5, True, 5)
        >>> print(output1, output2, output3)
        [3 2 0 4 1]
        [[0.92312991 0.49336370]
         [0.99248987 0.65806371]
         [0.73553443 0.73553443]]
        [0.73553443 0.82625800 0.99248987 0.65806371 0.92312991]

    """

    sampler = P.LogUniformCandidateSampler(num_true, num_sampled, unique, range_max, seed)
    sampler = _set_prim_op_user_data(sampler, "random_cache", False)
    return sampler(true_classes)


@_function_forbid_reuse
def choice_with_mask(input_x, count=256, seed=None):
    """
    Generates a random sample as index tensor with a mask tensor from a given tensor.

    The `input_x` must be a tensor whose dimension is not less than 1. If its dimension is greater than or equal to 2,
    the first dimension specifies the number of samples.
    The returned index tensor denotes the index of the nonzero
    sample, the mask tensor denotes which elements in the index tensor are valid.

    Args:
        input_x (Tensor[bool]): The input tensor.
            The input tensor rank must be greater than or equal to 1 and less than or equal to 5.
        count (int, optional): Number of items expected to get and the number must be greater than 0. Default: ``256`` .
        seed (int, optional): Seed is used as entropy source for Random number engines generating pseudo-random numbers.
            Default: ``None`` , which will be treated as 0.

    Returns:
        Two tensors, the first one is the index tensor and the other one is the mask tensor.

        - **index** (Tensor) - The output shape is 2-D.
        - **mask** (Tensor) - The output shape is 1-D.

    Raises:
        TypeError: If `count` is not an int.
        TypeError: If `seed` is not an int.
        TypeError: If `input_x` is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor(np.ones(shape=[240000, 4]).astype(np.bool))
        >>> output_y, output_mask = ops.choice_with_mask(input_x)
        >>> result = output_y.shape
        >>> print(result)
        (256, 2)
        >>> result = output_mask.shape
        >>> print(result)
        (256,)
    """
    seed1, seed2 = _get_seed(seed, "choice_with_mask")
    choice_with_mask_ = RandomChoiceWithMask(count=count, seed=seed1, seed2=seed2)
    choice_with_mask_ = _set_prim_op_user_data(choice_with_mask_, "random_cache", False)
    output = choice_with_mask_(input_x)
    return output


@constexpr
def is_cpu_backend():
    """Check if the CPU is used"""
    return context.get_context('device_target') == 'CPU'


@_function_forbid_reuse
def normal(shape, mean, stddev, seed=None):
    """
    Generates random numbers according to the Normal (or Gaussian) random number distribution.

    Args:
        shape (tuple): The shape of random tensor to be generated.
          The format is :math:`(N,*)` where :math:`*` means, any number of additional dimensions.
        mean (Union[Tensor, int, float]): The mean μ distribution parameter, which specifies the location of the peak.
        stddev (Union[Tensor, int, float]): The deviation σ distribution parameter. It should be greater than 0.
        seed (int): Seed is used as entropy source for the Random number engines to generate pseudo-random numbers.
          The value must be non-negative. Default: ``None`` , which will be treated as 0.

    Returns:
        Tensor. The shape should be equal to the broadcasted shape between the input `shape` and shapes
        of `mean` and `stddev`.
        The dtype is [float32, float64].

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> shape = (3, 1, 2)
        >>> mean = Tensor(np.array([[3, 4], [5, 6]]), mindspore.float32)
        >>> stddev = Tensor(1.0, mindspore.float32)
        >>> output = ops.normal(shape, mean, stddev, seed=5)
        >>> result = output.shape
        >>> print(result)
        (3, 2, 2)
        >>> shape = (3, 1, 3)
        >>> mean = Tensor(np.array([[3, 4, 3], [3, 5, 6]]), mindspore.float32)
        >>> stddev = Tensor(1.0, mindspore.float32)
        >>> output = ops.normal(shape, mean, stddev, seed=5)
        >>> result = output.shape
        >>> print(result)
        (3, 2, 3)
        >>> shape = (3, 1, 3)
        >>> mean = Tensor(np.array([[1, 2, 3], [3, 4, 3], [3, 5, 6]]), mindspore.float32)
        >>> stddev = Tensor(1.0, mindspore.float32)
        >>> output = ops.normal(shape, mean, stddev, seed=5)
        >>> result = output.shape
        >>> print(result)
        (3, 3, 3)
    """
    _check_param("normal", "mean", mean)
    _check_param("normal", "stddev", stddev)
    if not isinstance(mean, Tensor):
        mean = Tensor(mean)
    if not isinstance(stddev, Tensor):
        stddev = Tensor(stddev)
    seed1, seed2 = _get_seed(seed, "normal")
    stdnormal = P.StandardNormal(seed1, seed2)
    stdnormal = _set_prim_op_user_data(stdnormal, "random_cache", False)
    _check_shape(shape)
    random_normal = stdnormal(shape)
    value = random_normal * stddev + mean
    return value


@_function_forbid_reuse
def laplace(shape, mean, lambda_param, seed=None):
    r"""
    Generates random numbers according to the Laplace random number distribution.
    It is defined as:

    .. math::
        \text{f}(x;μ,λ) = \frac{1}{2λ}\exp(-\frac{|x-μ|}{λ}),

    Args:
        shape (tuple): The shape of random tensor to be generated.
          The format is :math:`(N,*)` where :math:`*` means, any number of additional dimensions.
        mean (Tensor): The mean μ distribution parameter, which specifies the location of the peak.
          With float32 data type.
        lambda_param (Tensor): The parameter used for controlling the variance of this random distribution. The
          variance of Laplace distribution is equal to twice the square of lambda_param. With float32 data type.
        seed (int, optional): Seed is used as entropy source for Random number engines generating pseudo-random numbers.
          Default: ``None`` , which will be treated as 0.

    Returns:
        Tensor. The shape should be the broadcasted shape of input `shape` and shapes of `mean` and `lambda_param`.
        The dtype is float32.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor
        >>> from mindspore import ops as ops
        >>> shape = (2, 3)
        >>> mean = Tensor(1.0, mindspore.float32)
        >>> lambda_param = Tensor(1.0, mindspore.float32)
        >>> output = ops.laplace(shape, mean, lambda_param, seed=5)
        >>> print(output.shape)
        (2, 3)
    """
    mean_dtype = F.dtype(mean)
    lambda_param_dtype = F.dtype(lambda_param)
    const_utils.check_tensors_dtype_same(mean_dtype, mstype.float32, "laplace")
    const_utils.check_tensors_dtype_same(lambda_param_dtype, mstype.float32, "laplace")
    seed1, seed2 = _get_seed(seed, "laplace")
    stdlaplace = P.StandardLaplace(seed1, seed2)
    stdlaplace = _set_prim_op_user_data(stdlaplace, "random_cache", False)
    _check_shape(shape)
    rnd = stdlaplace(shape)
    value = rnd * lambda_param + mean
    return value


@_function_forbid_reuse
def gamma(shape, alpha, beta, seed=None):
    r"""
    Generates random numbers according to the Gamma random number distribution.

    Args:
        shape (tuple): The shape of random tensor to be generated.
        alpha (Tensor): The :math:`\alpha` distribution parameter. It should be greater than 0 with float32 data type.
        beta (Tensor): The :math:`\beta` distribution parameter. It should be greater than 0 with float32 data type.
        seed (int, optional): Seed is used as entropy source for the random number engines to generate
            pseudo-random numbers, must be non-negative. Default: ``None`` , which will be treated as ``0`` .

    Returns:
        Tensor. The shape should be equal to the broadcasted shape between the input `shape` and shapes
        of `alpha` and `beta`.
        The dtype is float32.

    Raises:
        TypeError: If `shape` is not a tuple.
        TypeError: If neither `alpha` nor `beta` is a Tensor.
        TypeError: If `seed` is not an int.
        TypeError: If dtype of `alpha` and `beta` is not float32.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> # case 1: alpha_shape is (2, 2)
        >>> shape = (3, 1, 2)
        >>> alpha = Tensor(np.array([[3, 4], [5, 6]]), mindspore.float32)
        >>> beta = Tensor(np.array([1.0]), mindspore.float32)
        >>> output = ops.gamma(shape, alpha, beta, seed=5)
        >>> result = output.shape
        >>> print(result)
        (3, 2, 2)
        >>> # case 2: alpha_shape is (2, 3), so shape is (3, 1, 3)
        >>> shape = (3, 1, 3)
        >>> alpha = Tensor(np.array([[1, 3, 4], [2, 5, 6]]), mindspore.float32)
        >>> beta = Tensor(np.array([1.0]), mindspore.float32)
        >>> output = ops.gamma(shape, alpha, beta, seed=5)
        >>> result = output.shape
        >>> print(result)
        (3, 2, 3)
        >>> # case 3: beta_shape is (1, 2), the output is different.
        >>> shape = (3, 1, 2)
        >>> alpha = Tensor(np.array([[3, 4], [5, 6]]), mindspore.float32)
        >>> beta = Tensor(np.array([1.0, 2]), mindspore.float32)
        >>> output = ops.gamma(shape, alpha, beta, seed=5)
        >>> print(output)
        [[[ 2.2132034  5.8855834]
          [ 3.8825176  8.6066265]]
         [[ 3.3981476  7.5805717]
          [ 3.7190282 19.941492 ]]
         [[ 2.9512358  2.5969937]
          [ 3.786061   5.160872 ]]]
        >>> # case 4: beta_shape is (2, 1), the output is different.
        >>> shape = (3, 1, 2)
        >>> alpha = Tensor(np.array([[3, 4], [5, 6]]), mindspore.float32)
        >>> beta = Tensor(np.array([[1.0], [2.0]]), mindspore.float32)
        >>> output = ops.gamma(shape, alpha, beta, seed=5)
        >>> print(output)
        [[[ 5.6085486  7.8280783]
         [ 15.97684  16.116285]]
        [[ 1.8347423  1.713663]
         [ 3.2434065 15.667398]]
        [[ 4.2922077  7.3365674]
         [ 5.3876944  13.159832 ]]]
    """
    seed1, seed2 = _get_seed(seed, "gamma")
    gamma_v = P.Gamma(seed1, seed2)
    gamma_v = _set_prim_op_user_data(gamma_v, "random_cache", False)
    value = gamma_v(shape, alpha, beta)
    return value


@_primexpr
def _generate_shapes(shape):
    """Generate shapes for randn and rand."""
    if not shape:
        size = (1,)
    elif len(shape) == 1:
        if isinstance(shape[0], int):
            size = shape
        elif isinstance(shape[0], list):
            size = tuple(shape[0])
        elif isinstance(shape[0], tuple):
            size = shape[0]
        else:
            raise TypeError(f"If the length of the argument 'shape' is 1, the type of the argument 'shape' must be "
                            f"one of ['int', 'list', 'tuple'], but got {shape[0]}.")
    else:
        for value in shape:
            if not isinstance(value, int):
                raise TypeError(f"If the length of the argument 'shape' is > 1, the type of the argument 'shape' must "
                                f"all be int, but got {value}.")
        size = shape
    return size


@_function_forbid_reuse
def rand(*size, dtype=None, seed=None):
    r"""
    Returns a new tensor that fills numbers from the uniform distribution over an interval :math:`[0, 1)`
    based on the given shape and dtype.

    Args:
        size (Union[int, tuple(int), list(int)]): Shape of the new tensor, e.g. :math:`(2, 3)` or :math:`2`.

    Keyword Args:
        dtype (:class:`mindspore.dtype`, optional): Designated tensor dtype, it must be float type. If None,
            `mindspore.float32` will be applied. Default: ``None`` .
        seed (int, optional): Random seed, must be greater or equal to 0. Default: ``None`` , and ``0`` will be used.

    Returns:
        Tensor, with the designated shape and dtype, filled with random numbers from the uniform distribution on
        the interval :math:`[0, 1)`.

    Raises:
        TypeError: `seed` is not a non-negative integer.
        ValueError: If `dtype` is not a `mstype.float_type` type.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore.ops as ops
        >>> print(ops.rand((2,3)))
        [[4.1702199e-01 9.9718481e-01 7.2032452e-01]
         [9.3255734e-01 1.1438108e-04 1.2812445e-01]]
    """
    if dtype is None:
        dtype = mstype.float32
    elif dtype not in mstype.float_type:
        raise ValueError(f"For 'rand', the 'dtype' must be a float type, but got {dtype}.")
    shape = _generate_shapes(size)
    seed1, seed2 = _get_seed(seed, 'rand')
    rand_op = P.UniformReal(seed1, seed2)
    rand_op = _set_prim_op_user_data(rand_op, "random_cache", False)
    output = rand_op(shape)
    return cast_(output, dtype)


@_function_forbid_reuse
def rand_like(input, seed=None, *, dtype=None):
    r"""
    Returns a new tensor that fills numbers from the uniform distribution over an interval :math:`[0, 1)`
    based on the given shape and dtype.

    Args:
        input (Tensor): Input Tensor to specify the output shape and its default dtype.
        seed (int, optional): Random seed, must be greater or equal to 0. Default: ``None`` , and ``0`` will be used.

    Keyword Args:
        dtype (:class:`mindspore.dtype`, optional): Designated tensor dtype, it must be float type. If None,
            the same dtype of `input` will be applied. Default: ``None`` .

    Returns:
        Tensor, with the designated shape and dtype, filled with random numbers from the uniform distribution on
        the interval :math:`[0, 1)`.

    Raises:
        TypeError: If `seed` is not a non-negative integer.
        ValueError: If `dtype` is not a `mstype.float_type` type.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore as ms
        >>> from mindspore import Tensor, ops
        >>> a = Tensor([[2, 3, 4], [1, 2, 3]])
        >>> print(ops.rand_like(a, dtype=ms.float32))
        [[4.1702199e-01 9.9718481e-01 7.2032452e-01]
         [9.3255734e-01 1.1438108e-04 1.2812445e-01]]
    """
    if not isinstance(input, Tensor):
        raise TypeError(f"For 'rand_like', the 'input' must be a Tensor, but got {type(input)}")
    if dtype is None:
        dtype = input.dtype
    if dtype not in mstype.float_type:
        raise ValueError(f"For 'rand_like', the 'dtype' must be a float type, but got {dtype}.")
    shape = input.shape
    seed1, seed2 = _get_seed(seed, 'rand_like')
    rand_op = P.UniformReal(seed1, seed2)
    rand_op = _set_prim_op_user_data(rand_op, "random_cache", False)
    output = rand_op(shape)
    return cast_(output, dtype)


@_function_forbid_reuse
def randn(*size, dtype=None, seed=None):
    r"""
    Returns a new Tensor with given shape and dtype, filled with a sample (or samples)
    from the standard normal distribution.

    Args:
        size (Union[int, tuple(int), list(int)]): Shape of the new tensor, e.g., :math:`(2, 3)` or :math:`2`.

    Keyword Args:
        dtype (:class:`mindspore.dtype`, optional): Designated tensor dtype, it must be float type. If None,
            `mindspore.float32` will be used. Default: ``None`` .
        seed (int, optional): Random seed, must be greater or equal to 0. Default: ``None`` , and 0 will be used.

    Returns:
        Tensor, with the designated shape and dtype, filled with a sample (or samples) from the
        "standard normal" distribution.

    Raises:
        TypeError: `seed` is not a non-negative integer.
        ValueError: If `dtype` is not a `mstype.float_type`.
        ValueError: If `size` contains invalid number.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore.ops as ops
        >>> print(ops.randn((2, 2)))
        [[ 0.30639967 -0.42438635]
         [-0.4287376   1.3054721 ]]
    """
    if dtype is None:
        dtype = mstype.float32
    elif dtype not in mstype.float_type:
        raise ValueError(f"For 'randn', the 'dtype' must be a float type, but got {dtype}.")
    shape = _generate_shapes(size)
    seed1, seed2 = _get_seed(seed, 'randn')
    rand_op = P.StandardNormal(seed1, seed2)
    rand_op = _set_prim_op_user_data(rand_op, "random_cache", False)
    output = rand_op(shape)
    return cast_(output, dtype)


@_function_forbid_reuse
def randn_like(input, seed=None, *, dtype=None):
    r"""
    Returns a new Tensor with given shape and dtype, filled with a sample (or samples) from the standard normal
    distribution.

    Args:
        input (Tensor): Input Tensor to specify the output shape and its default dtype.
        seed (int, optional): Random seed, must be greater or equal to 0. Default: ``None`` , and 0 will be used.

    Keyword Args:
        dtype (:class:`mindspore.dtype`, optional): Designated tensor dtype, it must be float type. If None,
            `mindspore.float32` will be used. Default: ``None`` .

    Returns:
        Tensor, with the designated shape and dtype, filled with a sample (or samples) from the
        "standard normal" distribution.

    Raises:
        TypeError: `seed` is not a non-negative integer.
        ValueError: If `dtype` is not a `mstype.float_type`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore as ms
        >>> from mindspore import Tensor, ops
        >>> a = Tensor([[1, 2, 3], [4, 5, 6]])
        >>> print(ops.randn_like(a, dtype=ms.float32))
        [[ 0.30639967 -0.42438635 -0.20454668]
         [-0.4287376   1.3054721   0.64747655]]
    """
    if not isinstance(input, Tensor):
        raise TypeError(f"For 'randn_like', the 'input' must be a Tensor, but got {type(input)}")
    if dtype is None:
        dtype = mstype.float32
    if dtype not in mstype.float_type:
        raise ValueError(f"For 'randn_like', the 'dtype' must be a float type, but got {dtype}.")
    shape = input.shape
    seed1, seed2 = _get_seed(seed, 'randn_like')
    rand_op = P.StandardNormal(seed1, seed2)
    rand_op = _set_prim_op_user_data(rand_op, "random_cache", False)
    output = rand_op(shape)
    return cast_(output, dtype)


@_function_forbid_reuse
def randint(low, high, size, seed=None, *, dtype=None):
    r"""
    Returns a Tensor whose elements are random integers in the range of [ `low` , `high` ) .

    Args:
        low (int): Start value of interval.
        high (int): End value of interval.
        size (tuple): Shape of the new tensor.
        seed (int, optional): Random seed, must be greater or equal to 0. Default: ``None`` , and ``0`` will be used.

    Keyword Args:
        dtype (:class:`mindspore.dtype`, optional): Designated tensor dtype, it must be int type. If ``None`` ,
            `mindspore.int64` will be used. Default: ``None`` .

    Returns:
        Tensor, with the designated shape and dtype, filled with random integers from low (inclusive)
        to high (exclusive).

    Raises:
        TypeError: `seed` is not a non-negative integer.
        TypeError: `size` is not a tuple.
        TypeError: `low` or `high` is not an integer.
        ValueError: If `dtype` is not a `mstype.int_type`.


    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore.ops as ops
        >>> print(ops.randint(1, 10, (2,3)))
        [[4 9 7]
         [9 1 2]]
    """
    if dtype is None:
        dtype = mstype.int64
    elif dtype not in mstype.int_type:
        raise ValueError(f"For 'randint', the 'dtype' must be an int type, but got {dtype}.")
    if not isinstance(size, tuple):
        raise ValueError(f"For 'randint', the input 'size' must be a tuple, but got {size}.")
    if not isinstance(low, int) or isinstance(low, bool):
        raise TypeError(f"For 'randint_like', 'low' must be an int, but got {type(low)}.")
    if not isinstance(high, int) or isinstance(high, bool):
        raise TypeError(f"For 'randint_like', 'high' must be an int, but got {type(high)}.")
    seed1, seed2 = _get_seed(seed, 'randint')
    rand_op = P.UniformInt(seed1, seed2)
    rand_op = _set_prim_op_user_data(rand_op, "random_cache", False)
    low_ = Tensor(low, mstype.int32)
    high_ = Tensor(high, mstype.int32)
    output = rand_op(size, low_, high_)
    return cast_(output, dtype)


@_function_forbid_reuse
def randint_like(input, low, high, seed=None, *, dtype=None):
    r"""
    Returns a tensor with the same shape as Tensor `input` whose elements are random integers in the range
    of [ `low` , `high` ) .

    Args:
        input (Tensor): Input Tensor to specify the output shape and its default dtype.
        low(int): Start value of interval.
        high(int): End value of interval.
        seed (int, optional): Random seed, must be greater or equal to 0. Default: ``None`` , and 0 will be used.

    Keyword Args:
        dtype (:class:`mindspore.dtype`, optional): Designated tensor dtype, it must be int type. If ``None`` ,
            the same dtype of `input` will be applied. Default: ``None`` .

    Returns:
        Tensor, with the designated shape and dtype, filled with random integers from low (inclusive)
        to high (exclusive).

    Raises:
        TypeError: `seed` is not a non-negative integer.
        TypeError: `low` or `high` is not an integer.
        ValueError: If `dtype` is not a `mstype.int_type`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
       >>> from mindspore import Tensor, ops
       >>> a = Tensor([[1, 2, 3], [3, 2, 1]])
       >>> print(ops.randint_like(a, 1, 10))
       [[4 9 7]
        [9 1 2]]
    """
    if not isinstance(input, Tensor):
        raise TypeError(f"For 'randint_like', the 'input' must be a Tensor, but got {type(input)}")
    if dtype is None:
        dtype = input.dtype
    if dtype not in mstype.int_type:
        raise ValueError(f"For 'randint_like', the 'dtype' must be an int type, but got {dtype}.")
    if not isinstance(low, int) or isinstance(low, bool):
        raise TypeError(f"For 'randint_like', 'low' must be an int, but got {type(low)}.")
    if not isinstance(high, int) or isinstance(high, bool):
        raise TypeError(f"For 'randint_like', 'high' must be an int, but got {type(high)}.")
    size = input.shape
    seed1, seed2 = _get_seed(seed, 'randint_like')
    rand_op = P.UniformInt(seed1, seed2)
    rand_op = _set_prim_op_user_data(rand_op, "random_cache", False)
    low_ = Tensor(low, mstype.int32)
    high_ = Tensor(high, mstype.int32)
    size_ = Tensor(size, mstype.int32)
    output = rand_op(size_, low_, high_)
    return cast_(output, dtype)


@_function_forbid_reuse
def poisson(shape, mean, seed=None):
    r"""
    The ops.poisson is deprecated, please use :class:`mindspore.ops.random_poisson`
    Generates random numbers according to the Poisson random number distribution.

    .. math::

        \text{P}(i|μ) = \frac{\exp(-μ)μ^{i}}{i!}

    Args:
        shape (tuple): The shape of random tensor to be generated.
          The format is :math:`(N,*)` where :math:`*` means, any number of additional dimensions.
        mean (Tensor): The mean μ distribution parameter. It should be greater than 0 with float32 data type.
        seed (int): Seed is used as entropy source for the random number engines to generate pseudo-random numbers
          and must be non-negative. Default: ``None`` , which will be treated as 0.

    Returns:
        Tensor. The shape should be equal to the broadcasted shape between the input "shape" and shapes of `mean`.
        The dtype is float32.

    Raises:
        TypeError: If `shape` is not a tuple.
        TypeError: If `mean` is not a Tensor whose dtype is not float32.
        TypeError: If `seed` is not an int.

    Supported Platforms:
        deprecated

    Examples:
        >>> from mindspore import Tensor, ops
        >>> import mindspore
        >>> # case 1: It can be broadcast.
        >>> shape = (4, 1)
        >>> mean = Tensor(np.array([5.0, 10.0]), mindspore.float32)
        >>> output = ops.poisson(shape, mean, seed=5)
        >>> result = output.shape
        >>> print(result)
        (4, 2)
        >>> # case 2: It can not be broadcast. It is recommended to use the same shape.
        >>> shape = (2, 2)
        >>> mean = Tensor(np.array([[5.0, 10.0], [5.0, 1.0]]), mindspore.float32)
        >>> output = ops.poisson(shape, mean, seed=5)
        >>> result = output.shape
        >>> print(result)
        (2, 2)
    """
    seed1, seed2 = _get_seed(seed, "poisson")
    random_poisson_op = P.Poisson(seed1, seed2)
    random_poisson_op = _set_prim_op_user_data(random_poisson_op, "random_cache", False)
    value = random_poisson_op(shape, mean)
    return value


@_function_forbid_reuse
def multinomial(input, num_samples, replacement=True, seed=None):
    r"""
    Returns a tensor sampled from the multinomial probability distribution located in the corresponding
    row of the input tensor.

    The polynomial distribution is a probability distribution that generalizes the binomial distribution formula to
    multiple states. In the polynomial distribution, each event has a fixed probability, and the sum of these
    probabilities is 1. The purpose of the `mindspore.ops.multinomial` interface is to perform `num_samples` sampling
    on the input `input`, and the output tensor is the index of the input tensor for each sampling.
    The values in `input` represent the probability of selecting the corresponding index for each sampling.

    Here is an extreme example for better understanding. Suppose we have an input probability tensor with
    values `Tensor([90 / 100, 10 / 100, 0], mindspore.float32)`, which means we can sample three indices,
    namely index 0, index 1, and index 2, with probabilities of 90%, 10%, and 0%, respectively. We perform n samplings,
    and the resulting sequence is the calculation result of the polynomial distribution, with a length equal to the
    number of samplings.

    In case 1 of the sample code, we perform two non-replacement samplings (`replacement` is `False`).
    The calculation result is most likely `[0, 1]`, and less likely `[1, 0]`. Since the probability of selecting
    index 0 is 90% for each sampling, the first result is most likely to be index 0. Since the probability of selecting
    index 2 is 0, index 2 cannot appear in the sampling result. Therefore, the second result must be index 1,
    and the resulting sequence is `[0, 1]`.

    In case 2 of the sample code, we perform 10 replacement samplings (`replacement` is `True`).
    As expected, about 90% of the sampling results are index 0.

    In case 3 of the sample code, we extend the input to 2 dimensions, and the sampling results
    in each dimension also match our sampling expectations.

    Note:
        The rows of input do not need to sum to one (in which case we use the values as weights),
        but must be non-negative, finite and have a non-zero sum. When using values as weights, it can be understood as
        normalizing the input along the last dimension.

    Args:
        input (Tensor): The input tensor containing probabilities, must be 1 or 2 dimensions, with
          float32 data type.
        num_samples (int): Number of samples to draw.
        replacement (bool, optional): Whether to draw with replacement or not. Default: ``True`` .
        seed (int, optional): Seed is used as entropy source for the random number engines to generate
          pseudo-random numbers, must be non-negative. Default: ``None`` .

    Returns:
        Tensor, has the same rows with input. The number of sampled indices of each row is `num_samples`.
        The dtype is float32.

    Raises:
        TypeError: If `input` is not a Tensor whose dtype is not float32.
        TypeError: If `num_samples` is not an int.
        TypeError: If `seed` is neither an int nor None.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> # case 1: The output is random, and the length of the output is the same as num_sample.
        >>> # replacement is False.
        >>> input1 = Tensor([90 / 100, 10 / 100, 0], mindspore.float32)
        >>> input2 = Tensor([90, 10, 0], mindspore.float32)
        >>> # input1 and input2 have the same meaning.
        >>> output1 = ops.multinomial(input1, 2, replacement=False)
        >>> output2 = ops.multinomial(input2, 2, replacement=False)
        >>> # print(output1)
        >>> # [0 1]
        >>> # print(output2)
        >>> # [0 1]
        >>> print(len(output1))
        2
        >>> print(len(output2))
        2
        >>> # case 2: The output is random, and the length of the output is the same as num_sample.
        >>> # replacement is True.
        >>> output3 = ops.multinomial(input1, 10)
        >>> # print(output3)
        >>> # [0 0 1 0 0 0 0 0 0 0]
        >>> print(len(output3))
        10
        >>> # case 3: The output is random, and the length of the output is the same as num_sample.
        >>> # replacement is True.
        >>> # rank is 2
        >>> input4 = Tensor([[90, 10, 0], [10, 90, 0]], mstype.float32)
        >>> output4 = ops.multinomial(input4, 10)
        >>> # print(output4)
        >>> # [[0 0 0 0 0 0 0 0 1 0]
        >>> #  [1 1 1 1 1 0 1 1 1 1]]
    """
    def _check_valid_dim(dim, name):
        if dim not in (1, 2):
            raise ValueError(f"For '{name}', the dimension of inputs must be 1d or 2d, but got {dim}.")

    _check_valid_dim(len(shape_(input)), "multinomial")
    seed1, seed2 = _get_seed(seed, "multinomial")
    if not replacement:
        if shape_(input)[-1] < num_samples:
            const_utils.raise_value_error(f"For 'multinomial', the 'num_samples' must be less than "
                                          f"the last dimension of input without 'replacement', "
                                          f"but got 'num_samples': {num_samples} and "
                                          f"'replacement': {replacement}")
        n_dist = 1
        if len(shape_(input)) > 1:
            n_dist = shape_(input)[-2]
        random_uniform_real = P.UniformReal(seed1, seed2)
        random_cache_op = _set_prim_op_user_data(random_uniform_real, "random_cache", False)
        random_uniform = random_cache_op((n_dist * shape_(input)[-1],))
        if n_dist != 1:
            random_uniform = reshape_(random_uniform, (n_dist, shape_(input)[-1]))


        vals = real_div_(log_(random_uniform), input + 1e-6)
        _, indices = top_k_(vals, num_samples)
        return indices
    random_nomial = P.Multinomial(seed1, seed2)
    random_nomial = _set_prim_op_user_data(random_nomial, "random_cache", False)
    return random_nomial(input, num_samples)


def _check_shape(input_shape):
    """Check 'shape' value."""
    if not isinstance(input_shape, tuple):
        const_utils.raise_type_error(f"Type of 'shape' must be tuple, but got: {type(input_shape)}")
    for item in input_shape:
        if not isinstance(item, int):
            const_utils.raise_type_error(f"Elements of 'shape' must be int, but got: {type(item)}")
        if item < 1:
            const_utils.raise_value_error(f"Elements of 'shape' must be positive int, but got: {item}")
    return True


def _check_param(op_name, param_name, param_value):
    """Check type of param_value is Tensor, int, or float."""
    if not isinstance(param_value, (Tensor, int, float, np.ndarray)):
        const_utils.raise_type_error("For '{}', the type of '{}' must be Tensor, int, or float, "
                                     "but got: {}".format(op_name, param_name, type(param_value)))
    return True


__all__ = [
    'standard_laplace', 'random_categorical', 'uniform', 'standard_normal', 'random_gamma',
    'uniform_candidate_sampler', 'random_poisson', 'log_uniform_candidate_sampler', 'shuffle', 'choice_with_mask',
    'normal', 'laplace', 'gamma', 'poisson', 'multinomial', 'rand', 'rand_like', 'randn', 'randn_like', 'randint',
    'randint_like', 'multinomial_with_replacement', 'randperm'
]
__all__.sort()
