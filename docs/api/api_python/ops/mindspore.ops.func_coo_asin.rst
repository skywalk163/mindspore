mindspore.ops.coo_asin
=======================

.. py:function:: mindspore.ops.coo_asin(x: COOTensor)

    逐元素计算输入COOTensor的反正弦。

    .. math::
        out_i = \sin^{-1}(x_i)

    参数：
        - **x** (COOTensor) - 输入的COOTensor。COOTensor的shape为 :math:`(N,*)` ，其中 :math:`*` 表示任意数量的附加维度。数据类型应该是以下类型之一：float16、float32、float64、complex64、complex128。

    返回：
        COOTensor，数据类型和shape与 `x` 相同。

    异常：
        - **TypeError** - 如果 `x` 不是COOTensor。
        - **TypeError** - 如果 `x` 的数据类型不是float16、float32、float64、complex64、complex128。
