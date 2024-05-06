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
"""ms function for mixed precision."""
from __future__ import absolute_import

import os
from abc import ABC, abstractmethod
from mindspore.common import mutable
from mindspore.ops._primitive_cache import _get_cache_prim
from mindspore.ops.operations.math_ops import NPUGetFloatStatusV2, NPUClearFloatStatusV2
from mindspore import _checkparam as validator
from mindspore._c_expression import MSContext
from .common import dtype as mstype
from . import context
from . import ops
from .ops import constexpr
from .common.api import jit_class, jit
from .common.parameter import Parameter
from .common.tensor import Tensor
from .train.loss_scale_manager import DynamicLossScaleManager, LossScaleManager, FixedLossScaleManager
from .train.amp import build_train_network, auto_mixed_precision, custom_mixed_precision,\
    get_white_list, get_black_list


_hypermap = ops.HyperMap()
_partial = ops.Partial()


@constexpr
def _ascend_target():
    return context.get_context("device_target") == "Ascend"


@constexpr
def _ascend_910a_target():
    return MSContext.get_instance().get_ascend_soc_version() == "ascend910"


@constexpr
def _ascend_910bc_target():
    return MSContext.get_instance().get_ascend_soc_version() in ["ascend910b", "ascend910c"]


@constexpr
def _gpu_target():
    return context.get_context("device_target") == "GPU"


def _grad_unscale(scale, grad):
    return grad * ops.Reciprocal()(scale).astype(grad.dtype)


def _grad_scale(scale, grad):
    return grad * scale.astype(grad.dtype)


@jit
def _grad_scale_map(scale_value, inputs):
    return _hypermap(_partial(_grad_scale, scale_value), inputs)


@jit
def _grad_unscale_map(scale_value, inputs):
    return _hypermap(_partial(_grad_unscale, scale_value), inputs)


def _overflow(inputs):
    if _gpu_target():
        return ops.FloatStatus()(inputs)
    status = ops.isfinite(inputs)
    return 1 - status.all()


@jit
def _all_finite(inputs, check_overflow_mode):
    """all finite check"""
    if _ascend_target():
        if (_ascend_910a_target()) or \
           (_ascend_910bc_target() and check_overflow_mode == "SATURATION_MODE"):
            status = Tensor([0] * 8, mstype.int32)
            status = ops.depend(status, inputs)
            get_status = _get_cache_prim(NPUGetFloatStatusV2)()(status)
            status = ops.depend(status, get_status)
            clear_status = _get_cache_prim(NPUClearFloatStatusV2)()(status)
            get_status = ops.depend(get_status, clear_status)
            status_finite = get_status.equal(Tensor(0, mstype.int32)).all()
            return status_finite

    outputs = _hypermap(_partial(_overflow), inputs)
    flag_sum = ops.addn(outputs).reshape(())
    status_finite = ops.less(flag_sum, 1)
    return status_finite


def all_finite(inputs):
    r"""
    Returns a scalar Tensor indicating whether the inputs are finite.

    .. warning::
        This is an experimental API that is subject to change or deletion.

        The interface must be used in whole network training scenario to detect
        whether grads are finite, and the results may be different on different
        device targets.

    Args:
        inputs (Union(tuple(Tensor), list(Tensor))): a iterable Tensor.

    Returns:
        Tensor, a scalar Tensor and the dtype is bool.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> from mindspore import amp, Tensor
        >>> import numpy as np
        >>> x = (Tensor(np.array([np.log(-1), 1, np.log(0)])), Tensor(np.array([1.0])))
        >>> output = amp.all_finite(x)

    Tutorial Examples:
        - `Automatic Mix Precision - Loss Scaling
          <https://mindspore.cn/tutorials/en/master/advanced/mixed_precision.html#loss-scaling>`_
    """
    inputs = mutable(inputs)
    _check_overflow_mode = os.environ.get('MS_ASCEND_CHECK_OVERFLOW_MODE')
    return _all_finite(inputs, _check_overflow_mode)


