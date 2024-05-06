mindspore.ops.floor_divide
==========================

.. py:function:: mindspore.ops.floor_divide(input, other)

    按元素将第一个输入Tensor除以第二个输入Tensor，并向下取整。

    `input` 和 `other` 的输入遵循隐式类型转换规则，使数据类型一致。输入必须是两个Tensor或一个Tensor和一个Scalar。当输入是两个Tensor时，它们的数据类型不能同时为bool，其shape可以广播。当输入是一个Tensor和一个Scalar时，Scalar只能是一个常量。

    .. math::
        out_{i} = \text{floor}( \frac{input_i}{other_i})

    其中 :math:`floor` 表示Floor算子，有关更多详细信息，请参阅 :class:`mindspore.ops.Floor` 算子。

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    参数：
        - **input** (Union[Tensor, Number, bool]) - 第一个输入，为数值型，或bool，或数据类型为数值型或bool的Tensor。
        - **other** (Union[Tensor, Number, bool]) - 第二个输入，为数值型，或bool，或数据类型为数值型或bool的Tensor。

    返回：
        Tensor，输出的shape与广播后的shape相同，数据类型取两个输入中精度较高或数字较高的。

    异常：
        - **TypeError** - 如果 `input` 和 `other` 不是以下之一: Tensor，number.Number或bool。
