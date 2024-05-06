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
"""math"""
from __future__ import absolute_import

import numpy as np

from mindspore import log as logger
from mindspore.ops import operations as P
from mindspore.common.tensor import Tensor
from mindspore.common._decorator import deprecated
from mindspore.ops.primitive import constexpr, _primexpr
from mindspore.ops import functional as F
from mindspore.nn.cell import Cell
from mindspore.common import dtype as mstype
from mindspore import _checkparam as validator

__all__ = ['ReduceLogSumExp',
           'Range',
           'LGamma',
           'DiGamma',
           'IGamma',
           'LBeta',
           'CosineSimilarity',
           'MatMul',
           'Moments',
           'MatInverse',
           'MatDet',
           ]

_BASE_LANCZOS_COEFF = 0.99999999999980993227684700473478
_LANCZOS_COEFFICIENTS = [676.520368121885098567009190444019,
                         -1259.13921672240287047156078755283,
                         771.3234287776530788486528258894,
                         -176.61502916214059906584551354,
                         12.507343278686904814458936853,
                         -0.13857109526572011689554707,
                         9.984369578019570859563e-6,
                         1.50563273514931155834e-7]


@constexpr
def _check_input_dtype(param_name, input_dtype, allow_dtypes, cls_name):
    validator.check_type_name(param_name, input_dtype, allow_dtypes, cls_name)


class ReduceLogSumExp(Cell):
    r"""
    Reduces a dimension of a tensor by calculating exponential for all elements in the dimension,
    then calculate logarithm of the sum.

    .. math::

        ReduceLogSumExp(x) = \log(\sum(e^x))

    Args:
        axis (Union[int, tuple(int), list(int)]) - The dimensions to reduce. Default: ``()`` , reduce all dimensions.
            Only constant value is allowed.
        keep_dims (bool): If True, keep these reduced dimensions and the length is 1.
            If False, don't keep these dimensions.
            Default : False.

    Inputs:
        - **x** (Tensor) - The input tensor. With float16 or float32 data type.

    Outputs:
        Tensor, has the same dtype as the `x`.

        - If axis is (), and keep_dims is False,
          the output is a 0-D tensor representing the sum of all elements in the input tensor.
        - If axis is int, set as 2, and keep_dims is False,
          the shape of output is :math:`(x_1, x_3, ..., x_R)`.
        - If axis is tuple(int), set as (2, 3), and keep_dims is False,
          the shape of output is :math:`(x_1, x_4, ..., x_R)`.

    Raises:
        TypeError: If `axis` is not one of int, list, tuple.
        TypeError: If `keep_dims` is not bool.
        TypeError: If dtype of `x` is neither float16 nor float32.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore as ms
        >>> import numpy as np
        >>> x = ms.Tensor(np.random.randn(3, 4, 5, 6).astype(np.float32))
        >>> op = ms.nn.ReduceLogSumExp(1, keep_dims=True)
        >>> output = op(x)
        >>> print(output.shape)
        (3, 1, 5, 6)
    """

    def __init__(self, axis, keep_dims=False):
        """Initialize ReduceLogSumExp."""
        super(ReduceLogSumExp, self).__init__()
        validator.check_value_type('axis', axis, [int, list, tuple], self.cls_name)
        validator.check_value_type('keep_dims', keep_dims, [bool], self.cls_name)
        self.axis = axis
        self.exp = P.Exp()
        self.sum = P.ReduceSum(keep_dims)
        self.log = P.Log()
        self.max = P.ReduceMax()

    def construct(self, x):
        x_max = self.max(x)
        exp = self.exp(x - x_max)
        sumexp = self.sum(exp, self.axis)
        logsumexp = self.log(sumexp)
        return logsumexp + x_max


class Range(Cell):
    r"""
    The Range class will be deprecated in the future,
    this function can be replaced by :func:`ops.range`
    """

    def __init__(self, start, limit=None, delta=1):
        """Initialize Range."""
        super(Range, self).__init__()
        logger.warning("The Range class will be deprecated in the future,"
                       "this function can be replaced by :func:`ops.range`")
        if delta == 0:
            raise ValueError(f"For '{self.cls_name}', the 'delta' can not be zero.")
        data = np.arange(start, limit, delta)
        if data.dtype == np.float:
            self.ms_dtype = mstype.float32
        else:
            self.ms_dtype = mstype.int32
        self.result_tensor = Tensor(data, dtype=self.ms_dtype)

    def construct(self):
        return self.result_tensor


