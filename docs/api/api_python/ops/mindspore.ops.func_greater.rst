mindspore.ops.greater
=====================

.. py:function:: mindspore.ops.greater(input, other)

    按元素比较输入参数 :math:`input > other` 的值，输出结果为bool值。

    更多参考详见 :func:`mindspore.ops.gt()`。

    参数：
        - **input** (Union[Tensor, Number]) - 第一个输入，是一个Number或数据类型为 `number <https://www.mindspore.cn/docs/zh-CN/master/api_python/mindspore/mindspore.dtype.html#mindspore.dtype>`_ 或 `bool_ <https://www.mindspore.cn/docs/zh-CN/master/api_python/mindspore/mindspore.dtype.html#mindspore.dtype>`_ 的Tensor。
        - **other** (Union[Tensor, Number]) - 第二个输入，是一个Number或数据类型为 `number <https://www.mindspore.cn/docs/zh-CN/master/api_python/mindspore/mindspore.dtype.html#mindspore.dtype>`_ 或 `bool_ <https://www.mindspore.cn/docs/zh-CN/master/api_python/mindspore/mindspore.dtype.html#mindspore.dtype>`_ 的Tensor。

    返回：
        Tensor，shape与广播后的shape相同，数据类型为bool。
