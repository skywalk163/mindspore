mindspore.ops.bincount
======================

.. py:function:: mindspore.ops.bincount(input, weights=None, minlength=0)

    统计 `input` 中每个值的出现次数。

    如果不指定 `minlength` ，输出Tensor的长度为输入 `input` 中最大值加1。
    如果指定了 `minlength`，则输出Tensor的长度为 `input` 中最大值加1和 `minlength` 的最大值。

    输出Tensor中每个值标记了该索引值在 `input` 中的出现次数。如果指定了 `weights`，对输出的结果进行加权处理，即 `out[n]+=weight[i]` 而不是 `out[n]+=1`。

    .. note::
        如果 `input` 含有负数元素，则结果未定义。

    参数：
        - **input** (Tensor) - 一维的Tensor。
        - **weights** (Tensor, 可选) - 权重，与 `input` shape相同。默认值： ``None`` 。
        - **minlength** (int, 可选) - 输出Tensor的最小长度。默认值： ``0`` 。

    返回：
        Tensor，如果输入为非空，输出shape为[max(input)+1]的Tensor，否则shape为[0]。

    异常：
        - **TypeError** - 如果 `input` 或 `weights` 不是Tensor。
        - **ValueError** - 如果 `input` 不是一维的，或者 `input` 和 `weights` 不具有相同的shape。
        - **ValueError** - 如果 `minlength` 是负整数。
