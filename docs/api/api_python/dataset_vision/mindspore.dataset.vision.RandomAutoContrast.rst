mindspore.dataset.vision.RandomAutoContrast
===========================================

.. py:class:: mindspore.dataset.vision.RandomAutoContrast(cutoff=0.0, ignore=None, prob=0.5)

    以给定的概率自动调整图像的对比度。

    参数：
        - **cutoff** (float, 可选) - 输入图像直方图中需要剔除的最亮和最暗像素的百分比。该值必须在 [0.0, 50.0) 范围内。默认值： ``0.0`` 。
        - **ignore** (Union[int, sequence], 可选) - 要忽略的背景像素值，该值必须在 [0, 255] 范围内。默认值： ``None`` 。
        - **prob** (float, 可选) - 图像被调整对比度的概率，取值范围：[0.0, 1.0]。默认值： ``0.5`` 。

    异常：
        - **TypeError** - 如果 `cutoff` 不是float类型。
        - **TypeError** - 如果 `ignore` 不是int或sequence类型。
        - **TypeError** - 如果 `prob` 的类型不为float。
        - **ValueError** - 如果 `cutoff` 不在[0, 50.0) 范围内。
        - **ValueError** - 如果 `ignore` 不在[0, 255] 范围内。
        - **ValueError** - 如果 `prob` 不在 [0.0, 1.0] 范围。
        - **RuntimeError** - 如果输入图像的shape不是 <H, W> 或 <H, W, C>。

    教程样例：
        - `视觉变换样例库
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/dataset/vision_gallery.html>`_
