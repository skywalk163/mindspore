mindspore.ops.randperm
========================

.. py:function:: mindspore.ops.randperm(n, seed=0, offset=0, dtype=mstype.int64)

    生成从 0 到 n-1 的整数随机排列。

    返回由 n 推断出的具有确定shape的Tensor，其中的随机数取自给定类型可以表示的数据范围。

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    参数：
        - **n** (Union[Tensor, int]) - 输入大小，如果为Tensor，则形状为()或(1,)，数据类型为int64。数值必须大于0。
        - **seed** (int, 可选) - 随机种子。默认值： ``0`` 。当seed为-1（只有负值）时，offset为0，随机数由时间决定。
        - **offset** (int, 可选) - 偏移量，生成随机数，优先级高于随机种子。 默认值： ``0`` 。必须是非负数。
        - **dtype** (mindspore.dtype, 可选) - 输出的类型。必须是以下类型之一：int32、int16、int8、uint8、int64、float64、float32、float16。默认值：``mstype.int64`` 。

    返回：
        Tensor，shape由参数 `n` 决定，dtype由参数 `dtype` 决定。

    异常：
        - **TypeError** - 如果 `dtype` 不是一个 `mstype.float_type` 类型。
        - **ValueError** - 如果 `n` 是负数或0。
        - **ValueError** - 如果 `seed` 不是非负整数。
        - **ValueError** - 如果 `n` 是超过指定数据类型的最大范围。
