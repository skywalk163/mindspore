mindspore.ops.logaddexp
=======================

.. py:function:: mindspore.ops.logaddexp(input, other)

    计算输入的指数和的对数。
    该函数可以很好地解决统计学中计算得到的事件概率可能小到超过了正常浮点数表达的范围的问题。

    .. math::

        out_i = \log(exp(input_i) + \exp(other_i))

    参数：
        - **input** (Tensor) - 输入Tensor，其数据类型必须是float。
        - **other** (Tensor) - 输入Tensor，其数据类型必须是float。如果 `input` 的shape不等于 `other` 的shape，它们必须被广播成相同shape(输出的形状)。

    返回：
        Tensor。

    异常：
        - **TypeError** - `input` 或 `other` 不是Tensor。
        - **TypeError** - `input` 或 `other` 的数据类型不是float。
