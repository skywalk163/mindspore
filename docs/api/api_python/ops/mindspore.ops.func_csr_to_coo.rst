mindspore.ops.csr_to_coo
========================

.. py:function:: mindspore.ops.csr_to_coo(tensor: CSRTensor)

    将一个CSRTensor转化成一个COOTensor。

    .. note::
        现在只支持二维CSRTensor。

    参数：
        - **tensor** (CSRTensor) - 一个CSR矩阵，必须是二维。

    返回：
        返回一个二维的COOTensor，是原COOTensor的CSR格式表示。

    异常：
        - **TypeError** - `tensor` 不是CSRTensor。
        - **ValueError** - `tensor` 不是二维CSRTensor。
