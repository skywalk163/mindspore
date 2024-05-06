mindspore.Tensor.sum
====================

.. py:method:: mindspore.Tensor.sum(axis=None, dtype=None, keepdims=False, initial=None)

    返回指定维度上数组元素的总和。

    .. note::
        - 不支持NumPy参数 `out` 、 `where` 、 `casting` 、 `order` 、 `subok` 、 `signature` 和 `extobj` 。
        - Tensor类型的 `axis` 仅用作兼容旧版本，不推荐使用。

    参数：
        - **axis** (Union[None, int, tuple(int), list(int), Tensor]) - 指定维度，在该维度方向上进行求和运算。默认值： ``None`` 。如果参数值为 ``None`` ，会计算输入数组中所有元素的和。如果 `axis` 为负数，则从最后一维开始往第一维计算。如果 `axis` 为整数元组或列表，会对该元组或列表指定的所有 `axis` 方向上的元素进行求和。
        - **dtype** (`mindspore.dtype`, 可选) - 默认值为 ``None`` 。会覆盖输出Tensor的dtype。
        - **keepdims** (bool) - 如果这个参数为 ``True`` ，被删去的维度保留在结果中，且维度大小设为1。有了这个选项，结果就可以与输入数组进行正确的广播运算。如果设为默认值，那么 `keepdims` 不会被传递给ndarray子类的sum方法。但是任何非默认值都会被传递。如果子类的方法未实现 `keepdims` ，则引发异常。默认值： ``False`` 。
        - **initial** (scalar) - 初始化的起始值。默认值： ``None`` 。

    返回：
        Tensor。具有与输入相同shape的Tensor，删除了指定的 `axis` 。如果输入Tensor是零维数组，或 `axis` 为 ``None`` 时，返回一个标量。

    异常：
        - **TypeError** - input不是Tensor， `axis` 不是整数、整数元组、整数列表或Tensor， `keepdims` 不是整数，或者 `initial` 不是标量。
        - **ValueError** - 任意 `axis` 超出范围或存在重复的 `axis` 。

    其他API参考：
        - :func:`mindspore.Tensor.cumsum` ：返回沿给定 `axis` 的元素累加和。
