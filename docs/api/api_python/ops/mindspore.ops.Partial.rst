mindspore.ops.Partial
======================

.. py:class:: mindspore.ops.Partial

    生成偏函数的实例。通过给一般函数的部分参数提供初始值来衍生出有特定功能的新函数。

    输入：
        - **args** (Union[FunctionType, Tensor]) - 需传入的函数及其对应的参数。

    输出：
        FunctionType，偏函数及其对应的参数。
