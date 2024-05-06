mindspore.nn.ReflectionPad2d
============================

.. py:class:: mindspore.nn.ReflectionPad2d(padding)

    根据 `padding` 对输入 `x` 进行填充。

    参数：
        - **padding** (union[int, tuple]) - 填充大小，如果输入为int，则对所有边界进行相同大小的填充；如果是tuple，则顺序为 :math:`(pad\_left, pad\_right, pad\_up, pad\_down)`。

    输入：
        - **x** (Tensor) - 输入Tensor，shape为 :math:`(C, H_{in}, W_{in})` 或 :math:`(N, C, H_{in}, W_{in})` 。

    输出：
        Tensor，填充后的Tensor，shape为 :math:`(C, H_{out}, W_{out})` 或 :math:`(N, C, H_{out}, W_{out})`。其中 :math:`H_{out} = H_{in} + pad\_up + pad\_down`，:math:`W_{out} = W_{in} + pad\_left + pad\_right` 。

    异常：
        - **TypeError** - `padding` 不是tuple或int。
        - **TypeError** - `padding` 中存在不是int的元素。
        - **ValueError** - `padding` 是tuple，且长度不能被2整除。
        - **ValueError** - `padding` 是tuple，且存在负数。
        - **ValueError** - `padding` 是tuple，且长度和tensor的维度不匹配。
