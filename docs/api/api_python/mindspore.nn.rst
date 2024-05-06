mindspore.nn
=============

神经网络Cell。

用于构建神经网络中的预定义构建块或计算单元。

动态shape的支持情况详见 `nn接口动态shape支持情况 <https://mindspore.cn/docs/zh-CN/master/note/dynamic_shape_nn.html>`_ 。

MindSpore中 `mindspore.nn` 接口与上一版本相比，新增、删除和支持平台的变化信息请参考 `mindspore.nn API接口变更 <https://gitee.com/mindspore/docs/blob/master/resource/api_updates/nn_api_updates_cn.md>`_ 。

基本构成单元
------------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.Cell
    mindspore.nn.GraphCell
    mindspore.nn.LossBase
    mindspore.nn.Optimizer

容器
-----------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.CellDict
    mindspore.nn.CellList
    mindspore.nn.SequentialCell

封装层
-----------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.DistributedGradReducer
    mindspore.nn.DynamicLossScaleUpdateCell
    mindspore.nn.FixedLossScaleUpdateCell
    mindspore.nn.ForwardValueAndGrad
    mindspore.nn.GetNextSingleOp
    mindspore.nn.GradAccumulationCell
    mindspore.nn.MicroBatchInterleaved
    mindspore.nn.ParameterUpdate
    mindspore.nn.PipelineCell
    mindspore.nn.PipelineGradReducer
    mindspore.nn.TimeDistributed
    mindspore.nn.TrainOneStepCell
    mindspore.nn.TrainOneStepWithLossScaleCell
    mindspore.nn.WithEvalCell
    mindspore.nn.WithLossCell

卷积神经网络层
--------------------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.Conv1d
    mindspore.nn.Conv1dTranspose
    mindspore.nn.Conv2d
    mindspore.nn.Conv2dTranspose
    mindspore.nn.Conv3d
    mindspore.nn.Conv3dTranspose
    mindspore.nn.Unfold

循环神经网络层
-----------------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.RNN
    mindspore.nn.RNNCell
    mindspore.nn.GRU
    mindspore.nn.GRUCell
    mindspore.nn.LSTM
    mindspore.nn.LSTMCell

Transformer层
-----------------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.MultiheadAttention
    mindspore.nn.TransformerEncoderLayer
    mindspore.nn.TransformerDecoderLayer
    mindspore.nn.TransformerEncoder
    mindspore.nn.TransformerDecoder
    mindspore.nn.Transformer

嵌入层
-----------------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.Embedding
    mindspore.nn.EmbeddingLookup
    mindspore.nn.MultiFieldEmbeddingLookup

非线性激活函数层
------------------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.CELU
    mindspore.nn.ELU
    mindspore.nn.FastGelu
    mindspore.nn.GELU
    mindspore.nn.GLU
    mindspore.nn.get_activation
    mindspore.nn.Hardtanh
    mindspore.nn.HShrink
    mindspore.nn.HSigmoid
    mindspore.nn.HSwish
    mindspore.nn.LeakyReLU
    mindspore.nn.LogSigmoid
    mindspore.nn.LogSoftmax
    mindspore.nn.LRN
    mindspore.nn.Mish
    mindspore.nn.Softsign
    mindspore.nn.PReLU
    mindspore.nn.ReLU
    mindspore.nn.ReLU6
    mindspore.nn.RReLU
    mindspore.nn.SeLU
    mindspore.nn.SiLU
    mindspore.nn.Sigmoid
    mindspore.nn.Softmin
    mindspore.nn.Softmax
    mindspore.nn.Softmax2d
    mindspore.nn.SoftShrink
    mindspore.nn.Tanh
    mindspore.nn.Tanhshrink
    mindspore.nn.Threshold

线性层
-----------------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.Dense
    mindspore.nn.BiDense

Dropout层
-----------------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.Dropout
    mindspore.nn.Dropout1d
    mindspore.nn.Dropout2d
    mindspore.nn.Dropout3d

归一化层
---------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.BatchNorm1d
    mindspore.nn.BatchNorm2d
    mindspore.nn.BatchNorm3d
    mindspore.nn.GroupNorm
    mindspore.nn.InstanceNorm1d
    mindspore.nn.InstanceNorm2d
    mindspore.nn.InstanceNorm3d
    mindspore.nn.LayerNorm
    mindspore.nn.SyncBatchNorm

池化层
--------------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.AdaptiveAvgPool1d
    mindspore.nn.AdaptiveAvgPool2d
    mindspore.nn.AdaptiveAvgPool3d
    mindspore.nn.AdaptiveMaxPool1d
    mindspore.nn.AdaptiveMaxPool2d
    mindspore.nn.AdaptiveMaxPool3d
    mindspore.nn.AvgPool1d
    mindspore.nn.AvgPool2d
    mindspore.nn.AvgPool3d
    mindspore.nn.FractionalMaxPool3d
    mindspore.nn.LPPool1d
    mindspore.nn.LPPool2d
    mindspore.nn.MaxPool1d
    mindspore.nn.MaxPool2d
    mindspore.nn.MaxPool3d
    mindspore.nn.MaxUnpool1d
    mindspore.nn.MaxUnpool2d
    mindspore.nn.MaxUnpool3d

