mindspore.ops.binary_cross_entropy
==================================

.. py:function:: mindspore.ops.binary_cross_entropy(logits, labels, weight=None, reduction='mean')

    计算预测值 `logits` 和 目标值 `labels` 之间的二值交叉熵（度量两个概率分布间的差异性信息）损失。

    将 `logits` 设置为 :math:`x` ， `labels` 设置为 :math:`y` ，输出设置为 :math:`\ell(x, y)` ，第n个batch二值交叉熵的权重为 :math:`w_n`。则，

    .. math::
        L = \{l_1,\dots,l_N\}^\top, \quad
        l_n = - w_n \left[ y_n \cdot \log x_n + (1 - y_n) \cdot \log (1 - x_n) \right]

    其中，:math:`L` 表示所有batch_size的loss值，:math:`l` 表示一个batch_size的loss值，:math:`n` 表示在 :math:`1-N` 范围内的一个batch_size。

    .. math::
        \ell(x, y) = \begin{cases}
        L, & \text{if reduction} = \text{'none';}\\
        \operatorname{mean}(L), & \text{if reduction} = \text{'mean';}\\
        \operatorname{sum}(L),  & \text{if reduction} = \text{'sum'.}
        \end{cases}

    .. warning::
        - `logits` 的值必须要在0-1范围内。

    参数：
        - **logits** (Tensor) - 输入预测值，其数据类型为float16或float32。
        - **labels** (Tensor) - 输入目标值，shape与 `logits` 相同。数据类型为float16或float32。
        - **weight** (Tensor, 可选) - 指定每个批次二值交叉熵的权重。支持广播，使其shape与 `logits` 的shape保持一致。数据类型必须为float16或float32。默认值： ``None`` 。若为 ``None`` ，损失函数将不会考虑任何样本的权重，每个样本在计算损失时被视为具有相同的重要性。
        - **reduction** (str，可选) - 指定应用于输出结果的规约计算方式，可选 ``'none'`` 、 ``'mean'`` 、 ``'sum'`` ，默认值： ``'mean'`` 。

          - ``'none'``：不应用规约方法。
          - ``'mean'``：计算输出元素的加权平均值。
          - ``'sum'``：计算输出元素的总和。

    返回：
        Tensor或Scalar，如果 `reduction` 为 ``'none'`` ，则为shape和数据类型与输入 `logits` 相同的Tensor。否则，输出为Scalar。

    异常：
        - **TypeError** - 输入 `logits` ， `labels` ， `weight` 不为Tensor。
        - **TypeError** - 输入 `logits` ， `labels` ， `weight` 的数据类型既不是float16也不是float32。
        - **ValueError** - `reduction` 不为 ``'none'`` 、 ``'mean'``  或 ``'sum'`` 。
        - **ValueError** - `labels` 的shape大小与 `logits` 或者 `weight` 不相同。
