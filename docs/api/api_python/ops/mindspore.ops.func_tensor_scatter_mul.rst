mindspore.ops.tensor_scatter_mul
================================

.. py:function:: mindspore.ops.tensor_scatter_mul(input_x, indices, updates)

    根据索引，通过乘法运算得到输出Tensor的值。更新后的结果是通过算子output返回，而不是直接原地更新input。

    `indices` 的最后一个轴是每个索引向量的深度。对于每个索引向量， `updates` 中必须有相应的值。 `updates` 的shape应该等于 `input_x[indices]` 的shape。有关更多详细信息，请参见样例。

    .. note::
        - 如果 `indices` 的某些值超出 `input_x` 的维度范围，则相应的 `updates` 不会更新为 `input_x` ，而不是抛出索引错误。

    .. math::
        output\left [indices  \right ] = input\_x\times  update

    参数：
        - **input_x** (Tensor) - 输入Tensor。 `input_x` 的维度必须不小于 `indices.shape[-1]` 。
        - **indices** (Tensor) - `input_x` 执行scatter操作的目标索引，数据类型为int32或int64，rank必须大于等于2。
        - **updates** (Tensor) - 指定与 `input_x` 相加操作的Tensor，其数据类型与 `input_x` 相同。并且shape应等于 :math:`indices.shape[:-1] + input\_x.shape[indices.shape[-1]:]`。

    返回：
        Tensor，shape和数据类型与输入 `input_x` 相同。

    异常：
        - **TypeError** - `indices` 的数据类型不满足int32或int64。
        - **ValueError** - `input_x` 的rank小于 indices.shape的最后一维。
        - **RuntimeError** - `indices` 中的值超出了 `input_x` 的索引范围。
