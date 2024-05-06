mindspore.ops.MaxPoolWithArgmaxV2
=================================

.. py:class:: mindspore.ops.MaxPoolWithArgmaxV2(kernel_size, strides=None, pads=0, dilation=(1, 1), ceil_mode=False, argmax_type=mstype.int64)

    对输入Tensor执行最大池化运算，并返回最大值和索引。

    通常，输入的shape为 :math:`(N_{in}, C_{in}, H_{in}, W_{in})` ，MaxPool在 :math:`(H_{in}, W_{in})` 维度输出区域最大值。给定 `kernel_size` 为 :math:`(h_{ker}, w_{ker})` 和 `strides` 为 :math:`(s_0, s_1)` ，运算如下：

    .. math::
        \text{output}(N_i, C_j, h, w) = \max_{m=0, \ldots, h_{ker}-1} \max_{n=0, \ldots, w_{ker}-1}
        \text{input}(N_i, C_j, s_0 \times h + m, s_1 times\ w + n)

    .. warning::
        这是一个实验性API，后续可能修改或删除。

    参数：
        - **kernel_size** (Union[int, tuple[int]]) - 指定池化核尺寸大小。由一个整数或者两个整数组成的tuple，分别表示高和宽。
        - **strides** (Union[int, tuple[int]]，可选) - 池化操作的移动步长。由一个整数或者两个整数组成的tuple，分别表示在高和宽方向上的移动步长。默认值： ``None`` 。表示取 `kernel_size` 的值。
        - **pads** (Union[int, tuple[int]]，可选) - 池化操作的填充元素个数。由一个整数或者两个整数组成的tuple，分别表示在高和宽方向上的填充0的个数。默认值： ``0`` 。
        - **dilation** (Union[int, tuple[int]]，可选) - 控制池化核内元素的间距。由一个整数或者两个整数组成的tuple，分别表示在高和宽方向上的核内间距。默认值： ``(1, 1)`` 。
        - **ceil_mode** (bool，可选) - 控制是否使用Ceil计算输出shape。默认值： ``False`` 。表示使用Floor计算输出。
        - **argmax_type** (mindspore.dtype，可选) - 指定输出 `argmax` 的数据类型。默认值： ``mstype.int64`` 。【该参数在Ascend上不生效。】

    输入：
        - **x** (Tensor) - shape为 :math:`(N_{in}, C_{in}, H_{in}, W_{in})` 的Tensor。在CPU和GPU上，支持的数据类型包括：int8，int16，int32，int64，uint8，uint16，uint32，uint64，float16，float32和float64。在Ascend上，数据类型仅支持float16。

    输出：
        包含两个Tensor的tuple，分别表示最大值结果和最大值对应的索引。

        - **output** (Tensor) - 输出池化后的最大值，shape为 :math:`(N_{out}, C_{out}, H_{out}, W_{out})`。其数据类型与 `x` 相同。

          .. math::
              H_{out} = \left\lfloor\frac{H_{in} + 2 * \text{pads[0]} - \text{dilation[0]} \times (\text{kernel_size[0]} - 1) - 1}{\text{strides[0]}} + 1\right\rfloor

          .. math::
              W_{out} = \left\lfloor\frac{W_{in} + 2 * \text{pads[1]} - \text{dilation[1]} \times (\text{kernel_size[1]} - 1) - 1}{\text{strides[1]}} + 1\right\rfloor

        - **argmax** (Tensor) - 输出的最大值对应的索引。在CPU和GPU上，数据类型为int32或者int64。在Ascend上，数据类型为uint16。

    异常：
        - **TypeError** - `x` 不是Tensor。
        - **ValueError** - `x` 的维度不是4D。
        - **TypeError** - `kernel_size` 、 `strides` 、 `pads` 或者 `dilation` 即不是int也不是tuple。
        - **ValueError** - `kernel_size` 、 `strides` 或者 `dilation` 的元素小于1。
        - **ValueError** - `pads` 的元素值小于0。
        - **ValueError** - `pads` 的元素值大于 `kernel_size` 的一半。
        - **ValueError** - `argmax_type` 即不是mindspore.int64也不是mindspore.int32。
        - **TypeError** - `ceil_mode` 不是bool。
