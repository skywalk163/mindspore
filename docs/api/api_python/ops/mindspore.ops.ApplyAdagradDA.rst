mindspore.ops.ApplyAdagradDA
=============================

.. py:class:: mindspore.ops.ApplyAdagradDA(use_locking=False)

    ����Adagrad�㷨���� `var` ��

    Adagrad�㷨������ `Adaptive Subgradient Methods for Online Learning and Stochastic Optimization <http://www.jmlr.org/papers/volume12/duchi11a/duchi11a.pdf>`_ �������

    .. math::
        \begin{array}{ll} \\
            grad\_accum += grad \\
            grad\_squared\_accum += grad * grad \\
            tmp\_val=
                \begin{cases}
                     sign(grad\_accum) * max\left \{|grad\_accum|-l1*global\_step, 0\right \} & \text{ if } l1>0 \\
                     grad\_accum & \text{ otherwise } \\
                 \end{cases} \\
            x\_value = -1 * lr * tmp\_val \\
            y\_value = l2 * global\_step * lr + \sqrt{grad\_squared\_accum} \\
            var = \frac{ x\_value }{ y\_value }
        \end{array}

    `var` �� `gradient_accumulator` �� `gradient_squared_accumulator` �� `grad` ��������ѭ��ʽ����ת������ʹ��������һ�¡�
    ������Ǿ��в�ͬ���������ͣ���ϵ;��ȵ��������ͽ�ת��Ϊ�����߾��ȵ��������͡�

    ������
        - **use_locking** (bool) - ���Ϊ ``True`` �� `var` �� `gradient_accumulator` �ĸ��½��ܵ����ı�����������ΪΪδ���壬�ܿ��ܳ��ֽ��ٵĳ�ͻ��Ĭ��ֵΪ ``False`` ��

    ���룺
        - **var** (Parameter) - Ҫ���µı������������ͱ���Ϊfloat16��float32��shape�� :math:`(N, *)` ������ :math:`*` ��ʾ���������ĸ���ά�ȡ�
        - **gradient_accumulator** (Parameter) - Ҫ�����ۻ����ݶȣ�Ϊ��ʽ�е� :math:`grad\_accum` ��shape������ `var` ��ͬ��
        - **gradient_squared_accumulator** (Parameter) - Ҫ���µ�ƽ���ۻ����ݶȣ� Ϊ��ʽ�е� :math:`grad\_squared\_accum` ��shape������ `var` ��ͬ��
        - **grad** (Tensor) - �ݶȣ�Ϊһ��Tensor��shape������ `var` ��ͬ��
        - **lr** ([Number, Tensor]) - ѧϰ�ʡ�������Scalar����������Ϊfloat32��float16��
        - **l1** ([Number, Tensor]) - L1���򻯡�������Scalar����������Ϊfloat32��float16��
        - **l2** ([Number, Tensor]) - L2���򻯡�������Scalar����������Ϊfloat32��float16��
        - **global_step** ([Number, Tensor]) - ѵ������ı�š�������Scalar����������Ϊint32��int64��

    �����
        1��Tensor��ɵ�tuple�����º�Ĳ�����

        - **var** (Tensor) - shape������������ `var` ��ͬ��

    �쳣��
        - **TypeError** - ��� `var` �� `gradient_accumulator` �� `gradient_squared_accumulator` ����Parameter��
        - **TypeError** - ��� `grad` ���� Tensor��
        - **TypeError** - ��� `lr` �� `l1` �� `l2` ���� `global_step` �Ȳ�����ֵ��Ҳ����Tensor��
        - **TypeError** - ��� `use_locking` ����bool��
        - **TypeError** - ��� `var` �� `gradient_accumulator` �� `gradient_squared_accumulator` �� `grad` �� `lr` �� `l1` �� `l2` ���������ͼȲ���float16Ҳ����float32�� 
        - **TypeError** - ��� `gradient_accumulator` �� `gradient_squared_accumulator` �� `grad` �� `var` ���������Ͳ���ͬ��
        - **TypeError** - ��� `global_step` ���������Ͳ���int32Ҳ����int64��
        - **ValueError** - ��� `lr` �� `l1` �� `l2` �� `global_step` ��shape��С��Ϊ0��
        - **TypeError** - ��� `var` �� `gradient_accumulator` �� `gradient_squared_accumulator` �� `grad` ��֧����������ת����
