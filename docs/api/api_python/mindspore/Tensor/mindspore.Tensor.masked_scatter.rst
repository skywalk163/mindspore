mindspore.Tensor.masked_scatter
===============================

.. py:method:: mindspore.Tensor.masked_scatter(mask, x)

    返回一个Tensor。根据mask, 使用 `tensor` 中的值，更新Tensor本身的值，`mask` 和Tensor本身的shape必须相等或者 `mask` 是可广播的。

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    参数：
        - **mask** (Tensor[bool]) - 一个bool Tensor, 其shape可以被广播到Tensor本身。
        - **x** (Tensor) - 一个Tensor, 其数据类型与Tensor本身相同。 `tensor` 中的元素数量必须大于等于 `mask` 中的True元素
          的数量。

    返回：
        Tensor，其数据类型和shape与Tensor本身相同。

    异常：
        - **TypeError** - 如果 `mask` 或者 `x` 不是Tensor。
        - **TypeError** - 如果Tensor本身的数据类型不被支持。
        - **TypeError** - 如果 `mask` 的dtype不是bool。
        - **TypeError** - 如果Tensor本身的维度数小于 `mask` 的维度数。
        - **ValueError** - 如果 `mask` 不能广播到Tensor本身。
        - **ValueError** - 如果 `x` 中的元素数目小于Tensor本身需要更新的元素数目。
