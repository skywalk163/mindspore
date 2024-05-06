mindspore.nn.MultiLabelSoftMarginLoss
======================================

.. py:class:: mindspore.nn.MultiLabelSoftMarginLoss(weight=None, reduction='mean')

    基于最大熵计算用于多标签优化的损失。

    多标签软间隔损失通常用于多标签分类任务中，输入样本可以属于多个目标类别。
    给定输入 :math:`x` 和二元标签 :math:`y` ，其shape为 :math:`(N,C)` ， :math:`N` 表示样本数量， :math:`C` 为样本类别数，损失计算公式如下：

    .. math::
        \mathcal{loss\left( x , y \right)} = - \frac{1}{N}\frac{1}{C}\sum_{i = 1}^{N}
        \sum_{j = 1}^{C}\left(y_{ij}\log\frac{1}{1 + e^{- x_{ij}}} + \left( 1 - y_{ij}
        \right)\log\frac{e^{-x_{ij}}}{1 + e^{-x_{ij}}} \right)

    其中 :math:`x_{ij}` 表示样本 :math:`i` 在 :math:`j` 类别的概率得分。 :math:`y_{ij}` 表示样本 :math:`i` 是否属于类别 :math:`j` ，
    :math:`y_{ij}=1` 时属于，为0时不属于。对于多标签分类任务，每个样本可以属于多个类别，即标签中含有多个1。
    如果 `weight` 不为 ``None`` ，将会和每个分类的loss相乘。

    参数：
        - **weight** (Union[Tensor, int, float]) - 每个类别的缩放权重。默认值： ``None`` 。
        - **reduction** (str，可选) - 指定应用于输出结果的规约计算方式，可选 ``'none'`` 、 ``'mean'`` 、 ``'sum'`` ，默认值： ``'mean'`` 。

          - ``"none"``：不应用规约方法。
          - ``"mean"``：计算输出元素的加权平均值。
          - ``"sum"``：计算输出元素的总和。

    输入：
        - **x** (Tensor) - shape为 :math:`(N, C)` 的Tensor，N为batch size，C为类别个数。
        - **target** (Tensor) - 目标值，数据类型和shape与 `x` 的相同。

    输出：
        Tensor，数据类型和 `x` 相同。如果 `reduction` 为 ``"none"`` ，其shape为(N)。否则，其shape为0。

    异常：
        - **ValueError** - `x` 或 `target` 的维度不等于2。
