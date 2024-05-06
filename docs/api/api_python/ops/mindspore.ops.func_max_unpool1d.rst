mindspore.ops.max_unpool1d
===========================

.. py:function:: mindspore.ops.max_unpool1d(x, indices, kernel_size, stride=None, padding=0, output_size=None)

    `max_pool1d` 的逆过程。
    
    `max_unpool1d` 在计算过程中，保留最大值位置的元素，并将非最大值位置元素设置为0。
    支持的输入数据格式为 :math:`(N, C, H_{in})` 或 :math:`(C, H_{in})` ，输出数据的格式为 :math:`(N, C, H_{out})`
    或 :math:`(C, H_{out})` ，计算公式如下：

    .. math::
        \begin{array}{ll} \\
        H_{out} = (H_{in} - 1) \times stride[0] - 2 \times padding[0] + kernel\_size[0] \\
        \end{array}

    参数：
        - **x** (Tensor) - 待求逆的Tensor。shape为 :math:`(N, C, H_{in})` 或 :math:`(C, H_{in})` 。
        - **indices** (Tensor) - 最大值的索引。shape必须与输入 `x` 相同。取值范围需满足 :math:`[0, H_{in} - 1]` 。
          数据类型必须是int32或int64。
        - **kernel_size** (Union[int, tuple[int]]) - 池化核尺寸大小。
        - **stride** (Union[int, tuple[int]]) - 池化操作的移动步长，若取值为 '0'， '(0)' 或 ``None`` ， ``stride`` 值与 ``kernel_size``
          相同。默认值： ``None`` ，表示移动步长为 `kernel_size` 。
        - **padding** (Union[int, tuple[int]]) - 填充值。默认值： ``0`` 。
        - **output_size** (tuple[int], 可选) - 输出shape。默认值： ``None`` 。
          如果output_size为()，那么输出shape根据 `kernel_size` 、 `stride` 和 `padding` 计算得出。
          如果output_size不为()，那么 `output_size` 必须满足格式 :math:`(N, C, H)` ， :math:`(C, H)` 或 :math:`(H)`，取值范围需满足：
          :math:`[(N, C, H_{out} - stride[0]), (N, C, H_{out} + stride[0])]` 。

    返回：
        shape为 :math:`(N, C, H_{out})` 或 :math:`(C, H_{out})` 的Tensor，数据类型与输入 `x` 相同。

    异常：
        - **TypeError** - `x` 或 `indices` 的数据类型不支持。
        - **TypeError** - `kernel_size` 、 `stride` 或 `padding` 既不是整数也不是tuple。
        - **ValueError** - `stride` 、 `padding` 或 `kernel_size` 的值不是非负的。
        - **ValueError** - `x` 和 `indices` 的shape不一致。
        - **ValueError** - `x` 的长度不是2或3。
        - **ValueError** - `output_size` 的类型不是tuple。
        - **ValueError** - `output_size` 的长度不为0、2或3。
        - **ValueError** - `output_size` 的取值与根据 `kernel_size` 、 `stride` 、 `padding` 计算得到的结果差距太大。
