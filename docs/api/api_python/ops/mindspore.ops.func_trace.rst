mindspore.ops.trace
===================

.. py:function:: mindspore.ops.trace(input)

    返回 `input` 的主对角线方向上的总和。

    .. note::
        输入必须是Tensor，复数类型暂不支持。

    参数：
        - **input** (Tensor) - 二维Tensor。

    返回：
        Tensor，其数据类型与 `input` 一致，含有一个元素。

    异常：
        - **TypeError** - 如果 `input` 不是Tensor。
        - **ValueError** - 如果当 `input` 的维度不是2。