填充层
--------------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.Pad
    mindspore.nn.ConstantPad1d
    mindspore.nn.ConstantPad2d
    mindspore.nn.ConstantPad3d
    mindspore.nn.ReflectionPad1d
    mindspore.nn.ReflectionPad2d
    mindspore.nn.ReflectionPad3d
    mindspore.nn.ReplicationPad1d
    mindspore.nn.ReplicationPad2d
    mindspore.nn.ReplicationPad3d
    mindspore.nn.ZeroPad2d

损失函数
--------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.BCELoss
    mindspore.nn.BCEWithLogitsLoss
    mindspore.nn.CosineEmbeddingLoss
    mindspore.nn.CrossEntropyLoss
    mindspore.nn.CTCLoss
    mindspore.nn.DiceLoss
    mindspore.nn.FocalLoss
    mindspore.nn.GaussianNLLLoss
    mindspore.nn.HingeEmbeddingLoss
    mindspore.nn.HuberLoss
    mindspore.nn.KLDivLoss
    mindspore.nn.L1Loss
    mindspore.nn.MarginRankingLoss
    mindspore.nn.MAELoss
    mindspore.nn.MSELoss
    mindspore.nn.MultiClassDiceLoss
    mindspore.nn.MultilabelMarginLoss
    mindspore.nn.MultiLabelSoftMarginLoss
    mindspore.nn.MultiMarginLoss
    mindspore.nn.NLLLoss
    mindspore.nn.PoissonNLLLoss
    mindspore.nn.RMSELoss
    mindspore.nn.SampledSoftmaxLoss
    mindspore.nn.SmoothL1Loss
    mindspore.nn.SoftMarginLoss
    mindspore.nn.SoftmaxCrossEntropyWithLogits
    mindspore.nn.TripletMarginLoss

优化器
-------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.Adadelta
    mindspore.nn.Adagrad
    mindspore.nn.Adam
    mindspore.nn.AdaMax
    mindspore.nn.AdamOffload
    mindspore.nn.AdamWeightDecay
    mindspore.nn.AdaSumByDeltaWeightWrapCell
    mindspore.nn.AdaSumByGradWrapCell
    mindspore.nn.ASGD
    mindspore.nn.FTRL
    mindspore.nn.Lamb
    mindspore.nn.LARS
    mindspore.nn.LazyAdam
    mindspore.nn.Momentum
    mindspore.nn.ProximalAdagrad
    mindspore.nn.RMSProp
    mindspore.nn.Rprop
    mindspore.nn.SGD
    mindspore.nn.thor

动态学习率
-----------

LearningRateSchedule类
^^^^^^^^^^^^^^^^^^^^^^^

本模块中的动态学习率都是LearningRateSchedule的子类，将LearningRateSchedule的实例传递给优化器。在训练过程中，优化器以当前step为输入调用该实例，得到当前的学习率。

.. code-block::

    import mindspore.nn as nn

    min_lr = 0.01
    max_lr = 0.1
    decay_steps = 4
    cosine_decay_lr = nn.CosineDecayLR(min_lr, max_lr, decay_steps)

    net = Net()
    optim = nn.Momentum(net.trainable_params(), learning_rate=cosine_decay_lr, momentum=0.9)

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.CosineDecayLR
    mindspore.nn.ExponentialDecayLR
    mindspore.nn.InverseDecayLR
    mindspore.nn.NaturalExpDecayLR
    mindspore.nn.PolynomialDecayLR
    mindspore.nn.WarmUpLR

Dynamic LR函数
^^^^^^^^^^^^^^

本模块中的动态学习率都是function，调用function并将结果传递给优化器。在训练过程中，优化器将result[current step]作为当前学习率。

.. code-block::

    import mindspore.nn as nn

    min_lr = 0.01
    max_lr = 0.1
    total_step = 6
    step_per_epoch = 1
    decay_epoch = 4

    lr= nn.cosine_decay_lr(min_lr, max_lr, total_step, step_per_epoch, decay_epoch)

    net = Net()
    optim = nn.Momentum(net.trainable_params(), learning_rate=lr, momentum=0.9)

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.cosine_decay_lr
    mindspore.nn.exponential_decay_lr
    mindspore.nn.inverse_decay_lr
    mindspore.nn.natural_exp_decay_lr
    mindspore.nn.piecewise_constant_lr
    mindspore.nn.polynomial_decay_lr
    mindspore.nn.warmup_lr

图像处理层
-----------

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.PixelShuffle
    mindspore.nn.PixelUnshuffle
    mindspore.nn.Upsample

工具
-----

.. mscnplatformautosummary::
    :toctree: nn
    :nosignatures:
    :template: classtemplate.rst

    mindspore.nn.ChannelShuffle
    mindspore.nn.Flatten
    mindspore.nn.Identity
    mindspore.nn.Unflatten
