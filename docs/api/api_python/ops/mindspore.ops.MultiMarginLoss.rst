mindspore.ops.MultiMarginLoss
==================================

.. py:class:: mindspore.ops.MultiMarginLoss(p=1, margin=1.0, reduction="mean")

    创建一个损失函数，用于优化输入和输出之间的多分类合页损失。

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    更多参考详见 :func:`mindspore.ops.multi_margin_loss`。

    参数：
        - **p** (int, 可选) - 对偶距离的范数度。必须为1或2。默认值： ``1`` 。
        - **margin** (int, 可选) - 改变对偶距离的参数。默认值： ``1.0`` 。
        - **reduction** (str，可选) - 指定应用于输出结果的规约计算方式，可选 ``'none'`` 、 ``'mean'`` 、 ``'sum'`` ，默认值： ``'mean'`` 。

          - ``"none"``：不应用规约方法。
          - ``"mean"``：计算输出元素的加权平均值。
          - ``"sum"``：计算输出元素的总和。

    输入：
        - **inputs** (Tensor) - 输入，shape为 :math:`(N, C)`。数据类型只支持float32、float16或float64。
        - **target** (Tensor) - 真实标签，shape为 :math:`(N,)`。数据类型只支持int64。值应为非负值，且小于C。
        - **weight** (Tensor, 可选) - 每个类别的缩放权重，shape为 :math:`(C,)`。数据类型只支持float32、float16或float64。

    输出：
        Tensor，当 `reduction` 为"none"时，shape为 :math:`(N,)`。否则，为标量。类型与 `inputs` 相同。
