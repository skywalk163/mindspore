mindspore.ops.coo_ceil
=======================

.. py:function:: mindspore.ops.coo_ceil(x: COOTensor)

    COOTensor向上取整函数。

    .. math::
        out_i = \lceil x_i \rceil = \lfloor x_i \rfloor + 1

    参数：
        - **x** (COOTensor) - Ceil的输入。其数据类型为float16或float32。

    返回：
        COOTensor，shape与 `x` 相同。

    异常：
        - **TypeError** - `x` 的不是COOTensor。
        - **TypeError** - `x` 的数据类型既不是float16也不是float32。
