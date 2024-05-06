mindspore.dataset.vision.RandomAdjustSharpness
==============================================

.. py:class:: mindspore.dataset.vision.RandomAdjustSharpness(degree, prob=0.5)

    以给定的概率随机调整输入图像的锐度。

    参数：
        - **degree** (float) - 锐度调整度，必须是非负的。
          ``0.0`` 度表示模糊图像， ``1.0`` 度表示原始图像， ``2.0`` 度表示清晰度增加2倍。
        - **prob** (float, 可选) - 图像被锐化的概率，取值范围：[0.0, 1.0]。默认值： ``0.5`` 。

    异常：
        - **TypeError** - 如果 `degree` 的类型不为float。
        - **TypeError** - 如果 `prob` 的类型不为float。
        - **ValueError** - 如果 `prob` 不在 [0.0, 1.0] 范围。
        - **ValueError** - 如果 `degree` 为负数。
        - **RuntimeError** -如果给定的张量形状不是<H, W>或<H, W, C>。

    教程样例：
        - `视觉变换样例库
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/dataset/vision_gallery.html>`_
