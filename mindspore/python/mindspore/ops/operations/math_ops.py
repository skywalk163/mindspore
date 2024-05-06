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

"""Operators for math."""
# pylint: disable=unused-import
from __future__ import absolute_import
from __future__ import division

import numpy as np

from mindspore import context
from mindspore import log as logger
from mindspore.ops import signature as sig
from mindspore import _checkparam as validator
from mindspore.common import dtype as mstype
from mindspore.common.tensor import Tensor
from mindspore.ops._utils import get_broadcast_shape
from mindspore.ops.primitive import Primitive, PrimitiveWithInfer, PrimitiveWithCheck, prim_attr_register, _run_op
from mindspore._c_expression import Tensor as Tensor_
from ..auto_generate import (Add, Addcdiv, Addcmul, ReduceMean, ReduceSum, ReduceAll, ReduceAny,
                             ReduceMax, ReduceMin, ReduceProd, Betainc, Neg,
                             Mul, Square, Rsqrt, Sqrt, Reciprocal, Pow, Exp,
                             Logit, ReduceStd, Expm1, Log, Log1p, Erf, Erfc,
                             Minimum, RealDiv, FloorDiv, Floor, FloorMod, Ceil,
                             Acosh, Cosh, Asinh, Sinc, Sinh, Equal, NotEqual,
                             Greater, GreaterEqual, Gcd, LogicalNot, LogicalAnd, LogicalOr,
                             LogicalXor, Cos, ACos, Sin, Asin, Abs, Round, Atan, Atanh, Atan2,
                             LinSpace, MatrixDeterminant, LogMatrixDeterminant, Erfinv, Conj,
                             Real, Complex, Angle, MatrixExp, CholeskyInverse, Trace, Cholesky,
                             FFTWithSize, NextAfter, NanToNum, Eig, Qr, Roll, Maximum, Div, CumProd,
                             CumSum, Less, LessEqual, AssignAdd)


def _infer_shape_reduce(x, axis, keep_dims, prim_name):
    """Common infer for reduce operator"""

    def reduce_one_axis(one_axis):
        validator.check_int_range(one_axis, -dim, dim, validator.INC_LEFT, 'axis', prim_name)
        if one_axis < 0:
            one_axis += dim
        axis_reduce.add(one_axis)

    validator.check_value_type('axis', axis, [int, tuple, list], prim_name)
    dim = len(x)
    axis_reduce = set()

    if isinstance(axis, int):
        reduce_one_axis(axis)
    else:
        if not axis:
            if keep_dims:
                return [1] * dim
            return []
        for index, one_axis in enumerate(axis):
            validator.check_value_type('axis[%d]' % index, one_axis, [int], prim_name)
            reduce_one_axis(one_axis)

    out_shape = []
    for i in range(dim):
        if i in axis_reduce:
            if keep_dims:
                out_shape.append(1)
        else:
            out_shape.append(x[i])
    return out_shape


class _BinaryOp(PrimitiveWithInfer):
    """
    Define binary operators.
    """

    __mindspore_signature__ = (sig.sig_dtype.T, sig.sig_dtype.T)

    @prim_attr_register
    def __init__(self):
        """Initialize _BinaryOp"""
        self.init_prim_io_names(inputs=['x', 'y'], outputs=['output'])

    def infer_shape(self, x_shape, y_shape):
        return get_broadcast_shape(x_shape, y_shape, self.name)


class _MathBinaryOp(_BinaryOp):
    """
    Define math binary operators.
    """

    @staticmethod
    def do_infer_dtype(x_dtype, y_dtype, valid_dtype=mstype.number_type, prim_name=None):
        """Staticmethod of infer dtype for _MathBinaryOp."""
        args_type = {"x": x_dtype, "y": y_dtype}
        complex_types = [mstype.TensorType(mstype.complex64), mstype.TensorType(mstype.complex128)]
        if x_dtype in complex_types or y_dtype in complex_types:
            if (not isinstance(x_dtype, type(mstype.tensor_type))) or \
               (not isinstance(y_dtype, type(mstype.tensor_type))):
                raise TypeError('Only Tensor type support Complex')
            type_infer_dict = {
                (mstype.complex64, mstype.complex64): mstype.TensorType(mstype.complex64),
                (mstype.complex64, mstype.float32): mstype.TensorType(mstype.complex64),
                (mstype.float32, mstype.complex64): mstype.TensorType(mstype.complex64),
                (mstype.complex128, mstype.complex128): mstype.TensorType(mstype.complex128),
                (mstype.complex128, mstype.float64): mstype.TensorType(mstype.complex128),
                (mstype.float64, mstype.complex128): mstype.TensorType(mstype.complex128),
            }
            if (x_dtype.element_type(), y_dtype.element_type()) not in type_infer_dict.keys():
                raise TypeError('Complex math binary op expecting Tensor [Complex64, Complex64],'
                                + '[Complex64, Float32], [Float32, Complex64], [Complex128, Complex128],'
                                + '[Complex128, Float64], [Float64, Complex128],'
                                + f'but got : [{format(x_dtype)},{format(y_dtype)}].')
            return type_infer_dict.get((x_dtype.element_type(), y_dtype.element_type()))

        validator.check_tensors_dtypes_same_and_valid(args_type, valid_dtype, prim_name)
        return x_dtype

    def infer_dtype(self, x_dtype, y_dtype):
        return _MathBinaryOp.do_infer_dtype(x_dtype, y_dtype, mstype.number_type, self.name)

    def _convert_back_shape(self, shape_value, cmp_shape):
        if isinstance(cmp_shape, (Tensor, Tensor_)):
            cmp_shape = cmp_shape.asnumpy()
        if not isinstance(cmp_shape, tuple):
            return shape_value
        real_shape = [dim if cmp_dim > 0 else cmp_dim for dim, cmp_dim in zip(shape_value, cmp_shape)]
        return tuple(real_shape)

class SilentCheck(Primitive):
    """
    Implement SilentCheck on `pre_val`, `min_val`, `max_val`, `result` and
    update them inplace with given parameters.

    Args:
        c_min_steps (int): an int determines...

        c_thresh_l1 (float): a float determines...

        c_coeff_l1 (float): a float determines...

        c_thresh_l2 (float): a float determines...

        c_coeff_l2 (float): a float determines...

    Inputs:
        - **val** (Tensor) - Tensor with dtype float32.
        - **input_grad** (Parameter) - Tensor with dtype float32.
        - **pre_val** (Parameter) - Input Parameter with dtype float32.
        - **min_val** (Parameter) - Input Parameter with dtype float32.
        - **max_val** (Parameter) - Input Parameter with dtype float32.
        - **val_counter** (Parameter) - Input Parameter with dtype int32.

    Outputs:
        Tuple of 5 Tensors, the updated parameters.
        - **input_grad** (Tensor) - Tensor with dtype float32.
        - **pre_val** (Tensor) - Tensor with dtype float32.
        - **min_val** (Tensor) - Tensor with dtype float32.
        - **max_val** (Tensor) - Tensor with dtype float32.
        - **result** (Tensor) - Tensor with dtype int32.

    Raises:
        TypeError: If `val` is not Tensor with dtype float32.
        TypeError: If `result` is not Tensor with dtype int32.
        TypeError: If `pre_val`, `min_val`, `max_val`, `input_grad` are not all Parameter type with dtype float32.
        TypeError: If `c_thresh_l1` or `c_coeff_l1` is not a float number.
        TypeError: If `c_min_steps` is not an int number.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> from mindspore.ops.operations.math_ops import SilentCheck
        >>> silent_check = SilentCheck()
        xxx
    """

    @prim_attr_register
    def __init__(self, c_min_steps, c_thresh_l1, c_coeff_l1, c_thresh_l2, c_coeff_l2):
        """Initialize SilentCheck."""
        validator.check_value_type("c_min_steps", c_min_steps, [int], self.name)
        validator.check_value_type("c_thresh_l1", c_thresh_l1, [float], self.name)
        validator.check_value_type("c_coeff_l1", c_coeff_l1, [float], self.name)
        validator.check_value_type("c_thresh_l2", c_thresh_l2, [float], self.name)
        validator.check_value_type("c_coeff_l2", c_coeff_l2, [float], self.name)
        self.add_prim_attr('side_effect_mem', True)


class _BitwiseBinaryOp(_MathBinaryOp):
    """
    Define bitwise binary operators.
    """

    @prim_attr_register
    def __init__(self):
        """Initialize _BitwiseBinaryOp"""
        self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['y'])

    @staticmethod
    def _check_bitwise_op_input_type(x1_type, x2_type, prim):
        args = {'x1': x1_type, 'x2': x2_type}
        valid_dtypes = mstype.int_type + mstype.uint_type
        validator.check_tensors_dtypes_same_and_valid(args, valid_dtypes, prim)
        return x1_type

    def infer_dtype(self, x1_type, x2_type):
        return _BitwiseBinaryOp._check_bitwise_op_input_type(x1_type, x2_type, self.name)


class Ger(Primitive):
    r"""
    Ger product of `x1` and `x2`. Calculate the outer product of two arrays. If `x1` is a 1D Tensor of
    shape :math:`(m,)` and `x2` is a 1D Tensor of shape :math:`(n,)`, then `output` must be a 2D Tensor of shape
    :math:`(m, n)`.

    Refer to :func:`mindspore.ops.ger` for more details.

    Inputs:
        - **x1** - (Tensor) - 1-D input Tensor.
        - **x2** - (Tensor) - 1-D input Tensor, has the same dtype as `x1`.

    Outputs:
        Tensor, output matrix with the same dtype as inputs.With `x1` shape :math:`(m,)` and
        `x2` shape of :math:`(n,)`,the `output` has shape :math:`(m, n)`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> x1 = Tensor([1., 2., 3., 4.], mindspore.float32)
        >>> x2 = Tensor([1., 2., 3.], mindspore.float32)
        >>> ger = ops.Ger()
        >>> output = ger(x1, x2)
        >>> print(output)
        [[ 1.  2.  3.]
         [ 2.  4.  6.]
         [ 3.  6.  9.]
         [ 4.  8. 12.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Ger"""
        self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['y'])


class AddV2(Primitive):
    r"""
    Adds two input tensors element-wise.

    Inputs of `x` and `y` comply with the implicit type conversion rules to make the data types consistent.
    The inputs must be two tensors or one tensor and one scalar.
    When the inputs are two tensors, and the shapes of them can be broadcast.
    When the inputs are one tensor and one scalar, the scalar could only be a constant.
    CPU/Ascend does not support broadcast for now.

    .. math::

        out_{i} = x_{i} + y_{i}

    Inputs:
        - **x** (Union[Tensor]) - The first input is a tensor whose data type is one of
          uint8, int8, int16, int32, int64, float16, float32, float64,
          complex64, complex128 currently or scalar.
        - **y** (Union[Tensor]) - The second input is a tensor whose data type is one of
          uint8, int8, int16, int32, int64, float16, float32, float64,
          complex64, complex128 currently or scalar.

    Outputs:
        Tensor, the shape is the same as the input tensor,
        and the data type is the one with higher precision or higher digits among the two inputs.

    Raises:
        TypeError: If neither `x` nor `y` is a Tensor.
        TypeError: If dtype of `x` or `y` is not in [float16, float32, float64,
        uint8, int8, int16, int32, int64, complex64, complex128].
        ValueError: If the shape of 'x' and 'y' is not the same for CPU and Ascend.


    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore.ops.operations.math_ops import AddV2
        >>> addv2 = AddV2()
        >>> x = Tensor(np.array([1, 2, 3]).astype(np.int32))
        >>> y = Tensor(np.array([4, 5, 6]).astype(np.int32))
        >>> output = addv2(x, y)
        >>> print(output)
        [5 7 9]
    """
    __mindspore_signature__ = (sig.sig_dtype.T, sig.sig_dtype.T)

    @prim_attr_register
    def __init__(self):
        """Initialize AddV2"""
        self.init_prim_io_names(inputs=['x', 'y'], outputs=['output'])


class AssignSub(Primitive):
    """
    Updates a `Parameter` by subtracting a value from it.

    Refer to :func:`mindspore.ops.assign_sub` for more details.

    Inputs:
        - **variable** (Parameter) - The `Parameter`.
          :math:`(N,*)` where :math:`*` means, any number of additional dimensions, its rank be should be less than 8.
        - **value** (Union[numbers.Number, Tensor]) - The value to be subtracted from the `variable`.
          It must have the same shape as `variable` if it is a Tensor.

    Outputs:
        Tensor, has the same data type and shape as original `variable`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops, nn
        >>> from mindspore.common.initializer import initializer
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.AssignSub = ops.AssignSub()
        ...         self.variable = mindspore.Parameter(initializer(1, [1], mindspore.int32), name="global_step")
        ...
        ...     def construct(self, x):
        ...         self.AssignSub(self.variable, x)
        ...         return self.variable
        ...
        >>> net = Net()
        >>> value = Tensor(np.ones([1]).astype(np.int32)*100)
        >>> output = net(value)
        >>> print(net.variable.asnumpy())
        [-99]
    """

    __mindspore_signature__ = (
        sig.make_sig('val', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('value', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self):
        """Initialize AssignSub"""
        self.init_prim_io_names(inputs=['val', 'value'], outputs=['val'])
        self.add_prim_attr('side_effect_mem', True)


class _Reduce(PrimitiveWithCheck):
    """
    Definition of base class of reduction class operators.

    Args:
         keep_dims (bool): If ``True`` , keep these reduced dimensions and the length is 1.
                           If ``False`` , don't keep these dimensions. Default: ``False`` .
    """

    __mindspore_signature__ = (
        sig.make_sig('input_x'),
        sig.make_sig('axis', default=())
    )

    @prim_attr_register
    def __init__(self, keep_dims=False):
        """Initialize Reduce"""
        validator.check_value_type('keep_dims', keep_dims, [bool], self.name)
        self.init_prim_io_names(inputs=['input_x', 'axis'], outputs=['y'])

    def __call__(self, x, axis=()):
        args = [x, axis]
        output = _run_op(self, self.name, args)
        return output

    def infer_value(self, input_x, axis):
        """ return reduce op value"""
        value = None
        if input_x is not None and axis is not None:
            prim_map = {
                'ReduceMax': np.max,
                'ReduceMin': np.min,
                'ReduceProd': np.prod,
                'ReduceMean': np.mean,
                'ReduceAll': np.all,
                'ReduceAny': np.any,
            }
            np_reduce_func = prim_map.get(self.name, None)

            if np_reduce_func is not None:
                value = input_x.asnumpy()
                if isinstance(axis, int):
                    pass
                elif axis:
                    axis = tuple(set(axis))
                else:
                    axis = tuple(range(len(value.shape)))
                value = np_reduce_func(value, axis, keepdims=self.keep_dims)
                value = np.array(value)
                value = Tensor(value)
        return value


class EuclideanNorm(Primitive):
    """
    Calculates the Euclidean norm(aka L2 norm) of a Tensor along the specified axes.
    The specified `axes` are removed by default.

    Args:
        keep_dims (bool, optional): whether to retain the reduced dimensions. If ``True`` , retains them with length 1.
            If ``False`` , these dimensions are removed. Default: ``False`` .

    Inputs:
        - **x** (Tensor) - The input Tensor to reduce.
        - **axes** (Tensor) - The axes to perform reduction on. Must be one of the following types: int32, int64.
          It must be in range :math:`[-rank(x), rank(x))`.

    Outputs:
        Tensor, has the same type as the 'x'.

    Raises:
        TypeError: If `keep_dims` is not a bool.
        TypeError: If `x` is not a Tensor.
        ValueError: If `axes` is out of range.

    Supported Platforms:
        ``GPU``

    Examples:
        >>> x = Tensor(np.array([[3, 5], [4, 12]])).astype(np.int32)
        >>> axes = Tensor([0])
        >>> op = ops.EuclideanNorm(keep_dims=True)
        >>> output = op(x, axes)
        >>> print(output)
        [[5 13]]
    """

    @prim_attr_register
    def __init__(self, keep_dims=False):
        """Initialize"""
        self.init_prim_io_names(inputs=['x', 'axes'], outputs=['y'])
        validator.check_value_type("keep_dims", keep_dims, [bool], self.name)


class CumulativeLogsumexp(Primitive):
    """
    Compute the cumulative log-sum-exp of the input tensor `x` along `axis` . For example, with all parameters at
    default values, if the input `x` is a tensor [a, b, c], the output will be [a, log(exp(a) + exp(b)),
    log(exp(a) + exp(b) + exp(c))].

    Args:
        exclusive (bool, optional): If ``True`` , the last element will be skipped during the calculation and thus an
                                    exclusive cumulative log-sum-exp will be performed. For example, this operation
                                    will output [-inf, a, log(exp(a) * exp(b))] with tensor [a, b, c] as the input.
                                    Note that the minimal value -inf, for performance reasons, is representable by the
                                    floating point type. Default: ``False`` .
        reverse (bool, optional): If ``True`` , the function accumulation values will be calculated after the elements
                                  of `x` on `axis` are flipped, and the calculation result will be flipped afterwards.
                                  For example, this operation will output [log(exp(c) + exp(b) + exp(a)), log(exp(c) +
                                  exp(b)), c] with tensor [a, b, c] as the input. Default: ``False`` .

    Inputs:
        - **x** (Tensor) - The input tensor. Must be one of the following types: float16, float32, float64. The
          dimension of `x` must greater than 0.
        - **axis** (Tensor) - A 0-D tensor describing the dimension to compute the cumulative product. Must be one of
          the following types: int64, int32, int16. Must be in the range [-rank(x), rank(x)). Default: ``0`` .

    Outputs:
        Tensor, has the same dtype and shape as the `x`.

    Raises:
        TypeError: If `x` or `axis` not a Tensor.
        TypeError: If dtype of `x` is not in [float16, float32, float64].
        TypeError: If dtype of `axis` is not in [int16, int32, int64].
        TypeError: If `exclusive` or `reverse` is not a bool.
        ValueError: If the dimension of `x` is not greater than 0.
        RuntimeError: If `axis` is out of range [-rank(x), rank(x)).

    Supported Platforms:
        ``Ascend`` ``CPU`` ``GPU``

    Examples:
        >>> x = Tensor(np.array([1.0, 2.0, 3.0]).astype(np.float32))
        >>> op = ops.CumulativeLogsumexp(exclusive=False, reverse=False)
        >>> output = op(x, Tensor(0))
        >>> print(output)
        [1.        2.3132617 3.407606 ]
        >>> x = Tensor(np.array([1.0, 2.0, 3.0]).astype(np.float32))
        >>> op = ops.CumulativeLogsumexp(exclusive=True, reverse=False)
        >>> output = op(x, Tensor(0))
        >>> print(output)
        [-3.4028235e+38  1.0000000e+00  2.3132617e+00]
        >>> x = Tensor(np.array([1.0, 2.0, 3.0]).astype(np.float32))
        >>> op = ops.CumulativeLogsumexp(exclusive=False, reverse=True)
        >>> output = op(x, Tensor(0))
        >>> print(output)
        [3.407606  3.3132617 3.       ]
        >>> x = Tensor(np.array([1.0, 2.0, 3.0]).astype(np.float32))
        >>> op = ops.CumulativeLogsumexp(exclusive=True, reverse=True)
        >>> output = op(x, Tensor(0))
        >>> print(output)
        [ 3.3132617e+00  3.0000000e+00 -3.4028235e+38]
    """

    @prim_attr_register
    def __init__(self, exclusive=False, reverse=False):
        """Initialize  CumulativeLogsumexp"""
        self.init_prim_io_names(inputs=['x', 'axis'], outputs=['y'])
        validator.check_bool(exclusive, "exclusive", self.name)
        validator.check_bool(reverse, "reverse", self.name)


class Bucketize(Primitive):
    """
    Bucketizes `input` based on `boundaries`.

    Args:
        boundaries (list[float]): A sorted list of floats gives the boundary of the buckets, and no default value.

    Inputs:
        - **input** (Tensor) - A tensor containing the search value(s).

    Outputs:
        Tensor, with the same shape as the input, and data type is int32.

    Raises:
        TypeError: If `boundaries` is not a listFloat.
        TypeError: If `input` is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> class Bucketize(nn.Cell):
        ...     def __init__(self, boundaries):
        ...         super().__init__()
        ...         self.bucketize = ops.Bucketize(boundaries=boundaries)
        ...     def construct(self, input):
        ...         return self.bucketize(input)
        >>> input = Tensor(np.array([[3, 6, 9], [3, 6, 9]]).astype(np.int32))
        >>> boundaries = list(np.array([1., 3., 5., 7., 9.]))
        >>> net = Bucketize(boundaries)
        >>> output = net(input)
        >>> print(output)
        [[2 3 5]
         [2 3 5]]
    """

    @prim_attr_register
    def __init__(self, boundaries):
        """Initialize Bucketize"""
        validator.check_value_type("boundaries", boundaries, [list], self.name)
        for index, one_boundaries in enumerate(boundaries):
            validator.check_value_type('boundaries[%d]' % index, one_boundaries, [float], self.name)
        self.init_prim_io_names(inputs=['input'], outputs=['output'])


class Lcm(Primitive):
    """
    Computes least common multiplier of input tensors element-wise.
    The shape of two inputs should be broadcastable, and data type of them should be
    one of: int32, int64.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x1** (Tensor) - The first input tensor.
        - **x2** (Tensor) - The second input tensor.

    Outputs:
        Tensor, the shape is the same as the one after broadcasting, and the data type is one
        with higher digits in the two inputs.

    Raises:
        TypeError: If data type `x1` or `x2` is not int32 or int64.
        ValueError: If shape of two inputs are not broadcastable.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x1 = Tensor(np.array([7, 8, 9]))
        >>> x2 = Tensor(np.array([14, 6, 12]))
        >>> lcm_ = ops.Lcm()
        >>> y = lcm_(x1, x2)
        >>> print(y)
        [14 24 36]
    """

    __mindspore_signature__ = (sig.sig_dtype.T, sig.sig_dtype.T)

    @prim_attr_register
    def __init__(self):
        self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['y'])


class Cdist(Primitive):
    """
    Computes batched the p-norm distance between each pair of the two collections of row vectors.

    Refer to :func:`mindspore.ops.cdist` for more details.

    Args:
        p (float, optional): P value for the p-norm distance to calculate between each vector pair, P ∈ [0,∞].
            Default: ``2.0`` .

    Inputs:
        - **input_x** (Tensor) - Input tensor of shape :math:`(B, P, M)`.
          When :math:`B` is equal to 0, it means this dimension can be ignored,
          i.e. shape of the tensor is :math:`(P, M)`.
        - **input_y** (Tensor) - Input tensor of shape :math:`(B, R, M)` with the same dtype as `input_x`.

    Outputs:
        Tensor, has the same dtype as `input_x`, which shape is :math:`(B, P, R)`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor(np.array([[[1.0, 1.0], [2.0, 2.0]]]).astype(np.float32))
        >>> input_y = Tensor(np.array([[[3.0, 3.0], [3.0, 3.0]]]).astype(np.float32))
        >>> op = ops.Cdist(p=2.0)
        >>> output = op(input_x, input_y)
        >>> print(output)
        [[[2.8284273 2.8284273]
          [1.4142137 1.4142137]]]
    """

    @prim_attr_register
    def __init__(self, p=2.0):
        """Initialize Cdist"""
        validator.check_value_type("p", p, [float], self.name)
        if (p < 0 or np.isnan(p)):
            raise ValueError('Cdist p must be a non-negative value, but got `{p}`.')
        self.init_prim_io_names(inputs=['input_x', 'input_y'], outputs=['output'])


class LpNorm(Primitive):
    r"""
    Return the p-norm of a matrix or vector.

    .. math::
        output = \|input\|_{p}=\left(\sum_{i=1}^{n}\left|input\right|^{p}\right)^{1 / p}

    Args:
        axis(int,list,tuple): Specifies which dimension or dimensions of input to calculate the norm across.
        p(int, optional): The order of norm. Default: ``2`` .
        keep_dims(bool, optional): Whether the output tensors have dim retained or not. Default: ``False`` .
        epsilon(float, optional): The lower bound value, when the calculated norm is less than this value,
            replace this result with `epsilon`. Default: ``1e-12`` .

    Inputs:
        - **input** (Tensor) - Input tensor of type float16, float32.

    Outputs:
        Tensor, has the same dtype as `input`, its shape depends on `axis`. For example, if the shape of input
        is :math:`(2, 3, 4)`, `axis` is :math:`[0, 1]`, output shape will be :math:`(4,)`.

    Raises:
        TypeError: If `input` is not a Tensor.
        TypeError: If dtype of `input` is not one of: float16, float32.
        TypeError: If `p` is not an int.
        TypeError: If `axis` is not an int, a tuple or a list.
        TypeError: If `axis` is a tuple or a list, but the element of `axis` is not an int.
        TypeError: If `keep_dims` is not a bool.
        ValueError: If the element of `axis` is out of the range :math:`[-r, r)`,
            where :math:`r` is the rank of `input`.
        ValueError: If the length of shape of `axis` is bigger than the length of shape of `input`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor(np.array([[[1.0, 2.0], [3.0, 4.0]], [[5.0, 6.0], [7.0, 8.0]]]).astype(np.float32))
        >>> op = ops.LpNorm(axis=[0, 1], p=2, keep_dims=False)
        >>> output = op(input_x)
        >>> print(output)
        [ 9.165152 10.954452]
    """

    @prim_attr_register
    def __init__(self, axis, p=2, keep_dims=False, epsilon=1e-12):
        """Initialize LpNorm"""
        super().__init__("LpNorm")
        validator.check_value_type("p", p, [int], self.name)
        validator.check_value_type("axis", axis, [int, tuple, list], self.name)
        validator.check_value_type("keep_dims", keep_dims, [bool], self.name)
        validator.check_value_type("epsilon", epsilon, [float], self.name)
        validator.check_non_negative_int(p, "p", self.name)
        validator.check_non_negative_float(epsilon, "epsilon", self.name)
        if isinstance(axis, int):
            self.add_prim_attr('axis', [self.axis])
        else:
            for element_of_axis in axis:
                validator.check_value_type("element_of_axis", element_of_axis, [int], self.name)
        self.init_prim_io_names(inputs=['input'], outputs=['output'])


class MatMul(Primitive):
    r"""
    Multiplies matrix `a` and matrix `b`.

    .. math::

        (Output)_{i j}=\sum_{k=1}^{p} a_{i k} b_{k j}=a_{i 1} b_{1 j}+a_{i 2} b_{2 j}+\cdots+a_{i p} b_{p j}, p\in N

    where the :math:`i,j` indicates the output of the i-th row and j-th column element.

    Note:
        - If :math:`N * M` cannot be divided by 16, the performance will be poor in ascend environment.
        - The dtype of inputs must be same.
        - On Ascend, float64 doesn't be supported.

    Args:
        transpose_a (bool): If ``True`` , `a` is transposed before multiplication. Default: ``False`` .
        transpose_b (bool): If ``True`` , `b` is transposed before multiplication. Default: ``False`` .

    Inputs:
        - **a** (Tensor) - The first tensor to be multiplied. The shape of the tensor is :math:`(N, C)`. If
          `transpose_a` is ``True`` , its shape must be :math:`(C, N)` after transpose.
        - **b** (Tensor) - The second tensor to be multiplied. The shape of the tensor is :math:`(C, M)`. If
          `transpose_b` is ``True`` , its shape must be :math:`(M, C)` after transpose.

    Outputs:
        Tensor, the shape of the output tensor is :math:`(N, M)`.

    Raises:
        TypeError: If `transpose_a` or `transpose_b` is not a bool.
        TypeError: If the dtype of `a` and the dtype of `b` are not the same.
        ValueError: If the column of matrix dimensions of `a` is not equal to
                    the row of matrix dimensions of `b`.
        ValueError: If length of shape of `a` or `b` is not equal to 2.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> a = Tensor(np.ones(shape=[1, 3]), mindspore.float32)
        >>> b = Tensor(np.ones(shape=[3, 4]), mindspore.float32)
        >>> matmul = ops.MatMul()
        >>> output = matmul(a, b)
        >>> print(output)
        [[3. 3. 3. 3.]]
    """

    @prim_attr_register
    def __init__(self, transpose_a=False, transpose_b=False):
        """Initialize MatMul."""
        self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['output'])
        cls_name = self.name
        validator.check_value_type("transpose_a", transpose_a, [bool], cls_name)
        validator.check_value_type("transpose_b", transpose_b, [bool], cls_name)
        self.add_prim_attr('transpose_x1', self.transpose_a)
        self.add_prim_attr('transpose_x2', self.transpose_b)


