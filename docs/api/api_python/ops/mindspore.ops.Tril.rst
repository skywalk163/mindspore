mindspore.ops.Tril
===================

.. py:class:: mindspore.ops.Tril(diagonal=0)

    返回单个或一批二维矩阵下三角形部分，其他位置的元素将被置零。
    矩阵的下三角形部分定义为对角线本身和对角线以下的元素。

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    参数：
        - **diagonal** (int，可选) - 指定对角线位置，默认值： ``0`` ，指定主对角线。

    输入：
        - **x** (Tensor) - 输入Tensor。shape为 :math:`(M, N, *)` ，其中 :math:`*` 为任意数量的额外维度。

    输出：
        Tensor，其数据类型和shape维度与输入 `x` 相同。

    异常：
        - **TypeError** - 如果 `x` 不是Tensor。
        - **TypeError** - 如果 `diagonal` 不是int类型。
        - **ValueError** - 如果 `x` 的秩小于2。