class LGamma(Cell):
    r"""
    Calculates LGamma using Lanczos' approximation referring to "A Precision Approximation of the Gamma Function".
    The algorithm is:

    .. math::
        \begin{array}{ll} \\
            lgamma(z + 1) = \frac{(\log(2) + \log(pi))}{2} + (z + 1/2) * log(t(z)) - t(z) + A(z) \\
            t(z) = z + kLanczosGamma + 1/2 \\
            A(z) = kBaseLanczosCoeff + \sum_{k=1}^n \frac{kLanczosCoefficients[i]}{z + k}
        \end{array}

    However, if the input is less than 0.5 use Euler's reflection formula:

    .. math::

        lgamma(x) = \log(pi) - lgamma(1-x) - \log(abs(sin(pi * x)))

    And please note that

    .. math::

        lgamma(+/-inf) = +inf

    Thus, the behaviour of LGamma follows:

    - when x > 0.5, return log(Gamma(x))
    - when x < 0.5 and is not an integer, return the real part of Log(Gamma(x)) where Log is the complex logarithm
    - when x is an integer less or equal to 0, return +inf
    - when x = +/- inf, return +inf

    Inputs:
        - **x** (Tensor) - The input tensor. Only float16, float32 are supported.

    Outputs:
        Tensor, has the same shape and dtype as the `x`.

    Raises:
        TypeError: If dtype of `x` is neither float16 nor float32.

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
        >>> import mindspore as ms
        >>> import numpy as np
        >>> x = ms.Tensor(np.array([2, 3, 4]).astype(np.float32))
        >>> op = ms.nn.LGamma()
        >>> output = op(x)
        >>> print(output)
        [3.5762787e-07 6.9314754e-01 1.7917603e+00]
    """

    def __init__(self):
        """Initialize LGamma."""
        super(LGamma, self).__init__()
        # const numbers
        self.k_lanczos_gamma = 7
        self.k_base_lanczos_coeff = _BASE_LANCZOS_COEFF
        self.k_lanczos_coefficients = _LANCZOS_COEFFICIENTS
        self.one_half = 0.5
        self.one = 1
        self.two = 2
        self.inf = np.inf
        self.pi = np.pi
        self.log_2 = np.log(self.two)
        self.log_pi = np.log(np.pi)
        self.log_sqrt_two_pi = (self.log_2 + self.log_pi) / self.two
        self.lanczos_gamma_plus_one_half = self.k_lanczos_gamma + 0.5
        self.log_lanczos_gamma_plus_one_half = np.log(self.lanczos_gamma_plus_one_half)

        # operations
        self.log = P.Log()
        self.log1p = P.Log1p()
        self.abs = P.Abs()
        self.shape = P.Shape()
        self.dtype = P.DType()
        self.floor = P.Floor()
        self.equal = P.Equal()
        self.greater = P.Greater()
        self.less = P.Less()
        self.lessequal = P.LessEqual()
        self.select = P.Select()
        self.sin = P.Sin()
        self.isfinite = P.IsFinite()
        self.ones_like = P.OnesLike()

    def construct(self, x):
        input_dtype = self.dtype(x)
        _check_input_dtype("x", input_dtype, [mstype.float16, mstype.float32], self.cls_name)
        if F.is_sequence_value_unknown(self.shape(x)):
            infinity = self.ones_like(x) * F.cast(self.inf, input_dtype)
        else:
            infinity = F.fill(input_dtype, self.shape(x), self.inf)

        need_to_reflect = self.less(x, 0.5)
        neg_input = -x
        z = self.select(need_to_reflect, neg_input, x - 1)

        @constexpr
        def _calculate_reflected_x(z, k_base_lanczos_coeff, k_lanczos_coefficients):
            reflex_x = k_base_lanczos_coeff
            for i in range(8):
                product_ = k_lanczos_coefficients[i] / (z + i + 1)
                reflex_x = product_ + reflex_x
            return reflex_x

        reflex_x = _calculate_reflected_x(z, self.k_base_lanczos_coeff, self.k_lanczos_coefficients)

        t = z + self.lanczos_gamma_plus_one_half
        log_t = self.log1p(z / self.lanczos_gamma_plus_one_half) + self.log_lanczos_gamma_plus_one_half

        log_y = self.log(reflex_x) + (z + self.one_half - t / log_t) * log_t + self.log_sqrt_two_pi

        abs_input = self.abs(x)
        abs_frac_input = abs_input - self.floor(abs_input)
        x = self.select(self.lessequal(x, 0.0), self.select(self.equal(abs_frac_input, 0.0), infinity, x), x)
        reduced_frac_input = self.select(self.greater(abs_frac_input, 0.5),
                                         1 - abs_frac_input, abs_frac_input)
        reflection_denom = self.log(self.sin(self.pi * reduced_frac_input))

        reflection = self.select(self.isfinite(reflection_denom),
                                 -reflection_denom - log_y + self.log_pi,  # pylint: disable=invalid-unary-operand-type
                                 -reflection_denom)  # pylint: disable=invalid-unary-operand-type

        result = self.select(need_to_reflect, reflection, log_y)

        return self.select(self.isfinite(x), result, infinity)


