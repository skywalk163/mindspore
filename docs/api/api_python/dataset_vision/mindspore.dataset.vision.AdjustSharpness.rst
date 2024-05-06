mindspore.dataset.vision.AdjustSharpness
========================================

.. py:class:: mindspore.dataset.vision.AdjustSharpness(sharpness_factor)

    调整输入图像的锐度。

    参数：
        - **sharpness_factor** (float) - 锐度调节因子，需为非负数。输入 ``0`` 值将得到模糊图像， ``1`` 值将得到原始图像，
          ``2`` 值将调整图像锐度为原来的2倍。

    异常：
        - **TypeError** - 如果 `sharpness_factor` 不是float类型。
        - **ValueError** - 如果 `sharpness_factor` 小于0。
        - **RuntimeError** - 如果输入图像的形状不是<H, W, C>或<H, W>。

    教程样例：
        - `视觉变换样例库
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/dataset/vision_gallery.html>`_
