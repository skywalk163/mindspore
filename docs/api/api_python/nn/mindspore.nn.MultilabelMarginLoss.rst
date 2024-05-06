mindspore.nn.MultilabelMarginLoss
======================================

.. py:class:: mindspore.nn.MultilabelMarginLoss(reduction="mean")

    创建一个损失函数，用于最小化多分类任务的合页损失。
    它以一个2D mini-batch Tensor :math:`x` 作为输入，以包含目标类索引的2D Tensor :math:`y` 作为输出。

    对于每个小批量样本，loss值根据如下公式计算：

    .. math::
        \text{loss}(x, y) = \sum_{ij}\frac{\max(0, 1 - (x[y[j]] - x[i]))}{\text{x.size}(0)}

    其中 :math:`x \in \left\{0, \; \cdots , \; \text{x.size}(0) - 1\right\}`, \
    :math:`y \in \left\{0, \; \cdots , \; \text{y.size}(0) - 1\right\}`, \
    :math:`0 \leq y[j] \leq \text{x.size}(0)-1`, \
    并且对于所有 :math:`i` 和 :math:`j` ， :math:`i` 不等于 :math:`y[j]` 。

    :math:`y` 和 :math:`x` shape必须相同。

    .. note::
        该算子仅考虑从前方开始的连续非负目标块。这允许不同的样本具有不同数量的目标类别。

    参数：
        - **reduction** (str，可选) - 指定应用于输出结果的规约计算方式，可选 ``"none"`` 、 ``"mean"`` 、 ``"sum"`` ，默认值： ``"mean"`` 。

          - ``"none"``：不应用规约方法。
          - ``"mean"``：计算输出元素的平均值。
          - ``"sum"``：计算输出元素的总和。

    输入：
        - **x** (Tensor) - 预测值，shape为 :math:`(C)` 或 :math:`(N, C)`，其中 :math:`N`
          为批量大小，:math:`C` 为类别数。数据类型必须为：float16或float32。
        - **target** (Tensor) - 目标值，shape与 `x` 相同，数据类型必须为int32，标签目标值由-1填充。

    输出：
        - **y** (Union[Tensor, Scalar]) - MultilabelMarginLoss损失。如果 `reduction` 的值为 ``"none"`` ，
          那么返回shape为 :math:`(N)` 的Tensor类型数据。否则返回一个标量。

    异常：
        - **TypeError** - 当 `x` 或者 `target` 数据不是Tensor时。
        - **TypeError** - 当 `x` 数据类型不是以下其中之一时：float16、float32。
        - **TypeError** - 当 `target` 数据类型不是int32时。
        - **ValueError** - 当 `x` 的数据维度不是以下其中之一时：1、2。
        - **ValueError** - 当 `x` 和 `target` 的shape不相同时。
        - **ValueError** - 当 `reduction` 的值不是以下其中之一时： ``"none"`` 、 ``"mean"`` 、 ``"sum"`` 。