class DiGamma(Cell):
    r"""
    Calculates Digamma using Lanczos' approximation referring to "A Precision Approximation of the Gamma Function".
    The algorithm is:

    .. math::
        \begin{array}{ll} \\
            digamma(z + 1) = log(t(z)) + A'(z) / A(z) - kLanczosGamma / t(z) \\
            t(z) = z + kLanczosGamma + 1/2 \\
            A(z) = kBaseLanczosCoeff + \sum_{k=1}^n \frac{kLanczosCoefficients[i]}{z + k} \\
            A'(z) = \sum_{k=1}^n \frac{kLanczosCoefficients[i]}{{z + k}^2}
        \end{array}

    However, if the input is less than 0.5 use Euler's reflection formula:

    .. math::

        digamma(x) = digamma(1 - x) - pi * cot(pi * x)

    Inputs:
        - **x** (Tensor[Number]) - The input tensor. Only float16, float32 are supported.

    Outputs:
        Tensor, has the same shape and dtype as the `x`.

    Raises:
        TypeError: If dtype of `x` is neither float16 nor float32.

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
        >>> import mindspore as ms
        >>> import numpy as np
        >>> x = ms.Tensor(np.array([2, 3, 4]).astype(np.float32))
        >>> op = ms.nn.DiGamma()
        >>> output = op(x)
        >>> print(output)
        [0.42278463  0.92278427 1.2561178]
    """

    def __init__(self):
        """Initialize DiGamma."""
        super(DiGamma, self).__init__()
        # const numbers
        self.k_lanczos_gamma = 7
        self.k_base_lanczos_coeff = _BASE_LANCZOS_COEFF
        self.k_lanczos_coefficients = _LANCZOS_COEFFICIENTS
        self.nan = np.nan
        self.pi = np.pi
        self.lanczos_gamma_plus_one_half = self.k_lanczos_gamma + 0.5
        self.log_lanczos_gamma_plus_one_half = np.log(self.lanczos_gamma_plus_one_half)

        # operations
        self.log1p = P.Log1p()
        self.abs = P.Abs()
        self.shape = P.Shape()
        self.dtype = P.DType()
        self.floor = P.Floor()
        self.equal = P.Equal()
        self.less = P.Less()
        self.select = P.Select()
        self.sin = P.Sin()
        self.cos = P.Cos()
        self.logicaland = P.LogicalAnd()

    def construct(self, x):
        input_dtype = self.dtype(x)
        _check_input_dtype("x", input_dtype, [mstype.float16, mstype.float32], self.cls_name)
        need_to_reflect = self.less(x, 0.5)
        neg_input = -x
        z = self.select(need_to_reflect, neg_input, x - 1)

        @constexpr
        def _calculate_num_denom(z, k_base_lanczos_coeff, k_lanczos_coefficients):
            num = 0
            denom = k_base_lanczos_coeff
            for i in range(8):
                j = z + i + 1
                num = num - k_lanczos_coefficients[i] / (j * j)
                denom = denom + k_lanczos_coefficients[i] / j
            return num, denom

        num, denom = _calculate_num_denom(z, self.k_base_lanczos_coeff, self.k_lanczos_coefficients)

        t = z + self.lanczos_gamma_plus_one_half
        log_t = self.log1p(z / self.lanczos_gamma_plus_one_half) + self.log_lanczos_gamma_plus_one_half

        y = log_t + num / denom - self.k_lanczos_gamma / t

        reduced_input = x + self.abs(self.floor(x + 0.5))
        reflection = y - self.pi * self.cos(self.pi * reduced_input) / self.sin(self.pi * reduced_input)
        real_result = self.select(need_to_reflect, reflection, y)
        nan = F.fill(self.dtype(x), self.shape(x), np.nan)

        return self.select(self.logicaland(self.less(x, 0), self.equal(x, self.floor(x))),
                           nan, real_result)


def _while_helper_func(cond, body, vals):
    while cond(vals).any():
        vals = body(vals)
    return vals


def _igamma_series(ax, x, a, enabled):
    """Helper function for computing Igamma using a power series."""

    logicaland = P.LogicalAnd()
    greater = P.Greater()
    shape = P.Shape()
    dtype = P.DType()
    select = P.Select()

    # If more data types are supported, this epsilon need to be selected.
    epsilon = Tensor(np.finfo(np.float32).eps, mstype.float32)

    def cond(vals):
        enabled = vals[0]
        return enabled

    def body(vals):
        enabled = vals[0]
        r = vals[1]
        c = vals[2]
        ans = vals[3]
        x = vals[4]
        dc_da = vals[5]
        dans_da = vals[6]

        r = r + 1
        dc_da = dc_da * (x / r) + (-1 * c * x) / (r * r)
        dans_da = dans_da + dc_da
        c = c * (x / r)
        ans = ans + c
        conditional = logicaland(enabled, greater(c / ans, epsilon))

        return (conditional, select(enabled, r, vals[1]),
                select(enabled, c, vals[2]), select(enabled, ans, vals[3]),
                select(enabled, x, vals[4]), select(enabled, dc_da, vals[5]),
                select(enabled, dans_da, vals[6]))

    ones = F.fill(dtype(a), shape(a), 1)
    zeros = F.fill(dtype(a), shape(a), 0)
    vals = (enabled, a, ones, ones, x, zeros, zeros)

    vals = _while_helper_func(cond, body, vals)
    ans = vals[3]
    return (ans * ax) / a


