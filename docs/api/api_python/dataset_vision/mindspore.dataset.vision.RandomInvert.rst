mindspore.dataset.vision.RandomInvert
=====================================

.. py:class:: mindspore.dataset.vision.RandomInvert(prob=0.5)

    以给定的概率随机反转图像的颜色。

    参数：
        - **prob** (float, 可选) - 图像被反转颜色的概率，取值范围：[0.0, 1.0]。默认值： ``0.5`` 。

    异常：
        - **TypeError** - 如果 `prob` 的类型不为float。
        - **ValueError** - 如果 `prob` 不在 [0.0, 1.0] 范围。
        - **RuntimeError** - 如果输入图像的shape不是 <H, W, C>。

    教程样例：
        - `视觉变换样例库
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/dataset/vision_gallery.html>`_
