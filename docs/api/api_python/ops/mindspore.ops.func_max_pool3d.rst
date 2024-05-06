mindspore.ops.max_pool3d
========================

.. py:function:: mindspore.ops.max_pool3d(x, kernel_size, stride=None, padding=0, dilation=1, ceil_mode=False, return_indices=False)

    三维最大值池化。

    输入是shape为 :math:`(N_{in}, C_{in}, D_{in}, H_{in}, W_{in})` 的Tensor，输出 :math:`(D_{in}, H_{in}, W_{in})` 维度中的最大值。给定 `kernel_size`
    :math:`ks = (d_{ker}, h_{ker}, w_{ker})`，和 `stride` :math:`s = (s_0, s_1, s_2)`，运算如下：

    .. math::
        \text{output}(N_i, C_j, d, h, w) =
        \max_{l=0, \ldots, d_{ker}-1} \max_{m=0, \ldots, h_{ker}-1} \max_{n=0, \ldots, w_{ker}-1}
        \text{input}(N_i, C_j, s_0 \times d + l, s_1 \times h + m, s_2 \times w + n)

    参数：
        - **x** (Tensor) - shape为 :math:`(N_{in}, C_{in}, D_{in}, H_{in}, W_{in})` 的Tensor。支持数据类型包括int8、int16、int32、int64、uint8、uint16、uint32、uint64、float16、float32和float64。
        - **kernel_size** (Union[int, tuple[int]]) - 池化核尺寸大小。可以是一个整数表示池化核的深度，高度和宽度，或者包含三个整数的tuple，分别表示池化核的深度，高度和宽度。
        - **stride** (Union[int, tuple[int]]) - 池化操作的移动步长。可以是一个整数表示在深度，高度和宽度方向的移动步长，或者包含三个整数的tuple，分别表示在深度，高度和宽度方向的移动步长。默认值： ``None`` ， 表示移动步长为 `kernel_size` 。
        - **padding** (Union[int, tuple[int]]) - 池化填充长度。可以是一个整数表示在深度，高度和宽度方向的填充长度，或者包含三个整数的tuple，分别表示在深度，高度和宽度方向的填充长度。默认为 ``0`` 。
        - **dilation** (Union[int, tuple[int]]) - 控制池化核内元素的间距。默认为 ``1`` 。
        - **ceil_mode** (bool) - 是否是用ceil代替floor来计算输出的shape。默认为 ``False`` 。
        - **return_indices** (bool) - 是否输出最大值的索引。默认为 ``False`` 。

    返回：
        如果 `return_indices` 是False，返回Tensor `output`，否则返回tuple (`output`, `argmax`)。

        - **output** (Tensor) - 输出的池化后的最大值，shape 为 :math:`(N_{out}, C_{out}, D_{out}, H_{out}, W_{out})` 。其数据类型与 `x` 相同。

        .. math::
            D_{out} = \left\lfloor\frac{D_{in} + 2 \times \text{padding}[0] - \text{dilation}[0] \times
            (\text{kernel_size}[0] - 1) - 1}{\text{stride}[0]} + 1\right\rfloor

        .. math::
            H_{out} = \left\lfloor\frac{H_{in} + 2 \times \text{padding}[1] - \text{dilation}[1] \times
            (\text{kernel_size}[1] - 1) - 1}{\text{stride}[1]} + 1\right\rfloor

        .. math::
            W_{out} = \left\lfloor\frac{W_{in} + 2 \times \text{padding}[2] - \text{dilation}[2] \times
            (\text{kernel_size}[2] - 1) - 1}{\text{stride}[2]} + 1\right\rfloor

        - **argmax** (Tensor) - 输出的最大值对应的索引，数据类型为int64。仅当 `return_indices` 为 ``True`` 的时候才返回该值。

    异常：
        - **TypeError** - `x` 不是Tensor。
        - **ValueError** - `x` 的维度不是5D。
        - **TypeError** - `kernel_size` 、`stride` 、`padding` 、`dilation` 不是int或者tuple。
        - **ValueError** - `kernel_size` 或 `stride` 的元素值小于1。
        - **ValueError** - `padding` 的元素值小于0。
