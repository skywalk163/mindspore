﻿mindspore.train.Model
======================

.. py:class:: mindspore.train.Model(network, loss_fn=None, optimizer=None, metrics=None, eval_network=None, eval_indexes=None, amp_level="O0", boost_level="O0", **kwargs)

    模型训练或推理的高阶接口。 `Model` 会根据用户传入的参数封装可训练或推理的实例。

    .. note::

        - 如果使用混合精度功能，需要同时设置 `optimizer` 参数，否则混合精度功能不生效。
          当使用混合精度时，优化器中的 `global_step` 可能与模型中的 `cur_step_num` 不同。
        - 使用 `custom_mixed_precision` 或 `auto_mixed_precision` 进行精度转换后，不支持再次使用其他接口进行精度转换。
          如果使用 `Model` 来训练转换后的网络，则需要将 `amp_level` 配置为 ``O0`` 以避免重复的精度转换。

    参数：
        - **network** (Cell) - 用于训练或推理的神经网络。
        - **loss_fn** (Cell) - 损失函数。如果 `loss_fn` 为None，`network` 中需要进行损失函数计算。默认值： ``None`` 。
        - **optimizer** (Cell) - 用于更新网络权重的优化器。如果 `optimizer` 为None， 那么 `network` 的网络结构里需要包括反向传播和权重更新逻辑。默认值： ``None`` 。
        - **metrics** (Union[dict, set]) - 用于模型评估的一组评价函数。例如：{'accuracy', 'recall'}。默认值： ``None`` 。
        - **eval_network** (Cell) - 用于评估的神经网络。未定义情况下，`Model` 会使用 `network` 和 `loss_fn` 封装一个 `eval_network` 。默认值： ``None`` 。
        - **eval_indexes** (list) - 在定义 `eval_network` 的情况下使用。如果 `eval_indexes` 为默认值None，`Model` 会将 `eval_network` 的所有输出传给 `metrics` 。如果配置 `eval_indexes` ，必须包含三个元素，分别为损失值、预测值和标签在 `eval_network` 输出中的位置，此时，损失值将传给损失评价函数，预测值和标签将传给其他评价函数。推荐使用评价函数的 :func:`mindspore.train.Metric.set_indexes` 代替 `eval_indexes` 。默认值： ``None`` 。
        - **amp_level** (str) - `mindspore.amp.build_train_network <https://www.mindspore.cn/docs/zh-CN/master/api_python/amp/mindspore.amp.build_train_network.html>`_ 的可选参数 `level` ， `level` 为混合精度等级，该参数支持["O0", "O1", "O2", "O3", "auto"]。默认值： ``"O0"`` 。

          - "O0": 不变化。
          - "O1": 将白名单中的算子转为float16，剩余算子保持float32。白名单中的算子如下列表：[Conv1d, Conv2d, Conv3d, Conv1dTranspose, Conv2dTranspose, Conv3dTranspose, Dense, LSTMCell, RNNCell, GRUCell, MatMul, BatchMatMul, PReLU, ReLU, Ger]。
          - "O2": 将网络精度转为float16，BatchNorm保持float32精度，使用动态调整损失缩放系数（loss scale）的策略。
          - "O3": 将网络精度（包括BatchNorm）转为float16，不使用损失缩放策略。
          - "auto": 为不同处理器设置专家推荐的混合精度等级，如在GPU上设为"O2"，在Ascend上设为"O3"。该设置方式可能在部分场景下不适用，建议用户根据具体的网络模型自定义设置 `amp_level` 。

          在GPU上建议使用"O2"，在Ascend上建议使用"O3"。
          通过 `kwargs` 设置 `keep_batchnorm_fp32` ，可修改BatchNorm的精度策略， `keep_batchnorm_fp32` 必须为bool类型；通过 `kwargs` 设置 `loss_scale_manager` 可修改损失缩放策略，`loss_scale_manager` 必须为 :class:`mindspore.amp.LossScaleManager` 的子类，
          关于 `amp_level` 详见 `mindpore.amp.build_train_network` 。

        - **boost_level** (str) - `mindspore.boost` 的可选参数，为boost模式训练等级。支持["O0", "O1", "O2"]。默认值： ``"O0"`` 。

          - "O0": 不变化。
          - "O1": 启用boost模式，性能将提升约20%，准确率保持不变。
          - "O2": 启用boost模式，性能将提升约30%，准确率下降小于3%。

          如果想自行配置boost模式，可以将 `boost_config_dict` 设置为 `boost.py`。
          为使功能生效，需要同时设置optimizer、eval_network或metric参数。
          注意：当前默认开启的优化仅适用部分网络，并非所有网络都能获得相同收益。建议在图模式+Ascend平台下开启该模式，同时为了获取更好的加速效果，请参考文档配置boost_config_dict。

    .. py:method:: build(train_dataset=None, valid_dataset=None, sink_size=-1, epoch=1)

        数据下沉模式下构建计算图和数据图。

        .. warning:: 这是一个实验性API，后续可能修改或删除。

        .. note:: 如果预先调用该接口构建计算图，那么 `Model.train` 会直接执行计算图。预构建计算图目前仅支持GRAPH_MODE模式和Ascend处理器。仅支持数据下沉模式。

        参数：
            - **train_dataset** (Dataset) - 一个训练集迭代器。如果定义了 `train_dataset` ，将会构建训练计算图。默认值： ``None`` 。
            - **valid_dataset** (Dataset) - 一个验证集迭代器。如果定义了 `valid_dataset` ，将会构建验证计算图，此时 `Model` 中的 `metrics` 不能为None。默认值： ``None`` 。
            - **sink_size** (int) - 控制每次数据下沉的step数量。默认值： ``-1`` 。
            - **epoch** (int) - 控制训练轮次。默认值： ``1`` 。

    .. py:method:: eval(valid_dataset, callbacks=None, dataset_sink_mode=False)

        模型评估接口。

        使用PyNative模式或CPU处理器时，模型评估流程将以非下沉模式执行。

        .. note::
            如果 `dataset_sink_mode` 配置为True，数据将被发送到处理器中。此时数据集与模型绑定，数据集仅能在当前模型中使用。如果处理器是Ascend，数据特征将被逐一传输。每次数据传输的上限是256M。
            该接口会构建并执行计算图。如果使用前先执行了 `Model.build` ，那么它会直接执行计算图而不构建。

        参数：
            - **valid_dataset** (Dataset) - 评估模型的数据集。
            - **callbacks** (Optional[list(Callback), Callback]) - 评估过程中需要执行的回调对象或回调对象列表。默认值： ``None`` 。
            - **dataset_sink_mode** (bool) - 数据是否直接下沉至处理器进行处理。默认值： ``False`` 。

        返回：
            Dict，key是用户定义的评价指标名称，value是以推理模式运行的评估结果。

        教程样例：
            - `高阶封装：Model - 训练及保存模型
              <https://mindspore.cn/tutorials/zh-CN/master/advanced/model.html#训练及保存模型>`_

    .. py:method:: eval_network
        :property:

        获取该模型的评价网络。

        返回：
            评估网络实例。

    .. py:method:: fit(epoch, train_dataset, valid_dataset=None, valid_frequency=1, callbacks=None, dataset_sink_mode=False, valid_dataset_sink_mode=False, sink_size=-1, initial_epoch=0)

        模型边训练边推理接口。

        如果 `valid_dataset` 不为None，在训练过程中同时执行推理。

        更多详细信息请参考 :func:`mindspore.train.Model.train` 和 :func:`mindspore.train.Model.eval`。

        参数：
            - **epoch** (int) - 训练执行轮次。通常每个epoch都会使用全量数据集进行训练。当 `dataset_sink_mode` 设置为True且 `sink_size` 大于零时，则每个epoch训练次数为 `sink_size` 而不是数据集的总步数。如果 `epoch` 与 `initial_epoch` 一起使用，它表示训练的最后一个 `epoch` 是多少。
            - **train_dataset** (Dataset) - 训练数据集迭代器。如果定义了 `loss_fn` ，则数据和标签会被分别传给 `network` 和 `loss_fn` ，此时数据集需要返回一个元组（data, label）。如果数据集中有多个数据或者标签，可以设置 `loss_fn` 为None，并在 `network` 中实现损失函数计算，此时数据集返回的所有数据组成的元组（data1, data2, data3, ...）会传给 `network` 。
            - **valid_dataset** (Dataset) - 评估模型的数据集迭代器。默认值： ``None`` 。
            - **valid_frequency** (int, list) - 此参数只有在valid_dataset不为None时生效。如果为int类型，表示执行推理的频率，例如 `valid_frequency=2`，则每2个训练epoch执行一次推理；如果为list类型，指明在哪几个epoch时执行推理，例如 `valid_frequency=[1, 5]`，则在第1个和第5个epoch执行推理。默认值： ``1`` 。
            - **callbacks** (Optional[list[Callback], Callback]) - 训练过程中需要执行的回调对象或者回调对象列表。默认值： ``None`` 。
            - **dataset_sink_mode** (bool) - 训练数据是否直接下沉至处理器进行处理。使用PYNATIVE_MODE模式或CPU处理器时，模型训练流程将以非下沉模式执行。默认值： ``False`` 。
            - **valid_dataset_sink_mode** (bool) - 推理数据是否直接下沉至处理器进行处理。默认值： ``False`` 。
            - **sink_size** (int) - 控制每次数据下沉的step数量。`dataset_sink_mode` 为False时 `sink_size` 无效。如果sink_size=-1，则每一次epoch下沉完整数据集。如果sink_size>0，则每一次epoch下沉数据量为sink_size的数据集。默认值： ``-1`` 。
            - **initial_epoch** (int) - 从哪个epoch开始训练，一般用于中断恢复训练场景。默认值： ``0`` 。

        教程样例：
            - `高阶封装：Model - 训练及保存模型
              <https://mindspore.cn/tutorials/zh-CN/master/advanced/model.html#训练及保存模型>`_

    .. py:method:: infer_predict_layout(*predict_data, skip_backend_compile=False)

        在 `AUTO_PARALLEL` 或 `SEMI_AUTO_PARALLEL` 模式下为预测网络生成参数layout。数据可以是单个或多个张量。

        .. note:: 同一批次数据应放在一个张量中。

        参数：
            - **predict_data** (Union[Tensor, list[Tensor], tuple[Tensor]], 可选) - 预测样本，数据可以是单个张量、张量列表或张量元组。
            - **skip_backend_compile** (bool) - 生成参数layout时跳过后端编译流程。一般用于后端编译模型大小超过卡上内存的场景，其它场景不建议开启，开启时本次编译的缓存无法在二次编译时被使用。默认值： ``False``。

        返回：
            Dict，用于加载分布式checkpoint的参数layout字典。它总是作为 `load_distributed_checkpoint()` 函数的一个入参。

        异常：
            - **RuntimeError** - 非图模式（GRAPH_MODE）将会抛出该异常。

    .. py:method:: infer_train_layout(train_dataset, dataset_sink_mode=True, sink_size=-1)

        在 `AUTO_PARALLEL` 或 `SEMI_AUTO_PARALLEL` 模式下为训练网络生成参数layout。当前仅支持在数据下沉模式下使用。

        .. warning:: 这是一个实验性API，后续可能修改或删除。

        .. note:: 这是一个预编译函数。参数必须与Model.train()函数相同。

        参数：
            - **train_dataset** (Dataset) - 一个训练数据集迭代器。如果没有损失函数（loss_fn），返回一个包含多个数据的元组（data1, data2, data3, ...）并传递给网络。否则，返回一个元组（data, label），数据和标签将被分别传递给网络和损失函数。
            - **dataset_sink_mode** (bool) - 决定是否以数据集下沉模式进行训练。默认值： ``True`` 。PyNative模式下或处理器为CPU时，训练模型流程使用的是数据不下沉（non-sink）模式。默认值： ``True`` 。
            - **sink_size** (int) - 控制每次数据下沉的step数量，如果 `sink_size` =-1，则每一次epoch下沉完整数据集。如果 `sink_size` >0，则每一次epoch下沉数据量为 `sink_size` 的数据集。如果 `dataset_sink_mode` 为False，则设置 `sink_size` 为无效。默认值： ``-1`` 。

        返回：
            Dict，用于加载分布式checkpoint的参数layout字典。

    .. py:method:: predict(*predict_data, backend=None, config=None)

        输入样本得到预测结果。

        参数：
            - **predict_data** (Union[Tensor, list[Tensor], tuple[Tensor]], 可选) - 预测样本，数据可以是单个张量、张量列表或张量元组。
            - **backend** (str) - 选择预测后端，该参数为实验性质特性，主要用于MindSpore Lite云侧推理。默认值： ``None`` 。
            - **config** (dict，可选) - 当后端为 ‘lite’ 时，config 参数使能。config 包括两个部分：config_path（configPath，str）和 config_item（str，dict）。当 config_item 设置时，其优先级高于 config_path。设置推理的排名表文件。配置文件的内容如下：

              config_path 定义配置文件的路径，用于在构建模型期间传递用户定义选项。在以下场景中，用户可能需要设置参数。例如："/home/user/config.ini"。默认值： ``""`` , 以下是 config.ini 文件的内容：

              .. code-block::

                  [ascend_context]
                  rank_table_file=[path_a]（存储rank table文件的初始路径）
                  [execution_plan]
                  [op_name1]=data_type:float16（名字为op_name1的算子设置数据类型为float16）
                  [op_name2]=data_type:float32（名字为op_name2的算子设置数据类型为float32）

              当只配置config_path的方式如下：

              .. code-block::

                  config = {"configPath" : "/home/user/config.ini"}

              config_dict 配置参数字典，当只配置config_dict的方式如下：

              .. code-block::

                  config = {"ascend_context" : {"rank_table_file" : "path_b"}, "execution_plan" : {"op_name1" : "data_type:float16", "op_name2" : "data_type:float32"}}

              当同时配置config_path 和 config_dict的方式如下：

              .. code-block::

                  config = {"configPath" : "/home/user/config.ini", "ascend_context" : {"rank_table_file" : "path_b"}, "execution_plan" : {"op_name3" : "data_type:float16", "op_name4" : "data_type:float32"}}

              注意到 config_dict 和 config_item均配置了"configPath"，此时以 config_dict 中的 "path_b" 为准。

        返回：
            返回预测结果，类型是Tensor或Tensor元组。

    .. py:method:: predict_network
        :property:

        获得该模型的预测网络。

        返回：
            预测网络实例。

    .. py:method:: train(epoch, train_dataset, callbacks=None, dataset_sink_mode=False, sink_size=-1, initial_epoch=0)

        模型训练接口。

        使用PYNATIVE_MODE模式或CPU处理器时，模型训练流程将以非下沉模式执行。

        .. note::
            - 如果 `dataset_sink_mode` 配置为True，数据将被送到处理器中。如果处理器是Ascend，数据特征将被逐一传输，每次数据传输的上限是256M。
            - 如果 `dataset_sink_mode` 配置为True，在PyNative模式，每个step结束时调用Callback实例的 `step_end` 方法。在Graph模式，每个epoch结束时调用Callback实例的 `step_end` 方法。
            - 如果 `dataset_sink_mode` 配置为True，数据集仅能在当前模型中使用。
            - 如果 `sink_size` 大于零，每次epoch可以无限次遍历数据集，直到遍历数据量等于 `sink_size` 为止。
            - 每次epoch将从上一次遍历的最后位置继续开始遍历。该接口会构建并执行计算图，如果使用前先执行了 `Model.build` ，那么它会直接执行计算图而不构建。

        参数：
            - **epoch** (int) - 训练执行轮次。通常每个epoch都会使用全量数据集进行训练。当 `dataset_sink_mode` 设置为True且 `sink_size` 大于零时，则每个epoch训练次数为 `sink_size` 而不是数据集的总步数。如果 `epoch` 与 `initial_epoch` 一起使用，它表示训练的最后一个 `epoch` 是多少。
            - **train_dataset** (Dataset) - 一个训练数据集迭代器。如果定义了 `loss_fn` ，则数据和标签会被分别传给 `network` 和 `loss_fn` ，此时数据集需要返回一个元组（data, label）。如果数据集中有多个数据或者标签，可以设置 `loss_fn` 为None，并在 `network` 中实现损失函数计算，此时数据集返回的所有数据组成的元组（data1, data2, data3, ...）会传给 `network` 。
            - **callbacks** (Optional[list[Callback], Callback]) - 训练过程中需要执行的回调对象或者回调对象列表。默认值： ``None`` 。
            - **dataset_sink_mode** (bool) - 数据是否直接下沉至处理器进行处理。使用PYNATIVE_MODE模式或CPU处理器时，模型训练流程将以非下沉模式执行。默认值： ``False`` 。
            - **sink_size** (int) - 控制每次数据下沉的step数量。`dataset_sink_mode` 为False时 `sink_size` 无效。如果sink_size=-1，则每一次epoch下沉完整数据集。如果sink_size>0，则每一次epoch下沉数据量为sink_size的数据集。默认值： ``-1`` 。
            - **initial_epoch** (int) - 从哪个epoch开始训练，一般用于中断恢复训练场景。默认值： ``0`` 。

    .. py:method:: train_network
        :property:

        获得该模型的训练网络。

        返回：
            训练网络实例。
