mindspore.ops.sort
==================

.. py:function:: mindspore.ops.sort(input_x, axis=-1, descending=False)

    按指定顺序对输入Tensor的指定维上的元素进行排序。

    参数：
        - **input_x** (Tensor) - 进行排序的Tensor，shape为 :math:`(N,*)` ，其中 :math:`*` 表示任意数量的的额外维度。
        - **axis** (int，可选) - 指定排序的轴。默认值：``-1``，表示指定最后一维。当前Ascend后端只支持对最后一维进行排序。
        - **descending** (bool，可选) - 按降序还是升序。如果为 ``True`` ，则元素按降序排列，否则按升序排列。默认值： ``False`` 。

    .. warning::
        目前能良好支持的数据类型有：float16、uint8、int8、int16、int32、int64。如果使用float32，可能产生精度误差。

    返回：
        - **y1** (Tensor) - 排序后的值，其shape和数据类型与输入一致。
        - **y2** (Tensor) - 返回值在原输入Tensor里对应的索引，数据类型为int32。

    异常：
        - **TypeError** - `axis` 不是int类型。
        - **TypeError** -  `descending` 不是bool类型。
        - **TypeError** - `input_x` 不是float16、float32、uint8、int8、int16、int32或int64。
        - **ValueError** - `axis` 不在[-len(input_x.shape), len(input_x.shape))范围内。
