mindspore.Tensor.repeat
=======================

.. py:method:: mindspore.Tensor.repeat(repeats, axis=None)

    对数组中的元素进行重复复制。

    参数：
        - **repeats** (Union[int, tuple, list]) - 每个元素的重复次数，`repeats` 被广播以适应指定轴的shape。
        - **axis** (int, 可选) - 轴方向上的重复值。默认情况下，使用展开的输入Tensor，并返回一个展开的输出Tensor。默认值： ``None`` 。

    返回：
        Tensor，除了维度外，与输入Tensor具有相同的shape。

    异常：
        - **ValueError** - 维度超出范围。
        - **TypeError** - 参数类型不匹配。

    其他API参考：
        - :func:`mindspore.Tensor.reshape` ：为Tensor指定新的shape，不更改Tensor的数据。
        - :func:`mindspore.Tensor.resize` ：就地改变Tensor的shape和size。
