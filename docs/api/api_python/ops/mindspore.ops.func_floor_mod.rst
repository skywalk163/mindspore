mindspore.ops.floor_mod
========================

.. py:function:: mindspore.ops.floor_mod(x, y)

    将第一个输入Tensor除以第二个输入Tensor，并向下取余。

    例如 :math:`floor(x / y) * y + mod(x, y) = x` 。

    `x` 和 `y` 的输入遵循隐式类型转换规则，使数据类型一致。当输入是两个Tensor时，它们的数据类型不能同时是bool，它们的shape可以广播。当输入是一个Tensor和一个Scalar时，Scalar只能是一个常量。

    .. math::
        out_{i} =\text{floor}(x_{i} // y_{i})

    其中 :math:`floor` 表示Floor算子，有关更多详细信息，请参阅 :class:`mindspore.ops.Floor` 算子。

    .. warning::
        - 输入 `y` 的元素不能等于0，否则将返回当前数据类型的最大值。
        - 当输入元素数量超过2048时，算子的精度不能保证千分之二的要求。
        - 由于架构不同，该算子在NPU和CPU上的计算结果可能不一致。
        - 如果shape表示为 :math:`(D1, D2 ..., Dn)` ，那么 D1\*D2... \*DN<=1000000,n<=8。

    参数：
        - **x** (Union[Tensor, Number, bool]) - 第一个输入，为数值型，或bool，或数据类型为数值型或bool的Tensor。
        - **y** (Union[Tensor, Number, bool]) - 第二个输入，为数值型，或bool，或数据类型为数值型或bool的Tensor。

    返回：
        Tensor，输出的shape与广播后的shape相同，数据类型取两个输入中精度较高或数字较高的。

    异常：
        - **TypeError** - 如果 `x` 和 `y` 不是以下之一：Tensor，number.Number或bool。
