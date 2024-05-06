mindspore.ops.max_unpool2d
===========================

.. py:function:: mindspore.ops.max_unpool2d(x, indices, kernel_size, stride=None, padding=0, output_size=None)

    `max_pool2d` 的逆过程。
    
    `max_unpool2d` 在计算过程中，保留最大值位置的元素，并将非最大值位置元素设置为0。
    支持的输入数据格式为 :math:`(N, C, H_{in}, W_{in})` 或 :math:`(C, H_{in}, W_{in})` ，
    输出数据的格式为 :math:`(N, C, H_{out}, W_{out})` 或 :math:`(C, H_{out}, W_{out})` ，计算公式如下：

    .. math::
        \begin{array}{ll} \\
        H_{out} = (H{in} - 1) \times stride[0] - 2 \times padding[0] + kernel\_size[0] \\
        W_{out} = (W{in} - 1) \times stride[1] - 2 \times padding[1] + kernel\_size[1] \\
        \end{array}

    参数：
        - **x** (Tensor) - 待求逆的Tensor。shape为 :math:`(N, C, H_{in}, W_{in})` 或 :math:`(C, H_{in}, W_{in})` 。
        - **indices** (Tensor) - 最大值的索引。shape必须与输入 `x` 相同。取值范围需满足 :math:`[0, H_{in} \times W_{in} - 1]` 。
          数据类型必须是int32或int64。
        - **kernel_size** (Union[int, tuple[int]]) - 池化核尺寸大小。int类型表示池化核的长宽相同。
          tuple类型中的两个值分别代表池化核的长和宽。
        - **stride** (Union[int, tuple[int]]) - 池化操作的移动步长，int类型表示长宽方向的移动步长相同。
          tuple中的两个值分别代表长宽方向移动的步长。默认值： ``None`` ，表示移动步长为 `kernel_size` 。
        - **padding** (Union[int, tuple[int]]) - 填充值。默认值： ``0`` 。若为int类型，则长宽方向的填充大小相同，均为 `padding` 。
          若为tuple类型，则tuple中的两个值分别代表长宽方向填充的大小。
        - **output_size** (tuple[int]，可选) - 输出shape。默认值： ``None`` 。
          如果output_size为()，那么输出shape根据 ``kernel_size`` 、 ``stride`` 和 ``padding`` 计算得出。
          如果output_size不为()，那么 `output_size` 必须满足格式 :math:`(N, C, H, W)` ， :math:`(C, H, W)` 或 :math:`(H, W)` ，取值范围需满足：
          :math:`[(N, C, H_{out} - stride[0], W_{out} - stride[1]), (N, C, H_{out} + stride[0], W_{out} + stride[1])]`。

    返回：
        shape为 :math:`(N, C, H_{out}, W_{out})` 或 :math:`(C, H_{out}, W_{out})` 的Tensor，数据类型与输入 `x` 相同。

    异常：
        - **TypeError** - `x` 或 `indices` 的数据类型不支持。
        - **TypeError** - `kernel_size` 、 `stride` 或 `padding` 既不是整数也不是tuple。
        - **ValueError** - `stride` 、 `padding` 或 `kernel_size` 的值不是非负的。
        - **ValueError** - `x` 和 `indices` 的shape不一致。
        - **ValueError** - `kernel_size` 、 `stride` 或 `padding` 为tuple时长度不等于2。
        - **ValueError** - `x` 的长度不为3或4。
        - **ValueError** - `output_size` 的类型不是tuple。
        - **ValueError** - `output_size` 的长度不为0、3或4。
        - **ValueError** - `output_size` 的取值与根据 `kernel_size` 、 `stride` 、 `padding` 计算得到的结果差距太大。
