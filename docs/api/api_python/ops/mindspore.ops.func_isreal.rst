mindspore.ops.isreal
====================

.. py:function:: mindspore.ops.isreal(input)

    逐元素判断是否为实数。
    一个复数的虚部是0时也被看作是实数。

    参数：
        - **input** (Tensor) - 输入Tensor。

    返回：
        Tensor，对应 `input` 元素为实数的位置是 ``true`` ，反之为 ``false`` 。

    异常：
        - **TypeError** - `input` 不是Tensor。
