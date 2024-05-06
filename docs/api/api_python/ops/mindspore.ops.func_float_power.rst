mindspore.ops.float_power
==========================

.. py:function:: mindspore.ops.float_power(input, exponent)

    计算 `input` 的指数幂。对于实数类型，转换 `input` 和 `exponent` 到mindspore.float64计算。目前不支持复数运算。

    参数：
        - **input** (Union[Tensor, Number]) - 第一个输入，为Tensor或数值型数据类型。
        - **exponent** (Union[Tensor, Number]) - 第二个输入，如果第一个输入是Tensor，第二个输入可以是Number或Tensor。否则，必须是Tensor。

    返回：
        Tensor，输出的shape与广播后的shape相同。对于实数运算，返回类型为mindspore.float64。

    异常：
        - **TypeError** - `input` 和 `exponent` 都不是Tensor。
        - **TypeError** - `input` 或 `exponent` 数据类型不是Tensor或Number。
