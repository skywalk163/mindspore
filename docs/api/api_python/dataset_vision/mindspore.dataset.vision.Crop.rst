mindspore.dataset.vision.Crop
=============================

.. py:class:: mindspore.dataset.vision.Crop(coordinates, size)

    在输入图像上裁剪出指定区域。

    支持 Ascend 硬件加速，需要通过 `.device("Ascend")` 方式开启。

    参数：
        - **coordinates**  (sequence) - 裁剪区域的起始左上角坐标。必须是两个值的序列，形式为(上，左)。
        - **size**  (Union[int, sequence]) - 裁剪区域的尺寸大小。
          如果 `size` 是整数，则返回一个裁剪尺寸大小为 (size, size) 的正方形。
          如果 `size` 是一个长度为 2 的序列，则以2个元素分别为高和宽放缩至(高度, 宽度)大小。
          值必须大于 0。

    异常：
        - **TypeError** - 如果 `coordinates` 不是sequence类型。
        - **TypeError** - 如果 `size` 不是int或sequence类型。
        - **ValueError** - 如果 `coordinates` 小于 0。
        - **ValueError** - 如果 `size` 小于或等于 0。
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
