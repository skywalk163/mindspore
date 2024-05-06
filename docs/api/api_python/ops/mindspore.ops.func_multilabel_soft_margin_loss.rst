mindspore.ops.multilabel_soft_margin_loss
=========================================

.. py:function:: mindspore.ops.multilabel_soft_margin_loss(input, target, weight=None, reduction='mean')

    基于最大熵计算用于多标签优化的损失。

    多标签软间隔损失通常用于多标签分类任务中，输入样本可以属于多个目标类别。
    给定输入 :math:`input` 和二元标签 :math:`output` ，其shape为 :math:`(N,C)` ， :math:`N` 表示样本数量， :math:`C` 为样本类别数，损失计算公式如下：

    .. math::
        \mathcal{loss\left( input , output \right)} = - \frac{1}{N}\frac{1}{C}\sum_{i = 1}^{N}
        \sum_{j = 1}^{C}\left(output_{ij}\log\frac{1}{1 + e^{- input_{ij}}} + \left( 1 - output_{ij}
        \right)\log\frac{e^{-input_{ij}}}{1 + e^{-input_{ij}}} \right)

    其中 :math:`input_{ij}` 表示样本 :math:`i` 在 :math:`j` 类别的概率得分。 :math:`output_{ij}` 表示样本 :math:`i` 是否属于类别 :math:`j` ，
    :math:`output_{ij}=1` 时属于，为0时不属于。对于多标签分类任务，每个样本可以属于多个类别，即标签 :math:`output` 中含有多个1。
    如果 `weight` 不为None，将会和每个分类的loss相乘。

    参数：
        - **input** (Tensor) - shape为 :math:`(N, C)` 的Tensor，N为batch size，C为类别个数。
        - **target** (Tensor) - 目标值，数据类型和shape与 `input` 的相同。
        - **weight** (Union[Tensor, int, float]) - 每个类别的缩放权重。默认值： ``None`` 。
        - **reduction** (str，可选) - 指定应用于输出结果的规约计算方式，可选 ``'none'`` 、 ``'mean'`` 、 ``'sum'`` ，默认值： ``'mean'`` 。

          - ``'none'``：不应用规约方法。
          - ``'mean'``：计算输出元素的加权平均值。
          - ``'sum'``：计算输出元素的总和。

    返回：
        Tensor，数据类型和 `input` 相同。如果 `reduction` 为 ``'none'`` ，其shape为 :math:`(N)` 。否则，其shape为0。
