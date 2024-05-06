mindspore.nn.AvgPool3d
=======================

.. py:class:: mindspore.nn.AvgPool3d(kernel_size=1, stride=1, pad_mode='valid', padding=0, ceil_mode=False, count_include_pad=True, divisor_override=None)

    在一个输入Tensor上应用3D平均池化运算，输入Tensor可看成是由一系列3D平面组成的。

    通常，输入的shape为 :math:`(N_{in}, C_{in}, D_{in}, H_{in}, W_{in})` ，AvgPool3D输出 :math:`(D_{in}, H_{in}, W_{in})` 维度的区域平均值。给定 `kernel_size` 为 :math:`ks = (d_{ker}, h_{ker}, w_{ker})` 和 `stride` 为 :math:`s = (s_0, s_1, s_2)`，公式如下。

    .. warning::
        `kernel_size` 在[1, 255]的范围内取值。`stride` 在[1, 63]的范围内取值。

    .. math::
        \text{output}(N_i, C_j, d, h, w) =
        \frac{1}{d_{ker} * h_{ker} * w_{ker}} \sum_{l=0}^{d_{ker}-1} \sum_{m=0}^{h_{ker}-1} \sum_{n=0}^{w_{ker}-1}
        \text{input}(N_i, C_j, s_0 \times d + l, s_1 \times h + m, s_2 \times w + n)

    参数：
        - **kernel_size** (Union[int, tuple[int]]，可选) - 指定池化核尺寸大小。如果为整数或单元素tuple，则同时代表池化核的深度，高度和宽度。如果为tuple且长度不为 ``1`` ，其值必须包含三个正整数，分别表示池化核的深度，高度和宽度。默认值： ``1`` 。
        - **stride** (Union[int, tuple[int]]，可选) - 池化操作的移动步长。如果为整数或单元素tuple，则同时代表池化核的深度，高度和宽度方向上的移动步长。如果为tuple且长度不为 ``1`` ，其值必须包含三个整数值，分别表示池化核的深度，高度和宽度方向上的移动步长。取值必须为正整数。默认值： ``1`` 。
        - **pad_mode** (str，可选) - 指定填充模式，填充值为0。可选值为 ``"same"`` ， ``"valid"`` 或 ``"pad"`` 。默认值： ``"valid"`` 。

          - ``"same"``：在输入的深度、高度和宽度维度进行填充，使得当 `stride` 为 ``1`` 时，输入和输出的shape一致。待填充的量由算子内部计算，若为偶数，则均匀地填充在四周，若为奇数，多余的填充量将补充在前方/底部/右侧。如果设置了此模式， `padding` 必须为0。
          - ``"valid"``：不对输入进行填充，返回输出可能的最大深度、高度和宽度，不能构成一个完整stride的额外的像素将被丢弃。如果设置了此模式， `padding` 必须为0。
          - ``"pad"``：对输入填充指定的量。在这种模式下，在输入的深度、高度和宽度方向上填充的量由 `padding` 参数指定。如果设置此模式， `padding` 必须大于或等于0。

        - **padding** (Union(int, tuple[int], list[int])，可选) - 池化填充值，只有 `pad_mode` 为"pad"时才能设置为非 ``0`` 。默认值： ``0`` 。只支持以下情况：

          - `padding` 为一个整数或包含一个整数的tuple/list，此情况下分别在输入的前后上下左右六个方向进行 `padding` 次的填充。
          - `padding` 为一个包含三个int的tuple/list，此情况下在输入的前后进行 `padding[0]` 次的填充，上下进行 `padding[1]` 次的填充，在输入的左右进行 `padding[2]` 次的填充。

        - **ceil_mode** (bool，可选) - 若为 ``True`` ，使用ceil来计算输出shape。若为 ``False`` ，使用floor来计算输出shape。默认值： ``False`` 。
        - **count_include_pad** (bool，可选) - 平均计算是否包括零填充。默认值： ``True`` 。
        - **divisor_override** (int，可选) - 如果被指定为非0参数，该参数将会在平均计算中被用作除数，否则将会使用 `kernel_size` 作为除数，默认值： ``None`` 。

    输入：
        - **x** (Tensor) - shape为 :math:`(N, C, D_{in}, H_{in}, W_{in})` 或者 :math:`(C, D_{in}, H_{in}, W_{in})` 的Tensor。数据类型为float16、float32和float64。

    输出：
        shape为 :math:`(N, C, D_{out}, H_{out}, W_{out})` 或者 :math:`(C, D_{out}, H_{out}, W_{out})` 的Tensor。数据类型与 `x` 一致。

        其中，如果 `pad_mode` 为 `pad` 模式时，输出的shape计算公式如下：

        .. math::
            D_{out} = \left\lfloor\frac{D_{in} + 2 \times \text{padding}[0] -
                \text{kernel_size}[0]}{\text{stride}[0]} + 1\right\rfloor

        .. math::
            H_{out} = \left\lfloor\frac{H_{in} + 2 \times \text{padding}[1] -
                \text{kernel_size}[1]}{\text{stride}[1]} + 1\right\rfloor

        .. math::
            W_{out} = \left\lfloor\frac{W_{in} + 2 \times \text{padding}[2] -
                \text{kernel_size}[2]}{\text{stride}[2]} + 1\right\rfloor

    异常：
        - **TypeError** - `kernel_size` 既不是整数也不是元组。
        - **TypeError** - `stride` 既不是整数也不是元组。
        - **TypeError** - `padding` 既不是整数也不是元组/列表。
        - **TypeError** - `ceil_mode` 或 `count_include_pad` 不是bool。
        - **TypeError** - `divisor_override` 不是整数。
        - **ValueError** - `kernel_size` 或者 `stride` 中的数字不是正数。
        - **ValueError** - `kernel_size` 或 `stride` 是一个长度不为3的tuple。
        - **ValueError** - `padding` 为一个tuple/list时，长度不为1或者3。
        - **ValueError** - `padding` 的元素小于0。
        - **ValueError** - `x` 的shape长度不等于4或5。
        - **ValueError** - `divisor_override` 小于等于0。
        - **ValueError** -  `pad_mode` 不为 "pad" 的时候 `padding` 为非0。
