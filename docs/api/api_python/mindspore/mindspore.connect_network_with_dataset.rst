mindspore.connect_network_with_dataset
=======================================

.. py:function:: mindspore.connect_network_with_dataset(network, dataset_helper)

    �� `network` �� `dataset_helper` �е����ݼ����ӣ�ֻ֧�� `�³�ģʽ <https://mindspore.cn/tutorials/experts/zh-CN/master/optimize/execution_opt.html>`_��(dataset_sink_mode=True)��

    ������
        - **network** (Cell) - ���ݼ���ѵ�����硣
        - **dataset_helper** (DatasetHelper) - һ������MindData���ݼ����࣬�ṩ�����ݼ������͡���״��shape���Ͷ������ơ�

    ���أ�
        Cell��һ�������磬�������ݼ������͡���״��shape���Ͷ���������Ϣ��

    �쳣��
        - **RuntimeError** - ����ýӿ��ڷ������³�ģʽ���á�
