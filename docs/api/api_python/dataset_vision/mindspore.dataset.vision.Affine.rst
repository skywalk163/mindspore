mindspore.dataset.vision.Affine
===============================

.. py:class:: mindspore.dataset.vision.Affine(degrees, translate, scale, shear, resample=Inter.NEAREST, fill_value=0)

    对输入图像进行仿射变换，保持图像中心不动。

    支持 Ascend 硬件加速，需要通过 `.device("Ascend")` 方式开启。

    参数：
        - **degrees** (float) - 顺时针的旋转角度，取值需为-180到180之间。
        - **translate** (Sequence[float, float]) - 水平和垂直方向上的平移长度，需为2元素序列，取值在-1和1之间。
        - **scale** (float) - 放缩因子，需为正数。
        - **shear** (Union[float, Sequence[float, float]]) - 裁切度数，取值需为-180到180之间。
          若输入单个数值，表示平行于X轴的裁切角度，不进行Y轴上的裁切；
          若输入序列[float, float]，分别表示平行于X轴和Y轴的裁切角度。
        - **resample** (:class:`~.vision.Inter`, 可选) - 图像插值方法。可选值详见 :class:`mindspore.dataset.vision.Inter` 。
          默认值： ``Inter.NEAREST``。
        - **fill_value** (Union[int, tuple[int, int, int]], 可选) - 用于填充输出图像中变换之外的区域。元组中必须有三个值，取值范围是[0, 255]。默认值： ``0`` 。

    异常：
        - **TypeError** - 如果 `degrees` 不是float类型。
        - **TypeError** - 如果 `translate` 不是Sequence[float, float]类型。
        - **TypeError** - 如果 `scale` 不是float类型。
        - **ValueError** - 如果 `scale` 非正数。
        - **TypeError** - 如果 `shear` 不是float或Sequence[float, float]类型。
        - **TypeError** - 如果 `resample` 不是 :class:`mindspore.dataset.vision.Inter` 的类型。
        - **TypeError** - 如果 `fill_value` 不是int或tuple[int, int, int]类型。
        - **RuntimeError** - 如果输入图像的shape不是 <H, W> 或 <H, W, C>。

    教程样例：
        - `视觉变换样例库
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/dataset/vision_gallery.html>`_

    .. py:method:: device(device_target="CPU")

        指定该变换执行的设备。

        - 当执行设备是 Ascend 时，输入数据的维度限制为[4, 6]和[32768, 32768]之间。

        参数：
            - **device_target** (str, 可选) - 算子将在指定的设备上运行。当前支持 ``CPU`` 和 ``Ascend`` 。默认值： ``CPU`` 。

        异常：
            - **TypeError** - 当 `device_target` 的类型不为str。
            - **ValueError** - 当 `device_target` 的取值不为 ``CPU`` / ``Ascend`` 。
