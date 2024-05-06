mindspore.ops.standard_normal
=============================

.. py:function:: mindspore.ops.standard_normal(shape, seed=None)

    根据标准正态（高斯）随机数分布生成随机数。

    返回具有给定shape的Tensor，其中的随机数从平均值为0、标准差为1的标准正态分布中取样。

    .. math::
        f(x)=\frac{1}{\sqrt{2 \pi}} e^{\left(-\frac{x^{2}}{2}\right)}

    参数：
        - **shape** (Union[tuple, Tensor]) - 待生成的Tensor的shape。当为tuple类型时，只支持常量值；当为Tensor类型时，支持动态Shape。
        - **seed** (int, 可选) - 随机种子，非负值。默认值： ``None`` 。

    返回：
        Tensor。shape为输入 `shape` 。数据类型支持float32。

    异常：
        - **TypeError** - `shape` 既不是tuple，也不是Tensor。
        - **ValueError** - `shape` 为tuple时，包含非正的元素。
