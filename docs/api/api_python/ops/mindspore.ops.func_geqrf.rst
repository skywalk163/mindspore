mindspore.ops.geqrf
===================

.. py:function:: mindspore.ops.geqrf(input)

    将矩阵分解为正交矩阵 `Q` 和上三角矩阵 `R` 的乘积。该过程称为QR分解： :math:`A = QR` 。

    `Q` 和 `R` 矩阵都存储在同一个输出Tensor `y` 中。 `R` 的元素存储在对角线及上方。隐式定义矩阵 `Q` 的基本反射器（或户主向量）存储在对角线下方。

    此函数返回两个Tensor(`y`, `tau`)。

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    参数：
        - **input** (Tensor) - shape为 :math:`(*, m, n)` ，输入矩阵维度必须为大于等于两维，支持dtype为float32、float64、complex64、complex128。

    返回：
        - **y** (Tensor) - shape为 :math:`(*, m, n)` ，与 `input` 具有相同的dtype。
        - **tau** (Tensor) - shape为 :math:`(*, p)` ，并且 :math:`p = min(m, n)` ，与 `input` 具有相同的dtype。

    异常：
        - **TypeError** - 如果 `input` 不是一个Tensor。
        - **TypeError** - 如果 `input` 的dtype不是float32、float64、complex64、complext128中的一个。
        - **ValueError** - 如果 `input` 的维度小于2。
