mindspore.ops.LessEqual
========================

.. py:class:: mindspore.ops.LessEqual

    逐元素计算 :math:`x <= y` 的bool值。

    更多参考详见 :func:`mindspore.ops.le`。

    输入：
        - **x** (Union[Tensor, number.Number, bool]) - 第一个输入，是一个number.Number、bool值或数据类型为 `number <https://www.mindspore.cn/docs/zh-CN/master/api_python/mindspore/mindspore.dtype.html#mindspore.dtype>`_ 或 `bool_ <https://www.mindspore.cn/docs/zh-CN/master/api_python/mindspore/mindspore.dtype.html#mindspore.dtype>`_ 的Tensor。
        - **y** (Union[Tensor, number.Number, bool]) - 第二个输入，是一个number.Number、bool值或数据类型为 `number <https://www.mindspore.cn/docs/zh-CN/master/api_python/mindspore/mindspore.dtype.html#mindspore.dtype>`_ 或 `bool_ <https://www.mindspore.cn/docs/zh-CN/master/api_python/mindspore/mindspore.dtype.html#mindspore.dtype>`_ 的Tensor。

    输出：
        Tensor，shape与广播后的shape相同，数据类型为bool。
