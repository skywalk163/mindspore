mindspore.ops.truncate_div
==========================

.. py:function:: mindspore.ops.truncate_div(x, y)

    将第一个输入Tensor与第二个输入Tensor逐元素相除，结果将向0取整。相当于C语言风格的整数除法。

    输入 `x` 和 `y` 应能遵循隐式类型转换规则使数据类型一致。
    当输入为两个Tensor时，数据类型不能同时为bool类型。
    当输入是一个Tensor和一个标量时，标量只能是一个常数。

    .. note::
        支持shape广播。

    参数：
        - **x** (Union[Tensor, Number, bool]) - Number或bool类型的Tensor。
        - **y** (Union[Tensor, Number, bool]) - Number或bool类型的Tensor。`x` 和 `y` 不能同时为bool类型。

    返回：
        Tensor，shape为输入广播后的shape，数据类型为两个输入中精度较高的输入的类型。

    异常：
        - **TypeError** - `x` 和 `y` 数据类型不是以下之一：Tensor、Number或bool。