class BatchMatMul(Primitive):
    r"""
    Computes matrix multiplication between two tensors by batch.

    .. math::

        \text{output}[..., :, :] = \text{matrix}(x[..., :, :]) * \text{matrix}(y[..., :, :])

    The rank of both two input tensors must be same and not less than `2`.

    Args:
        transpose_a (bool): If ``True`` , the last two dimensions of `x` is transposed before multiplication.
            Default: ``False`` .
        transpose_b (bool): If ``True`` , the last two dimensions of `y` is transposed before multiplication.
            Default: ``False`` .

    Inputs:
        - **x** (Tensor) - The first tensor to be multiplied. The shape of the tensor is :math:`(*B, N, C)`,
          where :math:`*B` represents the batch size which can be multidimensional, :math:`N` and :math:`C` are the
          size of the last two dimensions. If `transpose_a` is ``True`` , its shape must be :math:`(*B, C, N)`.
        - **y** (Tensor) - The second tensor to be multiplied. The shape of the tensor is :math:`(*B, C, M)`. If
          `transpose_b` is ``True`` , its shape must be :math:`(*B, M, C)`.

    Outputs:
        Tensor, the shape of the output tensor is :math:`(*B, N, M)`.

    Raises:
        TypeError: If `transpose_a` or `transpose_b` is not a bool.
        ValueError: If length of shape of `x` is not equal to length of shape of `y` or
                    length of shape of inputs is less than 2.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.ones(shape=[2, 4, 1, 3]), mindspore.float32)
        >>> y = Tensor(np.ones(shape=[2, 4, 3, 4]), mindspore.float32)
        >>> batmatmul = ops.BatchMatMul()
        >>> output = batmatmul(x, y)
        >>> print(output.shape)
        (2, 4, 1, 4)
        >>> x = Tensor(np.ones(shape=[2, 4, 3, 1]), mindspore.float32)
        >>> y = Tensor(np.ones(shape=[2, 4, 3, 4]), mindspore.float32)
        >>> batmatmul = ops.BatchMatMul(transpose_a=True)
        >>> output = batmatmul(x, y)
        >>> print(output.shape)
        (2, 4, 1, 4)
    """

    @prim_attr_register
    def __init__(self, transpose_a=False, transpose_b=False):
        """Initialize BatchMatMul."""
        self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['output'])
        cls_name = self.name
        validator.check_value_type("transpose_a", transpose_a, [bool], cls_name)
        validator.check_value_type("transpose_b", transpose_b, [bool], cls_name)
        self.add_prim_attr('adj_x1', self.transpose_a)
        self.add_prim_attr('adj_x2', self.transpose_b)


class AddN(Primitive):
    """
    Computes addition of all input tensors element-wise.

    Refer to :func:`mindspore.ops.addn` for more details.

    Inputs:
        - **x** (Union(tuple[Tensor], list[Tensor])) - A tuple or list composed of Tensor, the data type is
          boolean or numeric.

    Outputs:
        Tensor, has the same shape and dtype as each Tensor of `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops
        >>> class NetAddN(nn.Cell):
        ...     def __init__(self):
        ...         super(NetAddN, self).__init__()
        ...         self.addN = ops.AddN()
        ...
        ...     def construct(self, *z):
        ...         return self.addN(z)
        ...
        >>> net = NetAddN()
        >>> x = Tensor(np.array([1, 2, 3]), mindspore.float32)
        >>> y = Tensor(np.array([4, 5, 6]), mindspore.float32)
        >>> output = net(x, y, x, y)
        >>> print(output)
        [10. 14. 18.]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize AddN."""
        self.init_prim_io_names(inputs=["inputs"], outputs=["sum"])

    def check_elim(self, inputs):
        if len(inputs) != 1:
            return False, None
        if isinstance(inputs[0], Tensor):
            return True, inputs[0]
        raise TypeError(f"For '{self.name}', the type of 'inputs[0]' must be a tensor, but "
                        f"got {type(inputs[0]).__name__}, "
                        f"or the length of 'inputs' should not be equal to 1, but got ({len(inputs)}).")


class AccumulateNV2(Primitive):
    """
    Computes accumulation of all input tensors element-wise.

    Refer to :func:`mindspore.ops.accumulate_n` for more details.

    Inputs:
        - **x** (Union(tuple[Tensor], list[Tensor])) - The input tuple or list
          is made up of multiple tensors whose dtype is number to be added together.
          Each element of tuple or list should have the same shape.

    Outputs:
        Tensor, has the same shape and dtype as each entry of the `x`.

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops, nn
        >>> class NetAccumulateNV2(nn.Cell):
        ...     def __init__(self):
        ...         super(NetAccumulateNV2, self).__init__()
        ...         self.accumulateNV2 = ops.AccumulateNV2()
        ...
        ...     def construct(self, *z):
        ...         return self.accumulateNV2(z)
        ...
        >>> net = NetAccumulateNV2()
        >>> x = Tensor(np.array([1, 2, 3]), mindspore.float32)
        >>> y = Tensor(np.array([4, 5, 6]), mindspore.float32)
        >>> output = net(x, y, x, y)
        >>> print(output)
        [10. 14. 18.]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize AccumulateNV2."""
        self.__setattr_flag__ = True
        self.init_prim_io_names(inputs=["inputs"], outputs=["sum"])

    def check_elim(self, inputs):
        if len(inputs) != 1:
            return False, None
        if isinstance(inputs[0], Tensor):
            return True, inputs[0]
        raise TypeError(f"For '{self.name}', the type of 'inputs[0]' must be a tensor, "
                        f"but got {type(inputs[0]).__name__}, "
                        f"or the length of 'inputs' should not be equal to 1, but got ({len(inputs)}).")


class InplaceUpdateV2(Primitive):
    r"""
    Updates specified values in `x` to `v` according to `indices`.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.inplace_update` for more details.

    Inputs:
        - **x** (Tensor) - A tensor which to be inplace updated. It can be one of the following data types:
          float32, float16 and int32.
        - **indices** (Union[int, tuple]): Indices into the left-most dimension of `x`, and determines which rows of x
          to update with v. It is an int or tuple, whose value is in [0, the first dimension size of x).
        - **v** (Tensor) - A tensor with the same type as `x` and the same dimension size as `x` except
          the first dimension, which must be the same as the size of `indices`.

    Outputs:
        Tensor, with the same type and shape as the input `x`.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> indices = (0, 1)
        >>> x = Tensor(np.array([[1, 2], [3, 4], [5, 6]]), mindspore.float32)
        >>> v = Tensor(np.array([[0.5, 1.0], [1.0, 1.5]]), mindspore.float32)
        >>> inplace_update_v2 = ops.InplaceUpdateV2()
        >>> output = inplace_update_v2(x, indices, v)
        >>> print(output)
        [[0.5 1. ]
         [1.  1.5]
         [5.  6. ]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize InplaceUpdateV2"""
        self.init_prim_io_names(inputs=['x', 'indices', 'v'], outputs=['y'])

    def __call__(self, x, indices, v):
        args = [x, indices, v]
        output = _run_op(self, self.name, args)
        return output


class InplaceAdd(Primitive):
    """
    Adds `v` into specified rows of `x`. Computes `y` = `x`; y[i,] += `v`.

    Refer to :func:`mindspore.ops.inplace_add` for more details.

    Args:
        indices (Union[int, tuple]): Indices into the left-most dimension of `x`, and determines which rows of `x`
            to add with `v`. It is an integer or a tuple, whose value is in [0, the first dimension size of `x`).

    Inputs:
        - **x** (Tensor) - The tensor to be added. It has shape :math:`(N,*)` where :math:`*` means
          any number of additional dimensions.
        - **input_v** (Tensor) - The value tensor add to `x`. It has the same dimension sizes as `x` except
          the first dimension, whose size must be the same as `indices`. It has the same data type with `x`.

    Outputs:
        Tensor, has the same shape and dtype as `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> indices = (0, 1)
        >>> x = Tensor(np.array([[1, 2], [3, 4], [5, 6]]), mindspore.float32)
        >>> input_v = Tensor(np.array([[0.5, 1.0], [1.0, 1.5]]), mindspore.float32)
        >>> inplaceAdd = ops.InplaceAdd(indices)
        >>> output = inplaceAdd(x, input_v)
        >>> print(output)
        [[1.5 3. ]
         [4.  5.5]
         [5.  6. ]]
    """

    @prim_attr_register
    def __init__(self, indices):
        """Initialize InplaceAdd"""
        self.init_prim_io_names(inputs=['x', 'v'], outputs=['y'])
        self.indices = indices
        validator.check_value_type('indices', indices, [tuple, int], self.name)
        if isinstance(indices, int):
            self.indices = (indices,)
        for item in self.indices:
            validator.check_value_type("item of indices", item, [int], self.name)
        self.add_prim_attr("indices", self.indices)


class InplaceIndexAdd(Primitive):
    """
    Adds Tensor `updates` to specified axis and indices of Tensor `var` element-wise.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.inplace_index_add` for more details.

    Args:
        axis (int): The dimension along which to index. It should be in range :math:`[0, len(var.dim))`.

    Inputs:
        - **var** (Parameter) - The input Parameter to add to, with data type uint8, int8, int16, int32,
          float16, float32, float64.
        - **indices** (Tensor) - The indies along `axis` to perform the addition. A 1D Tensor
          of shape :math:`(updates.shape[axis],)`, every value of it
          should be in range :math:`[0, var.shape[axis])` with data type int32.
        - **updates** (Tensor) - The input Tensor with the value to add. Must have same data type as `var`.
          The shape must be the same as `var` except the `axis` th dimension.

    Outputs:
        Tensor, updated result, has the same shape and dtype as `var`.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops, Parameter
        >>> var = Parameter(Tensor(np.array([[1, 2], [3, 4], [5, 6]]), mindspore.float32))
        >>> indices = Tensor(np.array([0, 1]), mindspore.int32)
        >>> updates = Tensor(np.array([[0.5, 1.0], [1.0, 1.5]]), mindspore.float32)
        >>> inplaceIndexAdd = ops.InplaceIndexAdd(axis=0)
        >>> var = inplaceIndexAdd(var, indices, updates)
        >>> print(var)
        [[1.5 3. ]
         [4.  5.5]
         [5.  6. ]]
    """

    __mindspore_signature__ = (
        sig.make_sig('var', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1),
        sig.make_sig('updates', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self, axis):
        """Initialize InplaceIndexAdd"""
        self.init_prim_io_names(inputs=['var', 'indices', 'updates'], outputs=['var'])
        self.axis = axis
        validator.check_value_type('axis', axis, [int], self.name)


class InplaceSub(Primitive):
    r"""
    Subtracts `v` into specified rows of `x`. Computes :math:`y = x`; :math:`y[i,] -= input\_v`.

    Refer to :func:`mindspore.ops.inplace_sub` for more details.

    Args:
        indices (Union[int, tuple]): Indices into the left-most dimension of `x`, and determines which rows of `x`
            to subtract by `v`. It is an integer or a tuple, whose value is in [0, the first dimension size of `x`).

    Inputs:
        - **x** (Tensor) - The tensor to be subtracted. It has shape :math:`(N,*)` where :math:`*` means
          any number of additional dimensions.
        - **input_v** (Tensor) - The value tensor subtract from `x`. It has the same dimension sizes as `x` except
          the first dimension, whose size must be the same as `indices`. It has the same data type with `x`.

    Outputs:
        Tensor, has the same shape and dtype as `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> indices = (0, 1)
        >>> x = Tensor(np.array([[1, 2], [3, 4], [5, 6]]), mindspore.float32)
        >>> input_v = Tensor(np.array([[0.5, 1.0], [1.0, 1.5]]), mindspore.float32)
        >>> inplaceSub = ops.InplaceSub(indices)
        >>> output = inplaceSub(x, input_v)
        >>> print(output)
        [[0.5 1. ]
         [2.  2.5]
         [5.  6. ]]
    """

    @prim_attr_register
    def __init__(self, indices):
        """Initialize InplaceSub"""
        self.init_prim_io_names(inputs=['x', 'v'], outputs=['y'])
        self.indices = indices
        validator.check_value_type('indices', indices, [tuple, int], self.name)
        if isinstance(indices, int):
            self.indices = (indices,)
        for item in self.indices:
            validator.check_value_type("item of indices", item, [int], self.name)
        self.add_prim_attr("indices", self.indices)


class Sub(_MathBinaryOp):
    r"""
    Subtracts the second input tensor from the first input tensor element-wise.

    Refer to :func:`mindspore.ops.sub` for more details.

    Note:
        - When the two inputs have different shapes, they must be able to broadcast to a common shape.
        - The two inputs can not be bool type at the same time,
          [True, Tensor(True, bool\_), Tensor(np.array([True]), bool\_)] are all considered bool type.
        - The two inputs comply with the implicit type conversion rules to make the data types
          consistent.

    Inputs:
        - **x** (Union[Tensor, number.Number, bool]) - The first input is a number.Number or
          a bool or a tensor whose data type is
          `number <https://www.mindspore.cn/docs/en/master/api_python/mindspore.html#mindspore.dtype>`_ or
          `bool_ <https://www.mindspore.cn/docs/en/master/api_python/mindspore.html#mindspore.dtype>`_.
        - **y** (Union[Tensor, number.Number, bool]) - The second input, when the first input is a Tensor,
          the second input should be a number.Number or bool value, or a Tensor whose data type is number or bool.

    Outputs:
        Tensor, the shape is the same as the two inputs after broadcasting,
        and the data type is the one with higher precision or higher digits among the two inputs.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([1, 2, 3]), mindspore.int32)
        >>> y = Tensor(np.array([4, 5, 6]), mindspore.int32)
        >>> sub = ops.Sub()
        >>> output = sub(x, y)
        >>> print(output)
        [-3 -3 -3]
    """

    def infer_value(self, x, y):
        if x is not None and y is not None:
            x = x.asnumpy()
            y = y.asnumpy()
            out = x - y
            out = np.array(out, x.dtype)
            return Tensor(out)
        return None


class SquaredDifference(Primitive):
    """
    Subtracts the second input tensor from the first input tensor element-wise and returns square of it.

    Inputs of `x` and `y` comply with the implicit type conversion rules to make the data types consistent.
    The inputs must be two tensors or one tensor and one scalar.
    When the inputs are two tensors,
    dtypes of them cannot be bool at the same time, and the shapes of them could be broadcast.
    When the inputs are one tensor and one scalar,
    the scalar could only be a constant.

    .. math::

        out_{i} = (x_{i} - y_{i}) * (x_{i} - y_{i}) = (x_{i} - y_{i})^2

    Inputs:
        - **x** (Union[Tensor, Number, bool]) - The first input is a number, or a bool, or a tensor.
        - **y** (Union[Tensor, Number, bool]) - The second input is a number, or a bool when the first input
          is a tensor, or a tensor.

    Outputs:
        Tensor, the shape is the same as the one after broadcasting,
        and the data type is the one with higher precision or higher digits among the two inputs.

    Raises:
        TypeError: if `x` and `y` is not a Number or a bool or a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([1.0, 2.0, 3.0]), mindspore.float32)
        >>> y = Tensor(np.array([2.0, 4.0, 6.0]), mindspore.float32)
        >>> squared_difference = ops.SquaredDifference()
        >>> output = squared_difference(x, y)
        >>> print(output)
        [1. 4. 9.]
    """
    __mindspore_signature__ = (sig.sig_dtype.T, sig.sig_dtype.T)

    @prim_attr_register
    def __init__(self):
        """Initialize _BinaryOp"""
        self.init_prim_io_names(inputs=['x', 'y'], outputs=['output'])


class Einsum(Primitive):
    """
    Sums the product of the elements of the input Tensor along
    dimensions specified notation based on the Einstein summation convention(Einsum).
    You can use this operator to perform diagonal/reducesum/transpose/matmul/mul/inner product operations, etc.

    Args:
        equation (str): An attribute, represent the operation you want to do.
            the value can contain only letters([a-z][A-Z]), commas(,), ellipsis(...),
            and arrow(->). the letters represent inputs's tensor dimension,
            commas(,)represent separate tensors, ellipsis(...) indicates
            the tensor dimension that you do not care about, the left of the
            arrow(->) indicates the input tensors,
            and the right of it indicates the desired output dimension.

    Inputs:
        - **x** () - Input tensor used for calculation.
          The inputs must be a tuple/list of Tensors.
          When the inputs are only one tensor, you can input (tensor, ).
          Dtypes of them should be float16/float32/float64 and dtype of the tensor(s) must be the same.

    Outputs:
        Tensor, the shape of it can be obtained from the equation,
        and the data type is the same as input tensors.

    Raises:
        TypeError: If equation itself is invalid, or the equation does not match the input tensor.
        TypeError: If dtype of the input Tensors are not the same or dtype is not float16, float32 or float64.

    Supported Platforms:
        ``GPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([1.0, 2.0, 4.0]), mindspore.float32)
        >>> equation = "i->"
        >>> einsum = ops.Einsum(equation)
        >>> output = einsum([x])
        >>> print(output)
        [7.]
        >>>
        >>> x = Tensor(np.array([1.0, 2.0, 4.0]), mindspore.float32)
        >>> y = Tensor(np.array([2.0, 4.0, 3.0]), mindspore.float32)
        >>> equation = "i,i->i"
        >>> einsum = ops.Einsum(equation)
        >>> output = einsum((x, y))
        >>> print(output)
        [ 2. 8. 12.]
        >>>
        >>> x = Tensor(np.array([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]), mindspore.float32)
        >>> y = Tensor(np.array([[2.0, 3.0], [1.0, 2.0], [4.0, 5.0]]), mindspore.float32)
        >>> equation = "ij,jk->ik"
        >>> einsum = ops.Einsum(equation)
        >>> output = einsum((x, y))
        >>> print(output)
        [[16. 22.]
        [37. 52.]]
        >>>
        >>> x = Tensor(np.array([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]), mindspore.float32)
        >>> equation = "ij->ji"
        >>> einsum = ops.Einsum(equation)
        >>> output = einsum((x,))
        >>> print(output)
        [[1. 4.]
        [2. 5.]
        [3. 6.]]
        >>>
        >>> x = Tensor(np.array([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]), mindspore.float32)
        >>> equation = "ij->j"
        >>> einsum = ops.Einsum(equation)
        >>> output = einsum((x,))
        >>> print(output)
        [5. 7. 9.]
        >>>
        >>> x = Tensor(np.array([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]), mindspore.float32)
        >>> equation = "...->"
        >>> einsum = ops.Einsum(equation)
        >>> output = einsum((x,))
        >>> print(output)
        [21.]
        >>>
        >>> x = Tensor(np.array([1.0, 2.0, 3.0]), mindspore.float32)
        >>> y = Tensor(np.array([2.0, 4.0, 1.0]), mindspore.float32)
        >>> equation = "j,i->ji"
        >>> einsum = ops.Einsum(equation)
        >>> output = einsum((x, y))
        >>> print(output)
        [[ 2. 4. 1.]
        [ 4. 8. 2.]
        [ 6. 12. 3.]]
    """

    @prim_attr_register
    def __init__(self, equation):
        if not isinstance(equation, str):
            raise TypeError("the equation must be str!")
        seg_equation = equation.split("->")
        if len(seg_equation) > 2:
            raise TypeError("the equation can contain only one arrow !")
        self.init_prim_io_names(inputs=['inputs'], outputs=['output'])


class Histogram(Primitive):
    """
    Computes the histogram of Tensor element distribution.

    The elements are sorted into equal width bins between `min` and `max`.
    If `min` and `max` are both zero, the minimum and maximum values of the data are used.

    Elements lower than min and higher than max are ignored.

    Args:
        bins (int, optional): Number of histogram bins, optional. Default: ``100`` . If specified, must be positive.
        min (float, optional): An optional float of the lower end of the range (inclusive). Default value is ``0.0`` .
        max (float, optional): An optional float of the upper end of the range (inclusive). Default value is ``0.0`` .

    Inputs:
        - **x** (Tensor) - the input tensor, type support list: [float16, float32, int32].

    Outputs:
        Tensor, 1-D Tensor with type int32.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If `x` datetype not in support list.
        TypeError: If attr `min` or `max` is not float.
        TypeError: If attr `bins` is not int.
        ValueError: If attr value `min` > `max`.
        ValueError: If attr `bins` <= 0.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> x = Tensor([1., 2, 1])
        >>> op = ops.Histogram(bins=4, min=0.0, max=3.0)
        >>> y = op(x)
        >>> print(y)
        [0 2 1 0]
    """

    @prim_attr_register
    def __init__(self, bins=100, min=0.0, max=0.0):  # pylint: disable=W0622
        """Initialize Histogram."""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])
        validator.check_value_type("bins", bins, [int], self.name)
        validator.check_value_type("min", min, [float], self.name)
        validator.check_value_type("max", max, [float], self.name)
        validator.check_positive_int(bins, 'bins', self.name)
        validator.check('min', min, 'max', max, validator.LE, self.name)