@jit_class
class LossScaler(ABC):
    r"""
    Loss scaler abstract class when using mixed precision.

    Derived class needs to implement all of its methods. During training, `scale` and `unscale` is used
    to scale and unscale the loss value and gradients to avoid overflow, `adjust` is used to update the
    loss scale value.

    For more information, refer to the `tutorials  <https://mindspore.cn/tutorials/en/master/advanced/
    mixed_precision.html#loss-scaling>`_.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Examples:
        >>> from mindspore.amp import LossScaler, _grad_scale_map, _grad_unscale_map
        >>> from mindspore import ops, Parameter, Tensor
        >>> from mindspore.common import dtype as mstype
        >>>
        >>> class MyLossScaler(LossScaler):
        ...     def __init__(self, scale_value):
        ...         self.scale_value = Parameter(Tensor(scale_value, dtype=mstype.float32), name="scale_value")
        ...
        ...     def scale(self, inputs):
        ...         inputs = mutable(inputs)
        ...         return _grad_scale_map(self.scale_value, inputs)
        ...
        ...     def unscale(self, inputs):
        ...         inputs = mutable(inputs)
        ...         return _grad_unscale_map(self.scale_value, inputs)
        ...
        ...     def adjust(self, grads_finite):
        ...         scale_mul_factor = self.scale_value * self.scale_factor
        ...         scale_value = ops.select(grads_finite, scale_mul_factor, self.scale_value)
        ...         ops.assign(self.scale_value, scale_value)
        ...         return True
        >>>
        >>> loss_scaler = MyLossScaler(1024)
    """
    @abstractmethod
    def scale(self, inputs):
        """
        Scaling inputs by `scale_value`.

        Args:
            inputs (Union(Tensor, tuple(Tensor))): the input loss value or gradients.
        """
        raise NotImplementedError

    @abstractmethod
    def unscale(self, inputs):
        """
        Unscaling inputs by `scale_value`.

        Args:
            inputs (Union(Tensor, tuple(Tensor))): the input loss value or gradients.
        """
        raise NotImplementedError

    @abstractmethod
    def adjust(self, grads_finite):
        """
        Adjust the `scale_value` dependent on whether grads are finite.

        Args:
            grads_finite (Tensor): a scalar bool Tensor indicating whether the grads are finite.
        """
        raise NotImplementedError


class StaticLossScaler(LossScaler):
    r"""
    Static Loss scale class.

    Scales and unscales loss or gradients by a fixed constant.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        scale_value (Union(float, int)): The initial loss scale value.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import amp, Tensor
        >>> import numpy as np
        >>> loss_scaler = amp.StaticLossScaler(scale_value=2**10)
        >>> loss_value = Tensor([1.], mindspore.float32)
        >>> scaled_loss_value = loss_scaler.scale(loss_value)
        >>> print(scaled_loss_value)
        [1024.]
        >>> grads = (Tensor(np.array([1.5, 1.0]), mindspore.float16),
        ...      Tensor(np.array([1.2]), mindspore.float16))
        >>> unscaled_grads = loss_scaler.unscale(grads)
        >>> print(unscaled_grads)
        (Tensor(shape=[2], dtype=Float16, value= [ 1.4648e-03,  9.7656e-04]),
        Tensor(shape=[1], dtype=Float16, value= [ 1.1721e-03]))
    """
    def __init__(self, scale_value):
        scale_value = validator.check_value_type("scale_value", scale_value, [float, int])
        if scale_value < 1.0:
            raise ValueError("The argument 'scale_value' must be > 1, but got {}".format(scale_value))
        self.scale_value = Parameter(Tensor(scale_value, dtype=mstype.float32), name="scale_value")

    def scale(self, inputs):
        """
        Scaling inputs by `scale_value`.

        Args:
            inputs (Union(Tensor, tuple(Tensor))): the input loss value or gradients.

        Returns:
            Union(Tensor, tuple(Tensor)), the scaled value.
        """
        inputs = mutable(inputs)
        return _grad_scale_map(self.scale_value, inputs)

    def unscale(self, inputs):
        """
        Unscaling inputs by `scale_value`.

        Args:
            inputs (Union(Tensor, tuple(Tensor))): the input loss value or gradients.

        Returns:
            Union(Tensor, tuple(Tensor)), the unscaled value.
        """
        inputs = mutable(inputs)
        return _grad_unscale_map(self.scale_value, inputs)

    def adjust(self, grads_finite):
        """
        Adjust `scale_value` in `LossScaler`. `scale_value` is fixed in `StaticLossScaler`, so this method
        return False directly.

        Args:
            grads_finite (Tensor): a scalar bool Tensor indicating whether the grads are finite.
        """
        return False


