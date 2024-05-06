mindspore.dataset.vision.Pad
============================

.. py:class:: mindspore.dataset.vision.Pad(padding, fill_value=0, padding_mode=Border.CONSTANT)

    填充图像。

    支持 Ascend 硬件加速，需要通过 `.device("Ascend")` 方式开启。

    参数：
        - **padding** (Union[int, Sequence[int, int], Sequence[int, int, int, int]]) - 图像各边填充的像素数。
          如果 `padding` 是一个整数，代表为图像的所有方向填充该值大小的像素。
          如果 `padding` 是一个包含2个值的元组或列表，第一个值会用于填充图像的左侧和右侧，第二个值会用于填充图像的上侧和下侧。
          如果 `padding` 是一个包含4个值的元组或列表，则分别填充图像的左侧、上侧、右侧和下侧。
          填充值必须为非负值。
        - **fill_value** (Union[int, tuple[int]], 可选) - 填充的像素值，仅在 `padding_mode` 取值为 ``Border.CONSTANT`` 时有效。
          如果是3元素元组，则分别用于填充R、G、B通道。
          如果是整数，则用于所有 RGB 通道。
          `fill_value` 值必须在 [0, 255] 范围内。默认值： ``0`` 。
        - **padding_mode** (:class:`~.vision.Border`, 可选) - 边界填充方式。可以是 ``Border.CONSTANT`` 、 ``Border.EDGE`` 、 ``Border.REFLECT`` 、 ``Border.SYMMETRIC`` 。默认值： ``Border.CONSTANT`` 。

          - **Border.CONSTANT** - 使用常量值进行填充。
          - **Border.EDGE** - 使用各边的边界像素值进行填充。
          - **Border.REFLECT** - 以各边的边界为轴进行镜像填充，忽略边界像素值。
          - **Border.SYMMETRIC** - 以各边的边界为轴进行对称填充，包括边界像素值。

    异常：
        - **TypeError** - 如果 `padding` 不是int或Sequence[int, int], Sequence[int, int, int, int]类型。
        - **TypeError** - 如果 `fill_value` 不是int或tuple[int]类型。
        - **TypeError** - 如果 `padding_mode` 不是 :class:`mindspore.dataset.vision.Border` 的类型。
        - **ValueError** - 如果 `padding` 为负数。
        - **ValueError** - 如果 `fill_value` 不在 [0, 255] 范围内。
        - **RuntimeError** - 如果输入图像的shape不是 <H, W> 或 <H, W, C>。

    教程样例：
        - `视觉变换样例库
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/dataset/vision_gallery.html>`_

    .. py:method:: device(device_target="CPU")

        指定该变换执行的设备。

        - 当执行设备是 Ascend 时，输入/输出数据的维度限制为[4, 6]和[32768, 32768]之间。

        参数：
            - **device_target** (str, 可选) - 算子将在指定的设备上运行。当前支持 ``CPU`` 和 ``Ascend`` 。默认值： ``CPU`` 。

        异常：
            - **TypeError** - 当 `device_target` 的类型不为str。
            - **ValueError** - 当 `device_target` 的取值不为 ``CPU`` / ``Ascend`` 。
