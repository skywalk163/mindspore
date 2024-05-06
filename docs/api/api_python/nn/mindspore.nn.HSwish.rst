mindspore.nn.HSwish
===================

.. py:class:: mindspore.nn.HSwish

    逐元素计算Hard Swish。

    Hard Swish定义如下：

    .. math::
        \text{hswish}(x_{i}) = x_{i} * \frac{ReLU6(x_{i} + 3)}{6},

    HSwish函数图：

    .. image:: ../images/HSwish.png
        :align: center

    输入：
        - **x** (Tensor) - 用于计算Hard Swish的Tensor。数据类型必须为float16或float32。shape为 :math:`(N,*)` ，其中 :math:`*` 表示任意的附加维度数。

    输出：
        Tensor，具有与 `x` 相同的数据类型和shape。

    异常：
        - **TypeError** - `x` 的数据类型既不是float16也不是float32。
