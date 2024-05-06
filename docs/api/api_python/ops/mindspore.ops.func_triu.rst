mindspore.ops.triu
===================

.. py:function:: mindspore.ops.triu(input, diagonal=0)

    返回输入Tensor `input` 的上三角形部分(包含对角线和下面的元素)，并将其他元素设置为0。

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    参数：
        - **input** (Tensor) - shape是 :math:`(M, N, *)` 的Tensor，其中*表示任意数量的维度。
        - **diagonal** (int，可选) - 指定对角线位置，默认值： ``0`` ，指定主对角线。

    返回：
        Tensor，其数据类型和shape与 `input` 相同。

    异常：
        - **TypeError** - 如果 `diagonal` 不是int。
        - **TypeError** - 如果 `input` 不是Tensor。
        - **ValueError** - 如果 `input` 的shape的长度小于2。