def _igammac_continued_fraction(ax, x, a, enabled):
    """Helper function for computing Igammac using a continued fraction."""

    abs_x = P.Abs()
    logicaland = P.LogicalAnd()
    greater = P.Greater()
    less = P.Less()
    notequal = P.NotEqual()
    shape = P.Shape()
    dtype = P.DType()
    select = P.Select()

    # If more data types are supported, this epsilon need to be selected.
    epsilon = Tensor(np.finfo(np.float32).eps, mstype.float32)

    def cond(vals):
        enabled = vals[0]
        c = vals[5]
        return logicaland(less(c, 2000), enabled)

    def body(vals):
        enabled = vals[0]
        ans = vals[1]
        t = vals[2]
        y = vals[3]
        z = vals[4]
        c = vals[5]
        pkm1 = vals[6]
        qkm1 = vals[7]
        pkm2 = vals[8]
        qkm2 = vals[9]

        dpkm2_da = vals[10]
        dqkm2_da = vals[11]
        dpkm1_da = vals[12]
        dqkm1_da = vals[13]
        dans_da = vals[14]

        c = c + 1
        y = y + 1
        z = z + 2

        yc = y * c
        pk = pkm1 * z - pkm2 * yc
        qk = qkm1 * z - qkm2 * yc
        qk_is_nonzero = notequal(qk, 0)
        r = pk / qk

        t = select(qk_is_nonzero, abs_x((ans - r) / r), F.fill(dtype(t), shape(t), 1))
        ans = select(qk_is_nonzero, r, ans)

        dpk_da = dpkm1_da * z - pkm1 - dpkm2_da * yc + pkm2 * c
        dqk_da = dqkm1_da * z - qkm1 - dqkm2_da * yc + qkm2 * c
        dans_da_new = select(qk_is_nonzero, (dpk_da - ans * dqk_da) / qk, dans_da)
        grad_conditional = select(qk_is_nonzero,
                                  abs_x(dans_da_new - dans_da),
                                  F.fill(dtype(dans_da), shape(dans_da), 1))

        pkm2 = pkm1
        pkm1 = pk
        qkm2 = qkm1
        qkm1 = qk

        dpkm2_da = dpkm1_da
        dqkm2_da = dqkm1_da
        dpkm1_da = dpk_da
        dqkm1_da = dqk_da

        rescale = greater(abs_x(pk), 1 / epsilon)
        pkm2 = select(rescale, pkm2 * epsilon, pkm2)
        pkm1 = select(rescale, pkm1 * epsilon, pkm1)
        qkm2 = select(rescale, qkm2 * epsilon, qkm2)
        qkm1 = select(rescale, qkm1 * epsilon, qkm1)

        dpkm2_da = select(rescale, dpkm2_da * epsilon, dpkm2_da)
        dqkm2_da = select(rescale, dqkm2_da * epsilon, dqkm2_da)
        dpkm1_da = select(rescale, dpkm1_da * epsilon, dpkm1_da)
        dqkm1_da = select(rescale, dqkm1_da * epsilon, dqkm1_da)

        conditional = logicaland(enabled, greater(grad_conditional, epsilon))

        return (conditional, select(enabled, ans, vals[1]), select(enabled, t, vals[2]),
                select(enabled, y, vals[3]), select(enabled, z, vals[4]),
                c, select(enabled, pkm1, vals[6]),
                select(enabled, qkm1, vals[7]), select(enabled, pkm2, vals[8]),
                select(enabled, qkm2, vals[9]), select(enabled, dpkm2_da, vals[10]),
                select(enabled, dqkm2_da, vals[11]), select(enabled, dpkm1_da, vals[12]),
                select(enabled, dqkm1_da, vals[13]), select(enabled, dans_da_new, vals[14]))

    y = 1 - a
    z = x + y + 1
    c = F.fill(dtype(x), shape(x), 0)
    pkm2 = F.fill(dtype(x), shape(x), 1)
    qkm2 = x
    pkm1 = x + 1
    qkm1 = z * x
    ans = pkm1 / qkm1
    t = F.fill(dtype(x), shape(x), 1)
    dpkm2_da = F.fill(dtype(x), shape(x), 0)
    dqkm2_da = F.fill(dtype(x), shape(x), 0)
    dpkm1_da = F.fill(dtype(x), shape(x), 0)
    dqkm1_da = -x
    dans_da = (dpkm1_da - ans * dqkm1_da) / qkm1
    vals = (enabled, ans, t, y, z, c, pkm1, qkm1, pkm2, qkm2, dpkm2_da, dqkm2_da, dpkm1_da, dqkm1_da, dans_da)
    vals = _while_helper_func(cond, body, vals)
    ans = vals[1]
    return ans * ax


