mindspore.ops.Logit
===================

.. py:class:: mindspore.ops.Logit(eps=-1.0)

    逐元素计算Tensor的logit值。 `x` 中的元素被截断到范围[eps, 1-eps]内。

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    更多参考详见 :func:`mindspore.ops.logit`。

    参数：
        - **eps** (float, 可选) - epsilon值。输入的数值界限被定义[eps, 1-eps]。默认值： ``-1.0`` 。

    输入：
        - **x** (Tensor) - Tensor输入，其数据类型为float16、float32或float64。

    输出：
        Tensor，具有与 `x` 相同的shape。
