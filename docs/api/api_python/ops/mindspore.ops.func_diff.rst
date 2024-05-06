mindspore.ops.diff
==================

.. py:function:: mindspore.ops.diff(x, n=1, axis=-1, prepend=None, append=None)

    沿着给定维度计算输入Tensor `x` 的n阶前向差分。

    第一阶差分沿着给定 `axis` 由如下公式计算：:math:`out[i] = x[i+1] - x[i]` ，更高阶差分通过将上一阶计算结果作为输入迭代调用 `diff` 计算实现。

    .. note::
        不支持空Tensor, 如果传入了空Tensor，会出现ValueError。空Tensor指的是，Tensor的任意一维为零。比如shape为 :math:`(0,)`  ， :math:`(1, 2, 0, 4)` 的Tensor都为空Tensor。

    参数：
        - **x** (Tensor) - 输入Tensor。x元素的数据类型不支持uint16、uint32 或 uint64。
        - **n** (int，可选) - 递归计算差分的阶数，目前只支持1。默认值： ``1`` 。
        - **axis** (int，可选) - 计算差分的维度，默认是最后一维。默认值： ``-1`` 。
        - **prepend** (Tensor，可选) - 在计算差分之前，沿 axis 添加到 `x` 之前的值。它们的维度必须与输入的维度相同，并且它们的shape必须与输入的shape匹配，但 axis 除外。默认值： ``None`` 。
        - **append** (Tensor，可选) - 在计算差分之前，沿 axis 添加到 `x` 之后的值。它们的维度必须与输入的维度相同，并且它们的shape必须与输入的shape匹配，但 axis 除外。默认值： ``None`` 。

    返回：
        Tensor，输入Tensor差分后的结果。输出的shape除了第 `axis` 维上尺寸缩小 `n` 以外，与 `x` 一致。输出的类型与 `x` 一致。

    异常：
        - **TypeError** - 如果 `x` 不是Tensor。
        - **TypeError** - 如果 `x` 的元素的数据类型是uint16、uint32 或 uint64。
        - **ValueError** - 如果 `x` 是空Tensor。
        - **ValueError** - 如果 `x` 是的维度小于1。
        - **RuntimeError** - 如果 `n` 不是1。
