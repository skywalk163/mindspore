mindspore.ops.Zeta
===================

.. py:class:: mindspore.ops.Zeta

    计算输入Tensor的Hurwitz zeta函数ζ(x,q)值。

    .. math::

        \zeta \left ( x,q \right )=  \textstyle \sum_{n=0} ^ {\infty} \left ( q+n\right )^{-x}

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    输入：
        - **x** (Tensor) - Tensor，数据类型为：float32、float64。
        - **q** (Tensor) - Tensor，数据类型与 `x` 一致。

    输出：
        Tensor，数据类型和shape与输入shape相同。

    异常：
        - **TypeError** - `x` 或 `q` 不是Tensor。
        - **TypeError** - `x` 的数据类型既不是float32也不是float64。
        - **TypeError** - `q` 的数据类型既不是float32也不是float64。
        - **ValueError** - `x` 和 `q` 的shape不同。
