﻿mindspore.ops.cat
==================

.. py:function:: mindspore.ops.cat(tensors, axis=0)

    在指定轴上拼接输入Tensor。

    输入的是一个tuple或list。其元素秩相同，即 :math:`R` 。将给定的轴设为 :math:`m` ，并且 :math:`0 \le m < R` 。输入元素的数量设为 :math:`N` 。对于第 :math:`i` 个数据， :math:`t_i` 的shape为 :math:`(x_1, x_2, ..., x_{mi}, ..., x_R)` 。 :math:`x_{mi}` 是第 :math:`t_i` 个元素的第 :math:`m` 个维度。则，输出Tensor的shape为：

    .. math::
        (x_1, x_2, ..., \sum_{i=1}^Nx_{mi}, ..., x_R)

    参数：
        - **tensors** (Union[tuple, list]) - 输入为Tensor组成的tuple或list。假设在这个tuple或list中有两个Tensor，即 `t1` 和 `t2` 。要在0轴方向上执行 `Concat` ，除  :math:`0` 轴外，其他轴的shape都应相等，即 :math:`t1.shape[1] = t2.shape[1], t1.shape[2] = t2.shape[2], ..., t1.shape[R-1] = t2.shape[R-1]` ，其中 :math:`R` 是Tensor的秩。
        - **axis** (int) - 表示指定的轴，取值范围是 :math:`[-R, R)` 。默认值： ``0`` 。

    返回：
        Tensor，shape为 :math:`(x_1, x_2, ..., \sum_{i=1}^Nx_{mi}, ..., x_R)` 。数据类型与 `tensors` 相同。

    异常：
        - **TypeError** - `axis` 不是int。
        - **ValueError** - `tensors` 是不同维度的Tensor。
        - **ValueError** - `axis` 的值不在区间 :math:`[-R, R)` 内。
        - **ValueError** - 除了 `axis` 之外， `tensors` 的shape不相同。
        - **ValueError** - `tensors` 为空tuple或list。