class HistogramFixedWidth(PrimitiveWithInfer):
    """
    Returns a rank 1 histogram counting the number of entries in values that fall into every bin. The bins are equal
    width and determined by the inputs `range` and the arguments `nbins`.

    Args:
        nbins (int): The number of histogram bins, the type is a positive integer.
        dtype (str, optional): An optional attribute. The dtype must be str. Default: ``'int32'`` .

    Inputs:
        - **x** (Tensor) - Numeric Tensor. Must be one of the following types: int32, float32, float16.
        - **range** (Tensor) - Must have the same data type as `x`, and the shape is :math:`(2,)`.
          x <= range[0] will be mapped to histogram[0], x >= range[1] will be mapped to histogram[-1].

    Outputs:
        1-D Tensor, whose length is the type is `nbins` with dtype of int32.

    Raises:
        TypeError: If `dtype` is not a str or `nbins` is not an int.
        ValueError: If `nbins` is less than 1.
        ValueError: If `dtype` is not 'int32'.

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> x = Tensor([-1.0, 0.0, 1.5, 2.0, 5.0, 15], mindspore.float16)
        >>> range_op = Tensor([0.0, 5.0], mindspore.float16)
        >>> hist = ops.HistogramFixedWidth(5)
        >>> output = hist(x, range_op)
        >>> print(output)
        [2 1 1 0 2]
    """

    @prim_attr_register
    def __init__(self, nbins, dtype='int32'):
        """Initialize HistogramFixedWidth."""
        self.nbins = validator.check_value_type("nbins", nbins, [int], self.name)
        validator.check_int(nbins, 1, validator.GE, "nbins", self.name)
        valid_values = ['int32']
        self.dtype = validator.check_string(dtype, valid_values, "dtype", self.name)
        self.init_prim_io_names(inputs=['x', 'range'], outputs=['y'])
        self.add_prim_attr('dtype', 3)


class Hypot(Primitive):
    """
    Computes hypotenuse of input tensors element-wise as legs of a right triangle.
    The shape of two inputs should be broadcastable, and data type of them should be
    one of: float32, float64.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x1** (Tensor) - The first input tensor.
        - **x2** (Tensor) - The second input tensor.

    Outputs:
        Tensor, the shape is the same as the one after broadcasting, and the data type is one
        with higher precision in the two inputs.

    Raises:
        TypeError: If data type `x1` or `x2` is not float32 or float64.
        ValueError: If shape of two inputs are not broadcastable.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x1 = Tensor(np.array([3., 5., 7.]))
        >>> x2 = Tensor(np.array([4., 12., 24.]))
        >>> hypot_ = ops.Hypot()
        >>> y = hypot_(x1, x2)
        >>> print(y)
        [ 5. 13. 25.]
        >>> x1 = Tensor(2.1, mindspore.float32)
        >>> x2 = Tensor(2.1, mindspore.float32)
        >>> hypot_ = ops.Hypot()
        >>> y = hypot_(x1, x2)
        >>> print(y)
        2.9698484
    """

    __mindspore_signature__ = (sig.sig_dtype.T, sig.sig_dtype.T)

    @prim_attr_register
    def __init__(self):
        self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['y'])


class Heaviside(Primitive):
    r"""
    Applies the Heaviside step function for input `x` element-wise.

    .. math::
            \text { heaviside }(\text { x, values })=\left\{\begin{array}{ll}
            0, & \text { if x }<0 \\
            \text { values, } & \text { if x }==0 \\
            1, & \text { if x }>0
            \end{array}\right.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - The input tensor. With real number data type.
        - **values** (Tensor) - The values to use where `x` is zero.
          It should be able to broadcast with `x` have the same dtype as `x`.

    Outputs:
        Tensor, has the same type as `x` and `values`.

    Raises:
        TypeError: If `x` or `values` is not Tensor.
        TypeError: If data type `x` and `values` is different.
        ValueError: If shape of two inputs are not broadcastable.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([-1.5, 0., 2.]))
        >>> values = Tensor(np.array([0.5]))
        >>> heaviside = ops.Heaviside()
        >>> y = heaviside(x, values)
        >>> print(y)
        [0.  0.5 1. ]
    """

    @prim_attr_register
    def __init__(self):
        self.init_prim_io_names(inputs=['x', 'values'], outputs=['y'])


class DivNoNan(Primitive):
    r"""
    Operates a safe division between `x1` and `x2` element-wise. Returns 0 if element of `x2` is zero.

    Inputs of `x1` and `x2` comply with the implicit type conversion rules to make the data types consistent.
    The inputs must be two tensors or one tensor and one scalar.
    When the inputs are two tensors,
    dtypes of them cannot be bool at the same time, and the shapes of them could be broadcast.
    When the inputs are one tensor and one scalar,
    the scalar could only be a constant.

    .. math::
        output_{i} = \begin{cases}
        0, & \text{ if } x2_{i} = 0\\
        x1_{i} / x2_{i}, & \text{ if } x2_{i} \ne 0
        \end{cases}

    Inputs:
        - **x1** (Union[Tensor, number.Number, bool]) - The first input is a number.Number or
          a bool or a tensor whose data type is
          `number <https://www.mindspore.cn/docs/en/master/api_python/mindspore.html#mindspore.dtype>`_ or
          `bool_ <https://www.mindspore.cn/docs/en/master/api_python/mindspore.html#mindspore.dtype>`_.
        - **x2** (Union[Tensor, number.Number, bool]) - The second input is a number.Number or
          a bool when the first input is a bool or a tensor whose data type is number or bool\_.
          When the first input is Scalar, the second input must be a Tensor whose data type is number or bool\_.

    Outputs:
        Tensor, the shape is the same as the one after broadcasting,
        and the data type is the one with higher precision or higher digits among the two inputs.

    Raises:
        TypeError: If `x1` and `x2` is not a number.Number or a bool or a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x1 = Tensor(np.array([-1.0, 0., 1.0, 5.0, 6.0]), mindspore.float32)
        >>> x2 = Tensor(np.array([0., 0., 0., 2.0, 3.0]), mindspore.float32)
        >>> div_no_nan = ops.DivNoNan()
        >>> output = div_no_nan(x1, x2)
        >>> print(output)
        [0.  0.  0.  2.5 2. ]
    """

    __mindspore_signature__ = (sig.sig_dtype.T, sig.sig_dtype.T)

    @prim_attr_register
    def __init__(self):
        """Initialize DivNoNan"""
        self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['y'])


class MulNoNan(_MathBinaryOp):
    r"""
    Computes `x` * `y` element-wise. If `y` is zero, no matter what `x` is, it will return 0.

    Inputs of `x` and `y` comply with the implicit type conversion rules to make the data types consistent.
    The inputs must be two tensors or one tensor and one scalar.
    When the inputs are two tensors, the shapes of them could be broadcasted.
    When the inputs are one tensor and one scalar, the scalar could only be a constant.

    .. math::
        output_{ij} = \begin{cases}
        0, & y_{ij} = 0;\\
        x_{ij} * y_{ij}, & otherwise.
        \end{cases}

    Note:
        The shapes of `x` and `y` should be the same or can be broadcasted.
        This is noncommutative: if `y` is NaN or infinite and `x` is 0, the result will be NaN.

    Inputs:
        - **x** (Union[Tensor]) - The first input is a tensor whose data type is one of
          int32, int64, float16, float32, float64, complex64, complex128 currently or scalar.
        - **y** (Union[Tensor]) - The second input is a tensor whose data type is one of
          int32, int64, float16, float32, float64, complex64, complex128 currently or scalar.

    Outputs:
        Tensor, the shape is the same as the shape after broadcasting,
        and the data type is the one with higher precision among the two inputs.

    Raises:
        TypeError: If neither `x` nor `y` is a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> # case 1 : same data type and shape of two inputs, there are some 0 in y.
        >>> x = Tensor(np.array([[-1.0, 6.0, np.inf], [np.nan, -7.0, 4.0]]), mindspore.float32)
        >>> y = Tensor(np.array([[-1.0, 4.0, 0], [0, -3.0, 1.0]]), mindspore.float32)
        >>> mul_no_nan = ops.MulNoNan()
        >>> output = mul_no_nan(x, y)
        >>> print(output)
        [[ 1. 24. 0.]
        [ 0. 21. 4.]]
        >>> # case 2 : the shape of two inputs is same, there are some 0 in x, y.
        >>> x = Tensor(np.array([[-1.0, 6.0, 0], [0, np.nan, 4.0]]), mindspore.float32)
        >>> y = Tensor(np.array([[-1.0, 4.0, np.inf], [np.nan, 0, 1.0]]), mindspore.float32)
        >>> output = mul_no_nan(x, y)
        >>> print(output)
        [[ 1. 24. nan]
         [nan  0. 4.]]
        >>> print(output.dtype)
        Float32
        >>> # case 3 : the y is a scalar.
        >>> x = Tensor(np.array([[-1.0, 6.0, 0], [0, np.nan, 4.0]]), mindspore.float32)
        >>> y = Tensor(0, mindspore.float32)
        >>> output = mul_no_nan(x, y)
        >>> print(output)
        [[0. 0. 0.]
         [0. 0. 0.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize _BinaryOp"""
        self.init_prim_io_names(inputs=['x', 'y'], outputs=['output'])


class TruncateDiv(Primitive):
    """
    Divides the first input tensor by the second input tensor element-wise and rounds the results
    of division towards zero. Equivalent to C-style integer division.

    Inputs of `x` and `y` comply with the implicit type conversion rules to make the data types consistent.
    When the inputs are two tensors,
    dtypes of them cannot be bool at the same time, and the shapes of them could be broadcast.
    When the inputs are one tensor and one scalar,
    the scalar could only be a constant.

    Note:
        Broadcasting is supported.

    Inputs:
        - **x** (Union[Tensor, Number, bool]) - The first input is a number, or a bool,
          or a tensor whose data type is number or bool.
        - **y** (Union[Tensor, Number, bool]) - The second input is a number, or a bool when the first input
          is a tensor, or a tensor whose data type is number or bool.

    Outputs:
        Tensor, the shape is the same as the one after broadcasting,
        and the data type is the one with higher precision or higher digits among the two inputs.

    Raises:
        TypeError: If `x` and `y` is not one of the following: Tensor, Number, bool.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([2, 4, -1]), mindspore.int32)
        >>> y = Tensor(np.array([3, 3, 3]), mindspore.int32)
        >>> truncate_div = ops.TruncateDiv()
        >>> output = truncate_div(x, y)
        >>> print(output)
        [0 1 0]
    """

    __mindspore_signature__ = (sig.sig_dtype.T, sig.sig_dtype.T)

    @prim_attr_register
    def __init__(self):
        """Initialize TruncateDiv."""
        self.init_prim_io_names(inputs=['x', 'y'], outputs=['output'])


class TruncateMod(Primitive):
    r"""
    Returns the remainder of division element-wise.

    Inputs of `x` and `y` comply with the implicit type conversion rules to make the data types consistent.
    When the inputs are two tensors,
    dtypes of them cannot be bool at the same time, and the shapes of them could be broadcast.
    When the inputs are one tensor and one scalar,
    the scalar could only be a constant.

    .. warning::
        - The input data does not support 0.
        - When the elements of input exceed 2048, the accuracy of operator cannot guarantee the requirement of
          double thousandths in the mini form.
        - Due to different architectures, the calculation results of this operator on NPU and CPU may be inconsistent.
        - If shape is expressed as (D1,D2... ,Dn), then D1\*D2... \*DN<=1000000,n<=8.

    Inputs:
        - **x** (Union[Tensor, numbers.Number, bool]) - The first input is a number, or a bool,
          or a tensor whose data type is number or bool.
        - **y** (Union[Tensor, numbers.Number, bool]) - The second input is a number, or a bool when the first input
          is a tensor, or a tensor whose data type is number or bool.

    Outputs:
        Tensor, the shape is the same as the one after broadcasting,
        and the data type is the one with higher precision among the two inputs.

    Raises:
        TypeError: If neither `x` nor `y` is one of the following: Tensor, number, bool.
        TypeError: If neither `x` nor `y` is a Tensor.
        ValueError: If the shape `x` and `y` cannot be broadcasted to each other.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([2, 4, -1]), mindspore.int32)
        >>> y = Tensor(np.array([3, 3, 3]), mindspore.int32)
        >>> truncate_mod = ops.TruncateMod()
        >>> output = truncate_mod(x, y)
        >>> print(output)
        [ 2  1 -1]
    """
    __mindspore_signature__ = (sig.sig_dtype.T, sig.sig_dtype.T)

    @prim_attr_register
    def __init__(self):
        """Initialize TruncateMod."""
        self.init_prim_io_names(inputs=['x', 'y'], outputs=['output'])


class Mod(_MathBinaryOp):
    r"""
    Computes the remainder of dividing the first input tensor by the second input tensor element-wise.

    Inputs of `x` and `y` comply with the implicit type conversion rules to make the data types consistent.
    The inputs must be two tensors or one tensor and one scalar. When the inputs are two tensors,
    both dtypes cannot be bool, and the shapes of them could be broadcast. When the inputs are one tensor
    and one scalar, the scalar could only be a constant.

    .. math::

        out_{i} = x_{i} \text{ % } y_{i}

    .. warning::
        - The input data does not support 0.
        - When the elements of input exceed 2048, the accuracy of operator cannot guarantee the requirement of
          double thousandths in the mini form.
        - Due to different architectures, the calculation results of this operator on NPU and CPU may be inconsistent.
        - If shape is expressed as :math:`(D1, D2, ..., Dn)`, then :math:`D1*D2... *DN<=1000000,n<=8`.

    Inputs:
        - **x** (Union[Tensor, numbers.Number, bool]) - The first input is a number, a bool
          or a tensor whose data type is number.
        - **y** (Union[Tensor, numbers.Number, bool]) - When the first input is a tensor, The second input
          could be a number, a bool or a tensor whose data type is number. When the first input is a number or a bool
          the second input must be a tensor whose data type is number.

    Outputs:
        Tensor, the shape is the same as the one after broadcasting,
        and the data type is the one with higher precision or higher digits among the two inputs.

    Raises:
        TypeError: If neither `x` nor `y` is one of the following: Tensor, number, bool.
        TypeError: If neither `x` nor `y` is a Tensor.
        ValueError: If the shape `x` and `y` cannot be broadcasted to each other.


    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([-4.0, 5.0, 6.0]), mindspore.float32)
        >>> y = Tensor(np.array([3.0, 2.0, 3.0]), mindspore.float32)
        >>> mod = ops.Mod()
        >>> output = mod(x, y)
        >>> print(output)
        [-1.  1.  0.]
    """

    def infer_value(self, x, y):
        if x is not None and y is not None:
            x = x.asnumpy()
            y = y.asnumpy()
            return Tensor(np.fmod(x, y))
        return None