class IGamma(Cell):
    r"""
    Calculates lower regularized incomplete Gamma function.
    The lower regularized incomplete Gamma function is defined as:

    .. math::
        P(a, x) = gamma(a, x) / Gamma(a) = 1 - Q(a, x)

    where

    .. math::
        gamma(a, x) = \int_0^x t^{a-1} \exp^{-t} dt

    is the lower incomplete Gamma function.

    Above :math:`Q(a, x)` is the upper regularized complete Gamma function.

    Inputs:
        - **a** (Tensor) - The input tensor. With float32 data type. `a` should have
          the same dtype with `x`.
        - **x** (Tensor) - The input tensor. With float32 data type. `x` should have
          the same dtype with `a`.

    Outputs:
        Tensor, has the same dtype as `a` and `x`.

    Raises:
        TypeError: If dtype of input x and a is not float16 nor float32,
                   or if x has different dtype with a.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore as ms
        >>> import numpy as np
        >>> a = ms.Tensor(np.array([2.0, 4.0, 6.0, 8.0]).astype(np.float32))
        >>> x = ms.Tensor(np.array([2.0, 3.0, 4.0, 5.0]).astype(np.float32))
        >>> igamma = ms.nn.IGamma()
        >>> output = igamma(a, x)
        >>> print (output)
        [0.593994  0.35276785  0.21486944  0.13337152]
    """

    def __init__(self):
        """Initialize IGamma."""
        super(IGamma, self).__init__()
        # const numbers
        # If more data types are supported, this float max value need to be selected.
        self.log_maxfloat32 = Tensor(np.log(np.finfo(np.float32).max), mstype.float32)

        # operations
        self.logicaland = P.LogicalAnd()
        self.logicalor = P.LogicalOr()
        self.logicalnot = P.LogicalNot()
        self.equal = P.Equal()
        self.greater = P.Greater()
        self.less = P.Less()
        self.neg = P.Neg()
        self.log = P.Log()
        self.exp = P.Exp()
        self.select = P.Select()
        self.zeroslike = P.ZerosLike()
        self.shape = P.Shape()
        self.dtype = P.DType()
        self.lgamma = LGamma()
        self.cast = P.Cast()

    def construct(self, a, x):
        a_dtype = self.dtype(a)
        x_dtype = self.dtype(x)
        _check_input_dtype("a", a_dtype, [mstype.float32], self.cls_name)
        _check_input_dtype("x", x_dtype, a_dtype, self.cls_name)
        domain_error = self.logicalor(self.less(x, 0), self.less(a, 0))
        use_igammac = self.logicaland(self.greater(x, 1), self.greater(x, a))
        ax = a * self.log(x) - x - self.lgamma(a)
        para_shape = self.shape(ax)
        if para_shape != ():
            x = F.broadcast_to(x, para_shape)
            a = F.broadcast_to(a, para_shape)
        x_is_zero = self.equal(x, 0)
        underflow = self.less(ax, self.neg(self.log_maxfloat32))
        ax = self.exp(ax)
        enabled = self.logicalnot(self.logicalor(self.logicalor(x_is_zero, domain_error), underflow))
        output = self.select(use_igammac,
                             1 - _igammac_continued_fraction(ax, x, a, self.logicaland(enabled, use_igammac)),
                             _igamma_series(ax, x, a, self.logicaland(enabled, self.logicalnot(use_igammac))))
        output = self.select(x_is_zero, self.zeroslike(output), output)
        output = self.select(domain_error, F.fill(self.dtype(a), self.shape(a), np.nan), output)
        return output


class LBeta(Cell):
    r"""
    This method avoids the numeric cancellation by explicitly
    decomposing lgamma into the Stirling approximation and an explicit log_gamma_correction, and cancelling
    the large terms from the Striling analytically.

    This is semantically equal to

    .. math::
        P(x, y) = lgamma(x) + lgamma(y) - lgamma(x + y).

    The method is more accurate for arguments above 8. The reason for accuracy loss in the naive computation
    is catastrophic cancellation between the lgammas.

    Inputs:
        - **x** (Tensor) - The input tensor. With float16 or float32 data type. `x` should have
          the same dtype with `y`.
        - **y** (Tensor) - The input tensor. With float16 or float32 data type. `y` should have
          the same dtype with `x`.

    Outputs:
        Tensor, has the same dtype as `x` and `y`.

    Raises:
        TypeError: If dtype of `x` or `y` is neither float16 nor float32,
                   or if `x` has different dtype with `y`.

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
        >>> import mindspore as ms
        >>> import numpy as np
        >>> x = ms.Tensor(np.array([2.0, 4.0, 6.0, 8.0]).astype(np.float32))
        >>> y = ms.Tensor(np.array([2.0, 3.0, 14.0, 15.0]).astype(np.float32))
        >>> lbeta = ms.nn.LBeta()
        >>> output = lbeta(y, x)
        >>> print(output)
        [-1.7917596  -4.094345  -12.000229  -14.754799]
    """

    def __init__(self):
        """Initialize LBeta."""
        super(LBeta, self).__init__()
        # const numbers
        self.log_2pi = np.log(2 * np.pi)
        self.minimax_coeff = [-0.165322962780713e-02,
                              0.837308034031215e-03,
                              -0.595202931351870e-03,
                              0.793650666825390e-03,
                              -0.277777777760991e-02,
                              0.833333333333333e-01]

        # operations
        self.log = P.Log()
        self.log1p = P.Log1p()
        self.less = P.Less()
        self.select = P.Select()
        self.shape = P.Shape()
        self.dtype = P.DType()
        self.lgamma = LGamma()
        self.const = P.ScalarToTensor()

    def construct(self, x, y):
        x_dtype = self.dtype(x)
        y_dtype = self.dtype(y)
        _check_input_dtype("x", x_dtype, [mstype.float16, mstype.float32], self.cls_name)
        _check_input_dtype("y", y_dtype, x_dtype, self.cls_name)
        x_plus_y = x + y
        para_shape = self.shape(x_plus_y)
        if para_shape != ():
            x = F.broadcast_to(x, para_shape)
            y = F.broadcast_to(y, para_shape)
        comp_less = self.less(x, y)
        x_min = self.select(comp_less, x, y)
        y_max = self.select(comp_less, y, x)

        @constexpr
        def _log_gamma_correction(x, minimax_coeff):
            inverse_x = 1. / x
            inverse_x_squared = inverse_x * inverse_x
            accum = minimax_coeff[0]
            for i in range(1, 6):
                accum = accum * inverse_x_squared + minimax_coeff[i]
            return accum * inverse_x

        log_gamma_correction_x = _log_gamma_correction(x_min, self.minimax_coeff)
        log_gamma_correction_y = _log_gamma_correction(y_max, self.minimax_coeff)
        log_gamma_correction_x_y = _log_gamma_correction(x_plus_y, self.minimax_coeff)

        # Two large arguments case: y >= x >= 8.
        log_beta_two_large = self.const(0.5 * self.log_2pi, x_dtype) - 0.5 * self.log(y_max) \
                             + log_gamma_correction_x + log_gamma_correction_y - log_gamma_correction_x_y \
                             + (x_min - 0.5) * self.log(x_min / (x_min + y_max)) - y_max * self.log1p(x_min / y_max)

        cancelled_stirling = -1 * (x_min + y_max - 0.5) * self.log1p(x_min / y_max) - x_min * self.log(y_max) + x_min
        correction = log_gamma_correction_y - log_gamma_correction_x_y
        log_gamma_difference_big_y = correction + cancelled_stirling

        # One large argument case: x < 8, y >= 8.
        log_beta_one_large = self.lgamma(x_min) + log_gamma_difference_big_y

        # Small arguments case: x <= y < 8.
        log_beta_small = self.lgamma(x_min) + self.lgamma(y_max) - self.lgamma(x_min + y_max)
        comp_xless8 = self.less(x_min, 8)
        comp_yless8 = self.less(y_max, 8)
        temp = self.select(comp_yless8, log_beta_small, log_beta_one_large)
        return self.select(comp_xless8, temp, log_beta_two_large)


