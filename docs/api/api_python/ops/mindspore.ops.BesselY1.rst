mindspore.ops.BesselY1
======================

.. py:class:: mindspore.ops.BesselY1

    逐元素计算输入数据的第二类一阶Bessel函数。

    计算公式定义如下：

    .. math::
        \begin{array}{ll} \\
            Y_{1}(x)=\lim_{n \to 1} \frac{J_{n}(x) \cos n \pi-J_{-n}(x)}{\sin n \pi}
        \end{array}

    其中 :math:`J_{1}` 第一类一阶的Bessel函数。

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    输入：
        - **x** (Tensor) - 输入Tensor。数据类型应为float16、float32或float64。

    输出：
        Tensor，shape和数据类型与 `x` 相同。

    异常：
        - **TypeError** - `x` 不是float16、float32或float64数据类型的Tensor。
