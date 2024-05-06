mindspore.ops.less_equal
========================

.. py:function:: mindspore.ops.less_equal(input, other)

    逐元素计算 :math:`input <= other` 的bool值。

    .. math::
        out_{i} =\begin{cases}
            & \text{True,    if } input_{i}<=other_{i} \\
            & \text{False,   if } input_{i}>other_{i}
            \end{cases}

    .. note::
        - 输入 `input` 和 `other` 遵循隐式类型转换规则，使数据类型保持一致。
        - 当输入是一个Tensor和一个Scalar时，Scalar只能是一个常数。

    参数：
        - **input** (Union[Tensor, Number, bool]) - 第一个输入，为数值型，或bool，或数据类型为数值型或bool的Tensor。
        - **other** (Union[Tensor, Number, bool]) - 第二个输入，为数值型，或bool，或数据类型为数值型或bool的Tensor。

    返回：
        Tensor，shape与广播后的shape相同，数据类型为bool。

    异常：
       - **TypeError** - 如果 `input` 和 `other` 不是以下之一：Tensor、数值型、bool。