@_primexpr
def get_broadcast_matmul_shape(x_shape, y_shape, prim_name=None):
    """get broadcast_matmul shape"""
    msg_prefix = f"For '{prim_name}', the" if prim_name else "The"

    def _check_len():
        if (len(x_shape) < 2) or (len(y_shape) < 2):
            raise ValueError(f"{msg_prefix} length of 'x_shape' and 'y_shape' must be equal to or greater than 2, "
                             f"but got the length of 'x_shape': {len(x_shape)} and the length of 'y_shape': "
                             f"{len(y_shape)}.")
    _check_len()
    x_shape_batch = x_shape[:-2]
    y_shape_batch = y_shape[:-2]
    if x_shape_batch == y_shape_batch:
        return x_shape, y_shape
    x_len = len(x_shape)
    y_len = len(y_shape)
    length = x_len if x_len < y_len else y_len
    broadcast_shape_back = []

    def _check_broadcast(x_val, y_val, i):
        if not (x_val == 1 or y_val == 1 or x_val == y_val):
            raise ValueError(f"{msg_prefix} 'x_shape[{i}]' must be equal to 1, or the 'y_shape[{i}]' must be equal "
                             f"to 1, or the 'x_shape[{i}]' must be equal to 'y_shape[{i}]', but got "
                             f"'x_shape[{i}]': {x_val}, 'y_shape[{i}]': {y_val}.")

    for i in range(-length, -2):
        _check_broadcast(x_shape[i], y_shape[i], i)
        if x_shape[i] == 1:
            broadcast_shape_back.append(y_shape[i])
        elif y_shape[i] == 1:
            broadcast_shape_back.append(x_shape[i])
        else:
            broadcast_shape_back.append(x_shape[i])

    broadcast_shape_front = y_shape[0: y_len - length] if length == x_len else x_shape[0: x_len - length]
    x_broadcast_shape = broadcast_shape_front + tuple(broadcast_shape_back) + x_shape[-2:]
    y_broadcast_shape = broadcast_shape_front + tuple(broadcast_shape_back) + y_shape[-2:]
    return x_broadcast_shape, y_broadcast_shape


@_primexpr
def check_col_row_equal(x1_shape, x2_shape, transpose_x1, transpose_x2, prim_name=None):
    """check col and row equal"""
    def _check(x1_col, x2_row):
        msg_prefix = f"For '{prim_name}', the" if prim_name else "The"
        if x1_col != x2_row:
            raise ValueError(f"{msg_prefix} column of matrix dimensions of 'x1' must be equal to "
                             f"the row of matrix dimensions of 'x2', but got 'x1_col' {x1_col} and 'x2_row' {x2_row}.")

    if len(x1_shape) == 1:
        transpose_x1 = False
        x1_shape = (1,) + x1_shape
    if len(x2_shape) == 1:
        transpose_x2 = False
        x2_shape = x2_shape + (1,)
    x1_last = x1_shape[-2:]
    x2_last = x2_shape[-2:]
    x1_col = x1_last[not transpose_x1]  # x1_col = x1_last[1] if (not transpose_a) else x1_last[0]
    x2_row = x2_last[transpose_x2]  # x2_row = x2_last[0] if (not transpose_b) else x2_last[1]
    _check(x1_col, x2_row)


