mindspore.ops.bitwise_left_shift
=================================

.. py:function:: mindspore.ops.bitwise_left_shift(input, other)

    逐元素对输入 `input` 进行左移位运算, 移动的位数由 `other` 指定。

    .. math::

        \begin{aligned}
        &out_{i} =input_{i} << other_{i}
        \end{aligned}

    参数：
        - **input** (Union[Tensor, int, bool]) - 被左移的输入。
        - **other** (Union[Tensor, int, bool]) - 左移的位数。

    返回：
        Tensor，左移位运算后的结果。

    异常：
        - **TypeError** - `input` 或 `other` 都不是Tensor。
        - **TypeError** - `input` 或 `other` 不是bool、int、int类型的Tensor或uint类型的Tensor。
        - **TypeError** - `input` 和 `other` 的数据类型不相同。
        - **ValueError** - `input` 的shape 与 `other` 的shape不能进行广播。
