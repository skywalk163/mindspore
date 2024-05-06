mindspore.nn.NLLLoss
====================

.. py:class:: mindspore.nn.NLLLoss(weight=None, ignore_index=-100, reduction='mean')

    计算预测值和目标值之间的负对数似然损失。

    :math:`reduction = none` 时，负对数似然损失公式如下：

    .. math::
        \ell(x, t)=L=\left\{l_{1}, \ldots, l_{N}\right\}^{\top},
        \quad l_{n}=-w_{t_{n}} x_{n, t_{n}},
        \quad w_{c}=\text { weight }[c] \cdot \mathbb{1}\{c \not= \text{ignore_index}\}

    其中， :math:`x` 表示预测值， :math:`t` 表示目标值， :math:`w` 表示权重， :math:`N` 表示batch size， :math:`c` 限定范围为 :math:`[0, C-1]`，表示类索引，其中 :math:`C` 表示类的数量。

    若 :math:`reduction \neq none` 时（默认为 ``'mean'`` ），则

    .. math::
        \ell(x, t)=\left\{\begin{array}{ll}
        \sum_{n=1}^{N} \frac{1}{\sum_{n=1}^{N} w_{t n}} l_{n}, & \text { if reduction }=\text { 'mean', } \\
        \sum_{n=1}^{N} l_{n}, & \text { if reduction }=\text { 'sum' }
        \end{array}\right.

    参数：
        - **weight** (Tensor) - 指定各类别的权重。若值不为None，则shape为 :math:`(C,)`。数据类型仅支持float32或float16。默认值： ``None`` 。
        - **ignore_index** (int) - 指定target中需要忽略的值(一般为填充值)，使其不对梯度产生影响。默认值： ``-100`` 。
        - **reduction** (str，可选) - 指定应用于输出结果的规约计算方式，可选 ``'none'`` 、 ``'mean'`` 、 ``'sum'`` ，默认值： ``'mean'`` 。

          - ``"none"``：不应用规约方法。
          - ``"mean"``：计算输出元素的加权平均值。
          - ``"sum"``：计算输出元素的总和。

    输入：
        - **logits** (Tensor) - 输入预测值，shape为 :math:`(N, C)` 或 :math:`(N, C, d_1, d_2, ..., d_K)` (针对 :math:`K` 维数据)。`inputs` 需为对数概率。数据类型仅支持float32或float16。
        - **labels** (Tensor) - 输入目标值，shape为 :math:`(N)` 或 :math:`(N, d_1, d_2, ..., d_K)` (针对 :math:`K` 维数据)。
          数据类型仅支持int32。

    返回：
        Tensor，一个数据类型与logits相同的Tensor。

    异常：
        - **TypeError** - `weight` 不是Tensor。
        - **TypeError** - `weight` 的dtype既不是float16，也不是float32。
        - **TypeError** - `ignore_index` 不是int。
        - **ValueError** - `reduction` 不为 ``"mean"`` 、 ``"sum"`` 或 ``"none"`` 。
        - **TypeError** - `logits` 不是Tensor。
        - **TypeError** - `labels` 不是Tensor。
