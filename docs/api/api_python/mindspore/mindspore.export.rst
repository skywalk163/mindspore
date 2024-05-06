mindspore.export
================

.. py:function:: mindspore.export(net, *inputs, file_name, file_format, **kwargs)

    将MindSpore网络模型导出为指定格式的文件。

    .. note::
        - 当导出文件格式为AIR、ONNX时，单个Tensor的大小不能超过2GB。
        - 当 `file_name` 没有后缀时，系统会根据 `file_format` 自动添加后缀。
        - 现已支持将 :func:`mindspore.jit` 修饰的函数导出成MINDIR格式文件。
        - 当导出 :func:`mindspore.jit` 修饰的函数时，函数内不能包含有类属性参与的计算。
        - AIR格式已弃用，将被删除。请使用其他格式或者MindSpore Lite进行离线推理。

    参数：
        - **net** (Union[Cell, function]) - MindSpore网络结构。
        - **inputs** (Union[Tensor, Dataset, List, Tuple, Number, Bool]) - 网络的输入，如果网络有多个输入，需要一同传入。当传入的类型为 `Dataset` 时，将会把数据预处理行为同步保存起来。需要手动调整batch的大小，当前仅支持获取 `Dataset` 的 `image` 列。
        - **file_name** (str) - 导出模型的文件名称。
        - **file_format** (str) - MindSpore目前支持导出"AIR"，"ONNX"和"MINDIR"格式的模型。

          - **AIR** - Ascend Intermediate Representation。一种Ascend模型的中间表示格式。推荐的输出文件后缀是".air"。
          - **ONNX** - Open Neural Network eXchange。一种针对机器学习所设计的开放式的文件格式。推荐的输出文件后缀是".onnx"。
          - **MINDIR** - MindSpore Native Intermediate Representation for Anf。一种MindSpore模型的中间表示格式。推荐的输出文件后缀是".mindir"。

        - **kwargs** (dict) - 配置选项字典。

          - **enc_key** (byte) - 用于加密的字节类型密钥，有效长度为16、24或者32。
          - **enc_mode** (Union[str, function]) - 指定加密模式，当设置 `enc_key` 时启用。

            - 对于 'AIR'和 'ONNX'格式的模型，当前仅支持自定义加密导出。
            - 对于 'MINDIR'格式的模型，支持的加密选项有： 'AES-GCM'， 'AES-CBC'， 'SM4-CBC'和用户自定义加密算法。默认值： ``"AES-GCM"``。
            - 关于使用自定义加密导出的详情，请查看 `教程 <https://www.mindspore.cn/mindarmour/docs/zh-CN/master/model_encrypt_protection.html>`_。
          
          - **dataset** (Dataset) - 指定数据集的预处理方法，用于将数据集的预处理导入MindIR。

          - **obf_config** (dict) - 模型混淆配置选项字典。

            - **type** (str) - 混淆类型，目前支持动态混淆，即 'dynamic' 。
            - **obf_ratio** (Union[str, float]) - 全模型算子的混淆比例，可取浮点数(0, 1]或者字符串 ``"small"`` 、 ``"medium"`` 、 ``"large"`` 。``"small"`` 、``"medium"`` 、``"large"`` 分别对应于 0.1、0.3、0.6。
            - **customized_func** (function) - 在自定义函数模式下需要设置的Python函数，用来控制混淆结构中的选择分支走向。它的返回值需要是bool类型，且是恒定的，用户可以参考不透明谓词进行设置（请查看 `动态混淆教程 <https://www.mindspore.cn/mindarmour/docs/zh-CN/master/dynamic_obfuscation_protection.html>`_　中的 `my_func()`）。如果设置了 `customized_func` ，那么在使用 `load` 接口导入模型的时候，需要把这个函数也传入。
            - **obf_random_seed** (int) - 混淆随机种子，是一个取值范围为(0, 9223372036854775807]的整数，不同的随机种子会使模型混淆后的结构不同。如果用户设置了 `obf_random_seed` ，那么在部署混淆模型的时候，需要在调用 :class:`mindspore.nn.GraphCell` 接口中传入 `obf_random_seed` 。需要注意的是，如果用户同时设置了 `customized_func` 和 `obf_random_seed` ，那么后一种模式将会被采用。
        
          - **custom_func** (function) - 用户自定义的导出策略的函数。该函数会在网络导出时，对模型使用该函数进行自定义处理。需要注意，当前仅支持对 `format` 为 `MindIR` 的文件使用 `custom_func` ，且自定义函数仅接受一个代表 `MindIR` 文件 `Proto` 对象的入参。当使用 `custom_func` 对模型进行修改时，需要保证修改后模型的正确性，否则可能导致模型加载失败或功能错误。默认值： ``None`` 。


    教程样例：
        - `保存与加载 - 保存和加载MindIR
          <https://mindspore.cn/tutorials/zh-CN/master/beginner/save_load.html#保存和加载mindir>`_