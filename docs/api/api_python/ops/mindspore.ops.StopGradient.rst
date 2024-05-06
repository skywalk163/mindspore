﻿mindspore.ops.StopGradient
===========================

.. py:class:: mindspore.ops.StopGradient

    用于消除某个值对梯度的影响，例如截断来自于函数输出的梯度传播。
    
    更多详情请查看： :class:`mindspore.ops.stop_gradient` 。

    输入：
        - **value** (Any) - 需要被消除梯度影响的值。

    输出：
        一个与 `value` 相同的值。
