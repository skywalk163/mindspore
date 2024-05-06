mindspore.ops.pixel_unshuffle
==============================

.. py:function:: mindspore.ops.pixel_unshuffle(input, downscale_factor)

    对输入 `input` 应用逆像素重组操作，这是像素重组的逆操作。关于pixel_unshuffle算法详细介绍，请参考 `Real-Time Single Image and Video Super-Resolution Using an Efficient Sub-Pixel Convolutional Neural Network <https://arxiv.org/abs/1609.05158>`_ 。

    通常情况下，`input` shape :math:`(*, C, H \times r, W \times r)` ，输出shape :math:`(*, C \times r^2, H, W)` 。`r` 是缩小因子。 `*` 是大于等于0的维度。

    参数：
        - **input** (Tensor) - Tensor，shape为 :math:`(*, C, H \times r, W \times r)` 。 `input` 的维度需要大于2，并且倒数第一和倒数第二维对应的值可以被 `downscale_factor` 整除。
        - **downscale_factor** (int) - 恢复输入Tensor的因子，是正整数。 `downscale_factor` 是上面提到的 :math:`r` 。

    返回：
        - **output** (Tensor) - Tensor，shape为 :math:`(*, C \times r^2, H, W)` 。

    异常：
        - **ValueError** - `downscale_factor` 不是正整数。
        - **ValueError** - `input` 倒数第一和倒数第二维度对应的值不能被 `downscale_factor` 整除。
        - **ValueError** - `input` 维度小于3。
        - **TypeError** - `input` 不是Tensor。
