mindspore.ops.coo_asinh
========================

.. py:function:: mindspore.ops.coo_asinh(x: COOTensor)

    计算COOTensor输入元素的反双曲正弦。

    .. math::

        out_i = \sinh^{-1}(input_i)

    参数：
        - **x** (COOTensor) - 需要计算反双曲正弦函数的输入。

    返回：
        COOTensor，数据类型和shape与 `x` 相同。

    异常：
        - **TypeError** - 如果 `x` 不是COOTensor。
