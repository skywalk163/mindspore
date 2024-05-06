mindspore.ops.logical_xor
=========================

.. py:function:: mindspore.ops.logical_xor(input, other)

    逐元素计算两个Tensor的逻辑异或运算。

    .. math::
        out_{i} = input_{i} \oplus other_{i}

    参数：
        - **input** (Tensor) - 第一个输入是数据类型可被隐式转换为bool的Tensor。
        - **other** (Tensor) - 第二个输入是数据类型可被隐式转换为bool的Tesor，与第一个输入进行异或计算。

    返回：
        Tensor，其shape与广播后的shape相同，数据类型为bool。
 
    异常：
        - **TypeError** - 如果 `input` 或 `other` 的dtype不是bool或不可被隐式转换为bool。
        - **ValueError** - 如果两个输入的shape不能被广播。