class Xdivy(Primitive):
    """
    Divides the first input tensor by the second input tensor element-wise. Returns zero when `x` is zero.

    Inputs of `x` and `y` comply with the implicit type conversion rules to make the data types consistent.
    When the inputs are two tensors,
    dtypes of them cannot be bool at the same time, and the shapes of them could be broadcast.
    If one of the inputs is scalar, the scalar could only be a constant.

    Inputs:
        - **x** (Union[Tensor, Number, bool]) - The first input is a number, or a bool,
          or a tensor whose data type is float16, float32, float64, complex64, complex128 or bool.
        - **y** (Union[Tensor, Number, bool]) - The second input is a number,
          or a bool when the first input is a tensor, or a tensor whose data type is float16,
          float32, float64, complex64, complex128 or bool.

    Outputs:
        Tensor, the shape is the same as the one after broadcasting,
        and the data type is the one with higher precision or higher digits among the two inputs.

    Raises:
        TypeError: If `x` and `y` is not one of the following: Tensor, Number, bool.
        TypeError: If dtype of `x` and 'y' is not in [float16, float32, float64, complex64, complex128, bool].
        ValueError: If `x` could not be broadcast to a tensor with shape of `y`.
        RuntimeError: If the data type of `x`, `y` conversion of Parameter is given
                      but data type conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([2, 4, -1]), mindspore.float32)
        >>> y = Tensor(np.array([2, 2, 2]), mindspore.float32)
        >>> xdivy = ops.Xdivy()
        >>> output = xdivy(x, y)
        >>> print(output)
        [ 1.   2.  -0.5]
    """

    # Let x/y using same sig_dtype to enable implicit conversion for compatibility
    __mindspore_signature__ = (
        sig.make_sig('x', rw=sig.sig_rw.RW_READ, dtype=sig.sig_dtype.T),
        sig.make_sig('y', rw=sig.sig_rw.RW_READ, dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self):
        """Initialize Xdivy."""
        self.init_prim_io_names(inputs=['x', 'y'], outputs=['output'])

    def infer_shape(self, x_shape, y_shape):
        """
        Infer shape for output of Xdivy
        :param x_shape: input shape of x
        :param y_shape: input shape of y
        :return:
        """
        output_shape = get_broadcast_shape(x_shape, y_shape, self.name)
        return output_shape

    def infer_dtype(self, x_dtype, y_dtype):
        """
        Infer type for output of Xdivy
        :param x_dtype: input type of x
        :param y_dtype: input type of y
        :return:
        """
        args = {'x': x_dtype, 'y': y_dtype}
        validator.check_scalar_or_tensor_types_same(args,
                                                    [mstype.float16, mstype.float32, mstype.float64, mstype.complex64,
                                                     mstype.complex128], self.name, True)
        return x_dtype

    def infer_value(self, x, y):
        """
        Infer value for constant folding
        :param x:
        :param y:
        :return:
        """
        if x is not None and y is not None:
            x = x.asnumpy()
            y = y.asnumpy()
            out = x / y
            out = np.array(out, x.dtype)
            return Tensor(out)
        return None


class Xlogy(Primitive):
    r"""
    Computes the first input tensor multiplied by the logarithm of second input tensor element-wise.
    Returns zero when `x` is zero.

    Refer to :func:`mindspore.ops.xlogy` for more details.

    Inputs:
        - **x** (Union[Tensor, number.Number, bool]) - The first input is a number.Number or
          a bool or a tensor whose data type is
          `number <https://www.mindspore.cn/docs/en/master/api_python/mindspore.html#mindspore.dtype>`_ or
          `bool_ <https://www.mindspore.cn/docs/en/master/api_python/mindspore.html#mindspore.dtype>`_.
        - **y** (Union[Tensor, number.Number, bool]) - The second input is a number.Number or
          a bool when the first input is a tensor or a tensor whose data type is number or bool\_.
          When the first input is Scalar, the second input must be a Tensor whose data type is number or bool\_.

    Outputs:
        Tensor, the shape is the same as the one after broadcasting,
        and the data type is the one with higher precision or higher digits among the two inputs.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([-5, 0, 4]), mindspore.float32)
        >>> y = Tensor(np.array([2, 2, 2]), mindspore.float32)
        >>> xlogy = ops.Xlogy()
        >>> output = xlogy(x, y)
        >>> print(output)
        [-3.465736   0.        2.7725887]
    """
    __mindspore_signature__ = (sig.sig_dtype.T, sig.sig_dtype.T)

    @prim_attr_register
    def __init__(self):
        """Initialize Xlogy."""
        self.init_prim_io_names(inputs=['x', 'y'], outputs=['output'])


class _LogicBinaryOp(_BinaryOp):
    """
    Define logic binary operators.
    """

    @staticmethod
    def do_infer_dtype(x_dtype, y_dtype, valid_type=mstype.number_type, prim_name=None):
        """Staticmethod of infer dtype for _LogicBinaryOp."""
        args_dtype = {"x": x_dtype, "y": y_dtype}
        validator.check_tensors_dtypes_same_and_valid(args_dtype, valid_type, prim_name)
        return mstype.TensorType(mstype.bool_)

    def infer_dtype(self, x_dtype, y_dtype):
        return _LogicBinaryOp.do_infer_dtype(x_dtype, y_dtype, prim_name=self.name)


class Quantile(Primitive):
    r"""
    Computes the q-th quantiles of all elements in the input tensor, doing a linear interpolation when the
    q-th quantile lies between two data points.

    Refer to :func:`mindspore.ops.quantile` and :func:`mindspore.ops.nanquantile` for more details.

    Args:
        dim (int, optional): The dimension to reduce. By default, `axis` is ``None`` resulting in the
            input tensor being flattened before computation. Default: ``None`` .
        keep_dims (bool, optional): Whether the output tensor has dim retained or not. Default: ``False`` .
        ignore_nan (bool, optional): Whether to ignore NaN values in the input. Default: ``False`` .

    Inputs:
        - **input** (Tensor) - The shape of tensor is :math:`(x_1, x_2, ..., x_R)`.
          Supported dtypes: float32, float64.
        - **q** (Union[float, Tensor]) - A scalar or 1D tensor of quantile values in the range [0, 1].
          Supported dtypes: float32, float64.

    Outputs:
        Tensor, has the same dtype as the `input`.

    Supported Platforms:


    Examples:
        >>> quantile = ops.Quantile()
        >>> input = Tensor(np.array([0.0700, -0.5446,  0.9214]), mindspore.float32)
        >>> q = Tensor(np.array([0, 0.5, 1]), mindspore.float32)
        >>> output = quantile(input, q)
        >>> print(output)
        [-0.5446  0.07  0.9214]
    """

    @prim_attr_register
    def __init__(self, dim=None, keep_dims=False, ignore_nan=False):
        """Initialize Quantile"""
        if dim is not None:
            validator.check_value_type("dim", dim, [int], self.name)
        else:
            self.add_prim_attr("dim", 10000)
        if keep_dims is not None:
            validator.check_value_type("keep_dims", keep_dims, [bool], self.name)
        else:
            self.add_prim_attr("keep_dims", False)
        if ignore_nan is not None:
            validator.check_value_type("ignore_nan", ignore_nan, [bool], self.name)
        else:
            self.add_prim_attr("ignore_nan", False)


class ApproximateEqual(_LogicBinaryOp):
    r"""
    Returns ``True`` if abs(x-y) is smaller than tolerance element-wise, otherwise False.

    .. math::

        out_i = \begin{cases}
        & \text{ if } \left | x_{i} - y_{i} \right | < \text{tolerance},\ \ True  \\
        & \text{ if } \left | x_{i} - y_{i} \right | \ge \text{tolerance},\ \  False
        \end{cases}

    where `tolerance` indicates Acceptable maximum tolerance.

    Inputs of `x` and `y` comply with the implicit type conversion rules to make the data types consistent.
    If they have different data types, the lower precision data type will be converted to
    the relatively highest precision data type.

    Args:
        tolerance (float): The maximum deviation that two elements can be considered equal. Default: ``1e-05`` .

    Inputs:
        - **x** (Tensor) - A tensor. Must be one of the following types: float32, float16.
          :math:`(N,*)` where :math:`*` means, any number of additional dimensions, its rank should be less than 8.
        - **y** (Tensor) - A tensor of the same type and shape as `x`.

    Outputs:
        Tensor, the shape is the same as the shape of `x`, and the data type is bool.

    Raises:
        TypeError: If `tolerance` is not a float.
        TypeError: If the data type of `x`, `y` conversion of Parameter is given
                      but data type conversion of Parameter is not supported.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([1, 2, 3]), mindspore.float32)
        >>> y = Tensor(np.array([2, 3, 6]), mindspore.float32)
        >>> approximate_equal = ops.ApproximateEqual(2.)
        >>> output = approximate_equal(x, y)
        >>> print(output)
        [ True  True  False]
    """

    @prim_attr_register
    def __init__(self, tolerance=1e-05):
        """Initialize ApproximateEqual"""
        validator.check_value_type("tolerance", tolerance, [float], self.name)


class EqualCount(PrimitiveWithInfer):
    """
    Computes the number of the same elements of two tensors.

    The two input tensors must have the same data type and shape.

    Inputs:
        - **x** (Tensor) - The first input tensor. If the data type and shape of `y` are determined, then `x`
          must be the same as `y`, and vice versa.
          :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **y** (Tensor) - The second input tensor. If the data type and shape of `x` are determined, then `y`
          must be the same as `x`, and vice versa.

    Outputs:
        Tensor, with the type same as input tensor and shape as :math:`(1,)`.

    Raises:
        TypeError: If `x` or `y` is not a Tensor.
        ValueError: If shape of `x` is not equal to shape of `y`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([1, 2, 3]), mindspore.int32)
        >>> y = Tensor(np.array([1, 2, 4]), mindspore.int32)
        >>> equal_count = ops.EqualCount()
        >>> output = equal_count(x, y)
        >>> print(output)
        [2]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize EqualCount"""
        self.init_prim_io_names(inputs=['x', 'y'], outputs=['output'])


class Lerp(Primitive):
    """
    Does a linear interpolation of two tensors start and end based on a float or tensor weight.

    Refer to :func:`mindspore.ops.lerp` for more details.

    Inputs:
        - **start** (Tensor) - The tensor with the starting points. Data type must be float16, float32 or float64.
        - **end** (Tensor) - The tensor with the ending points. Data type must be the same as `start`.
        - **weight** (Union[float, Tensor]) - The weight for the interpolation formula. Must be a float
          or a scalar tensor with float16 or float32 data type.

    Outputs:
        Tensor, has the same type and shape as input `start`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> start = Tensor(np.array([1., 2., 3., 4.]), mindspore.float32)
        >>> end = Tensor(np.array([10., 10., 10., 10.]), mindspore.float32)
        >>> lerp = ops.Lerp()
        >>> output = lerp(start, end, 0.5)
        >>> print(output)
        [5.5 6. 6.5 7. ]
    """

    @prim_attr_register
    def __init__(self):
        self.init_prim_io_names(inputs=['start', 'end', 'weight'], outputs=['output'])


class IsNan(Primitive):
    r"""
    Determines which elements are NaN for each position.

    Refer to :func:`mindspore.ops.isnan` for more details.

    Inputs:
        - **x** (Tensor) - The input tensor.

    Outputs:
        Tensor, has the same shape of input, and the dtype is bool.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> is_nan = ops.IsNan()
        >>> x = Tensor(np.array([np.log(-1), 1, np.log(0)]), mindspore.float32)
        >>> output = is_nan(x)
        >>> print(output)
        [ True False False]
        >>> x = Tensor(2.1, mindspore.float64)
        >>> output = is_nan(x)
        >>> print(output)
        False
    """

    @prim_attr_register
    def __init__(self):
        """Initialize IsNan"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class IsInf(Primitive):
    r"""
    Determines which elements are inf or -inf for each position.

    Refer to :func:`mindspore.ops.isinf` for more details.

    Inputs:
        - **x** (Tensor) - The input tensor.

    Outputs:
        Tensor, has the same shape of input, and the dtype is bool.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> is_inf = ops.IsInf()
        >>> x = Tensor(np.array([np.log(-1), 1, np.log(0)]), mindspore.float32)
        >>> output = is_inf(x)
        >>> print(output)
        [False False True]
        >>> x = Tensor(2.1, mindspore.float64)
        >>> output = is_inf(x)
        >>> print(output)
        False
    """

    @prim_attr_register
    def __init__(self):
        """Initialize IsInf"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class IsFinite(Primitive):
    r"""
    Determines which elements are finite for each position.

    Refer to :func:`mindspore.ops.isfinite` for more details.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> is_finite = ops.IsFinite()
        >>> x = Tensor(np.array([np.log(-1), 1, np.log(0)]), mindspore.float32)
        >>> output = is_finite(x)
        >>> print(output)
        [False  True False]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize IsFinite"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class FloatStatus(Primitive):
    """
    Determines if the elements contain Not a Number(NaN), infinite or negative infinite. 0 for normal, 1 for overflow.

    Inputs:
        - **x** (Tensor) - The input tensor. The data type must be float16, float32 or float64.
          :math:`(N,*)` where :math:`*` means, any number of additional dimensions.

    Outputs:
        Tensor, has the shape of :math:`(1,)`, and the dtype is `mindspore.dtype.float32`.

    Raises:
        TypeError: If dtype of `x` is not in [float16, float32, float64].

    Supported Platforms:
        ``GPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> float_status = ops.FloatStatus()
        >>> x = Tensor(np.array([np.log(-1), 1, np.log(0)]), mindspore.float32)
        >>> result = float_status(x)
        >>> print(result)
        [1.]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize FloatStatus"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class NPUAllocFloatStatus(Primitive):
    """
    Allocates a flag to store the overflow status.

    The flag is a tensor whose shape is :math:`(8,)` and data type is `mindspore.dtype.float32`.

    Note:
        Please refer to the Examples of :class:`mindspore.ops.NPUGetFloatStatus`.

    Outputs:
        Tensor, has the shape of :math:`(8,)`.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> from mindspore import ops
        >>> alloc_status = ops.NPUAllocFloatStatus()
        >>> output = alloc_status()
        >>> print(output)
        [0. 0. 0. 0. 0. 0. 0. 0.]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize NPUAllocFloatStatus"""
        logger.warning("The 'NPUAllocFloatStatus' operator will be deprecated in the future, "
                       "please use 'nn.TrainOneStepWithLossScaleCell' or 'amp.all_finite'.")


class NPUGetFloatStatus(Primitive):
    """
    `mindspore.ops.NPUGetFloatStatus` updates the flag which is
    the output tensor of :class:`mindspore.ops.NPUAllocFloatStatus` with the latest overflow status.


    Note:
        The flag is a tensor whose shape is :math:`(8,)` and data type is `mindspore.dtype.float32`.
        If the sum of the flag equals to 0, there is no overflow happened. If the sum of the
        flag is bigger than 0, there is overflow happened.
        In addition, there are strict sequencing requirements for use, i.e., before
        using the NPUGetFloatStatus operator, need to ensure that the NPUClearFlotStatus
        and your compute has been executed. We use :class:`mindspore.ops.Depend` to ensure the execution order.

    Inputs:
        - **x** (Tensor) - The output tensor of `NPUAllocFloatStatus`.
          The data type must be float16 or float32.
          :math:`(N,*)` where :math:`*` means, any number of additional dimensions, its rank should be less than 8.

    Outputs:
        Tensor, has the same shape as `x`. All the elements in the tensor will be zero.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If dtype of `x` is neither float16 nor float32.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import numpy as np
        >>> import mindspore.nn as nn
        >>> from mindspore import ops
        >>> from mindspore import dtype as mstype
        >>> from mindspore.common.tensor import Tensor
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super().__init__()
        ...         self.alloc_status = ops.NPUAllocFloatStatus()
        ...         self.get_status = ops.NPUGetFloatStatus()
        ...         self.clear_status = ops.NPUClearFloatStatus()
        ...         self.sub = ops.Sub()
        ...         self.neg = ops.Neg()
        ...
        ...     def construct(self, x):
        ...         init = self.alloc_status()
        ...         clear_status = self.clear_status(init)
        ...         x = ops.depend(x, clear_status)
        ...         res = self.sub(x, self.neg(x))
        ...         init = ops.depend(init, res)
        ...         get_status = self.get_status(init)
        ...         res = ops.depend(res, get_status)
        ...         return res
        >>>
        >>> value = 5
        >>> data = np.full((2, 3), value, dtype=np.float16)
        >>> x = Tensor(data, dtype=mstype.float16)
        >>> net = Net()
        >>> res = net(x)
        >>> print(res)
        [[10. 10. 10.]
         [10. 10. 10.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize NPUGetFloatStatus"""
        logger.warning("The 'NPUGetFloatStatus' operator will be deprecated in the future, "
                       "please use 'nn.TrainOneStepWithLossScaleCell' or 'amp.all_finite'.")


class NPUClearFloatStatus(Primitive):
    """
    Clears the flag which stores the overflow status.

    Note:
        The flag is in the register on the `Ascend` device. It will be reset and can not be reused again after the
        `NPUClearFloatStatus` is called.
        In addition, there are strict sequencing requirements for use, i.e., before using the NPUGetFloatStatus
        operator, need to ensure that the NPUClearFlotStatus and your compute has been executed.
        We use :class:`mindspore.ops.Depend` on ensure the execution order.

        Please refer to the Examples of :class:`mindspore.ops.NPUGetFloatStatus`.

    Inputs:
        - **x** (Tensor) - The output tensor of `NPUAllocFloatStatus`.
          The data type must be float16 or float32.

    Outputs:
        Tensor, has the same shape as `x`. All the elements in the tensor will be zero.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import numpy as np
        >>> import mindspore.nn as nn
        >>> from mindspore import ops
        >>> from mindspore import dtype as mstype
        >>> from mindspore.common.tensor import Tensor
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super().__init__()
        ...         self.alloc_status = ops.NPUAllocFloatStatus()
        ...         self.get_status = ops.NPUGetFloatStatus()
        ...         self.clear_status = ops.NPUClearFloatStatus()
        ...         self.sub = ops.Sub()
        ...         self.neg = ops.Neg()
        ...
        ...     def construct(self, x):
        ...         init = self.alloc_status()
        ...         clear_status = self.clear_status(init)
        ...         x = ops.depend(x, clear_status)
        ...         res = self.sub(x, self.neg(x))
        ...         init = ops.depend(init, res)
        ...         get_status = self.get_status(init)
        ...         res = ops.depend(res, get_status)
        ...         return res
        >>>
        >>> value = 5
        >>> data = np.full((2, 3), value, dtype=np.float16)
        >>> x = Tensor(data, dtype=mstype.float16)
        >>> net = Net()
        >>> res = net(x)
        >>> print(res)
        [[10. 10. 10.]
         [10. 10. 10.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize NPUClearFloatStatus"""
        logger.warning("The 'NPUClearFloatStatus' operator will be deprecated in the future,"
                       "please use 'nn.TrainOneStepWithLossScaleCell' or 'amp.all_finite'.")


class NPUGetFloatStatusV2(Primitive):
    """
    Get the flag for storage overflow status. This flag is located in a register at a
    fixed address on the `Ascend` device, and overflow information is automatically
    written to this register.
    The flag is a one-dimensional Tensor with shape :math:`(8,)` and data type `mindspore.dtype.int32`.
    If the value of flag is zero, no overflow has occurred, otherwise, overflow.
    When performing overflow detection on the network, you should first call `NPUClearFloatStatusV2` to
    reset the register before the detection, and then call `NPUGetFloatStatusV2` to get the register
    status after the network execution is completed.

    Note:
        - In order to avoid mis-optimization by the compiler, additional input is added to
          this operator. The input is defined as a shape of: math:`(8,)` and data type of
          `mindspore.dtype.int32` Tensor, meaningless.
        - Since this op lacks contextual dependencies with parameters in the network,
          :class:`mindspore.ops.Depend` needs to be used to ensure order of execution.

    Inputs:
        Tensor, an additional input created to avoid compiler optimization, is specified as shape :math:`(8,)`,
        data type is `mindspore.dtype.int32`, and has no actual meaning.
        Usually use the output of `NPUClearFloatStatusV2`.

    Outputs:
        Tensor, shape and data type are the same as input. If all are zero, it means no overflow, otherwise, overflow.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If dtype of `x` is not int32.
        ValueError: If shape of `x` is not equal to :math:`(8,)`.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import mindspore as ms
        >>> import numpy as np
        >>> from mindspore import ops, nn, Tensor
        >>> from mindspore.ops.operations.math_ops import NPUGetFloatStatusV2, NPUClearFloatStatusV2
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super().__init__()
        ...         self.clear_status = NPUClearFloatStatusV2()
        ...         self.get_status = NPUGetFloatStatusV2()
        ...         self.sub = ops.Sub()
        ...         self.neg = ops.Neg()
        ...         self.equal = ops.Equal()
        ...         self.reduce_all = ops.ReduceAll(keep_dims=False)
        ...         self.base = Tensor([0], dtype=ms.int32)
        ...         self.logic_not = ops.LogicalNot()
        ...
        ...     def construct(self, x):
        ...         init = Tensor([0]*8, dtype=ms.int32)
        ...         clear_status = self.clear_status(init)
        ...         x = ops.depend(x, clear_status)
        ...         res = self.sub(x, self.neg(x))
        ...         init = ops.depend(init, res)
        ...         get_status = self.get_status(init)
        ...         flag = self.equal(self.base, get_status)
        ...         overall_finite = self.reduce_all(flag)
        ...         overflow = self.logic_not(overall_finite)
        ...         return overflow
        ...
        >>> value = 65504
        >>> data = np.full((2, 3), value, dtype=np.float16)
        >>> x = Tensor(data, dtype=ms.float16)
        >>> net = Net()
        >>> res = net(x)
        >>> print(res)
        True
        >>> value = 10
        >>> data = np.full((2, 3), value, dtype=np.float16)
        >>> x = Tensor(data, dtype=ms.float16)
        >>> net = Net()
        >>> res = net(x)
        >>> print(res)
        False
    """

    @prim_attr_register
    def __init__(self):
        """Initialize NPUGetFloatStatusV2"""



class NPUClearFloatStatusV2(Primitive):
    """
    Clear the flag for storage overflow status. This flag is located in a register at a
    fixed address on the `Ascend` device, and overflow information is automatically
    written to this register.
    The flag is a one-dimensional Tensor with shape :math:`(8,)` and data type `mindspore.dtype.int32`.
    If the value of flag is zero, no overflow has occurred, otherwise, overflow.
    When performing overflow detection on the network, you should first call `NPUClearFloatStatusV2` to
    reset the register before the detection, and then call `NPUGetFloatStatusV2` to get the register
    status after the network execution is completed.

    Note:
        - In order to avoid mis-optimization by the compiler, additional input and output are added to
          this operator. The input and output are defined as a shape of: math:`(8,)` and data type of
          `mindspore.dtype.int32` Tensor, meaningless.
        - Since this op lacks contextual dependencies with parameters in the network,
          :class:`mindspore.ops.Depend` needs to be used to ensure order of execution.

    Inputs:
        Tensor, an additional input created to avoid compiler optimization, is specified as shape :math:`(8,)`,
        data type is `mindspore.dtype.int32`, and has no actual meaning.

    Outputs:
        Tensor, shape and data type are the same as input, meaningless.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If dtype of `x` is not int32.
        ValueError: If shape of `x` is not equal to :math:`(8,)`.

    Supported Platforms:
        ``Ascend``

    Examples:
        >>> import mindspore as ms
        >>> import numpy as np
        >>> from mindspore import ops, nn, Tensor
        >>> from mindspore.ops.operations.math_ops import NPUGetFloatStatusV2, NPUClearFloatStatusV2
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super().__init__()
        ...         self.clear_status = NPUClearFloatStatusV2()
        ...         self.get_status = NPUGetFloatStatusV2()
        ...         self.sub = ops.Sub()
        ...         self.neg = ops.Neg()
        ...         self.equal = ops.Equal()
        ...         self.reduce_all = ops.ReduceAll(keep_dims=False)
        ...         self.base = Tensor([0], dtype=ms.int32)
        ...         self.logic_not = ops.LogicalNot()
        ...
        ...     def construct(self, x):
        ...         init = Tensor([0]*8, dtype=ms.int32)
        ...         clear_status = self.clear_status(init)
        ...         x = ops.depend(x, clear_status)
        ...         res = self.sub(x, self.neg(x))
        ...         init = ops.depend(init, res)
        ...         get_status = self.get_status(init)
        ...         flag = self.equal(self.base, get_status)
        ...         overall_finite = self.reduce_all(flag)
        ...         overflow = self.logic_not(overall_finite)
        ...         return overflow
        ...
        >>> value = 65504
        >>> data = np.full((2, 3), value, dtype=np.float16)
        >>> x = Tensor(data, dtype=ms.float16)
        >>> net = Net()
        >>> res = net(x)
        >>> print(res)
        True
        >>> value = 10
        >>> data = np.full((2, 3), value, dtype=np.float16)
        >>> x = Tensor(data, dtype=ms.float16)
        >>> net = Net()
        >>> res = net(x)
        >>> print(res)
        False
    """

    @prim_attr_register
    def __init__(self):
        """Initialize NPUClearFloatStatusV2"""


class NMSWithMask(PrimitiveWithInfer):
    r"""
    Non-maximum Suppression. When object detection problem is performed in the computer vision field,
    object detection algorithm generates
    a plurality of bounding boxes. Use the box with the highest score, calculate the overlap between other boxes and
    the current box, and delete the box based on a certain threshold(IOU). On Ascend platform, the input box score is
    ignored, which only selects boexs based on the IOU between boxes, which means if you want to remove boxes that has
    lower scores, you need to sort the input boxes by score in descending order in advance. The IOU is as follows:

    .. math::
        \text{IOU} = \frac{\text{Area of Overlap}}{\text{Area of Union}}

    .. warning::
        Only supports up to 2864 input boxes at one time.

    Args:
        iou_threshold (float): Specifies the threshold of overlap boxes with respect to
            IOU. Default: ``0.5`` .

    Inputs:
        - **bboxes** (Tensor) - The shape of tensor is :math:`(N, 5)`. Input bounding boxes.
          `N` is the number of input bounding boxes. Every bounding box
          contains 5 values, the first 4 values are the coordinates(x0, y0, x1, y1) of bounding box which
          represents the point of top-left and bottom-right, and the last value is the score of this bounding box.
          The data type must be float16 or float32.

    Outputs:
        tuple[Tensor], tuple of three tensors, they are output_boxes, output_idx and selected_mask.

        - **output_boxes** (Tensor) - The shape of tensor is :math:`(N, 5)`. On GPU and CPU platform, it is a sorted
          list of bounding boxes by sorting the input `bboxes` in descending order of score. On Ascend platform,
          it is same as input `bboxes`.
        - **output_idx** (Tensor) - The shape of tensor is :math:`(N,)`. The indexes list of `output_boxes`.
        - **selected_mask** (Tensor) - The shape of tensor is :math:`(N,)`. A mask list of
          valid output bounding boxes. Apply this mask on `output_boxes` to get the list of bounding boxes after
          non-max suppression calculation, or apply this mask on `output_idx` to get the indexes list of bounding boxes
          after non-max suppression calculation.

    Raises:
        ValueError: If the `iou_threshold` is not a float number.
        ValueError:  if the first dimension of input Tensor is less than or equal to 0.
        TypeError: if the dtype of the `bboxes` is not float16 or float32.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bbox = np.array([[100.0, 100.0, 50.0, 68.0, 0.63], [150.0, 75.0, 165.0, 115.0, 0.55],
        ...                  [12.0, 190.0, 288.0, 200.0, 0.9], [28.0, 130.0, 106.0, 172.0, 0.3]])
        >>> bbox[:, 2] += bbox[:, 0]
        >>> bbox[:, 3] += bbox[:, 1]
        >>> inputs = Tensor(bbox, mindspore.float32)
        >>> nms = ops.NMSWithMask(0.1)
        >>> output_boxes, indices, mask = nms(inputs)
        >>> indices_np = indices.asnumpy()
        >>> print(indices_np[mask.asnumpy()])
        [0 1 2]
    """

    @prim_attr_register
    def __init__(self, iou_threshold=0.5):
        """Initialize NMSWithMask"""
        validator.check_value_type("iou_threshold", iou_threshold, [float], self.name)
        self.init_prim_io_names(inputs=['bboxes'], outputs=['selected_boxes', 'selected_idx', 'selected_mask'])

    def infer_shape(self, bboxes_shape):
        cls_name = self.name
        validator.check_equal_int(len(bboxes_shape), 2, "bboxes rank", cls_name)
        if bboxes_shape[0] != -1:
            validator.check_positive_int(bboxes_shape[0], "bboxes.shape[0]", cls_name)
        validator.check_equal_int(bboxes_shape[1], 5, "bboxes.shape[1]", cls_name)
        num = bboxes_shape[0]
        return bboxes_shape, (num,), (num,)

    def infer_dtype(self, bboxes_dtype):
        validator.check_tensor_dtype_valid("bboxes", bboxes_dtype, [mstype.float16, mstype.float32], self.name)
        return bboxes_dtype, mstype.int32, mstype.bool_


class Sign(Primitive):
    r"""
    Performs sign on the tensor element-wise.

    .. math::
        sign(x) = \begin{cases} -1, &if\ x < 0 \cr
        0, &if\ x = 0 \cr
        1, &if\ x > 0\end{cases}

    Inputs:
        - **x** (Tensor) - The input tensor of any dimension.

    Outputs:
        Tensor, has the same shape and dtype as the `x`.

    Raises:
        TypeError: If `x` is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
         >>> import mindspore
         >>> import numpy as np
         >>> from mindspore import Tensor, ops
         >>> x = Tensor(np.array([[2.0, 0.0, -1.0]]), mindspore.float32)
         >>> sign = ops.Sign()
         >>> output = sign(x)
         >>> print(output)
         [[ 1.  0. -1.]]
    """

    @prim_attr_register
    def __init__(self):
        pass


class Tan(Primitive):
    r"""
    Computes tangent of `x` element-wise.

    Refer to :func:`mindspore.ops.tan` for more details.

    Inputs:
        - **x** (Tensor) - Input tensor of any dimension.

    Outputs:
        Tensor, has the same shape as `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> tan = ops.Tan()
        >>> x = Tensor(np.array([-1.0, 0.0, 1.0]), mindspore.float32)
        >>> output = tan(x)
        >>> print(output)
        [-1.5574081 0. 1.5574081]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Tan"""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])


