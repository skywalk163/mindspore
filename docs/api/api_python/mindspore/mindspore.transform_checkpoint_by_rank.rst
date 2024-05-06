mindspore.transform_checkpoint_by_rank
======================================

.. py:function:: mindspore.transform_checkpoint_by_rank(rank_id, checkpoint_files_map, save_checkpoint_file_name, src_strategy_file=None, dst_strategy_file=None)

    将一个分布式网络的Checkpoint由源切分策略转换到目标切分策略，对特定一个rank进行转换。关于更多分布式Checkpoint转换的细节，请参考：`模型转换 <https://www.mindspore.cn/tutorials/experts/zh-CN/master/parallel/model_transformation.html>`_。

    参数：
        - **rank_id** (int) - 待转换得到的Checkpoint的rank号。
        - **checkpoint_files_map** (dict) - 源Checkpoint字典，其key为rank号，值为该rank号对应的Checkpoint文件路径。
        - **save_checkpoint_file_name** (str) - 目标Checkpoint路径以及名字。
        - **src_strategy_file** (str) - 源切分策略proto文件名，由mindspore.set_auto_parallel_context(strategy_ckpt_save_file)接口存储下来的文件。当其为 ``None`` 时，表示切分策略为不切分。默认值： ``None`` 。
        - **dst_strategy_file** (str) - 目标切分策略proto文件名，由mindspore.set_auto_parallel_context(strategy_ckpt_save_file)接口存储下来的文件。当其为 ``None`` 时，表示切分策略为不切分。默认值： ``None`` 。

    异常：
        - **ValueError** - `src_strategy_file` 或者 `dst_strategy_file` 不是正确的切分策略proto文件。
        - **ValueError** - `checkpoint_files_map` 内的元素不是正确的Checkpoint文件。
        - **ValueError** - `save_checkpoint_file_name` 不以“.ckpt”结尾。
        - **TypeError** - `checkpoint_files_map` 不是一个字典。
        - **TypeError** - `src_strategy_file` 或者 `dst_strategy_file` 不是字符串。
        - **TypeError** - `rank_id` 不是一个整数。
        - **TypeError** - `save_checkpoint_file_name` 不是字符串。
