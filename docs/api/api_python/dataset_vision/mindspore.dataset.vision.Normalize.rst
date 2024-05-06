mindspore.dataset.vision.Normalize
==================================

.. py:class:: mindspore.dataset.vision.Normalize(mean, std, is_hwc=True)

    根据均值和标准差对输入图像进行归一化。

    此处理将使用以下公式对输入图像进行归一化：output[channel] = (input[channel] - mean[channel]) / std[channel]，其中 channel 代表通道索引，channel >= 1。

    支持 Ascend 硬件加速，需要通过 `.device("Ascend")` 方式开启。

    .. note:: 此操作默认通过 CPU 执行，也支持异构加速到 GPU 或 Ascend 上执行。

    参数：
        - **mean** (sequence) - 图像每个通道的均值组成的列表或元组。平均值必须在 [0.0, 255.0] 范围内。
        - **std** (sequence) - 图像每个通道的标准差组成的列表或元组。标准差值必须在 (0.0, 255.0] 范围内。
        - **is_hwc** (bool, 可选) - 表示输入图像是否为HWC格式， ``True`` 为HWC格式， ``False`` 为CHW格式。默认值： ``True`` 。

    异常：
        - **TypeError** - 如果 `mean` 不是sequence类型。
        - **TypeError** - 如果 `std` 不是sequence类型。
        - **TypeError** - 如果 `is_hwc` 不是bool类型。
        - **ValueError** - 如果 `mean` 不在 [0.0, 255.0] 范围内。
        - **ValueError** - 如果 `std` 不在 (0.0, 255.0] 范围内。
        - **RuntimeError** - 如果给定的tensor format不是<H, W>或<...,H, W, C>。

    教程样例：
        - `视觉变换样例库
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/dataset/vision_gallery.html>`_

    .. py:method:: device(device_target="CPU")

        指定该变换执行的设备。

        - 当执行设备是 CPU 时，输入数据支持 `uint8` 、 `float32` 或者 `float64` 类型，输入数据的通道支持 1/2/3 。
        - 当执行设备是 Ascend 时，输入数据支持 `uint8` 或者 `float32` 类型，输入数据的通道仅支持 1/3。输入数据的维度限制为[4, 6]和[8192, 4096]之间。

        参数：
            - **device_target** (str, 可选) - 算子将在指定的设备上运行。当前支持 ``CPU`` 和 ``Ascend`` 。默认值： ``CPU`` 。

        异常：
            - **TypeError** - 当 `device_target` 的类型不为str。
            - **ValueError** - 当 `device_target` 的取值不为 ``CPU`` / ``Ascend`` 。
