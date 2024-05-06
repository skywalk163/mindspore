mindspore.ops.triu_indices
==========================

.. py:function:: mindspore.ops.triu_indices(row, col, offset=0, *, dtype=mstype.int64)

    计算 `row` * `col` 行列矩阵的上三角元素的索引，并将它们作为一个 2xN 的Tensor返回。张量的第一行包含行坐标，第二行包含列坐标。坐标按行排序，然后按列排序。

    矩阵的上三角部分包括对角线及其以上的所有元素。

    .. note::
        在CUDA上运行的时候， `row` * `col` 必须小于2^59以防止计算时溢出。

    参数：
        - **row** (int) - 2-D 矩阵的行数。
        - **col** (int) - 2-D 矩阵的列数。
        - **offset** (int，可选) - 对角线偏移量。默认值： ``0`` 。

    关键字参数：
        - **dtype** (:class:`mindspore.dtype`，可选) - 指定输出Tensor数据类型，支持的数据类型为 ``mstype.int32`` 和 ``mstype.int64`` ，默认值： ``mstype.int64`` 。

    返回：
        - **y** (Tensor) - 矩阵的上三角形部分的索引。数据类型由 `dtype` 指定，shape为 :math:`(2, triu\_size)` ，其中， :math:`triu\_size` 为上三角矩阵的元素总数。

    异常：
        - **TypeError** - 如果 `row` 、 `col` 或 `offset` 不是int。
        - **TypeError** - 如果 `dtype` 的类型不是int32或int64。
        - **ValueError** - 如果 `row` 或者 `col` 小于零。
