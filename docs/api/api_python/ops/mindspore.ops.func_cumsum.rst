mindspore.ops.cumsum
====================

.. py:function:: mindspore.ops.cumsum(x, axis, dtype=None)

    计算输入Tensor `x` 沿轴 `axis` 的累积和。

    .. math::
        y_i = x_1 + x_2 + x_3 + ... + x_i

    .. note::
        目前Ascend平台上，对于静态shape的场景， `x` 的数据类型暂仅支持：int8、uint8、int32，float32和float16；对于动态shape的场景， `x` 的数据类型暂仅支持：int32、float32和float16。

    参数：
        - **x** (Tensor) - 输入Tensor。shape为 :math:`(N, *)` ，其中 :math:`*` 为任意数量的额外维度。
        - **axis** (int) - 累积和计算的轴。
        - **dtype** (:class:`mindspore.dtype`, 可选) - 输出数据类型。如果不为None，则输入会转化为 `dtype`。这有利于防止数值溢出。如果为None，则输出和输入的数据类型一致。默认值： ``None`` 。

    返回：
        Tensor，和输入Tensor的shape相同。

    异常：
        - **TypeError** - 如果 `x` 不是Tensor。
        - **ValueError** - 如果 `axis` 超出范围。
