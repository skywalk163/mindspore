﻿mindspore.ops.RandomChoiceWithMask
=====================================

.. py:class:: mindspore.ops.RandomChoiceWithMask(count=256, seed=0, seed2=0)

    对输入进行随机取样，返回取样索引和掩码。

    更多参考详见 :func:`mindspore.ops.choice_with_mask`。

    .. note::
        - 随机种子：通过一些复杂的数学算法，可以得到一组有规律的随机数，而随机种子就是这个随机数的初始值。随机种子相同，得到的随机数就不会改变。
        - 全局的随机种子和算子层的随机种子都没设置或都设置为0：完全随机。
        - 全局的随机种子设置了，算子层的随机种子未设置：采用全局的随机种子和0拼接。
        - 全局的随机种子未设置，算子层的随机种子设置了：使用0和算子层的随机种子拼接。
        - 全局的随机种子和算子层的随机种子都设置了：全局的随机种子和算子层的随机种子拼接。

    参数：
        - **count** (int，可选) - 取样数量，必须大于0。默认值： ``256`` 。
        - **seed** (int，可选) - 算子层的随机种子，用于生成随机数。必须是非负的。默认值： ``0`` 。
        - **seed2** (int，可选) - 全局的随机种子，和算子层的随机种子共同决定最终生成的随机数。必须是非负的。默认值： ``0`` 。

    输入：
        - **input_x** (Tensor[bool]) - 输入Tensor，bool类型。秩必须大于等于1且小于等于5。

    输出：
        两个Tensor，第一个为索引，另一个为掩码。

        - **index** (Tensor) - 二维Tensor，shape为 :math:`(count, input_x的秩)`。
        - **mask** (Tensor) - 一维Tensor，shape为 :math:`(count)`。
