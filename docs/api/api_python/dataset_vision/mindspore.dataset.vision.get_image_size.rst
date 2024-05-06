﻿mindspore.dataset.vision.get_image_size
========================================

.. py:function:: mindspore.dataset.vision.get_image_size(image)

    获取输入图像的大小为[高度, 宽度]。

    参数：
        - **image** (Union[numpy.ndarray, PIL.Image.Image]) - 要获取大小的图像。

    返回：
        list[int, int]，图像大小。

    异常：
        - **RuntimeError** - `image` 参数的维度小于2。
        - **TypeError** - `image` 参数的类型既不是 np.ndarray，也不是 PIL Image。
