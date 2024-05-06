mindspore.ops.log1p
===================

.. py:function:: mindspore.ops.log1p(input)

    对输入Tensor逐元素加一后计算自然对数。

    .. math::
        out_i = \{log_e}(input_i + 1)

    参数：
        - **input** (Tensor) - 输入Tensor。其值必须大于-1。

    返回：
        Tensor，与 `input` 的shape相同。

    异常：
        - **TypeError** - `input` 不是Tensor。
