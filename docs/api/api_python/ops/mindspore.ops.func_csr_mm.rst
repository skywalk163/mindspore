mindspore.ops.csr_mm
=================================

.. py:function:: mindspore.ops.csr_mm(a: CSRTensor, b: CSRTensor, trans_a: bool = False, trans_b: bool = False, adjoint_a: bool = False, adjoint_b: bool = False)

    返回稀疏矩阵a与稀疏矩阵或稠密矩阵b的矩阵乘法结果。

    .. note::
        若右矩阵为Tensor，则仅支持安装了LLVM12.0.1及以上版本的CPU后端或GPU后端。
        若右矩阵为CSRTensor， 则仅支持GPU后端。

    参数：
        - **a** (CSRTensor) - 稀疏的 CSRTensor。
        - **b** (CSRTensor) - 稀疏的 CSRTensor或稠密矩阵。
        - **trans_a** (bool, 可选) - 是否对矩阵a进行转置。默认值： ``False`` 。
        - **trans_b** (bool, 可选) - 是否对矩阵b进行转置。默认值： ``False`` 。
        - **adjoint_a** (bool, 可选) - 是否对矩阵a进行共轭。默认值： ``False`` 。
        - **adjoint_b** (bool, 可选) - 是否对矩阵b进行共轭。默认值： ``False`` 。

    返回：
        返回稀疏矩阵，类型为CSRTensor。