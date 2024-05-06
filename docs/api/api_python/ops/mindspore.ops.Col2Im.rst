﻿mindspore.ops.Col2Im
=====================

.. py:class:: mindspore.ops.Col2Im(kernel_size, dilation=1, padding=0, stride=1)

    将一个行向量重新排列成图像。通常用于从一组图像补丁（或滑动局部块）中重建图像。

    假设输入Tensor的shape为 :math:`(N, C, \prod(\text{kernel_size}), L)` ，:math:`N` 代表Batch数量，:math:`C` 代表Channel数量，:math:`\prod(\text{kernel_size})` 代表滑窗大小，:math:`L` 代表滑窗总数。Col2Im将这些局部块组合成shape为 :math:`(N, C, \text{output_size}[0], \text{output_size}[1], \dots)` 的Tensor作为输出，对于滑块间的重叠值采取求和策略。

    输入与输出的shape之间的关系可以表示为：

    .. math::
        L = \prod_d \left\lfloor\frac{\text{output_size}[d] + 2 \times \text{padding}[d] %
            - \text{dilation}[d] \times (\text{kernel_size}[d] - 1) - 1}{\text{stride}[d]} + 1\right\rfloor

    式中 :math:`d` 代表高度与宽度上两个维度， `dilation` 、 `padding` 和 `stride` 决定了滑窗如何滑动与检索元素。

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    参数：
        - **kernel_size** (Union[int, tuple[int], list[int]]) - 滑窗大小，由两个正整数组成，分别代表滑窗的高度与宽度。如果数据类型为int，代表不同方向上的填充大小相等。取值必须由用户指定。
        - **dilation** (Union[int, tuple[int], list[int]], 可选) - 滑窗之间的间距，由两个正整数组成，分别代表横向与纵向上滑窗移动时与上一个滑窗间的距离。如果数据类型为int，代表不同方向上的填充大小相等。默认值： ``1`` 。
        - **padding** (Union[int, tuple[int], list[int]], 可选) - 滑窗取数前在输入 `x` 周围隐式填充0（若需）的范围，由两个正整数组成，分别代表横向与纵向上的填充范围。如果数据类型为int，代表不同方向上的填充大小相等。默认值： ``0`` 。
        - **stride** (Union[int, tuple[int], list[int]], 可选) - 滑窗移动的步长，由两个正整数组成，分别代表滑窗在横向与纵向上的移动步长。如果数据类型为int，代表不同方向上的步长相等。默认值： ``1`` 。

    输入：
        - **x** (Tensor) - 4D 输入Tensor。
        - **output_size** (Tensor) - 1D Tensor，输出Tensor的后两维的shape，包含2个元素且其数据类型为int32或int64。

    输出：
        4D Tensor，类型与输入 `x` 一致。

    异常：
        - **TypeError** - 输入 `kernel_size` 、 `dilation` 、 `padding` 、 `stride` 的数据类型不是int、tuple[int]或list[int]之一。
        - **ValueError** - 输入 `kernel_size` 、 `dilation` 、 `padding` 、 `stride` 的值小于等于0或者其中元素的个数大于2。
        - **ValueError** - x.shape[2] != kernel_size[0] * kernel_size[1]。
        - **ValueError** - x.shape[3]与计算出的滑动块数量不匹配。