def matmul_op_select(x1_shape, x2_shape, transpose_x1, transpose_x2):
    """select matmul op"""
    x1_dim, x2_dim = len(x1_shape), len(x2_shape)
    if x1_dim == 1 and x2_dim == 1:
        matmul_op = P.Mul()
    elif x1_dim <= 2 and x2_dim <= 2:
        transpose_x1 = False if x1_dim == 1 else transpose_x1
        transpose_x2 = False if x2_dim == 1 else transpose_x2
        matmul_op = P.MatMul(transpose_x1, transpose_x2)
    elif x1_dim == 1 and x2_dim > 2:
        matmul_op = P.BatchMatMul(False, transpose_x2)
    elif x1_dim > 2 and x2_dim == 1:
        matmul_op = P.BatchMatMul(transpose_x1, False)
    else:
        matmul_op = P.BatchMatMul(transpose_x1, transpose_x2)
    return matmul_op


class MatMul(Cell):
    r"""
    The nn.MatMul interface is deprecated, please use the :class:`mindspore.ops.matmul` instead.

    Supported Platforms:
        deprecated
    """

    @deprecated('1.2', 'ops.matmul', False)
    def __init__(self, transpose_x1=False, transpose_x2=False):
        """Initialize MatMul."""
        super(MatMul, self).__init__()

        validator.check_value_type('transpose_x1', transpose_x1, [bool], self.cls_name)
        validator.check_value_type('transpose_x2', transpose_x2, [bool], self.cls_name)
        self.transpose_x1 = transpose_x1
        self.transpose_x2 = transpose_x2
        self.shape_op = P.Shape()
        self.expand_op = P.ExpandDims()
        self.squeeze_left_op = P.Squeeze(-2)
        self.squeeze_right_op = P.Squeeze(-1)
        self.reduce_sum_op = P.ReduceSum(keep_dims=False)

    def construct(self, x1, x2):
        x1_shape = self.shape_op(x1)
        x2_shape = self.shape_op(x2)
        check_col_row_equal(x1_shape, x2_shape, self.transpose_x1, self.transpose_x2, self.cls_name)
        matmul_op = matmul_op_select(x1_shape, x2_shape, self.transpose_x1, self.transpose_x2)

        x1_dim, x2_dim = len(x1_shape), len(x2_shape)
        if x1_dim == x2_dim and x2_dim == 1:
            return self.reduce_sum_op(matmul_op(x1, x2), -1)
        if x1_dim == 1:
            x1 = self.expand_op(x1, 0)
            x1_shape = self.shape_op(x1)
        if x2_dim == 1:
            x2 = self.expand_op(x2, 1)
            x2_shape = self.shape_op(x2)

        x1_broadcast_shape, x2_broadcast_shape = get_broadcast_matmul_shape(x1_shape, x2_shape)
        if x1_broadcast_shape != x1_shape:
            x1 = F.broadcast_to(x1, x1_broadcast_shape)
        if x2_broadcast_shape != x2_shape:
            x2 = F.broadcast_to(x2, x2_broadcast_shape)

        matmul_broadcast = matmul_op(x1, x2)

        if x1_dim == 1:
            matmul_broadcast = self.squeeze_left_op(matmul_broadcast)
        if x2_dim == 1:
            matmul_broadcast = self.squeeze_right_op(matmul_broadcast)

        return matmul_broadcast


class CosineSimilarity(Cell):
    r"""
    Computes cosine similarity.

    .. math::
        \mathcal{K} = \frac{\textbf{x}\textbf{y}^{\top}}{\parallel \textbf{x} \parallel \parallel \textbf{y} \parallel},

    where :math:`\mathcal{K}` is the similarity, :math:`\textbf{x}` is the first tensor `x1`,
    :math:`\textbf{y}` is the second tensor `x2`.

    To avoid numerical errors when dividing by small numbers,
    the lower bound of :math:`\parallel \textbf{x} \parallel \parallel \textbf{y} \parallel` is set to `eps`.

    Args:
        dim (int, optional): Dimension. Default: ``1`` .
        eps (float, optional): Small value. Default: ``1e-08`` .

    Inputs:
        - **x1** (Tensor) - The first tensor :math:`\textbf{x}`.
          Shape: :math:`(\ast_1, D, \ast_2)` where :math:`D` is at position `dim`.
        - **x2** (Tensor) - The second tensor :math:`\textbf{y}`. The shape is the same as `x1`.

    Outputs:
        Tensor, with shape :math:`(\ast_1, \ast_2)`, the data type will be inferred automatically.

    Raises:
        TypeError: If `x1` or `x2` is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore as ms
        >>> import numpy as np
        >>> x1 = ms.Tensor([[1.0, 3.0, 4.0, 7.0], [2.0, 4.0, 2.0, 5.0], [3.0, 1.0, 5.0, 8.0]])
        >>> x2 = ms.Tensor([[2.0, 4.0, 2.0, 5.0], [3.0, 1.0, 5.0, 8.0], [1.0, 3.0, 4.0, 7.0]])
        >>> func = ms.nn.layer.CosineSimilarity()
        >>> out = func(x1, x2)
        >>> print(out.asnumpy())
        [0.9402562 0.8614609 0.9516245]
    """

    def __init__(self, dim=1, eps=1e-08):
        """Initialize CosineSimilarity."""
        super().__init__()
        self.dim = dim
        self.eps = eps
        self.mul = P.Mul()
        self.div = P.DivNoNan()
        self.maximum = P.Maximum()
        self.cast = P.Cast()

    def construct(self, x1, x2):
        if not isinstance(x1, Tensor):
            raise TypeError(f"For 'CosineSimilarity', 'x1' must be a tensor, but got {type(x1)}")
        if not isinstance(x2, Tensor):
            raise TypeError(f"For 'CosineSimilarity', 'x2' must be a tensor, but got {type(x2)}")
        w12 = self.mul(x1, x2).sum(self.dim)
        w1 = self.mul(x1, x1).sum(self.dim)
        w2 = self.mul(x2, x2).sum(self.dim)
        n12 = self.maximum(self.mul(w1, w2), self.eps * self.eps).sqrt()
        out = self.div(w12, n12)
        return out


