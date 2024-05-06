mindspore.ops.Eye
==================

.. py:class:: mindspore.ops.Eye

    创建一个主对角线上元素为1，其余元素为0的Tensor。

    .. note::
        Ascend平台支持返回Tensor的数据类型包括float16, float32, int8, int16, int32, int64, uint8和bool。

    更多参考详见 :func:`mindspore.ops.eye`。

    输入：
        - **n** (int) - 指定返回Tensor的行数。仅支持常量值。
        - **m** (int) - 指定返回Tensor的列数。仅支持常量值。
        - **t** (mindspore.dtype) - 指定返回Tensor的数据类型。
    输出：
        Tensor，主对角线上为1，其余的元素为0。它的shape由 `n` 和 `m` 指定。数据类型由 `t` 指定。
