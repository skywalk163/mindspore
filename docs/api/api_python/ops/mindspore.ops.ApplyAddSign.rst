﻿mindspore.ops.ApplyAddSign
===========================

.. py:class:: mindspore.ops.ApplyAddSign

    根据AddSign算法更新相关参数。

    .. math::
        \begin{array}{ll} \\
            m_{t+1} = \beta * m_{t} + (1 - \beta) * g \\
            \text{update} = (\alpha + \text{sign_decay} * sign(g) * sign(m)) * g \\
            var = var - lr_{t+1} * \text{update}
        \end{array}

    :math:`t` 代表更新步长，而 :math:`m` 代表第一个动量矩阵， :math:`m_{t}` 是 :math:`m_{t+1}` 的最后时刻， :math:`lr` 代表学习率 `lr` ， :math:`g` 代表 `grad` ， :math:`\alpha` 代表 `alpha` ， :math:`\beta` 代表 `beta` 。

    所有输入数据在Ascend上支持的类型为：float16和float32，CPU和GPU上支持的类型为：float16、float32和float64。

    `var` 、 `accum` 和 `grad` 的输入， `sign_decay` 和 `beta` 的输入都遵循隐式类型转换规则，使数据类型一致。 如果它们具有不同的数据类型，则低精度数据类型将转换为相对最高精度的数据类型。

    输入：
        - **var** (Parameter) - 要更新的权重。任意维度，其shape为 :math:`(N, *)` ，其中 :math:`*` 为任意数量的额外维度。
        - **m** (Parameter) - 要更新的权重，shape与 `var` 相同。
        - **lr** (Union[Number, Tensor]) - 学习率，必须是Scalar。
        - **sign_decay** (Union[Number, Tensor]) - 必须是Scalar。
        - **alpha** (Union[Number, Tensor]) - 必须是Scalar。
        - **beta** (Union[Number, Tensor]) - 指数衰减率，必须是Scalar。
        - **grad** (Tensor) - 梯度，shape与 `var` 相同。

    输出：
        2个Tensor组成的tuple，更新后的数据。

        - **var** (Tensor) - shape和数据类型与 `var` 相同。
        - **m** (Tensor) - shape和数据类型与 `m` 相同。

    异常：
        - **TypeError** - 如果 `var` 、 `lr` 、 `alpha` 的数据类型既不是float16也不是float32或者float64。
        - **TypeError** - 如果 `sign_decay` 和 `beta` 的数据类型都不是float16、float32或者float64。
        - **TypeError** - 如果 `lr` 、 `alpha` 或 `sign_decay` 既不是数值型，也不是Tensor。
        - **TypeError** - 如果 `grad` 不是Tensor。
        - **TypeError** - 如果不支持参数的 `var` 、 `accum` 和 `grad` 数据类型转换。
