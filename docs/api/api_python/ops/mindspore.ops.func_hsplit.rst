mindspore.ops.hsplit
=====================

.. py:function:: mindspore.ops.hsplit(input, indices_or_sections)

    水平地将输入Tensor分割成多个子Tensor。等同于 :math:`axis=1` 时的 `ops.tensor_split` 。

    参数：
        - **input** (Tensor) - 待分割的Tensor。
        - **indices_or_sections** (Union[int, tuple(int), list(int)]) - 参考 :func:`mindspore.ops.tensor_split`。

    返回：
        tuple[Tensor]。

    异常：
        - **TypeError** - 如果 `input` 不是Tensor。
        - **ValueError** - 如果 `input` 的维度小于2。
