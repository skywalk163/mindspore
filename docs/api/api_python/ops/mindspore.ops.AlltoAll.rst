mindspore.ops.AlltoAll
======================

.. py:class:: mindspore.ops.AlltoAll(split_count, split_dim, concat_dim, group=GlobalComm.WORLD_COMM_GROUP)

    AlltoAll是一个集合通信函数。

    AlltoAll将输入数据在特定的维度切分成特定的块数（blocks），并按顺序发送给其他rank。一般有两个阶段：

    - 分发阶段：在每个进程上，操作数沿着 `split_dim` 拆分为 `split_count` 个块（blocks），且分发到指定的rank上，例如，第i块被发送到第i个rank上。
    - 聚合阶段：每个rank沿着 `concat_dimension` 拼接接收到的数据。

    .. note::
        聚合阶段，所有进程中的Tensor必须具有相同的shape和格式。

        要求全连接配网方式，每台设备具有相同的vlan id，ip和mask在同一子网，请查看 `详细信息 <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/ops/communicate_ops.html#注意事项>`_ 。

    参数：
        - **split_count** (int) - 在每个进程上，将块（blocks）拆分为 `split_count` 个。
        - **split_dim** (int) - 在每个进程上，沿着 `split_dim` 维度进行拆分。
        - **concat_dim** (int) - 在每个进程上，沿着 `concat_dimension` 拼接接收到的块（blocks）。
        - **group** (str) - AlltoAll的通信域。默认值： ``GlobalComm.WORLD_COMM_GROUP`` 。

    输入：
        - **input_x** (Tensor) - shape为 :math:`(x_1, x_2, ..., x_R)`。

    输出：
        Tensor，设输入的shape是 :math:`(x_1, x_2, ..., x_R)`，则输出的shape为 :math:`(y_1, y_2, ..., y_R)`，其中：

        - :math:`y_{split\_dim} = x_{split\_dim} / split\_count`
        - :math:`y_{concat\_dim} = x_{concat\_dim} * split\_count`
        - :math:`y_{other} = x_{other}` 。

    异常：
        - **TypeError** - 如果 `group` 不是字符串。

    样例：

    .. note::
        .. include:: mindspore.ops.comm_note.rst

        该样例需要在8卡环境下运行。
    
    教程样例：
        - `分布式集合通信原语 - AlltoAll
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/ops/communicate_ops.html#alltoall>`_