class SquareSumAll(Primitive):
    r"""
    Returns the square sum of a tensor element-wise.

    .. math::
        \left\{\begin{matrix}out_{x} = {\textstyle \sum_{0}^{N}} (x_{i})^2
        \\out_{y} = {\textstyle \sum_{0}^{N}} (y_{i})^2
        \end{matrix}\right.

    Note:
        SquareSumAll only supports float16 and float32 data type.

    Inputs:
        - **x** (Tensor) - The input tensor. The data type must be float16 or float32.
          :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **y** (Tensor) - The input tensor has the same type and shape as the `x`.

    Outputs:
        - **output_x** (Tensor) - The same type as the `x`.
        - **output_y** (Tensor) - The same type as the `x`.

    Raises:
        TypeError: If neither `x` nor `y` is a Tensor.
        ValueError: If `x` and `y` are not the same shape.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore
        >>> import mindspore.ops as ops
        >>> from mindspore import Tensor
        >>> x = Tensor(np.array([0, 0, 2, 0]), mindspore.float32)
        >>> y = Tensor(np.array([0, 0, 2, 4]), mindspore.float32)
        >>> square_sum_all = ops.SquareSumAll()
        >>> output = square_sum_all(x, y)
        >>> print(output)
        (Tensor(shape=[], dtype=Float32, value= 4),
         Tensor(shape=[], dtype=Float32, value= 20))
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SquareSumAll"""
        self.init_prim_io_names(inputs=['x', 'y'], outputs=['output_x', 'output_y'])


class BitwiseAnd(_BitwiseBinaryOp):
    r"""
    Returns bitwise `and` of two tensors element-wise.

    Refer to :func:`mindspore.ops.bitwise_and` for more details.

    Inputs:
        - **x** (Tensor) - The first input tensor with shape
          :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **y** (Tensor) - The second input tensor with same type as the `x`.

    Outputs:
        Tensor, has the same type as the `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([0, 0, 1, -1, 1, 1, 1]), mindspore.int16)
        >>> y = Tensor(np.array([0, 1, 1, -1, -1, 2, 3]), mindspore.int16)
        >>> bitwise_and = ops.BitwiseAnd()
        >>> output = bitwise_and(x, y)
        >>> print(output)
        [ 0  0  1 -1  1  0  1]
    """


class BitwiseOr(_BitwiseBinaryOp):
    r"""
    Returns bitwise `or` of two tensors element-wise.

    Refer to :func:`mindspore.ops.bitwise_or` for more details.

    Inputs:
        - **x** (Tensor) - The first input tensor with shape
          :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **y** (Tensor) - The second input tensor with same type as the `x`.

    Outputs:
        Tensor, has the same type as the `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([0, 0, 1, -1, 1, 1, 1]), mindspore.int16)
        >>> y = Tensor(np.array([0, 1, 1, -1, -1, 2, 3]), mindspore.int16)
        >>> bitwise_or = ops.BitwiseOr()
        >>> output = bitwise_or(x, y)
        >>> print(output)
        [ 0  1  1 -1 -1  3  3]
    """


class BitwiseXor(_BitwiseBinaryOp):
    r"""
    Returns bitwise `xor` of two tensors element-wise.

    Refer to :func:`mindspore.ops.bitwise_xor` for more details.

    Inputs:
        - **x** (Tensor) - The first input tensor with shape
          :math:`(N, *)` where :math:`*` means, any number of additional dimensions.
        - **y** (Tensor) - The second input tensor with same type as the `x`.

    Outputs:
        Tensor, has the same type as the `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([0, 0, 1, -1, 1, 1, 1]), mindspore.int16)
        >>> y = Tensor(np.array([0, 1, 1, -1, -1, 2, 3]), mindspore.int16)
        >>> bitwise_xor = ops.BitwiseXor()
        >>> output = bitwise_xor(x, y)
        >>> print(output)
        [ 0  1  0  0 -2  3  2]
    """


class BesselI0(Primitive):
    r"""
    Computes modified Bessel function of the first kind, order 0 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            I_{0}(x)=J_{0}(\mathrm{i} x)=\sum_{m=0}^{\infty}
            \frac{x^{2 m}}{2^{2 m} (m !)^{2}}
        \end{array}

    where :math:`J_{0}` is Bessel function of the first kind, order 0.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.bessel_i0` for more details.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16, float32 or float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_i0 = ops.BesselI0()
        >>> x = Tensor(np.array([0.24, 0.83, 0.31, 0.09]), mindspore.float32)
        >>> output = bessel_i0(x)
        >>> print(output)
        [1.0144521 1.1797839 1.0241698 1.0020262]
    """

    @prim_attr_register
    def __init__(self):
        self.init_prim_io_names(inputs=['x'], outputs='y')


class BesselI1(Primitive):
    r"""
    Computes modified Bessel function of the first kind, order 1 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            I_{1}(x)=\mathrm{i}^{-1} J_{1}(\mathrm{i} x)=\sum_{m=0}^
            {\infty} \frac{x^{2m+1}}{2^{2m+1} m ! (m+1) !}
        \end{array}

    where :math:`J_{1}` is Bessel function of the first kind, order 1.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.bessel_i1` for more details.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16, float32 or float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_i1 = ops.BesselI1()
        >>> x = Tensor(np.array([0.24, 0.83, 0.31, 0.09]), mindspore.float32)
        >>> output = bessel_i1(x)
        >>> print(output)
        [0.1208661  0.45177728 0.1568694  0.04504559]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize BesselI1"""


class BesselI0e(Primitive):
    r"""
    Computes exponential scaled modified Bessel function of the first kind, order 0 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            \text I_{0}e(x)=e^{(-|x|)} * I_{0}(x)=e^{(-|x|)} * \sum_{m=0}^
            {\infty} \frac{x^{2 m}}{2^{2 m} (m !)^{2}}
        \end{array}

    where :math:`I_{0}` is modified Bessel function of the first kind, order 0.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16, float32 or float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If dtype of `x` is not float16, float32 or float64.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_i0e = ops.BesselI0e()
        >>> x = Tensor(np.array([0.24, 0.83, 0.31, 0.09]), mindspore.float32)
        >>> output = bessel_i0e(x)
        >>> print(output)
        [0.7979961  0.5144438  0.75117415  0.9157829 ]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize BesselI0e"""
        self.init_prim_io_names(inputs=['x'], outputs='output')


class BesselI1e(Primitive):
    r"""
    Computes exponential scaled modified Bessel function of the first kind, order 1 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            \text I_{1}e(x)=e^{(-|x|)} * I_{1}(x)=e^{(-|x|)} * \sum_{m=0}^
            {\infty} \frac{x^{2m+1}}{2^{2m+1} m ! (m+1) !}
        \end{array}

    where :math:`I_{1}` is  modified Bessel function of the first kind, order 1.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16 or float32, float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If dtype of `x` is not float16, float32 or float64.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_i1e = ops.BesselI1e()
        >>> x = Tensor(np.array([0.24, 0.83, 0.31, 0.09]), mindspore.float32)
        >>> output = bessel_i1e(x)
        >>> print(output)
        [0.09507662 0.19699717 0.11505538 0.04116856]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize BesselI1e"""
        self.init_prim_io_names(inputs=['x'], outputs='output')


class BesselK0(Primitive):
    r"""
    Computes modified Bessel function of the second kind, order 0 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            K_{0}(x)= \lim_{\nu \to 0} \left(\frac{\pi}{2}\right) \frac
            {I_{-\nu}(x)-I_{\nu}(x)}{\sin (\nu \pi)} = \int_{0}^{\infty} e^{-x \cosh t} d t
        \end{array}

    where :math:`I_{0}` is modified Bessel function of the first kind, order 0.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16, float32, float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Raises:
        TypeError: If `x` is not a Tensor of float16, float32, float64.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_k0 = ops.BesselK0()
        >>> x = Tensor(np.array([0.24, 0.83, 0.31, 0.09]), mindspore.float32)
        >>> output = bessel_k0(x)
        >>> print(output)
        [1.579826  0.5402144 1.3424659 2.5310173]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize BesselK0"""


class BesselK1(Primitive):
    r"""
    Computes modified Bessel function of the second kind, order 1 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            K_{1}(x)=\lim_{\nu \to 1} \left(\frac{\pi}{2}\right) \frac{I_{-\nu}(x)-
            I_{\nu}(x)}{\sin (\nu \pi)} = \int_{0}^{\infty} e^{-x \cosh t} \cosh (t) d t
        \end{array}

    where :math:`I_{1}` is modified Bessel function of the first kind, order 1.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16, float32, float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Raises:
        TypeError: If `x` is not a Tensor of float16, float32, float64.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_k1 = ops.BesselK1()
        >>> x = Tensor(np.array([0.24, 0.83, 0.31, 0.09]), mindspore.float32)
        >>> output = bessel_k1(x)
        >>> print(output)
        [3.9190812  0.8143549  2.9440577 10.974864]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize BesselK1"""


class BesselK0e(Primitive):
    r"""
    Computes exponential scaled modified Bessel function of the second kind, order 0 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            K_{0}e(x)= e^{(-|x|)} * K_{0}(x) = e^{(-|x|)} * \int_{0}^
            {\infty} e^{-x \cosh t} d t
        \end{array}

    where :math:`K_{0}` is modified Bessel function of the second kind, order 0.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16, float32, float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Raises:
        TypeError: If `x` is not a Tensor of float16, float32, float64.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_k0e = ops.BesselK0e()
        >>> x = Tensor(np.array([0.24, 0.83, 0.31, 0.09]), mindspore.float32)
        >>> output = bessel_k0e(x)
        >>> print(output)
        [2.0083523 1.2388839 1.8303517 2.769374 ]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize BesselK0e"""


class BesselK1e(Primitive):
    r"""
    Computes exponential scaled modified Bessel function of the second kind, order 1 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            K_{1}e(x)= e^{(-|x|)} * K_{1}(x) = e^{(-|x|)} * \int_{0}
            ^{\infty} e^{-x \cosh t} \cosh (t) d t
        \end{array}

    where :math:`K_{1}` is modified Bessel function of the second kind, order 1.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16, float32, float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Raises:
        TypeError: If `x` is not a Tensor of float16, float32, float64.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_k1e = ops.BesselK1e()
        >>> x = Tensor(np.array([0.24, 0.83, 0.31, 0.09]), mindspore.float32)
        >>> output = bessel_k1e(x)
        >>> print(output)
        [ 4.9821286  1.8675754  4.0140023 12.008413 ]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize BesselK1e"""


