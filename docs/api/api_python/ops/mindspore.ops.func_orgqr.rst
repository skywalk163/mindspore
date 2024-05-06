mindspore.ops.orgqr
====================

.. py:function:: mindspore.ops.orgqr(input, input2)

    计算 :class:`mindspore.ops.Geqrf` 返回的正交矩阵 :math:`Q` 的显式表示。

    下面以输入无batch维的情况为例， 计算 `Householder <https://en.wikipedia.org/wiki/Householder_transformation#Householder_matrix>`_ 矩阵的前 :math:`N` 列。
    假设输入 `input` 的shape经过 `Householder转换 <https://en.wikipedia.org/wiki/Householder_transformation#Householder_matrix>`_ 之后为：:math:`(M, N)` 。
    当 `input` 的对角线被置为1， `input` 中下三角形的每一列都表示为： :math:`w_j` ，其中 :math:`j` 在 :math:`j=1, \ldots, M` 范围内，此函数返回Householder矩阵乘积的前 :math:`N` 列：

    .. math::
        H_{1} H_{2} \ldots H_{k} \quad \text { with } \quad H_{j}=\mathrm{I}_{M}-\tau_{j} w_{j} w_{j}^{\mathrm{H}}

    其中：:math:`\mathrm{I}_{M}` 是 :math:`M` 维单位矩阵。当 :math:`w` 是复数的时候，:math:`w^{\mathrm{H}}` 是共轭转置，否则是一般转置。输出矩阵的shape与输入矩阵 `input` 相同。
    :math:`tau` 即输入 `input2` 。

    参数：
        - **input** (Tensor) - shape :math:`(*, M, N)` 的Tensor，表示二维或者三维矩阵。数据类型为float32、float64、complex64或者complex128。
        - **input2** (Tensor) - Householder转换的反射系数，其shape为 :math:`(*, K)` ，其中 `K` 小于等于 `N` 。数据类型与 `input` 一致。

    返回：
        Tensor，数据类型与shape与 `input` 一致。

    异常：
        - **TypeError** - `input` 或者 `input2` 不是Tensor。
        - **TypeError** -  `input` 和 `input2` 的数据类型不是float64、float32、complex64或者complex128。
        - **ValueError** -  `input` 和 `input2` 的batch维度不同。
        - **ValueError** - `input`.shape[-2] < `input`.shape[-1]。
        - **ValueError** - `input`.shape[-1] < `input2`.shape[-1]。
        - **ValueError** - rank(`input`) - rank(`input2`) != 1。
        - **ValueError** - rank(`input`) != 2 or 3。
