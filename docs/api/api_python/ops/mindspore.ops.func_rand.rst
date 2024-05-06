mindspore.ops.rand
===================

.. py:function:: mindspore.ops.rand(*size, dtype=None, seed=None)

    返回一个Tensor，shape和dtype由输入决定，其元素为服从均匀分布的 :math:`[0, 1)` 区间的数字。

    参数：
        - **size** (Union[int, tuple(int), list(int)]) - 输出的Tensor的shape，例如，:math:`(2, 3)` or :math:`2`。

    关键字参数：
        - **dtype** (:class:`mindspore.dtype`，可选) - 指定的输出Tensor的dtype，必须是float类型。如果是None，`mindspore.float32` 会被使用。默认值： ``None`` 。
        - **seed** (int，可选) - 随机种子，必须大于或等于0。默认值： ``None`` ，值将取 ``0`` 。

    返回：
        Tensor，shape和dtype由输入决定其元素为服从均匀分布的 :math:`[0, 1)` 区间的数字。

    异常：
        - **TypeError** - 如果 `seed` 不是非负整数。
        - **ValueError** - 如果 `dtype` 不是一个 `mstype.float_type` 类型。
