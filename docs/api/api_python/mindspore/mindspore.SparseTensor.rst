mindspore.SparseTensor
======================

.. py:class:: mindspore.SparseTensor(indices, values, shape)

    用来表示某一张量在给定索引上非零元素的集合。

    `SparseTensor` 只能在 `Cell` 的构造方法中使用。

    .. note::
        此接口从 1.7 版本开始弃用，并计划在将来移除。请使用 :class:`mindspore.COOTensor`。

    对于稠密张量，其 `SparseTensor(indices, values, shape)` 具有 `dense[indices[i]] = values[i]` 。

    参数：
        - **indices** (Tensor) - shape为 :math:`(N, ndims)` 的二维整数张量，其中N和ndims分别表示稀疏张量中 `values` 的数量和SparseTensor维度的数量。
        - **values** (Tensor) - shape为 :math:`(N)` 的一维张量，其内部可以为任何数据类型，用来给 `indices` 中的每个元素提供数值。
        - **shape** (tuple(int)) - shape为 :math:`(ndims)` 的整数元组，用来指定稀疏矩阵的稠密形状。

    返回：
        SparseTensor，由 `indices` 、 `values` 和 `shape` 组成。

    .. py:method:: indices
        :property:

        返回SparseTensor的索引值。

    .. py:method:: shape
        :property:

        返回SparseTensor的稠密shape。

    .. py:method:: values
        :property:

        返回SparseTensor的非零元素值。
