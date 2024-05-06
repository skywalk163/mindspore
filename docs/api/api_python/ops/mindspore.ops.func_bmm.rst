mindspore.ops.bmm
=================

.. py:function:: mindspore.ops.bmm(input_x, mat2)

    基于batch维度的两个Tensor的矩阵乘法。

    .. math::
        \text{output}[..., :, :] = \text{matrix}(input_x[..., :, :]) * \text{matrix}(mat2[..., :, :])

    `input_x` 的维度不能小于 `3` ， `mat2` 的维度不能小于 `2` 。

    参数：
        - **input_x** (Tensor) - 输入相乘的第一个Tensor。其shape为 :math:`(*B, N, C)` ，其中 :math:`*B` 表示批处理大小，可以是多维度， :math:`N` 和 :math:`C` 是最后两个维度的大小。
        - **mat2** (Tensor) - 输入相乘的第二个Tensor。Tensor的shape为 :math:`(*B, C, M)` 。

    返回：
        Tensor，输出Tensor的shape为 :math:`(*B, N, M)` 。

    异常：
        - **ValueError** - `input_x` 的维度小于 `3` 或者 `mat2` 的维度小于2。
        - **ValueError** - `input_x` 第三维的长度不等于 `mat2` 第二维的长度。