class BesselJ0(Primitive):
    r"""
    Computes Bessel function of the first kind, order 0 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            J_{0}(x) = \frac{1}{\pi} \int_{0}^{\pi} \cos (x \sin \theta) d \theta
            =\sum_{m=0}^{\infty} \frac{(-1)^{m} x^{2 m}}{2^{2 m} (m !)^2}
        \end{array}

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16, float32 or float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Raises:
        TypeError: If `x` is not a Tensor of float16, float32 or float64.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_j0 = ops.BesselJ0()
        >>> x = Tensor(np.array([0.5, 1., 2., 4.]), mindspore.float32)
        >>> output = bessel_j0(x)
        >>> print(output)
        [0.93846981  0.76519769  0.22389078  -0.39714981]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize BesselJ0"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class BesselJ1(Primitive):
    r"""
    Computes Bessel function of the first kind, order 1 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            J_{1}(x) = \frac{1}{\pi} \int_{0}^{\pi} \cos (x \sin \theta- \theta) d \theta
            =\sum_{m=0}^{\infty} \frac{(-1)^{m} x^{2 m+1}}{2^{2 m+1} m !(m+1) !}
        \end{array}

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16, float32 or float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Raises:
        TypeError: If `x` is not a Tensor of float16, float32 or float64.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_j1 = ops.BesselJ1()
        >>> x = Tensor(np.array([0.5, 1., 2., 4.]), mindspore.float32)
        >>> output = bessel_j1(x)
        >>> print(output)
        [0.24226846  0.44005059  0.57672481  -0.06604333]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize BesselJ1"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class BesselY0(Primitive):
    r"""
    Computes Bessel function of the second kind, order 0 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            Y_{0}(x)=\lim_{n \to 0} \frac{J_{n}(x) \cos n \pi-J_{-n}(x)}{\sin n \pi}
        \end{array}

    where :math:`J_{0}` is Bessel function of the first kind, order 0.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16, float32 or float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Raises:
        TypeError: If `x` is not a Tensor of float16, float32, float64.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_y0 = ops.BesselY0()
        >>> x = Tensor(np.array([0.5, 1., 2., 4.]), mindspore.float32)
        >>> output = bessel_y0(x)
        >>> print(output)
        [-0.44451873  0.08825696  0.51037567  -0.01694074]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize BesselY0"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class BesselY1(Primitive):
    r"""
    Computes Bessel function of the second kind, order 1 element-wise.

    The formula is defined as:

    .. math::
        \begin{array}{ll} \\
            Y_{1}(x)=\lim_{n \to 1} \frac{J_{n}(x) \cos n \pi-J_{-n}(x)}{\sin n \pi}
        \end{array}

    where :math:`J_{1}` is Bessel function of the first kind, order 1.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - The input tensor.
          Data type must be float16, float32 or float64.

    Outputs:
        Tensor, has the same shape as `x`.

    Raises:
        TypeError: If `x` is not a Tensor of float16, float32, float64.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> bessel_y1 = ops.BesselY1()
        >>> x = Tensor(np.array([0.5, 1., 2., 4.]), mindspore.float32)
        >>> output = bessel_y1(x)
        >>> print(output)
        [-1.47147239  -0.78121282  -0.10703243  0.39792571]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize BesselY1"""
        self.init_prim_io_names(inputs=['x'], outputs=['output'])


class Inv(Primitive):
    r"""
    Computes Reciprocal of input tensor element-wise.

    Refer to :func:`mindspore.ops.inv` for more details.

    Inputs:
        - **x** (Tensor) - Input tensor, it must be one of the following types: float16, float32 or int32.

    Outputs:
        Tensor, has the same shape and data type as `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> inv = ops.Inv()
        >>> x = Tensor(np.array([0.25, 0.4, 0.31, 0.52]), mindspore.float32)
        >>> output = inv(x)
        >>> print(output)
        [4.        2.5       3.2258065 1.923077 ]
    """

    @prim_attr_register
    def __init__(self):
        pass


class Invert(Primitive):
    r"""
    Flips all bits of input tensor element-wise.

    Refer to :func:`mindspore.ops.invert` for more details.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> invert = ops.Invert()
        >>> x = Tensor(np.array([25, 4, 13, 9]), mindspore.int16)
        >>> output = invert(x)
        >>> print(output)
        [-26 -5 -14 -10]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Invert"""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])


class Eps(Primitive):
    """
    Create a Tensor with the same data type and shape as input, and the element value is the minimum value that the
    corresponding data type can express.

    Refer to :func:`mindspore.ops.eps` for more detail.

    Inputs:
        - **x** (Tensor) - Tensor of any dimension used to obtain the minimum value that its data type can express.
          The data type must be float16, float32 or float64.

    Outputs:
        Tensor, has the same type and shape as `x`, but filled with `x` dtype minimum val.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If data type of `x` is neither float16, float32, nor float64.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> x = Tensor([4, 1, 2, 3], mindspore.float32)
        >>> output = ops.Eps()(x)
        >>> print(output)
        [1.1920929e-07 1.1920929e-07 1.1920929e-07 1.1920929e-07]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Eps"""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])


class MatrixInverse(Primitive):
    """
    Returns the inverse of the input matrix. If the matrix is irreversible, an error may be reported or an unknown
    result may be returned.

    Note:
        The parameter 'adjoint' is only supporting ``False``  right now, because complex number is not supported at
        present.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        adjoint (bool) : An optional bool. Default: ``False`` .

    Inputs:
        - **x** (Tensor) - A matrix to be calculated. The matrix must be at least two dimensions, and the last two
          dimensions must be the same size.

    Outputs:
        Tensor, has the same type and shape as input `x`.

    Raises:
        TypeError: If `adjoint` is not a bool.
        TypeError: If `x` is not a Tensor.
        ValueError: If the last two dimensions of `x` is not same size.
        ValueError: If the dimension of `x` is less than 2.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([[[-0.710504  , -1.1207525],
        ...                       [-1.7651395 , -1.7576632]],
        ...                      [[ 0.52412605,  1.9070215],
        ...                       [ 1.3384849 ,  1.4274558]]]), mindspore.float32)
        >>> matrix_inverse = ops.MatrixInverse(adjoint=False)
        >>> output = matrix_inverse(x)
        >>> print(output)
        [[[ 2.4095478  -1.5364188 ]
          [-2.419797    0.9740167 ]]
         [[-0.79111797  1.0569006 ]
          [ 0.74180895 -0.2904787 ]]]
    """

    @prim_attr_register
    def __init__(self, adjoint=False):
        """Initialize MatrixInverse"""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])
        validator.check_value_type('adjoint', adjoint, [bool], self.name)


class MatrixPower(Primitive):
    """
    Calculates the n-th power of a batch of square matrices.
    When n equals 0, it returns a group of identity matrices. If n is negative,
    it computes the inverse of each matrix (if possible) raised to the power of abs(n).

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        n (int) : The exponent, a required int.

    Inputs:
        - **x** (Tensor) - A 3-D Tensor. The shape is :math:`(b, m, m)`, represents b m-D square matrices.

    Outputs:
        - **y** (Tensor) - A 3-D Tensor. Data type and shape are the same as `x`'s.

    Raises:
        TypeError: If the data type of `n` is not int.
        TypeError: If x is not a Tensor.
        ValueError: If `x` is not a 3-D tensor.
        ValueError: If shape[1] and shape[2] of `x` are not the same.
        ValueError: If n is negative but got input x has singular matrices.
        ValueError: If `n` < 0 and input is int type.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> x = Tensor([[[0, 1], [-1, 0]], [[1, 0], [0, -1]]], dtype=ms.float32)
        >>> matrix_power = ops.MatrixPower(n=2)
        >>> y = matrix_power(x)
        >>> print(y)
        [[[-1.  0.]
          [-0. -1.]]
         [[ 1.  0.]
          [ 0.  1.]]]
    """

    @prim_attr_register
    def __init__(self, n):
        super().__init__(name="MatrixPower")
        self.n = validator.check_value_type("n", n, [int], self.name)


class MatrixLogarithm(Primitive):
    """
    Return the matrix logarithm of one or more square matrices.

    Inputs:
        - **x** (Tensor) - x is a tensor. The shape of tensor is :math:`[..., M, M]`.
          Must be one of the following types:complex64, complex128. And shape must be 2D-7D.

    Outputs:
        - **y** (Tensor) - has the same shape and type as input.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If dtype of `x` is not one of: complex64, complex128.
        ValueError: If the dimension of `x` is less to 2.
        ValueError: If the size of last two dimensions are not equal.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> x = Tensor([[1 + 2j, 2 + 1j], [4 + 1j, 5 + 2j]])
        >>> matrix_logarithm = ops.MatrixLogarithm()
        >>> y = matrix_logarithm(x)
        >>> print(y)
        [[0.69155775+1.71618359j 0.64665196-0.34928196j]
         [1.02426074-0.88736831j 1.44677531+0.6400109j ]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize MatrixLogarithm"""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])


class IndexAdd(Primitive):
    """
    Adds tensor `y` to specified axis and indices of tensor `x`. The axis should be in [-len(x.dim),  len(x.dim) - 1],
    and indices should be in [0, the size of `x` - 1] at the axis dimension.

    Args:
        axis (int): The dimension along which to index.
        use_lock (bool, optional): Whether to enable a lock to protect the updating process of variable tensors.
            If ``True`` , when updating the value of `x`, this process will be protected by a lock by using atomic
            operation.
            If ``False`` , the result may be unpredictable. Default: ``True`` .
        check_index_bound (bool, optional): If ``True`` , check index boundary. If ``False`` ,
            don't check index boundary. Default: ``True`` .

    Inputs:
        - **x** (Parameter) - The input Parameter to add to.
        - **indices** (Tensor) - Add the value of `x` and `y` along the dimension of the `axis` according to the
          specified index value, with data type int32.
          The `indices` must be 1D with the same size as the size of `y` in the `axis` dimension. The values
          of `indices` should be in [0, b), where the b is the size of `x` in the `axis` dimension.
        - **y** (Tensor) - The input tensor with the value to add. Must have same data type as `x`.
          The shape must be the same as `x` except the `axis` th dimension.

    Outputs:
        Tensor, has the same shape and dtype as `x`.

    Raises:
        TypeError: If `x` is not a Parameter.
        TypeError: If neither `indices` nor `y` is a Tensor.
        ValueError: If axis is out of `x` rank's range.
        ValueError: If `x` rank is not the same as `y` rank.
        ValueError: If shape of `indices` is not 1D or size of `indices` is not equal to dimension of y[axis].
        ValueError: If `y`'s shape is not the same as `x` except the `axis` th dimension.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, nn, ops, Parameter
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.index_add = ops.IndexAdd(axis=1)
        ...         self.x = Parameter(Tensor(np.array([[1, 2, 3], [4, 5, 6], [7, 8, 9]]), mindspore.float32),
        ...                 name="name_x")
        ...         self.indices = Tensor(np.array([0, 2]), mindspore.int32)
        ...
        ...     def construct(self, y):
        ...         return self.index_add(self.x, self.indices, y)
        ...
        >>> y = Tensor(np.array([[0.5, 1.0], [1.0, 1.5], [2.0, 2.5]]), mindspore.float32)
        >>> net = Net()
        >>> output = net(y)
        >>> print(output)
        [[ 1.5  2.   4. ]
         [ 5.   5.   7.5]
         [ 9.   8.  11.5]]
    """
    __mindspore_signature__ = (
        sig.make_sig('input_x', sig.sig_rw.RW_WRITE, dtype=sig.sig_dtype.T),
        sig.make_sig('indices', dtype=sig.sig_dtype.T1),
        sig.make_sig('input_y', dtype=sig.sig_dtype.T)
    )

    @prim_attr_register
    def __init__(self, axis, use_lock=True, check_index_bound=True):
        """Initialize InplaceAdd"""
        self.init_prim_io_names(inputs=['input_x', 'indices', 'input_y'], outputs=['output'])
        self.axis = axis
        validator.check_value_type('axis', axis, [int], self.name)
        self.add_prim_attr('side_effect_mem', True)


class ComplexAbs(Primitive):
    r"""
    Returns a Tensor that contains the magnitudes of the input.

    The complex numbers in input must be of the form :math:`a + bj`,
    where :math:`a` is the real part and :math:`b` is the imaginary part.

    .. math::

        y = \sqrt{a^2+b^2}

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - A Tensor, types: complex64, complex128.

    Outputs:
        Tensor, has the same shape as x. If the type of x is complex64, the type of output is float32.
        If the type of x is complex128, the type of output is float64.

    Raises:
       TypeError: If the input is not a Tensor.
       TypeError: If the input type is not complex64 or complex128.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.asarray(np.complex(3+4j)), mindspore.complex64)
        >>> complex_abs = ops.ComplexAbs()
        >>> output = complex_abs(x)
        >>> print(output)
        5.0
    """

    @prim_attr_register
    def __init__(self):
        """Initialize ComplexAbs"""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])


class Imag(Primitive):
    """
    Returns a new tensor containing imaginary value of the input.
    If input is real, it is returned zeros.

    Inputs:
        - **input** (Tensor) - The input tensor.

    Outputs:
        Tensor, the shape is the same as the input.

    Raises:
       TypeError: If the input is not a Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.asarray(np.complex(1.3+0.4j)), mindspore.complex64)
        >>> imag = ops.Imag()
        >>> output = imag(x)
        >>> print(output)
        0.4
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Imag"""
        self.init_prim_io_names(inputs=['input'], outputs=['output'])


class Trunc(Primitive):
    """
    Returns a new tensor with the truncated integer values of the elements of input.

    Refer to :func:`mindspore.ops.trunc` for more details.

    Inputs:
        - **input_x** (Tensor) - Input tensor of any dimension.

    Outputs:
        Tensor, the same shape and data type as `input_x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([3.4742, 0.5466, -0.8008, -3.9079]), mindspore.float32)
        >>> output = ops.Trunc()(x)
        >>> print(output)
        [ 3.  0. -0. -3.]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Trunc"""
        self.init_prim_io_names(inputs=['input'], outputs=['output'])


class TridiagonalMatMul(Primitive):
    """
    Return the result of a multiplication of two matrices, where the left one is a Tridiagonal Matrix.

    Inputs:
        - **superdiag** (Tensor) - Superdiagonals of Tridiagonal Matrices to the left of multiplication.
          Data types must be: float16, float32, double, complex64, complex128.
          The shape is :math:`(..., 1, M)`.
          Last element is ignored.
        - **maindiag** (Tensor) - Maindiagonals of Tridiagonal Matrices to the left of multiplication.
          Data types must be: float16, float32, double, complex64, complex128.
          The shape is :math:`(..., 1, M)`.
        - **subdiag** (Tensor) - Subdiagonals of Tridiagonal Matrices to the left of multiplication.
          Data types must be: float16, float32, double, complex64, complex128.
          The shape is :math:`(..., 1, M)`.
          First element is ignored.
        - **rhs** (Tensor) - MxN Matrices to the right of multiplication.
          Data types must be: float16, float32, double, complex64, complex128.
          The shape is :math:`(..., 1, M)`.

    Outputs:
        Tensor, with the same shape and data type as the `rhs`.

    Raises:
        TypeError: If dtypes of `superdiag`, `maindiag`, `subdiag` and `rhs`
                   are not float16, float32, double, complex64, complex128.
        ValueError: If the col of input `superdiag`, the col of input `maindiag`,
                    the col of input `subdiag` and the row of input `rhs` are not equal.
        ValueError: If the row of input `superdiag`, the row of input `maindiag` and
                    the row of input `subdiag` are not 1.
        ValueError: If the rank of input `superdiag`, the rank of input `maindiag`,
                    the rank of input `subdiag` and rank row of input `rhs`
                    are not equal to or greater than 2.
        ValueError: If the shape of input `superdiag`, the shape of input `maindiag` and
                    the shape of input `subdiag` are not same.
        ValueError: If the shape of input `superdiag` ignoring the last two elements,
                    the shape of input `maindiag` ignoring the last two elements,
                    the shape of input `subdiag` ignoring the last two elements and
                    the shape of input `rhs` ignoring the last two elements
                    are not same.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> tridiagonalmatmul = ops.TridiagonalMatMul()
        >>> superdiag = Tensor(np.array([[1, 2, 3]]).astype(np.float32))
        >>> maindiag = Tensor(np.array([[1, 2, 3]]).astype(np.float32))
        >>> subdiag = Tensor(np.array([[1, 2, 3]]).astype(np.float32))
        >>> rhs = Tensor(np.array([[1, 1, 1], [1, 1, 1], [1, 1, 1]]).astype(np.float32))
        >>> output = tridiagonalmatmul(superdiag,maindiag,subdiag,rhs)
        >>> print(output)
        [[ 2.  2.  2. ]
         [ 6.  6.  6.]
         [ 6.  6.  6.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize TridiagonalMatMul"""
        self.init_prim_io_names(
            inputs=['superdiag', 'maindiag', 'subdiag', 'rhs'],
            outputs=['y'])


class Igamma(Primitive):
    r"""
    Calculates lower regularized incomplete Gamma function.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.igamma` for more details.

    Inputs:
        - **a** (Tensor) - The input tensor.
        - **x** (Tensor) - The input tensor. It should have the same dtype with `a`.

    Outputs:
        Tensor, has the same dtype as `a` and `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> a = Tensor(np.array([2.0, 4.0, 6.0, 8.0]).astype(np.float32))
        >>> x = Tensor(np.array([2.0, 3.0, 4.0, 5.0]).astype(np.float32))
        >>> igamma = ops.Igamma()
        >>> output = igamma(a, x)
        >>> print (output)
        [0.593994  0.35276785  0.21486944  0.13337152]
        >>> a = Tensor(2.1, mindspore.float32)
        >>> x = Tensor(2.1, mindspore.float32)
        >>> igamma = ops.Igamma()
        >>> output = igamma(a, x)
        >>> print (output)
        0.5917439
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Igamma"""
        self.init_prim_io_names(inputs=['a', 'x'], outputs=['z'])


class Igammac(Primitive):
    r"""
    Compute the upper regularized incomplete Gamma function Q(a, x).

    Refer to :func:`mindspore.ops.igammac` for more details.

    Inputs:
        - **a** (Tensor) - The input tensor.
        - **x** (Tensor) - The input tensor. It should have the same dtype with `a`.

    Outputs:
        Tensor, has the same dtype as `a` and `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> a = Tensor(np.array([2.0, 4.0, 6.0, 8.0]).astype(np.float32))
        >>> x = Tensor(np.array([2.0, 3.0, 4.0, 5.0]).astype(np.float32))
        >>> igammac = ops.Igammac()
        >>> output = igammac(a, x)
        >>> print (output)
        [0.40600586 0.6472318  0.7851304  0.8666283 ]
        >>> a = Tensor(2.1, mindspore.float32)
        >>> x = Tensor(2.1, mindspore.float32)
        >>> igammac = ops.Igammac()
        >>> output = igammac(a, x)
        >>> print (output)
        0.40825662
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Igammac"""
        self.init_prim_io_names(inputs=['a', 'x'], outputs=['z'])


class IsClose(Primitive):
    r"""
    Returns a tensor of Boolean values indicating whether two input tensors
    are element-wise equal within a given tolerance.

    Refer to :func:`mindspore.ops.isclose` for more details.

    Args:
        rtol(float, optional): Relative tolerance. Default: ``1e-05`` .
        atol(float, optional): Absolute tolerance. Default: ``1e-08`` .
        equal_nan(bool, optional): If ``True`` , then two NaNs will be considered equal. Default: ``True`` .

    Inputs:
        - **input** (Tensor) - First tensor to compare, with data type belongs to float32, float16, int32.
        - **other** (Tensor) - Second tensor to compare, with data type belongs to float32, float16, int32.

    Outputs:
        Tensor, with the same shape as `input` and `other` after broadcasting, its dtype is bool.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor
        >>> from mindspore.ops import IsClose
        >>> input = Tensor(np.array([1.3, 2.1, 3.2, 4.1, 5.1]), mindspore.float16)
        >>> other = Tensor(np.array([1.3, 3.3, 2.3, 3.1, 5.1]), mindspore.float16)
        >>> isclose = IsClose()
        >>> output = isclose(input, other)
        >>> print(output)
        [ True False False False  True]
    """

    @prim_attr_register
    def __init__(self, rtol=1e-05, atol=1e-08, equal_nan=True):
        """Initialize IsClose"""
        validator.check_value_type('rtol', rtol, [float], self.name)
        validator.check_value_type('atol', atol, [float], self.name)
        validator.check_value_type('equal_nan', equal_nan, [bool], self.name)
        if context.get_context("device_target") == "Ascend" and not equal_nan:
            raise ValueError("For IsClose, the `equal_nan` must be True on Ascend, but got False.")
        validator.check_non_negative_float(rtol, 'rtol', self.name)
        validator.check_non_negative_float(atol, 'atol', self.name)


class MatrixSolve(Primitive):
    """
    Solves systems of linear equations.

    Args:
        adjoint (bool, optional): Indicates whether the adjoint of the
            matrix is used during the computation. Default: ``False`` ,  use its transpose instead.

    Inputs:
        - **matrix** (Tensor) - A tensor of shape :math:`(..., M, M)`,
          is a matrix of coefficients for a system of linear equations.
        - **rhs** (Tensor) - A tensor of shape :math:`(..., M, K)`,
          is a matrix of the resulting values of a system of linear equations.
          `rhs` must have the same type as `matrix`.

    Outputs:
        Tensor, a matrix composed of solutions to a system of linear equations,
        which has the same type and shape as `rhs`.

    Raises:
        TypeError: If `adjoint` is not the type of bool.
        TypeError: If the type of `matrix` is not one of the following dtype:
                   mstype.float16, mstype.float32, mstype.float64, mstype.complex64,
                   mstype.complex128.
        TypeError: If the type of `matrix` is not the same as that of `rhs`.
        ValueError: If the rank of `matrix` less than 2.
        ValueError: If the dimension of `matrix` is not the same as `rhs` .
        ValueError: If the inner-most 2 dimension of `matrix` is not the same.
        ValueError: If the inner-most 2 dimension of `rhs` does not match `matrix` .

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> matrix = Tensor(np.array([[1.0  , 4.0],
        ...                       [2.0 , 7.0]]), mindspore.float32)
        >>> rhs = Tensor(np.array([[1.0]  , [3.0]]), mindspore.float32)
        >>> matrix_solve = ops.MatrixSolve(adjoint = False)
        >>> output = matrix_solve(matrix, rhs)
        >>> print(output)
        [[5.0]
         [-1.0]]
    """

    @prim_attr_register
    def __init__(self, adjoint=False):
        super().__init__(name="MatrixSolve")
        self.adjoint = validator.check_value_type("adjoint", adjoint, [bool], self.name)


class MatrixSolveLs(Primitive):
    r"""
    Solves one or more linear least-squares problems.

    If `fast` is `True`,then the solution is computed by solving the normal equations using Cholesky decomposition.
    If `fast` is `False` an algorithm based on the numerically robust complete orthogonal decomposition is used. This
    path is typically 6-7 times slower than the fast path. If `fast` is `False` then `l2_regularizer` is ignored.

    Args:
        fast (bool): An optional bool. Default: ``True`` .

    Inputs:
        - **matrix** (Tensor) -  A Tensor. Must be one of the following data types: float64, float32, complex64,
          complex128. Shape is :math:`(*, M, N)`.
        - **rhs** (Tensor) -  A Tensor. Must have the same data type as matrix. Shape is :math:`(*, M, K)`.
          `matrix` and `rhs` should have the same dimensions except the last one.
        - **l2_regularizer** (Tensor) - A Tensor of type float64. Scalar tensor.

    Outputs:
        Tensor of shape :math:`(*, N, K)` with the same data type as `matrix`.

    Raises:
        TypeError: If `matrix`, `rhs` or `l2_regularizer` is not tensor.
        TypeError: If either of `matrix` and `rhs` is not float32, float64, complex64 or complex128.
        TypeError: If `l2_regularizer` is not float64.
        TypeError: If `fast` is not bool.
        ValueError: If dimensions of `matrix` or `rhs` is less than 2.
        ValueError: If shape of `matrix` dose not match the shape of `rhs`.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> matrix_solve_ls = ops.MatrixSolveLs(fast=True)
        >>> matrix = Tensor([[3, 0, 0, 0], [2, 1, 0, 0], [1, 0, 1, 0], [1, 1, 1, 1]], mstype.float32)
        >>> rhs = Tensor(np.array([[4], [2], [4], [2]]), mstype.float32)
        >>> l2 = Tensor(0.0, mstype.float64)
        >>> output = matrix_solve_ls(matrix, rhs, l2)
        >>> print(output)
        [[ 1.3333334]
        [-0.6666667]
        [ 2.6666665]
        [-1.3333333]]
    """

    @prim_attr_register
    def __init__(self, fast=True):
        """Initialize MatrixSolveLs"""
        validator.check_value_type('fast', fast, [bool], self.name)


class Lu(Primitive):
    """
    Computes the LU decomposition of one or more square matrices.

    Args:
        output_idx_type (:class:`mindspore.dtype`): An optional data type of `mindspore.dtype.int32`.
            Default: ``mindspore.dtype.int32`` .

    Inputs:
        - **input** (Tensor) - A tensor of shape `[..., M, M]` whose inner-most 2 dimensions form
          matrices of size `[M, M]`, with data type float32, float64, complex64, complex128.

    Outputs:
        - **lu** (Tensor) - A tensor of shape `[..., M, M]` whose strictly lower triangular part denotes the lower
          triangular factor `L` with unit diagonal. Upper triangular part denotes the upper triangular factor `U`.
        - **p** (Tensor) - Permutation of the rows encoded as a list of indices in `0..M-1`, shape is `[..., M]`.

    Raises:
        TypeError: If the dtype of `input` is not one of the following dtype:
            float32, float64, complex64, complex128.
        TypeError: If `output_idx_type` is neither int32 nor int64.
        ValueError: If `input` rank is less than 2.
        ValueError: If input[-1] is not equal to input[-2].

    Supported Platforms:
        ``GPU``

    Examples:
        >>> input = Tensor(np.array([[2.5,3.1,3.5], [4.7,1.9,0.2], [1.1,3.6,2.0]]), mindspore.float32)
        >>> lu, p = ops.Lu(output_idx_type=mindspore.int32)(input)
        >>> print(lu)
        [[4.7        1.9        0.2       ]
         [0.23404257 3.155319   1.9531915 ]
         [0.5319149  0.6621713  2.1002696 ]]
        >>> print(p)
        [1 2 0]
    """

    @prim_attr_register
    def __init__(self, output_idx_type):
        super().__init__(name="Lu")
        self.init_prim_io_names(inputs=['input'], outputs=['lu', 'p'])
        validator.check_type_name("output_idx_type", output_idx_type, [mstype.int32, mstype.int64], self.name)
        self.add_prim_attr('output_idx_type', output_idx_type)


class LuSolve(Primitive):
    r"""
    Computes the solution y to the system of linear equations :math:`Ay = b` ,
    given LU decomposition A and column vector b.

    LU decomposition of a matrix can be generated from :func:`mindspore.scipy.linalg.lu` .

    Note:
        The batch dimensions of lu_pivots must match the batch dimensions of lu_data, the size of the dimension and the
        number of each dimension must be the same. For example, lu_data is :math:`(3, 3, 2, 2)` lu_pivots is
        :math:`(3, 3, 2)`,
        lu_data's batch dimensions is :math:`(3, 3)`, lu_pivots's batch dimensions is :math:`(3, 3)`.

        The batch dimensions of lu_data must match the batch dimensions of x, the batch dimensions may have
        different sizes, from right to left, the corresponding dimensions must be equal. For example, lu_data
        is :math:`(3, 3, 2, 2)` x is :math:`(2, 3, 3, 2, 1)`, lu_data's batch dimensions is
        :math:`(3, 3)`, x's batch dimensions is :math:`(2, 3, 3)`.

    Inputs:
        - **x** (Tensor) - Column vector `b` in the above equation. It has shape :math:`(*, m, k)`,
          where :math:`*` is batch dimensions, with data type float32, float16.
        - **lu_data** (Tensor) - LU decomposition. It has shape :math:`(*, m, m)`, where * is batch
          dimensions, that can be decomposed into an upper triangular matrix U and a lower triangular
          matrix L, with data type float32, float16.
        - **lu_pivots** (Tensor) - Permutation matrix P of LU decomposition. It has
          shape :math:`(*, m)`, where :math:`*` is batch dimensions, that can be converted
          to a permutation matrix P, with data type int32.

    Outputs:
        Tensor, the same data type as the x and lu_data.

    Raises:
        TypeError: If dtype of `x` or `lu_data` is not one of: float32, float16.
        TypeError: If dtype of `lu_pivots` is not: int32.
        TypeError: If `x`, `lu_data` or `lu_pivots` is not Tensor.
        TypeError: If dtype of `x` is not same as dtype of `lu_data`.
        ValueError: If the batch dimensions of lu_pivots does not match the batch dimensions of lu_data.
        ValueError: If `x` dimension less than 2, `lu_data` dimension less than 2 or `lu_pivots` dimension less than 1.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> x = Tensor(np.array([[1], [3], [3]]), mindspore.float32)
        >>> lu_data = Tensor(np.array([[2, 1, 1], [0.5, 1, 1.5], [0.5, 0, 2.5]]), mindspore.float32)
        >>> lu_pivots = Tensor(np.array([2, 2, 3]), mindspore.int32)
        >>> net = ops.LuSolve()
        >>> y = net(x, lu_data, lu_pivots)
        >>> print(y)
        [[ 1.9000002]
         [-1.4000001]
         [ 0.6      ]]
    """

    @prim_attr_register
    def __init__(self):
        pass


class LuUnpack(Primitive):
    """
    Converts `LU_data` and `LU_pivots` back into P, L and U matrices, where
    P is a permutation matrix, L is a lower triangular matrix, and U is an
    upper triangular matrix. Typically, `LU_data` and `LU_pivots` are generated
    from the LU decomposition of a matrix.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.lu_unpack` for more details.

    Args:
        unpack_data (bool, optional): A flag indicating if the LU_data should be unpacked.
            If ``False`` , then the returned L and U are None. Default: ``True`` .
        unpack_pivots (bool, optional): A flag indicating if the LU_pivots should be unpacked
            into a permutation matrix P. If ``False`` , then the returned P is None. Default: ``True`` .

    Inputs:
        - **LU_data** (Tensor) - The packed LU factorization data. The shape of a tensor is :math:`(*, M, N)`,
          where :math:`*` is batch dimensions, with data type int8, uint8, int16, int32, int64, float16,
          float32, float64. The dims of LU_data must be equal to or greater than 2.
        - **LU_pivots** (Tensor) - The packed LU factorization pivots. The shape of a tensor is :math:`(*, min(M, N))`,
          where :math:`*` is batch dimensions, with data type int8, uint8, int16, int32, int64.

    Outputs:
        - **pivots** (Tensor) - The permutation matrix of LU factorization. The shape is :math:`(*, M, M)`,
          the dtype is same as `LU_data`.
        - **L** (Tensor) - The L matrix of LU factorization. The dtype is the same as `LU_data`.
        - **U** (Tensor) - The U matrix of LU factorization. The dtype is the same as `LU_data`.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> from mindspore import dtype as mstype
        >>> LU_data = Tensor(np.array([[[-0.3806, -0.4872,  0.5536],
        ...                             [-0.1287,  0.6508, -0.2396],
        ...                             [ 0.2583,  0.5239,  0.6902]],
        ...                             [[ 0.6706, -1.1782,  0.4574],
        ...                             [-0.6401, -0.4779,  0.6701],
        ...                             [ 0.1015, -0.5363,  0.6165]]]), mstype.float32)
        >>> LU_pivots = Tensor(np.array([[1, 3, 3],
        ...                              [2, 3, 3]]), mstype.int32)
        >>> lu_unpack = ops.LuUnpack()
        >>> pivots, L, U = lu_unpack(LU_data, LU_pivots)
        >>> print(pivots)
        [[[1. 0. 0.]
          [0. 0. 1.]
          [0. 1. 0.]]
        <BLANKLINE>
         [[0. 0. 1.]
          [1. 0. 0.]
          [0. 1. 0.]]]
        >>> print(L)
        [[[ 1.      0.      0.    ]
          [-0.1287  1.      0.    ]
          [ 0.2583  0.5239  1.    ]]
        <BLANKLINE>
         [[ 1.      0.      0.    ]
          [-0.6401  1.      0.    ]
          [ 0.1015 -0.5363  1.    ]]]
        >>> print(U)
        [[[-0.3806 -0.4872  0.5536]
          [ 0.      0.6508 -0.2396]
          [ 0.      0.      0.6902]]
        <BLANKLINE>
         [[ 0.6706 -1.1782  0.4574]
          [ 0.     -0.4779  0.6701]
          [ 0.      0.      0.6165]]]
    """

    @prim_attr_register
    def __init__(self, unpack_data=True, unpack_pivots=True):
        """Initialize LuUnpack"""
        validator.check_value_type("unpack_data", unpack_data, [bool], self.name)
        validator.check_value_type("unpack_pivots", unpack_pivots, [bool], self.name)


class Lgamma(Primitive):
    r"""
    Computes the natural logarithm of the absolute value of the gamma function on input.

    Refer to :func:`mindspore.ops.lgamma` for more details.

    Inputs:
        - **x** (Tensor) - The input tensor. The dtype can be float16, float32 or float64.

    Outputs:
        Tensor, has the same dtype as `x`.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> x = Tensor(np.array([0.5, 3.2, 8.5]), mindspore.float32)
        >>> lgamma = ops.Lgamma()
        >>> output = lgamma(x)
        >>> print(output)
        [0.5723649 0.8854049 9.549267 ]
        >>> x = Tensor(2.1, mindspore.float32)
        >>> output = lgamma(x)
        >>> print(output)
        0.045437694
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Lgamma"""
        self.init_prim_io_names(inputs=['x'], outputs=['y'])


class Digamma(Primitive):
    r"""
    Computes the grad of the lgamma function on input.

    .. math::
        P(x) = grad(ln(gamma(x)))

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - The input tensor. With type of float16 or float32 or float64.

    Outputs:
        Tensor, has the same dtype as `x`.

    Raises:
        TypeError: If x is not a Tensor.
        TypeError: If dtype of input x is not float16 or float32 or float64.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([1.5, 0.5, 9]).astype(np.float16))
        >>> digamma = ops.Digamma()
        >>> output = digamma(x)
        >>> print(output)
        [ 0.0365 -1.964   2.14  ]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Digamma"""
        self.init_prim_io_names(inputs=['input'], outputs=['output'])


class Polygamma(Primitive):
    r"""
    Computes the :math:`a`th derivative of the polygamma function on `x`.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.polygamma` for more details.

    Inputs:
        - **a** (Tensor) - The order of the polygamma function, it has shape :math:`()`,
          supported types: int32, int64.
        - **x** (Tensor) - The tensor to compute the :math:`a`-th derivative of the polygamma function with,
          supported types: float16, float32, float64.

    Outputs:
        Tensor, has the same dtype as `x`.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([1.0, -0.5]), mindspore.float32)
        >>> a = Tensor(np.array(1), mindspore.int64)
        >>> polygamma = ops.Polygamma()
        >>> output = polygamma(a, x)
        >>> print(output)
        [1.644934 8.934802]
        >>> a = Tensor(np.array(2), mindspore.int64)
        >>> output = polygamma(a, x)
        >>> print(output)
        [-2.404114  -0.8287967]
        >>> a = Tensor(np.array(3), mindspore.int64)
        >>> output = polygamma(a, x)
        >>> print(output)
        [  6.4939404 193.40909  ]
        >>> a = Tensor(np.array(4), mindspore.int64)
        >>> output = polygamma(a, x)
        >>> print(output)
        [-24.886265   -3.4742498]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Polygamma"""
        self.init_prim_io_names(inputs=['a', 'x'], outputs=['y'])


class Cross(Primitive):
    """
    Returns the cross product of vectors in dimension `dim` of x1 and x2.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.cross` for more details.

    Args:
        dim (int): Spefcified dim along which to cumpute cross product with. Default: ``-65530`` .

    Inputs:
        - **x1** (Tensor) - Input Tensor.
        - **x2** (Tensor) - Another input Tensor, must have the same shape and
          the same type as `x1`, and the size of their `dim` dimension should be 3.

    Outputs:
        Tensor, has the same shape and type as inputs.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor
        >>> from mindspore import dtype as mstype
        >>> import mindspore.ops as ops
        >>> cross = ops.Cross(dim = 0)
        >>> x1 = Tensor([1, 2, 3], mstype.int8)
        >>> x2 = Tensor([1, 2, 3], mstype.int8)
        >>> output = cross(x1, x2)
        >>> print(output)
        [0 0 0]
    """

    @prim_attr_register
    def __init__(self, dim=-65530):
        validator.check_value_type('dim', dim, [int], self.name)
        self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['y'])


class RaggedRange(Primitive):
    """
    Returns a `RaggedTensor` containing the specified sequences of numbers.

    Args:
        Tsplits (mindspore.dtype): An mindspore.dtype from: mindspore.int32, mindspore.int64.

    Inputs:
        - **starts** (Tensor) - The starts of each range, whose type is int32, int64, float32 or float64,
          and shape is 0D or 1D.
        - **limits** (Tensor) - The limits of each range, whose type and shape should be same as input `starts`.
        - **deltas** (Tensor) - The deltas of each range, whose type and shape should be same as input `starts`,
          and each element in the tensor should not be equal to 0.

    Outputs:
        - **rt_nested_splits** (Tensor) - The nested splits of the return `RaggedTensor`,
          and type of the tensor is `Tsplits`,
          shape of the tensor is equal to shape of input `starts` plus 1.
        - **rt_dense_values**  (Tensor) - The dense values of the return `RaggedTensor`,
          and type of the tensor should be same as input `starts`.
          Let size of input `starts`, input `limits` and input `deltas` are i,

          - if type of the input `starts`, input `limits` and input `deltas`
            are int32 or int64, shape of the output `rt_dense_values` is equal to
            :math:`sum(abs(limits[i] - starts[i]) + abs(deltas[i] - 1) / abs(deltas[i]))`.
          - if type of the input `starts`, input `limits` and input `deltas`
            are float32 or float64, shape of the output `rt_dense_values` is equal to
            :math:`sum(ceil(abs((limits[i] - starts[i]) / deltas[i])))`.

    Raises:
        TypeError: If any input is not Tensor.
        TypeError: If the type of `starts` is not one of the following dtype: int32, int64, float32, float64.
        TypeError: If the type of `starts`, `limits` and `deltas` are not same.
        TypeError: If the type of `Tsplits` is not one of the following dtype: mstype.int32, mstype.int64.
        ValueError: If the inputs `starts`, `limits`, and `deltas` are not 0D or 1D.
        ValueError: If the input `deltas` is equal to 0.
        ValueError: If the shape of `starts`, `limits` and `deltas` are not same.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> raggedrange = ops.RaggedRange(Tsplits=mstype.int64)
        >>> starts = Tensor(np.array([2, 5, 8]).astype(np.int32))
        >>> limits = Tensor(np.array([3, 5, 12]).astype(np.int32))
        >>> deltas = Tensor(np.array([1, 1, 1]).astype(np.int32))
        >>> (rt_nested_splits, rt_dense_values) = raggedrange(starts, limits, deltas)
        >>> print(rt_nested_splits)
        [0 1 1 5]
        >>> print(rt_dense_values)
        [ 2  8  9 10 11]
    """

    @prim_attr_register
    def __init__(self, Tsplits):
        """Initialize RaggedRange."""
        self.add_prim_attr("max_length", 1000000)
        self.init_prim_io_names(inputs=['starts', 'limits', 'deltas'], outputs=['rt_nested_splits', 'rt_dense_values'])
        validator.check_value_type("Tsplits", Tsplits, [mstype.Type], self.name)
        valid_values = (mstype.int64, mstype.int32)
        validator.check_type_name("Tsplits", Tsplits, valid_values, self.name)


class Median(Primitive):
    """
    Computes the median and its corresponding indices of input tensor in the `axis` dimension.
    If `global_median` is True, computes the  median of all elements of tensor.

    .. warning::
        - `indices` does not necessarily contain the first occurrence of each median value found in the `input`,
          unless it is unique. The specific implementation of this API is device-specific.
          The results may be different on CPU and GPU.
        - When attr `global_median` is ``True`` , the value of the second output tensor `indices` is meaningless.

    Args:
        global_median (bool, optional): Whether the output tensor is the median of all
            input tensor elements or not. Default: ``False`` .
        axis (int, optional): The specified dimension to compute median. Default: ``0`` .
        keep_dims (bool, optional): Whether the output tensor need to retain `axis` dimension or not.
            Default: ``False`` .
        ignore_nan (bool, optional): Whether to ignore the NaN values in input Tensor. Default: ``False`` .

    Inputs:
        - **x** (Tensor) - A Tensor to calculate median with.

    Outputs:
        - **y** (Tensor) - Median, has the same dtype as the `x`.

          - If `global_median` is ``True`` , the `y` has only one element.
          - If `keep_dims` is ``True`` , the `y` has the same shape as the `x` except the size
            of `y` in dimension `axis` is 1.
          - Otherwise, the `y` lacks `axis` dimension than input.

        - **indices** (Tensor) - Indices, Has the same shape as the `y`, with dtype int64.

    Raises:
        TypeError: If input `x` is not a Tensor.
        TypeError: If `global_median` , `keep_dims` or `ignore_nan` is assigned a nonboolean value.
        TypeError: If `axis` is not int.
        ValueError: If `axis` is not in range of [-x.dim, x.dim-1].

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> # case 1 : common median compute
        >>> from mindspore import Tensor, ops
        >>> import numpy as np
        >>> x = Tensor(np.array([[5, 1, 2],[3, 5, 7], [1, 6, 4]]).astype(np.int64))
        >>> median = ops.Median(global_median=False, axis=0, keep_dims=False)
        >>> y = median(x)
        >>> print(y)
        (Tensor(shape=[3], dtype=Int64, value= [3, 5, 4]), Tensor(shape=[3], dtype=Int64, value= [1, 1, 2]))
        >>> # case 2 : global median compute
        >>> from mindspore import Tensor, ops
        >>> import numpy as np
        >>> x = Tensor(np.array([[1, 7, 6],[5, 1, 3],[9, 17, 1]]).astype(np.int32))
        >>> median = ops.Median(global_median=True)
        >>> y = median(x)
        >>> print(y)
        (Tensor(shape=[], dtype=Int32, value= 5), Tensor(shape=[], dtype=Int64, value= 0))
    """

    @prim_attr_register
    def __init__(self, global_median=False, axis=0, keep_dims=False, ignore_nan=False):
        self.add_prim_attr("cust_aicpu", self.name)
        validator.check_value_type("global_median", global_median, [bool], self.name)
        self.global_median = global_median
        if global_median is False:
            validator.check_value_type("axis", axis, [int], self.name)
            validator.check_value_type("keep_dims", keep_dims, [bool], self.name)
        self.init_prim_io_names(inputs=['x'], outputs=['y', 'indices'])
        validator.check_value_type("ignore_nan", ignore_nan, [bool], self.name)


class SparseSegmentMean(Primitive):
    """
    Computes the mean along sparse segments of a Tensor.

    Refer to :func:`mindspore.ops.sparse_segment_mean` for more details.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import Tensor
        >>> from mindspore.ops.operations.math_ops import SparseSegmentMean
        >>> x = Tensor([[0, 1, 2], [1, 2, 3], [3, 6, 7]], dtype=mindspore.float32)
        >>> indices = Tensor([0, 1, 2], dtype=mindspore.int32)
        >>> segment_ids = Tensor([1,2,2], dtype=mindspore.int32)
        >>> sparse_segment_mean = SparseSegmentMean()
        >>> out = sparse_segment_mean(x, indices, segment_ids)
        >>> print(out)
        [[0. 0. 0.]
         [0. 1. 2.]
         [2. 4. 5.]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize SparseSegmentMean"""
        self.init_prim_io_names(inputs=['x', 'indices', 'segment_ids'], outputs=['y'])


class Zeta(Primitive):
    r"""
    Compute the Hurwitz zeta function ζ(x,q) of input Tensor.

    .. math::
        \zeta \left ( x,q \right )=  \textstyle \sum_{n=0} ^ {\infty} \left ( q+n\right )^{-x}

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Inputs:
        - **x** (Tensor) - A Tensor, types: float32, float64.
        - **q** (Tensor) - A Tensor, must have the same shape and type as `x`.

    Outputs:
        Tensor, has the same dtype and shape as the x.

    Raises:
        TypeError: If either of `x` and `q` is not tensor.
        TypeError: If dtype of `x` is neither float32 nor float64.
        TypeError: If dtype of `q` is neither float32 nor float64.
        ValueError: If shape of `x` is not same as the `q`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([10.]), mindspore.float32)
        >>> q = Tensor(np.array([1.]), mindspore.float32)
        >>> zeta = ops.Zeta()
        >>> z = zeta(x, q)
        >>> print(z)
        [1.0009946]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Zeta"""


class Bernoulli(Primitive):
    """
    Randomly set the elements of output to 0 or 1 with the probability of P which follows the Bernoulli distribution.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.bernoulli` for more details.

    Args:
        seed (int, optional): The seed value for random generating. The value of `seed` must be -1 or a
            positive integer, and -1 means using the current timestamp. Default: ``-1`` .
        offset (int, optional): Used to change the starting position during the generation of
            random number sequence. Default: ``0`` .

    Inputs:
        - **x** (Tensor) - Input Tensor.
        - **p** (Union[Tensor, float], optional) - Success probability, representing the probability of
          setting 1 for the corresponding position of the current Tensor. It has the same shape as `x`,
          the value of `p` must be in the range `[0, 1]`. Default: ``0.5`` .

    Outputs:
        - **y** (Tensor) - with the same shape and type as `x` .

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import Tensor, ops
        >>> input_x = Tensor([0.1, 0.2, 0.3], mindspore.float32)
        >>> bernoulli = ops.Bernoulli()
        >>> output = bernoulli(input_x, Tensor([1.0]))
        >>> print(output)
        [1. 1. 1.]
        >>> input_p = Tensor([0.0, 1.0, 1.0], mindspore.float32)
        >>> output = bernoulli(input_x, input_p)
        >>> print(output)
        [0. 1. 1.]
    """

    @prim_attr_register
    def __init__(self, seed=-1, offset=0):
        """Initialize Bernoulli"""
        self.init_prim_io_names(inputs=['x', 'p'], outputs=['y'])
        validator.check_value_type("seed", seed, [int], self.name)
        if seed != -1 and seed < 0:
            raise ValueError(f"Seed must be -1 or a non-negative integer, but got {seed}.")


class TridiagonalSolve(Primitive):
    """
    Return the results of tridiagonal systems of equations.

    Solve the tridiagonal systems of equations like:AX = B.
    and only the main diagonal, superdiagonal and subdiagonal has values.
    The type of diagonals and rhs should be the same.
    The penultimate dimension of diagonals must be 3.

    Args:
        partial_pivoting (bool): decide if use the method of partial_pivoting. Default: ``True`` .

    Inputs:
        - **diagonals** [Tensor] - The input tensor A of the equation AX = B, with data type of float32,
          float64, complex64, complex128.
          The penultimate dimension of diagonals must be 3.
          Diagonals and rhs must have the same rank and the same type.
        - **rhs** [Tensor] - The input tensor B of the equation AX = B, with data type of float32,
          float64, complex64, complex128.
          The penultimate dimension of rhs should be the same to the last dimension of diagonals.
          Diagonals and rhs must have the same rank and the same type.

    Outputs:
        Tensor, has the same type and shape as the input "rhs".

    Raises:
        TypeError: If `diagonals` and "rhs" are not a float32, float64, complex64 or complex128.
        TypeError: If the args `partial_pivoting` is not bool.
        ValueError: If the last second value of the "diagonals" is not "3".
        ValueError: If the last value of the "diagonals" is not equal to the last second value of the "rhs".
        ValueError: If diagonals and rhs have different rank of shape.

    Supported Platforms:
        ``CPU``
    Examples:
        >>> diagonals = Tensor(np.array([[1.0,2.0,3.0],[2.0,3.0,4.0],[3.0,4.0,5.0]]).astype(np.float32))
        >>> rhs = Tensor(np.array([[1.0],[2.0],[3.0]]).astype(np.float32))
        >>> y = P.TridiagonalSolve()(diagonals,rhs)
        >>> print(output)
        [[ 0. ]
         [ 1. ]
         [-0.5]]
    """

    @prim_attr_register
    def __init__(self, partial_pivoting=True):
        self.init_prim_io_names(inputs=['diagonals', 'rhs'], outputs=['y'])
        self.partial_pivoting = validator.check_value_type(
            "partial_pivoting", partial_pivoting, [bool], self.name)


class Renorm(Primitive):
    """
    Renormalizes the sub-tensors along dimension `dim`, and each sub-tensor's p-norm should not exceed the
    'maxnorm'. The values of current sub-tensor don't need change if the p-norm of the sub-tensor is less than
    `maxnorm`. Otherwise the sub-tensor needs to be modified to the original value of the corresponding position
    divided by the p-norm of the substensor and then multiplied by `maxnorm`.

    Refer to :func:`mindspore.ops.renorm` for more details.

    Args:
        p (int): Power of norm calculation.
        dim (int): The dimension that expected to get the slice-tensor.
        maxnorm (float32): Max norm.

    Inputs:
        - **x** (Tensor) - A Tensor, types: float32 or float16.

    Outputs:
        Tensor, has the same dtype and shape as input.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([[1, 1, 1], [2, 2, 2], [3, 3, 3]]), mindspore.float32)
        >>> y = ops.Renorm(p=1, dim=0, maxnorm=5.)(x)
        >>> print(y)
        [[1.       1.        1.        ]
        [1.6666666 1.6666666 1.6666666 ]
        [1.6666667 1.6666667 1.6666667 ]]
    """

    @prim_attr_register
    def __init__(self, p, dim, maxnorm):
        """Initialize Renorm."""
        if int(p) <= 0:
            raise ValueError(f"Renorm op don't support non-positive-norm, but got{p}")
        validator.check_value_type("p", p, [int], self.name)
        validator.check_value_type("dim", dim, [int], self.name)
        validator.check_value_type("maxnorm", maxnorm, [float], self.name)
        self.init_prim_io_names(inputs=['x'], outputs=['y'])
        self.add_prim_attr("p", float(p))


class STFT(Primitive):
    """
    Applies Short-time Fourier transform (STFT) on input signal.

    STFT segments the signal into narrow time intervals and takes the Fourier transform
    of each segment to quantify the change of a nonstationary signal’s frequency
    and phase content over time.

    Refer to :func:`mindspore.ops.stft` for more details.

    Args:
        n_fft (int): The size of Fourier transform.
        hop_length (int): The distance between neighboring sliding window frames.
        win_length (int): the size of window frame and STFT filter.
        normalized (bool): controls whether to return the normalized STFT results.
        onesided (bool): controls whether to return half of results to
            avoid redundancy for real inputs.
        return_complex (bool): If ``True`` , return a complex tensor. If False, return
            a real tensor with an extra last dimension for the real and imaginary components.

    Inputs:
        - **x** (Tensor) - Time sequence of stft, must be either a 1-D time tensor or a 2-D tensor.
        - **window** (Tensor) - the optional window function.

    Outputs:
        Tensor, containing the result after STFT.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> import mindspore as ms
        >>> from mindspore.ops import STFT
        >>> import numpy as np
        >>> x = ms.Tensor(np.random.rand(2,7192), ms.float32)
        >>> window = ms.Tensor(np.random.rand(64), ms.float32)
        >>> stft = STFT(64, 16, 64, False, True, True)
        >>> output = stft(x, window)
        >>> print(output.shape)
        (2, 33, 446)
    """

    @prim_attr_register
    def __init__(self, n_fft, hop_length, win_length, normalized, onesided, return_complex):
        """Initialize STFT."""
        self.init_prim_io_names(inputs=['x', 'window'], outputs=['y'])
        validator.check_value_type('n_fft', n_fft, [int], self.name)
        validator.check_value_type('hop_length', hop_length, [int], self.name)
        validator.check_value_type('win_length', win_length, [int], self.name)
        validator.check_value_type('normalized', normalized, [bool], self.name)
        validator.check_value_type('onesided', onesided, [bool], self.name)
        validator.check_value_type('return_complex', return_complex, [bool], self.name)


class CholeskySolve(Primitive):
    """
    Computes the solution of a set of linear equations with a positive definite matrix,
    according to its Cholesky decomposition factor `u` , and outputs the result as `c`.

    If `upper` is set to ``True`` , `u` is upper triangular and `c` is returned such that:

    .. math::
        c = (u^{T}u)^{{-1}}b

    If `upper` is set to `False`, `u` is lower triangular and `c` is returned such that:

    .. math::
        c = (uu^{T})^{{-1}}b

    Args:
        upper (bool, optional): A flag indicates whether to treat the Cholesky factor
            as an upper or a lower triangular matrix. Default: ``False`` .

    Inputs:
        - **x1** (Tensor) - Tensor of shape :math:`(*, N, M)`, indicating 2D or 3D matrices,
          with float32 or float64 data type.
        - **x2** (Tensor) - Tensor of shape :math:`(*, N, N)`, indicating 2D or 3D square matrices composed of
          upper or lower triangular Cholesky factor, with float32 or float64 data type.
          x1 and x2 must have the same type.

    Outputs:
        Tensor, has the same shape and data type as `x1`.

    Raises:
        TypeError: If `upper` is not a bool.
        TypeError: If dtype of `x1` and `x2` is not one of: float64, float32.
        TypeError: If `x1` is not a Tensor.
        TypeError: If `x2` is not a Tensor.
        ValueError: If `x1` and `x2` have different batch size.
        ValueError: If `x1` and `x2` have different row numbers.
        ValueError: If `x1` is not 2D or 3D matrices.
        ValueError: If `x2` is not 2D or 3D square matrices.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> x1 = Tensor(np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1]]), mindspore.float32)
        >>> x2 = Tensor(np.array([[2, 0, 0], [4, 1, 0], [-1, 1, 2]]), mindspore.float32)
        >>> net = ops.CholeskySolve()
        >>> y = net(x1, x2)
        >>> print(y)
        [[ 5.8125 -2.625   0.625 ]
         [-2.625   1.25   -0.25  ]
         [ 0.625  -0.25    0.25  ]]
    """

    @prim_attr_register
    def __init__(self, upper=False):
        """Initialize CholeskySolve"""
        self.init_prim_io_names(inputs=['x1', 'x2'], outputs=['y'])
        validator.check_value_type('upper', upper, [bool], self.name)


class Polar(Primitive):
    r"""
    Converts polar coordinates to Cartesian coordinates.

    Refer to :func:`mindspore.ops.polar` for more details.

    Inputs:
        - **abs** (Tensor) - Radial distance. Tensor of any dimension,
          must be one of the following types: float32, float64.

        - **angle** (Tensor) - Polar angle. It has the same shape and dtype as `abs`.

    Outputs:
        Tensor, has the same shape and data type as `abs`.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> polar = ops.Polar()
        >>> x1 = Tensor(np.array([1, 2]), mindspore.float64)
        >>> x2 = Tensor(np.array([3, 4]), mindspore.float64)
        >>> output = polar(x1, x2)
        >>> print(output)
        [-0.9899925 +0.14112001j -1.30728724-1.51360499j]
        >>> x1 = Tensor(2.1, mindspore.float32)
        >>> x2 = Tensor(2.1, mindspore.float32)
        >>> output = polar(x1, x2)
        >>> print(output)
        (-1.0601766+1.8127397j)
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Polar"""
        self.init_prim_io_names(inputs=['abs', 'angle'], outputs=['y'])


class TrilIndices(Primitive):
    r"""
    Calculates the indices of the lower triangular elements in a `row` * `col` matrix
    and returns them as a 2-by-N Tensor.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.tril_indices` for more details.

    Args:
        row (int): number of rows in the 2-D matrix.
        col (int): number of columns in the 2-D matrix.
        offset (int, optional): diagonal offset from the main diagonal. Default: ``0`` .
        dtype (:class:`mindspore.dtype`, optional): The specified type of output tensor.
            An optional data type of ``mstype.int32`` and ``mstype.int64`` . Default: ``mstype.int32`` .

    Outputs:
        - **y** (Tensor) - indices of the elements in lower triangular part of matrix. The type specified by `dtype`.
          The shape of output is :math:`(2, tril\_size)`, where :math:`tril\_size` is the number of elements in the
          lower triangular matrix.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import ops
        >>> from mindspore import dtype as mstype
        >>> net = ops.TrilIndices(4, 3, -1, mstype.int64)
        >>> output = net()
        >>> print(output)
        [[1 2 2 3 3 3]
         [0 0 1 0 1 2]]
        >>> print(output.dtype)
        Int64
    """

    @prim_attr_register
    def __init__(self, row, col, offset=0, dtype=mstype.int32):
        """Initialize TrilIndices"""
        self.init_prim_io_names(inputs=[], outputs=['y'])
        validator.check_int(row, 0, validator.GE, "row", self.name)
        validator.check_int(col, 0, validator.GE, "col", self.name)
        validator.check_value_type("offset", offset, [int], self.name)
        valid_values = (mstype.int32, mstype.int64)
        validator.check_type_name("dtype", dtype, valid_values, self.name)


class MatrixTriangularSolve(Primitive):
    r"""
    Returns a new tensor with the solution of a linear equation system with an
    upper or lower triangular matrix.

    Note:
        Only GPU platforms now support the broadcast mechanism.

    Args:
        lower (bool, optional): If ``True`` , the innermost matrices in `matrix` is
            are lower triangular. Default: ``True`` .
        adjoint (bool, optional): Indicates whether the adjoint of the
            matrix is used during the computation. Default: ``False`` ,  use its transpose instead.

    Inputs:
        - **matrix** (Tensor) - Tensor of shape :math:`(*, M, M)`,
          with float32, float64, complex64 and complex128 data type.
        - **rhs** (Tensor) - Tensor of shape :math:`(*, M, N)`,
          with float32, float64, complex64 and complex128 data type.

    Outputs:
        Tensor, has the shape of :math:`(*, M, N)` and the same data type as `matrix`.

    Raises:
        TypeError: If `matrix` or `rhs` is not a Tensor.
        TypeError: If `lower` or `adjoint` is not bool.
        ValueError: For GPU platform, if the batch sizes of `matrix` and `rhs` do not satisfy broadcasting rules.
            For other platforms, if the batch sizes of `matrix` and `rhs` are not equal.
        ValueError: If the inner-most 2 dimensions of `matrix` are not equal.
        ValueError: If the second-last dimensions of `matrix` and `rhs` are not equal.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> matrix_triangular_solve = ops.MatrixTriangularSolve(lower=True, adjoint=False)
        >>> matrix = np.array([[3, 0, 0, 0], [2, 1, 0, 0], [1, 0, 1, 0], [1, 1, 1, 1]])
        >>> rhs = np.array([[1, 0],[2, 2],[1, 5],[0, 3]])
        >>> output = matrix_triangular_solve(Tensor(matrix, mindspore.float32), Tensor(rhs, mindspore.float32))
        >>> print(output)
        [[ 0.33333334  0.        ]
         [ 1.3333333   2.        ]
         [ 0.6666666   5.        ]
         [-2.3333333  -4.        ]]
    """

    @prim_attr_register
    def __init__(self, lower=True, adjoint=False):
        """Initialize MatrixTriangularSolve"""
        validator.check_value_type('adjoint', adjoint, [bool], self.name)
        validator.check_value_type('lower', lower, [bool], self.name)


class CompareAndBitpack(Primitive):
    """
    Compare values of `x` to `threshold` and pack resulting bits into a `uint8`.

    Each comparison returns a boolean ``True`` (if x_value > threshold) or and ``False`` otherwise.

    Given an `x` shaped :math:`(s_0, s_1, ..., s_n)`, the output is a `uint8`
    Tensor shaped :math:`(s_0, s_1, ..., s_n / 8)`.

    Inputs:
        - **x** (Tensor) - Input tensor. Values to compare against `threshold` and bitpack. The data type must be
          bool, float16, float32, float64, int8, int16, int32, int64.
          Note: Currently, the innermost dimension of the tensor must be divisible by 8.
        - **threshold** (Tensor) - A 0D Tensor, whose data type is same as x.

    Outputs:
        Tensor, has the uint8 type.

    Raises:
        TypeError: If `x` or `threshold` is not a Tensor.
        TypeError: If the dtype of 'x' is not one of: bool, float16, float32, float64, int8, int16, int32, int64.
        TypeError: If `threshold`'s type is not as same 'x'.
        ValueError: If `threshold` is not a 0D Tensor.
        ValueError: If `x` is a 0D Tensor.
        ValueError: If the innermost dimension of `x`'s shape is not disvisible by 8.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> x = Tensor(np.array([1, 2, 3, 4, 5, 6, 7, 8]), mindspore.float32)
        >>> threshold = Tensor(6, mindspore.float32)
        >>> net = ops.CompareAndBitpack()
        >>> output = net(x, threshold)
        >>> print(output)
        [3]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize CompareAndBitPack"""


class Orgqr(Primitive):
    r"""
    Calculates the explicit representation of the orthogonal matrix :math:`Q`
    returned by :class:`mindspore.ops.Geqrf`.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.orgqr` for more details.

    Inputs:
        - **x** (Tensor) - Tensor of shape :math:`(*, M, N)`, indicating 2D or 3D matrices,
          with float32, float64, complex64 and complex128 data type.
        - **tau** (Tensor) - Indicates the reflecting coefficient in Householder transformation, it has
          shape :math:`(*, K)`, where `K` is less than or equal to `N`, and it has the same type as `x`.

    Outputs:
        Tensor, has the same shape and data type as `x`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([[-114.6, 10.9, 1.1], [-0.304, 38.07, 69.38], [-0.45, -0.17, 62.]]), mindspore.float32)
        >>> tau = Tensor(np.array([1.55, 1.94, 0.0]), mindspore.float32)
        >>> net = ops.Orgqr()
        >>> y = net(x, tau)
        >>> print(y)
        [[-0.54999995 -0.2128925   0.8137956 ]
         [ 0.47119996 -0.8752807   0.08240613]
         [ 0.69749993  0.42560163  0.57772595]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize Orgqr"""
        self.init_prim_io_names(inputs=['x', 'tau'], outputs=['y'])


class TriuIndices(Primitive):
    r"""
    Calculates the indices of the upper triangular elements in a `row` * `col` matrix
    and returns them as a 2-by-N Tensor.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.triu_indices` for more details.

    Args:
        row (int): number of rows in the 2-D matrix.
        col (int): number of columns in the 2-D matrix.
        offset (int, optional): diagonal offset from the main diagonal. Default: ``0`` .
        dtype (:class:`mindspore.dtype`, optional): The specified type of output tensor.
            An optional data type of ``mstype.int32`` and ``mstype.int64`` . Default: ``mstype.int32`` .

    Outputs:
        - **y** (Tensor) - indices of the elements in lower triangular part of matrix. The type specified by `dtype`.
          The shape of output is :math:`(2, tril\_size)`, where :math:`tril\_size` is the number of elements in the
          lower triangular matrix.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import ops
        >>> from mindspore import dtype as mstype
        >>> net = ops.TriuIndices(5, 4, 2, mstype.int64)
        >>> output = net()
        >>> print(output)
        [[0 0 1]
         [2 3 3]]
        >>> print(output.dtype)
        Int64
    """

    @prim_attr_register
    def __init__(self, row, col, offset=0, dtype=mstype.int32):
        """Initialize TriuIndices"""
        self.init_prim_io_names(inputs=[], outputs=['y'])
        validator.check_int(row, 0, validator.GE, "row", self.name)
        validator.check_int(col, 0, validator.GE, "col", self.name)
        validator.check_value_type("offset", offset, [int], self.name)
        valid_values = (mstype.int32, mstype.int64)
        validator.check_type_name("dtype", dtype, valid_values, self.name)


class Fmin(Primitive):
    """
    Computes the minimum of input tensors element-wise.

    Refer to :func:`mindspore.ops.fmin` for more detail.

    Supported Platforms:


    Examples:
        >>> x1 = Tensor(np.array([1.0, 5.0, 3.0]), mstype.float32)
        >>> x2 = Tensor(np.array([4.0, 2.0, 6.0]), mstype.float32)
        >>> fmin = ops.Fmin()
        >>> output = fmin(x1, x2)
        >>> print(output)
        [1. 2. 3.]
    """

    __mindspore_signature__ = (sig.sig_dtype.T, sig.sig_dtype.T)

    @prim_attr_register
    def __init__(self):
        """Initialize Fmin"""
        self.add_prim_attr('ignore_nan', True)
        self.init_prim_io_names(inputs=['x1, x2'], outputs=['y'])


class Fmax(Primitive):
    """
    Computes the maximum of input tensors element-wise.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Refer to :func:`mindspore.ops.fmax` for more detail.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x1 = Tensor(np.array([1.0, 5.0, 3.0]), mindspore.float32)
        >>> x2 = Tensor(np.array([4.0, 2.0, 6.0]), mindspore.float32)
        >>> fmax = ops.Fmax()
        >>> output = fmax(x1, x2)
        >>> print(output)
        [4. 5. 6.]
    """

    __mindspore_signature__ = (sig.sig_dtype.T, sig.sig_dtype.T)

    @prim_attr_register
    def __init__(self):
        """Initialize Fmax"""
        self.add_prim_attr('ignore_nan', True)
        self.init_prim_io_names(inputs=['x1, x2'], outputs=['y'])


class SelfAdjointEig(Primitive):
    r"""
    Computes the eigenvalues and (optionally) eigenvectors of each inner matrix in input
    such that input[..., :, :] = v[..., :, :] * diag(e[..., :]).
    The eigenvalues are sorted in non-decreasing order.

    Args:
         compute_v(bool): If ``True``  then eigenvectors will be computed and returned in v;
              If ``False`` , only the eigenvalues will be computed. Default: ``True`` .

    Inputs:
         - **x** (Tensor) - Must be one of the following types:
           float64, float32, complex64, complex128. Tensor input of shape :math:`[...,N, N]`.

    Outputs:
         - **eigen_value** (Tensor) - Has the same type as input, the shape is :math:`[...,N]`.
         - **eigen_vector** (Tensor) - If `compute_v` is `False`, it’s an empty tensor.
           Otherwise, it has the same type and shape as input, the shape is the same as the input.

    Raises:
         TypeError: If `compute_v` is not a bool.
         TypeError: If dtype of `x` is not one of: float64, float32, complex64 or complex128.
         TypeError: If `x` is not a Tensor.
         ValueError: If `x` is not a square(batch squares).

    Supported Platforms:
         ``CPU``

    Examples:
           >>> from mindspore.ops.operations.math_ops import SelfAdjointEig
           >>> input_x = Tensor(np.array([[1.0, 0.0], [0.0, 2.0]]).astype(np.float32))
           >>> SelfAdjointEig = SelfAdjointEig()
           >>> eigen_value, eigen_vector = SelfAdjointEig(input_x)
           >>> print(eigen_value)
           [1.  2.]
           >>> print(eigen_vector)
           [[1.  0.]
            [0.  1.]]
    """

    @prim_attr_register
    def __init__(self, compute_v=True):
        """Initialize SelfAdjointEig."""
        self.init_prim_io_names(inputs=['x'], outputs=['eigen_value', 'eigen_vector'])
        validator.check_value_type("compute_v", compute_v, [bool], self.name)


class Cauchy(Primitive):
    r"""
    Create a tensor of shape `size` with random numbers drawn from Cauchy distribution.
    It is defined as follows:

    .. math::
        f(x)= \frac{1}{\pi} \frac{\sigma}{(x-median)^2 +\sigma^2}

    Args:
        size (list[int]): The size of tensor.
        median (float, optional): the location parameter, specifying the location
            of the peak of the distribution. Default: 0.0.
        sigma (float, optional): the scale parameter which specifies the half-width
            at half-maximum. Default: 1.0.

    Outputs:
        Tensor with cauchy distribution data. Tensor shape is size, and data type is float32.

    Raises:
        TypeError: If `sigma` is not a float.
        TypeError: If `median` is not a float.
        TypeError: If `size` is not a list.
        ValueError: If `size` list is empty.
        ValueError: If data of `size` is not a positive integer.

    Supported Platforms:
        ``Ascend`` ``CPU``

    Examples:
        >>> size = [1]
        >>> net = ops.Cauchy(size)
        >>> y = net()
        >>> print(y)
        [0.03128606]
    """

    @prim_attr_register
    def __init__(self, size, median=0.0, sigma=1.0):
        validator.check_value_type('median', median, [float], self.name)
        validator.check_value_type('sigma', sigma, [float], self.name)
        validator.check_value_type('size', size, (list), self.name)
        for index, size_ in enumerate(size):
            validator.check_positive_int(size_, 'size[%d]' % index, self.name)


class Ormqr(Primitive):
    r"""
    Computes the matrix-matrix multiplication of a product of Householder matrices with a general matrix.
    Multiplies a(m, n) matrix C (given by other) with a matrix Q, where Q is represented using Householder
    reflectors (x, tau), which is the output of :func:`mindspore.ops.geqrf`.

    Refer to :func:`mindspore.ops.ormqr` for more details.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        left (bool, optional): controls the order of multiplication. If ``True`` , compute op(Q)*C.
            If ``False`` , compute C*op(Q). Default: ``True`` .
        transpose(bool, optional): controls whether the matrix Q is conjugate transposed or not.Default: ``False`` .

    Inputs:
        - **x** (Tensor) - Tensor of shape :math:`(*, mn, k)` where the value of mn depending on `left`,
          When `left` is ``True``, the value of mn is equal to m; otherwise, the value of mn is equal to n.
          and `*` is zero or more batch dimensions.
        - **tau** (Tensor) - Tensor of shape :math:`(*, min(mn, k))` where `*` is zero or more batch dimensions,
          and its type is the same as `x`.
        - **other** (Tensor) - Tensor of shape :math:`(*, m, n)` where `*` is zero or more batch dimensions,
          and its type is the same as `x`.

    Outputs:
        - **y** (Tensor) - the output Tensor, has the same shape and data type as `other`.

    Raises:
        TypeError: If `x` or `tau` or `other` is not Tensor.
        TypeError: If dtype of `x` or `tau` or `other` is not one of: float64, float32, complex64, complex128.
        ValueError: If `x` or `other` is less than 2D.
        ValueError: If rank(x) - rank(tau) != 1.
        ValueError: If tau.shape[:-1] != x.shape[:-2]
        ValueError: If other.shape[:-2] != x.shape[:-2]
        ValueError: If left == True, other.shape[-2] < tau.shape[-1].
        ValueError: If left == True, other.shape[-2] != x.shape[-2].
        ValueError: If left == False, other.shape[-1] < tau.shape[-1].
        ValueError: If left == False, other.shape[-1] != x.shape[-2].

    Supported Platforms:
        ``GPU``

    Examples:
        >>> import mindspore
        >>> import numpy as np
        >>> from mindspore import Tensor, ops
        >>> x = Tensor(np.array([[-114.6, 10.9, 1.1], [-0.304, 38.07, 69.38], [-0.45, -0.17, 62]]), mindspore.float32)
        >>> tau = Tensor(np.array([1.55, 1.94, 3.0]), mindspore.float32)
        >>> other = Tensor(np.array([[-114.6, 10.9, 1.1],
        ...                          [-0.304, 38.07, 69.38],
        ...                          [-0.45, -0.17, 62]]), mindspore.float32)
        >>> net = ops.Ormqr()
        >>> y = net(x, tau, other)
        >>> print(y)
        [[  63.82713   -13.823125 -116.28614 ]
         [ -53.659264  -28.157839  -70.42702 ]
         [ -79.54292    24.00183   -41.34253 ]]
    """

    @prim_attr_register
    def __init__(self, left=True, transpose=False):
        """Initialize Ormqr"""
        self.init_prim_io_names(inputs=['x', 'tau', 'other'], outputs=['y'])
        self.left = validator.check_value_type('left', left, [bool], self.name)
        self.transpose = validator.check_value_type('transpose', transpose, [bool], self.name)
        self.add_prim_attr('left', self.left)
        self.add_prim_attr('transpose', self.transpose)


        self.init_prim_io_names(inputs=['input_x'], outputs=['output'])
