mindspore.ops.gcd
=================

.. py:function:: mindspore.ops.gcd(input, other)

    按元素计算输入Tensor的最大公约数。
    两个输入的shape应该是可广播的，它们的数据类型应该是：int32，int64之一。

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    参数：
        - **input** (Tensor) - 第一个输入。
        - **other** (Tensor) - 第二个输入。

    返回：
        Tensor，返回的shape与广播后的shape，数据类型为两个输入中数字精度较高的类型。

    异常：
        - **TypeError** - 如果 `input` 或 `other` 的数据类型既不是int32也不是int64。
        - **ValueError** - 如果两个输入的shape不可广播。
