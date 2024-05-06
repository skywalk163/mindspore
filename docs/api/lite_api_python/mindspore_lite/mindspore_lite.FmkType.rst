mindspore_lite.FmkType
======================

.. py:class:: mindspore_lite.FmkType

    当Converter时， `FmkType` 定义输入模型的框架类型。

    目前，支持以下模型框架类型：

    ===========================  ====================================================
    定义                          说明
    ===========================  ====================================================
    `FmkType.TF`                 TensorFlow模型的框架类型，该模型使用.pb作为后缀。
    `FmkType.CAFFE`              Caffe模型的框架类型，该模型使用.prototxt作为后缀。
    `FmkType.ONNX`               ONNX模型的框架类型，该模型使用.onnx作为后缀。
    `FmkType.MINDIR`             MindSpore模型的框架类型，该模型使用.mindir作为后缀。
    `FmkType.TFLITE`             TensorFlow Lite模型的框架类型，该模型使用.tflite作为后缀。
    `FmkType.PYTORCH`            PyTorch模型的框架类型，该模型使用.pt或.pth作为后缀。
    ===========================  ====================================================
