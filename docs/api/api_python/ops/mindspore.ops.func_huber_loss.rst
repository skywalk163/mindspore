mindspore.ops.huber_loss
========================

.. py:function:: mindspore.ops.huber_loss(input, target, reduction='mean', delta=1.0)

    计算预测值和目标值之间的误差，兼具 :func:`mindspore.ops.l1_loss` 和 :func:`mindspore.ops.mse_loss` 的优点。

    假设 :math:`x` 和 :math:`y` 为一维Tensor，长度 :math:`N` ，reduction参数设置为 ``'none'`` ，计算 :math:`x` 和 :math:`y` 的loss而不进行降维操作。公式如下：

    .. math::
        \ell(x, y) = L = \{l_1,\dots,l_N\}^\top

    以及

    .. math::
        l_n = \begin{cases}
            0.5 * (x_n - y_n)^2, & \text{if } |x_n - y_n| < delta; \\
            delta * (|x_n - y_n| - 0.5 * delta), & \text{otherwise. }
        \end{cases}

    其中， :math:`N` 为batch size。

    如果 `reduction` 是 ``'mean'`` 或 ``'sum'`` ，则：

    .. math::
        \ell(x, y) =
        \begin{cases}
            \operatorname{mean}(L), & \text{if reduction} = \text{"mean";}\\
            \operatorname{sum}(L),  & \text{if reduction} = \text{"sum".}
        \end{cases}

    参数：
        - **input** (Tensor) - 输入预测值，任意维度的Tensor。
        - **target** (Tensor) - 目标值，通常情况下与 `input` 的shape和dtype相同。但是当 `target` 和 `x` 的shape不同时，需要保证他们之间可以互相广播。
        - **reduction** (str，可选) - 指定应用于输出结果的规约计算方式，可选 ``'none'`` 、 ``'mean'`` 、 ``'sum'`` ，默认值： ``'mean'`` 。

          - ``'none'``：不应用规约方法。
          - ``'mean'``：计算输出元素的平均值。
          - ``'sum'``：计算输出元素的总和。

        - **delta** (Union[int, float]) - 两种损失之间变化的阈值。该值必须大于零。默认值： ``1.0`` 。

    返回：
        Tensor或Scalar，如果 `reduction` 为 `none` ，则返回与 `input` 具有相同shape和dtype的Tensor。否则，将返回Scalar。

    异常：
        - **TypeError** - `input` 或 `target` 不是Tensor。
        - **TypeError** - `delta` 不是float或int。
        - **ValueError** - `delta` 的值小于或等于0。
        - **ValueError** - `reduction` 不为 ``"mean"`` 、 ``"sum"`` 或 ``"none"`` 。
        - **ValueError** - `input` 和 `target` 有不同的shape，且不能互相广播。
