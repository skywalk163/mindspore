mindspore.ops.prod
==================

.. py:function:: mindspore.ops.prod(input, axis=None, keep_dims=False)

    默认情况下，使用指定维度的所有元素的乘积代替该维度的其他元素，以移除该维度。也可仅缩小该维度大小至1。 `keep_dims` 控制输出和输入的维度是否相同。

    .. note::
        Tensor类型的 `axis` 仅用作兼容旧版本，不推荐使用。

    参数：
        - **input** (Tensor[Number]) - 输入Tensor，其数据类型为数值型。shape： :math:`(N, *)` ，其中 :math:`*` 表示任意数量的附加维度。
        - **axis** (Union[int, tuple(int), list(int), Tensor]) - 要减少的维度。默认值:  ``None`` ，缩小所有维度。只允许常量值。假设 `input` 的秩为r，取值范围[-r,r)。
        - **keep_dims** (bool) - 如果为 ``True`` ，则保留缩小的维度，大小为1。否则移除维度。默认值： ``False`` 。

    返回：
        Tensor。

        - 如果 `axis` 为 ``None`` ，且 `keep_dims` 为 ``False`` ，则输出一个零维Tensor，表示输入Tensor中所有元素的乘积。
        - 如果 `axis` 为int，取值为1，并且 `keep_dims` 为 ``False`` ，则输出的shape为 :math:`(input_0, input_2, ..., input_R)` 。
        - 如果 `axis` 为tuple(int)或list(int)，取值为(1, 2)，并且 `keep_dims` 为 ``False`` ，则输出Tensor的shape为 :math:`(input_0, input_3, ..., input_R)` 。
        - 如果 `axis` 为一维Tensor，例如取值为[1, 2]，并且 `keep_dims` 为 ``False`` ，则输出Tensor的shape为 :math:`(input_0, input_3, ..., input_R)` 。

    异常：
        - **TypeError** - `input` 不是Tensor。
        - **TypeError** - `axis` 不是以下数据类型之一：int、Tuple、list或Tensor。
        - **TypeError** - `keep_dims` 不是bool类型。
        - **ValueError** - `axis` 超出范围。