class DynamicLossScaler(LossScaler):
    r"""
    Dynamic Loss scale class.

    Dynamic loss scaling tries to determine the largest loss scale value that
    will keep gradients finite. It does this by increasing the loss scale every
    `scale_window` steps by `factor` if the grads remain finite, otherwise it reduces
    the loss scale by `1 / factor` and resets the counter.

    .. warning::
        This is an experimental API that is subject to change or deletion.

    Args:
        scale_value (Union(float, int)): The initial loss scale value.
        scale_factor (int): The scale factor.
        scale_window (int): Maximum continuous training steps that do not have
            overflow to increase the loss scale.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import mindspore
        >>> from mindspore import amp, Tensor
        >>> import numpy as np
        >>> loss_scaler = amp.DynamicLossScaler(scale_value=2**10, scale_factor=2, scale_window=1)
        >>> grads = (Tensor(np.array([np.log(-1), 1.0]), mindspore.float16),
        ...             Tensor(np.array([0.2]), mindspore.float16))
        >>> unscaled_grads = loss_scaler.unscale(grads)
        >>> grads_finite = amp.all_finite(unscaled_grads)
        >>> loss_scaler.adjust(grads_finite)
        True
        >>> print(loss_scaler.scale_value.asnumpy())
        512.0
    """
    def __init__(self, scale_value, scale_factor, scale_window):
        scale_value = validator.check_value_type("scale_value", scale_value, [float, int])
        if scale_value < 1.0:
            raise ValueError("The argument 'scale_value' must be > 1, but got {}".format(scale_value))
        self.scale_value = Parameter(Tensor(scale_value, dtype=mstype.float32), name="scale_value")
        self.scale_window = validator.check_positive_int(scale_window, "scale_window")
        self.scale_factor = validator.check_positive_int(scale_factor, "scale_factor")
        self.counter = Parameter(Tensor(0, dtype=mstype.int32), name="counter")

    def scale(self, inputs):
        """
        Scaling inputs by `scale_value`.

        Args:
            inputs (Union(Tensor, tuple(Tensor))): the input loss value or gradients.

        Returns:
            Union(Tensor, tuple(Tensor)), the scaled value.

        Tutorial Examples:
            - `Automatic Mix Precision - Loss Scaling
              <https://mindspore.cn/tutorials/en/master/advanced/mixed_precision.html#loss-scaling>`_
        """
        inputs = mutable(inputs)
        return _grad_scale_map(self.scale_value, inputs)

    def unscale(self, inputs):
        """
        Unscaling inputs by `scale_value`.

        Args:
            inputs (Union(Tensor, tuple(Tensor))): the input loss value or gradients.

        Returns:
            Union(Tensor, tuple(Tensor)), the unscaled value.

        Tutorial Examples:
            - `Automatic Mix Precision - Loss Scaling
              <https://mindspore.cn/tutorials/en/master/advanced/mixed_precision.html#loss-scaling>`_
        """
        inputs = mutable(inputs)
        return _grad_unscale_map(self.scale_value, inputs)

    def adjust(self, grads_finite):
        """
        Adjust the `scale_value` dependent on whether grads are finite.

        Args:
            grads_finite (Tensor): a scalar bool Tensor indicating whether the grads are finite.

        Tutorial Examples:
            - `Automatic Mix Precision - Loss Scaling
              <https://mindspore.cn/tutorials/en/master/advanced/mixed_precision.html#loss-scaling>`_
        """
        one = ops.ones((), self.scale_value.dtype)
        scale_mul_factor = self.scale_value * self.scale_factor
        scale_value = ops.select(
            grads_finite,
            ops.select(
                self.counter == (self.scale_window - 1),
                ops.select(ops.isfinite(scale_mul_factor),
                           scale_mul_factor,
                           self.scale_value),
                self.scale_value),
            ops.maximum(one, self.scale_value / self.scale_factor))
        ops.assign(self.scale_value, scale_value)

        counter = ((self.counter + 1) % self.scale_window) * grads_finite
        ops.assign(self.counter, counter)
        return True

__all__ = [
    "DynamicLossScaleManager", "LossScaleManager", "FixedLossScaleManager",
    "build_train_network", "DynamicLossScaler", "StaticLossScaler", "LossScaler",
    "auto_mixed_precision", "all_finite", "custom_mixed_precision",
    "get_white_list", "get_black_list"
]
