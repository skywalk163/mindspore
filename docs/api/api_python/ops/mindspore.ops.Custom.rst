mindspore.ops.Custom
=====================

.. py:class:: mindspore.ops.Custom(func, bprop=None, out_dtype=None, func_type="hybrid", out_shape=None, reg_info=None)

    `Custom` 算子是MindSpore自定义算子的统一接口。用户可以利用该接口自行定义MindSpore内置算子库尚未包含的算子。
    根据输入函数的不同，你可以创建多个自定义算子，并且把它们用在神经网络中。
    关于自定义算子的详细说明和介绍，包括参数的正确书写，见 `自定义算子教程 <https://www.mindspore.cn/tutorials/experts/zh-CN/master/operation/op_custom.html>`_ 。

    .. warning::
        - 这是一个实验性API，后续可能修改或删除。

    .. note::
        不同自定义算子的函数类型（func_type）支持的平台类型不同。每种类型支持的平台如下：

        - "hybrid": ["Ascend", "GPU", "CPU"].
        - "akg": ["Ascend", "GPU", "CPU"].
        - "tbe": ["Ascend"].
        - "aot": ["GPU", "CPU"].
        - "pyfunc": ["CPU"].
        - "julia": ["CPU"].
        - "aicpu": ["Ascend"].

        当运行在ge后端时，通过 `CustomRegOp` 生成"aicpu"和"tbe"类型的自定义算子的算子信息，通过 `custom_info_register` 将算子信息绑定到"tbe"类型的自定义算子的 `func` 上，然后将"aicpu"类型的自定义算子的算子信息以及"tbe"类型的自定义算子的 `func` 实现保存在一个或多个文件里，并且将这些文件保存在一个单独的目录里，在网络运行前将此目录的绝对路径设置到环境变量"MS_DEV_CUSTOM_OPP_PATH"。

    参数：
        - **func** (Union[function, str]) - 自定义算子的函数表达。
        - **out_shape** (Union[function, list, tuple]) - 自定义算子的输入的形状或者输出形状的推导函数。默认值： ``None`` 。
        - **out_dtype** (Union[function, :class:`mindspore.dtype`, tuple[:class:`mindspore.dtype`]]) - 自定义算子的输入的数据类型或者输出数据类型的推导函数。默认值： ``None`` 。
        - **func_type** (str) - 自定义算子的函数类型，必须是[ ``"hybrid"`` , ``"akg"`` , ``"tbe"`` , ``"aot"`` , ``"pyfunc"`` , ``"julia"`` , ``"aicpu"`` ]中之一。默认值： ``"hybrid"`` 。
        - **bprop** (function) - 自定义算子的反向函数。默认值： ``None``。
        - **reg_info** (Union[str, dict, list, tuple]) - 自定义算子的算子注册信息。默认值： ``None`` 。

    输入：
        - **input** (Union(tuple, list)) - 输入要计算的Tensor。

    输出：
        Tensor。自定义算子的计算结果。

    异常：
        - **TypeError** - 如果输入 `func` 不合法，或者 `func` 对应的注册信息类型不对。
        - **ValueError** - `func_type` 的值不在列表内。
        - **ValueError** - 算子注册信息不合法，包括支持平台不匹配，算子输入和属性与函数不匹配。