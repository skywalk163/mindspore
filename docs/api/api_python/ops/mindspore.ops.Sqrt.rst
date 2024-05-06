﻿mindspore.ops.Sqrt
===================

.. py:class:: mindspore.ops.Sqrt

    逐元素计算输入Tensor的平方根。
	
    .. note::
        当输入数据存在一些负数，则负数对应位置上的返回结果为NaN。

    .. math::
        out_{i} =  \sqrt{x_{i}}

    输入：
        - **x** (Tensor) - 输入Tensor。

    输出：
        Tensor，shape和数据类型与输入 `x` 相同。

    异常：
        - **TypeError** - `x` 不是Tensor。