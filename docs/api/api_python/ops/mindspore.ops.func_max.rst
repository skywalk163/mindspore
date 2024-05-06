mindspore.ops.max
=================

.. py:function:: mindspore.ops.max(input, axis=None, keepdims=False, *, initial=None, where=None)

    在给定轴上计算输入Tensor的最大值。并且返回最大值和索引。

    .. note::
        - 在auto_parallel和semi_auto_parallel模式下，不能使用第一个输出索引。
        - `axis` 为 ``None`` 时，`keepdims` 及以后参数均不会生效，同时索引固定返回0。

    .. warning::
        - 如果有多个最大值，则取第一个最大值的索引。

    参考：:class:`mindspore.ops.ArgMaxWithValue`。

    参数：
        - **input** (Tensor) - 输入任意维度的Tensor。不支持complex类型。
        - **axis** (int) - 指定计算维度，当为 ``None`` 时，计算 `input` 中所有元素的最大值。默认值： ``None`` 。
        - **keepdims** (bool) - 表示是否减少维度，如果为 ``True`` ，输出将与输入保持相同的维度；如果为 ``False`` ，输出将减少维度。默认值： ``False`` 。

    关键字参数：
        - **initial** (scalar, 可选) - 输出元素的最小值。如果 `input` 为空，则该参数必须设置。默认值： ``None`` 。
        - **where** (Tensor[bool], 可选) - 表示是否需要将 `input` 中的原始值替换为 `initial` 值的Tensor。若为 ``True`` 则不替换，若为 ``False`` 则替换。`where` 中为 ``False`` 的位置，必须提供对应的 `initial` 值。默认值： ``None`` ，表示默认为 ``True`` 。

    返回：
        tuple (Tensor)，表示2个Tensor组成的tuple，包含对应的索引和输入Tensor的最大值。

        - **output_x** (Tensor) - 输入Tensor的最大值，其shape与索引相同。
        - **index** (Tensor) - 输入Tensor最大值的索引。如果 `keepdims` 为True，则输出Tensor的shape为 :math:`(input_1, input_2, ..., input_{axis-1}, 1, input_{axis+1}, ..., input_N)` 。否则，shape为 :math:`(input_1, input_2, ..., input_{axis-1}, input_{axis+1}, ..., input_N)` 。

    异常：
        - **TypeError** - `input` 不是Tensor。
        - **TypeError** - `keepdims` 不是bool。
        - **TypeError** - `axis` 不是int。
        - **TypeError** - `initial` 不是scalar。