class Moments(Cell):
    """
    The Moments class will be deprecated in the future,
    this function can be replaced by :func:`ops.var_mean`
    """

    def __init__(self, axis=None, keep_dims=None):
        """Initialize Moments."""
        super(Moments, self).__init__()
        logger.warning("The Moments class will be deprecated in the future,"
                       "this function can be replaced by :func:`ops.var_mean`")
        if axis is None:
            axis = ()
        if isinstance(axis, tuple):
            for idx, item in enumerate(axis):
                validator.check_value_type("axis[%d]" % idx, item, [int], self.cls_name)
        self.axis = validator.check_value_type('axis', axis, [int, tuple], self.cls_name)
        if keep_dims is None:
            keep_dims = False
        self.keep_dims = validator.check_value_type('keep_dims', keep_dims, [bool], self.cls_name)
        self.cast = P.Cast()
        self.reduce_mean = P.ReduceMean(keep_dims=True)
        self.square_diff = P.SquaredDifference()
        self.squeeze = P.Squeeze(self.axis)

    def construct(self, x):
        tensor_dtype = F.dtype(x)
        _check_input_dtype("input x", tensor_dtype, [mstype.float16, mstype.float32], self.cls_name)
        if tensor_dtype == mstype.float16:
            x = self.cast(x, mstype.float32)
        mean = self.reduce_mean(x, self.axis)
        variance = self.reduce_mean(self.square_diff(x, F.stop_gradient(mean)), self.axis)
        if not self.keep_dims:
            mean = self.squeeze(mean)
            variance = self.squeeze(variance)
        if tensor_dtype == mstype.float16:
            mean = self.cast(mean, mstype.float16)
            variance = self.cast(variance, mstype.float16)
            return mean, variance
        return mean, variance


class MatInverse(Cell):
    """
    Calculates the inverse of Positive-Definite Hermitian matrix using Cholesky decomposition.

    Inputs:
        - **x** (Tensor[Number]) - The input tensor. It must be a positive-definite matrix.
          With float16 or float32 data type.

    Outputs:
        Tensor, has the same dtype as the `x`.

    Raises:
        TypeError: If dtype of `x` is neither float16 nor float32.

    Supported Platforms:
        ``GPU``

    Examples:
        >>> import mindspore as ms
        >>> import numpy as np
        >>> x = ms.Tensor(np.array([[4, 12, -16], [12, 37, -43], [-16, -43, 98]]).astype(np.float32))
        >>> op = ms.nn.MatInverse()
        >>> output = op(x)
        >>> print(output)
        [[49.36112  -13.555558  2.1111116]
         [-13.555558  3.7777784  -0.5555557]
         [2.1111116  -0.5555557  0.11111113]]
    """

    def __init__(self):
        """Initialize MatInverse."""
        super(MatInverse, self).__init__()
        self.dtype = P.DType()
        self.choleskytrsm = P.CholeskyTrsm()
        self.matmul = MatMul(transpose_x1=True)

    def construct(self, a):
        input_dtype = self.dtype(a)
        _check_input_dtype("input_a", input_dtype, [mstype.float16, mstype.float32], self.cls_name)
        l_inverse = self.choleskytrsm(a)
        a_inverse = self.matmul(l_inverse, l_inverse)
        return a_inverse


class MatDet(Cell):
    """
    Calculates the determinant of Positive-Definite Hermitian matrix using Cholesky decomposition.

    Inputs:
        - **x** (Tensor[Number]) - The input tensor. It must be a positive-definite matrix.
          With float16 or float32 data type.

    Outputs:
        Tensor, has the same dtype as the `x`.

    Raises:
        TypeError: If dtype of `x` is neither float16 nor float32.

    Supported Platforms:
        ``GPU``

    Examples:
        >>> import mindspore as ms
        >>> import numpy as np
        >>> x = ms.Tensor(np.array([[4, 12, -16], [12, 37, -43], [-16, -43, 98]]).astype(np.float32))
        >>> op = ms.nn.MatDet()
        >>> output = op(x)
        >>> print(output)
        35.999996
    """

    def __init__(self):
        """Initialize MatDet."""
        super(MatDet, self).__init__()
        self.dtype = P.DType()
        self.cholesky = P.Cholesky()
        self.det_triangle = P.DetTriangle()
        self.square = P.Square()

    def construct(self, a):
        input_dtype = self.dtype(a)
        _check_input_dtype("input_a", input_dtype, [mstype.float16, mstype.float32], self.cls_name)
        l = self.cholesky(a)
        l_det = self.det_triangle(l)
        a_det = self.square(l_det)
        return a_det
