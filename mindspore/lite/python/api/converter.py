# Copyright 2022-2023 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================
"""
Converter API.
"""
from __future__ import absolute_import
import os
from enum import Enum

from mindspore_lite._checkparam import check_isinstance, check_input_shape, check_config_info
from mindspore_lite.lib import _c_lite_wrapper
from mindspore_lite.tensor import DataType, Format, data_type_py_cxx_map, data_type_cxx_py_map, format_py_cxx_map, \
    format_cxx_py_map
from mindspore_lite.model import ModelType, model_type_py_cxx_map, model_type_cxx_py_map, set_env

__all__ = ['FmkType', 'Converter']


class FmkType(Enum):
    """
    When Converter, the `FmkType` is used to define Input model framework type.

    Currently, the following model framework types are supported:

    ===========================  ============================================================================
    Definition                    Description
    ===========================  ============================================================================
    `FmkType.TF`                 TensorFlow model's framework type, and the model uses .pb as suffix.
    `FmkType.CAFFE`              Caffe model's framework type, and the model uses .prototxt as suffix.
    `FmkType.ONNX`               ONNX model's framework type, and the model uses .onnx as suffix.
    `FmkType.MINDIR`             MindSpore model's framework type, and the model uses .mindir as suffix.
    `FmkType.TFLITE`             TensorFlow Lite model's framework type, and the model uses .tflite as suffix.
    `FmkType.PYTORCH`            PyTorch model's framework type, and the model uses .pt or .pth as suffix.
    ===========================  ============================================================================

    Examples:
        >>> # Method 1: Import mindspore_lite package
        >>> import mindspore_lite as mslite
        >>> print(mslite.FmkType.TF)
        FmkType.TF
        >>> # Method 2: from mindspore_lite package import FmkType
        >>> from mindspore_lite import FmkType
        >>> print(FmkType.TF)
        FmkType.TF
    """

    TF = 0
    CAFFE = 1
    ONNX = 2
    MINDIR = 3
    TFLITE = 4
    PYTORCH = 5


class Converter:
    r"""
    Constructs a `Converter` class.

    Used in the following scenarios:

    1. Convert the third-party model into MindSpore model or MindSpore Lite model.

    2. Convert MindSpore model into MindSpore model or MindSpore Lite model.

    Convert to MindSpore model is recommended. Currently, Convert to MindSpore Lite model is supported,
    but it will be deprecated in the future. If you want to convert to MindSpore Lite model, please use
    `converter_tool <https://www.mindspore.cn/lite/docs/en/master/use/cloud_infer/converter_tool.html>`_  instead of
    The Python interface. The Model api and ModelParallelRunner api only support MindSpore model.

    Note:
        Please construct the `Converter` class first, and then generate the model by executing the Converter.convert()
        method.

        The encryption and decryption function is only valid when it is set to `MSLITE_ENABLE_MODEL_ENCRYPTION=on` at
        compile time, and only supports Linux x86 platforms. `decrypt_key` and `encrypt_key` are string expressed in
        hexadecimal. For example, if `encrypt_key` is set as ``"30313233343637383939414243444546"``, the corresponding
        hexadecimal expression is ``(b)0123456789ABCDEF`` . Linux platform users can use the' xxd 'tool to convert the
        key expressed in bytes into hexadecimal expressions. It should be noted that the encryption and decryption
        algorithm has been updated in version 1.7, resulting in the new Python interface does not support the conversion
        of MindSpore Lite's encryption exported models in version 1.6 and earlier.

    Examples:
        >>> # testcase based on cloud inference package.
        >>> import mindspore_lite as mslite
        >>> converter = mslite.Converter()
        >>> # The ms model may be generated only after converter.convert() is executed after the class is constructed.
        >>> converter.weight_fp16 = True
        >>> converter.input_shape = {"inTensor1": [1, 3, 32, 32]}
        >>> converter.input_format = mslite.Format.NHWC
        >>> converter.input_data_type = mslite.DataType.FLOAT32
        >>> converter.output_data_type = mslite.DataType.FLOAT32
        >>> converter.save_type = mslite.ModelType.MINDIR
        >>> converter.decrypt_key = "30313233343637383939414243444546"
        >>> converter.decrypt_mode = "AES-GCM"
        >>> converter.enable_encryption = True
        >>> converter.encrypt_key = "30313233343637383939414243444546"
        >>> converter.infer = True
        >>> converter.optimize = "general"
        >>> converter.device = "Ascend"
        >>> section = "common_quant_param"
        >>> config_info_in = {"quant_type": "WEIGHT_QUANT"}
        >>> converter.set_config_info(section, config_info_in)
        >>> print(converter.get_config_info())
        {'common_quant_param': {'quant_type': 'WEIGHT_QUANT'}}
        >>> print(converter)
        config_info: {'common_quant_param': {'quant_type': 'WEIGHT_QUANT'}},
        weight_fp16: True,
        input_shape: {'inTensor1': [1, 3, 32, 32]},
        input_format: Format.NHWC,
        input_data_type: DataType.FLOAT32,
        output_data_type: DataType.FLOAT32,
        save_type: ModelType.MINDIR,
        decrypt_key: 30313233343637383939414243444546,
        decrypt_mode: AES-GCM,
        enable_encryption: True,
        encrypt_key: 30313233343637383939414243444546,
        infer: True,
        optimize: general,
        device: Ascend.
    """

    def __init__(self):
        self._converter = _c_lite_wrapper.ConverterBind()
        self.optimize_user_defined = "general"

    def __str__(self):
        res = f"config_info: {self.get_config_info()},\n" \
              f"weight_fp16: {self.weight_fp16},\n" \
              f"input_shape: {self.input_shape},\n" \
              f"input_format: {self.input_format},\n" \
              f"input_data_type: {self.input_data_type},\n" \
              f"output_data_type: {self.output_data_type},\n" \
              f"save_type: {self.save_type},\n" \
              f"decrypt_key: {self.decrypt_key},\n" \
              f"decrypt_mode: {self.decrypt_mode},\n" \
              f"enable_encryption: {self.enable_encryption},\n" \
              f"encrypt_key: {self.encrypt_key},\n" \
              f"infer: {self.infer},\n" \
              f"optimize: {self.optimize},\n" \
              f"device: {self.device}."
        return res

    @property
    def decrypt_key(self):
        """
        Get the key used to decrypt the encrypted MindIR file.

        Returns:
            str, the key used to decrypt the encrypted MindIR file, expressed in hexadecimal characters. Only valid when
            `fmk_type` is ``FmkType.MINDIR``.
        """
        return self._converter.get_decrypt_key()

    @decrypt_key.setter
    def decrypt_key(self, decrypt_key):
        """
        Set the key used to decrypt the encrypted MindIR file

        Args:
            decrypt_key (str): Set the key used to decrypt the encrypted MindIR file, expressed in hexadecimal
                characters. Only valid when fmk_type is FmkType.MINDIR.

        Raises:
            TypeError: `decrypt_key` is not a str.
        """
        check_isinstance("decrypt_key", decrypt_key, str)
        self._converter.set_decrypt_key(decrypt_key)

    @property
    def decrypt_mode(self):
        """
        Get decryption mode for the encrypted MindIR file.

        Returns:
            str, decryption mode for the encrypted MindIR file. Only valid when dec_key is set.
            Options are ``"AES-GCM"``, ``"AES-CBC"``.
        """
        return self._converter.get_decrypt_mode()

    @decrypt_mode.setter
    def decrypt_mode(self, decrypt_mode):
        """
        Set decryption mode for the encrypted MindIR file.

        Args:
            decrypt_mode (str): Set decryption mode for the encrypted MindIR file. Only valid when dec_key is set.
                Options are "AES-GCM" | "AES-CBC".

        Raises:
            TypeError: `decrypt_mode` is not a str.
            ValueError: `decrypt_mode` is neither "AES-GCM" nor "AES-CBC" when it is a str.
        """
        check_isinstance("decrypt_mode", decrypt_mode, str)
        if decrypt_mode not in ["AES-GCM", "AES-CBC"]:
            raise ValueError(f"decrypt_mode must be in [AES-GCM, AES-CBC], but got {decrypt_mode}.")
        self._converter.set_decrypt_mode(decrypt_mode)

    @property
    def device(self):
        """
        Get target device when converter model.

        Returns:
            str, target device when converter model. Only valid for Ascend. The use case is when on the Ascend device,
            if you need to the converted model to have the ability to use Ascend backend to perform inference,
            you can set the parameter. If it is not set, the converted model will use CPU backend to perform
            inference by default. Option is ``"Ascend"``.
        """
        return self._converter.get_device()

    @device.setter
    def device(self, device):
        """
        Set target device when converter model.

        Args:
            device (str): Set target device when converter model. Only valid for Ascend. The use case is when on the
                Ascend device, if you need to the converted model to have the ability to use Ascend backend to perform
                inference, you can set the parameter. If it is not set, the converted model will use CPU backend to
                perform inference by default. Option is "Ascend".

        Raises:
            TypeError: `device` is not a str.
            ValueError: `device` is not "Ascend" when it is a str.
        """
        check_isinstance("device", device, str)
        if device not in ["Ascend"]:
            raise ValueError(f"device must be in [Ascend], but got {device}.")
        self._converter.set_device(device)

    @property
    def enable_encryption(self):
        """
        Get the status whether to encrypt the model when exporting.

        Returns:
            bool, whether to encrypt the model when exporting. Export encryption can protect the integrity of the model,
            but it will increase the initialization time at runtime.
        """
        return self._converter.get_enable_encryption()

    @enable_encryption.setter
    def enable_encryption(self, enable_encryption):
        """
        Set whether to encrypt the model when exporting.

        Args:
            enable_encryption (bool): Whether to encrypt the model when exporting. Export encryption can protect the
                integrity of the model, but it will increase the initialization time at runtime.

        Raises:
            TypeError: `enable_encryption` is not a bool.
        """
        check_isinstance("enable_encryption", enable_encryption, bool)
        self._converter.set_enable_encryption(enable_encryption)

    @property
    def encrypt_key(self):
        """
        Get the key used to encrypt the model when exporting.

        Returns:
            str, the key used to encrypt the model when exporting, expressed in hexadecimal characters. Only support to
            use it when `decrypt_mode` is ``"AES-GCM"``, the key length is 16.
        """
        return self._converter.get_encrypt_key()

    @encrypt_key.setter
    def encrypt_key(self, encrypt_key):
        """
        Set the key used to encrypt the model when exporting, expressed in hexadecimal characters.

        Args:
            encrypt_key (str): Set the key used to encrypt the model when exporting, expressed in hexadecimal
                characters. Only support when `decrypt_mode` is "AES-GCM", the key length is 16.

        Raises:
            TypeError: `encrypt_key` is not a str.
        """
        check_isinstance("encrypt_key", encrypt_key, str)
        self._converter.set_encrypt_key(encrypt_key)

    @property
    def infer(self):
        """
        Get the status whether to perform pre-inference at the completion of the conversion.

        Returns:
            bool, whether to perform pre-inference at the completion of the conversion.
        """
        return self._converter.get_infer()

    @infer.setter
    def infer(self, infer):
        """
        Set whether to perform pre-inference at the completion of the conversion.

        Args:
            infer (bool): whether to perform pre-inference at the completion of the conversion.

        Raises:
            TypeError: `infer` is not a bool.
        """
        check_isinstance("infer", infer, bool)
        self._converter.set_infer(infer)

    @property
    def input_data_type(self):
        """
        Get the data type of the quantization model input Tensor.

        Returns:
            DataType, the data type of the quantization model input Tensor. It is only valid when the quantization
            parameters ( `scale` and `zero point` ) of the model input Tensor are available. The following 4
            DataTypes are supported: ``DataType.FLOAT32`` , ``DataType.INT8`` , ``DataType.UINT8`` ,
            ``DataType.UNKNOWN``. For details, see
            `DataType <https://mindspore.cn/lite/api/en/master/mindspore_lite/mindspore_lite.DataType.html>`_ .

            - DataType.FLOAT32: 32-bit floating-point number.
            - DataType.INT8:    8-bit integer.
            - DataType.UINT8:   unsigned 8-bit integer.
            - DataType.UNKNOWN: Set the Same DataType as the model input Tensor.

        """
        return data_type_cxx_py_map.get(self._converter.get_input_data_type())

    @input_data_type.setter
    def input_data_type(self, input_data_type):
        """
        Set the data type of the quantization model input Tensor.

        Args:
            input_data_type (DataType): Set the data type of the quantization model input Tensor. It is only valid when
                the quantization parameters ( `scale` and `zero point` ) of the model input Tensor are available. The
                following 4 DataTypes are supported: DataType.FLOAT32 | DataType.INT8 | DataType.UINT8 |
                DataType.UNKNOWN. For details, see
                `DataType <https://mindspore.cn/lite/api/en/master/mindspore_lite/mindspore_lite.DataType.html>`_ .

                - DataType.FLOAT32: 32-bit floating-point number.
                - DataType.INT8:    8-bit integer.
                - DataType.UINT8:   unsigned 8-bit integer.
                - DataType.UNKNOWN: Set the Same DataType as the model input Tensor.

        Raises:
            TypeError: `input_data_type` is not a DataType.
            ValueError: `input_data_type` is not in [DataType.FLOAT32, DataType.INT8, DataType.UINT8, DataType.UNKNOWN]
                when `input_data_type` is a DataType.
        """
        check_isinstance("input_data_type", input_data_type, DataType)
        if input_data_type not in [DataType.FLOAT32, DataType.INT8, DataType.UINT8, DataType.UNKNOWN]:
            raise ValueError(f"input_data_type must be in [DataType.FLOAT32, DataType.INT8, DataType.UINT8, "
                             f"DataType.UNKNOWN].")
        self._converter.set_input_data_type(data_type_py_cxx_map.get(input_data_type))

    @property
    def input_format(self):
        """
        Get the input format of model.

        Returns:
            Format, the input format of model. Only Valid for 4-dimensional input. The following 2 input formats are
            supported: ``Format.NCHW``, ``Format.NHWC``. For details,
            see `Format <https://mindspore.cn/lite/api/en/master/mindspore_lite/mindspore_lite.Format.html>`_ .

            - Format.NCHW: Store Tensor data in the order of batch N, channel C, height H and width W.
            - Format.NHWC: Store Tensor data in the order of batch N, height H, width W and channel C.

        """
        return format_cxx_py_map.get(self._converter.get_input_format())

    @input_format.setter
    def input_format(self, input_format):
        """
        Set the input format of model.

        Args:
            input_format (Format): Set the input format of model. Only Valid for 4-dimensional input.The
                following 2 input formats are supported: Format.NCHW | Format.NHWC. For details,
                see `Format <https://mindspore.cn/lite/api/en/master/mindspore_lite/mindspore_lite.Format.html>`_ .

                - Format.NCHW: Store Tensor data in the order of batch N, channel C, height H and width W.
                - Format.NHWC: Store Tensor data in the order of batch N, height H, width W and channel C.

        Raises:
            TypeError: `input_format` is not a Format.
            ValueError: `input_format` is neither Format.NCHW nor Format.NHWC when it is a Format.
        """
        check_isinstance("input_format", input_format, Format)
        if input_format not in [Format.NCHW, Format.NHWC]:
            raise ValueError(f"input_format must be in [Format.NCHW, Format.NHWC].")
        self._converter.set_input_format(format_py_cxx_map.get(input_format))

    @property
    def input_shape(self):
        """
        Get the dimension of the model input.

        Returns:
            dict{str, list[int]}, the dimension of the model input. The order of input dimensions is consistent with the
            original model. In the following scenarios, users may need to set the parameter.
            For example, {"inTensor1": [1, 32, 32, 32], "inTensor2": [1, 1, 32, 32]}. Default: ``None``,
            equivalent to {}.

            - Usage 1:The input of the model to be converted is dynamic shape, but prepare to use fixed shape for
              inference, then set the parameter to fixed shape. After setting, when inferring on the converted
              model, the default input shape is the same as the parameter setting, no need to resize.
            - Usage 2: No matter whether the original input of the model to be converted is dynamic shape or not,
              but prepare to use fixed shape for inference, and the performance of the model is expected to be
              optimized as much as possible, then set the parameter to fixed shape. After setting, the model
              structure will be further optimized, but the converted model may lose the characteristics of dynamic
              shape(some operators strongly related to shape will be merged).
            - Usage 3: When using the converter function to generate code for Micro inference execution, it is
              recommended to set the parameter to reduce the probability of errors during deployment. When the model
              contains a Shape ops or the input of the model to be converted is a dynamic shape, you must set the
              parameter to fixed shape to support the relevant shape optimization and code generation.

        """
        return self._converter.get_input_shape()

    @input_shape.setter
    def input_shape(self, input_shape):
        """
        Set the dimension of the model input.

        Args:
            input_shape (dict{str, list[int]}): Set the dimension of the model input. The order of input dimensions is
                consistent with the original model. In the following scenarios, users may need to set the parameter.
                For example, {"inTensor1": [1, 32, 32, 32], "inTensor2": [1, 1, 32, 32]}.

                - Usage 1:The input of the model to be converted is dynamic shape, but prepare to use fixed shape for
                  inference, then set the parameter to fixed shape. After setting, when inferring on the converted
                  model, the default input shape is the same as the parameter setting, no need to resize.
                - Usage 2: No matter whether the original input of the model to be converted is dynamic shape or not,
                  but prepare to use fixed shape for inference, and the performance of the model is expected to be
                  optimized as much as possible, then set the parameter to fixed shape. After setting, the model
                  structure will be further optimized, but the converted model may lose the characteristics of dynamic
                  shape(some operators strongly related to shape will be merged).
                - Usage 3: When using the converter function to generate code for Micro inference execution, it is
                  recommended to set the parameter to reduce the probability of errors during deployment. When the model
                  contains a Shape ops or the input of the model to be converted is a dynamic shape, you must set the
                  parameter to fixed shape to support the relevant shape optimization and code generation.

        Raises:
            TypeError: `input_shape` is not a dict.
            TypeError: `input_shape` is a dict, but the keys are not str.
            TypeError: `input_shape` is a dict, the keys are str, but the values are not list.
            TypeError: `input_shape` is a dict, the keys are str, the values are list, but the value's elements are not
                int.
        """
        check_input_shape("input_shape", input_shape)
        self._converter.set_input_shape(input_shape)

    @property
    def optimize(self):
        """
        Get the status whether avoid fusion optimization.

        optimize is used to set the mode of optimization during the offline conversion. If this property is set to
        ``"none"``, no relevant graph optimization operations will be performed during the offline conversion phase of
        the model, and the relevant graph optimization operations will be performed during the execution of the
        inference phase. The advantage of this property is that the converted model can be deployed directly to any
        CPU/GPU/Ascend hardware backend since it is not optimized in a specific way, while the disadvantage is that
        the initialization time of the model increases during inference execution. If this property is set to
        ``"general"``, general optimization will be performed, such as constant folding and operator fusion (the
        converted model only supports CPU/GPU hardware backend, not Ascend backend). If this property is set to
        ``"gpu_oriented"``, the general optimization and extra optimization for GPU hardware will be performed (the
        converted model only supports GPU hardware backend). If this property is set to ``"ascend_oriented"``, the
        optimization for Ascend hardware will be performed (the converted model only supports Ascend hardware backend).

        For the MindSpore model, since it is already a `mindir` model, two approaches are suggested:

        1. Inference is performed directly without offline conversion.

        2. When using offline conversion, setting optimize to ``"general"`` in `CPU/GPU` hardware backend (for general
        optimization), setting optimize to ``"gpu_oriented"`` in `GPU` hardware (for GPU extra optimization based on
        general optimization), setting optimize to ``"ascend_oriented"`` in `Ascend` hardware.
        The relevant optimization is done in the offline phase to reduce the initialization time of inference execution.

        Returns:
            str, whether avoid fusion optimization. Options are ``"none"`` , ``"general"`` , ``"gpu_oriented"`` ,
            ``"ascend_oriented"``. ``"none"`` means fusion optimization is not allowed. ``"general"``,
            ``"gpu_oriented"`` and ``"ascend_oriented"`` means fusion optimization is allowed.
        """
        return self.optimize_user_defined

    @optimize.setter
    def optimize(self, optimize):
        """
        Set whether avoid fusion optimization.

        Args:
            optimize(str): Whether avoid fusion optimization. Options are "none" | "general" | "gpu_oriented"
                | "ascend_oriented". "none" means fusion optimization is not allowed. "general", "gpu_oriented" and
                "ascend_oriented" means fusion optimization is allowed.

        Raises:
            TypeError: `optimize` is not a str.
            ValueError: `optimize` is not in ["none", "general", "gpu_oriented", "ascend_oriented"] when it is a str.
        """
        check_isinstance("optimize", optimize, str)
        if optimize == "none":
            self._converter.set_no_fusion(True)
            self.optimize_user_defined = "none"
        elif optimize == "general":
            self._converter.set_no_fusion(False)
            self.optimize_user_defined = "general"
        elif optimize == "gpu_oriented":
            self._converter.set_no_fusion(False)
            self.device = "GPU"
            self.optimize_user_defined = "gpu_oriented"
        elif "ascend_oriented" in optimize:
            self._converter.set_no_fusion(False)
            self.device = "Ascend"
            split_str = optimize.split(":")
            if len(split_str) == 1:
                chip_name = "default"
            elif len(split_str) == 2:
                chip_name = split_str[1]
            else:
                raise ValueError(f"chip_name must be single")
            check_isinstance("chip_name", chip_name, str)
            if chip_name not in ["default", "910b"]:
                raise ValueError(f"chip_name must be in [default, 910b], but got {chip_name}.")
            self._converter.set_chip_name(chip_name)
            self.optimize_user_defined = "ascend_oriented"
        else:
            raise ValueError(
                f"optimize must be 'none', 'general', 'gpu_oriented', 'ascend_oriented' or 'ascend_oriented:910b'.")

    @property
    def output_data_type(self):
        """
        Get the data type of the quantization model output Tensor.

        Returns:
            DataType, the data type of the quantization model output Tensor. It is only valid when the quantization
            parameters ( `scale` and `zero point` ) of the model output Tensor are available. The following 4
            DataTypes are supported: ``DataType.FLOAT32`` , ``DataType.INT8`` , ``DataType.UINT8`` ,
            ``DataType.UNKNOWN``.
            For details, see
            `DataType <https://mindspore.cn/lite/api/en/master/mindspore_lite/mindspore_lite.DataType.html>`_ .

            - DataType.FLOAT32: 32-bit floating-point number.
            - DataType.INT8:    8-bit integer.
            - DataType.UINT8:   unsigned 8-bit integer.
            - DataType.UNKNOWN: Set the Same DataType as the model output Tensor.

        """
        return data_type_cxx_py_map.get(self._converter.get_output_data_type())

    @output_data_type.setter
    def output_data_type(self, output_data_type):
        """
        Set the data type of the quantization model output Tensor.

        Args:
            output_data_type (DataType): Set the data type of the quantization model output Tensor. It is only valid
                when the quantization parameters ( `scale` and `zero point` ) of the model output Tensor are available.
                The following 4 DataTypes are supported: DataType.FLOAT32 | DataType.INT8 | DataType.UINT8 |
                DataType.UNKNOWN. For details, see
                `DataType <https://mindspore.cn/lite/api/en/master/mindspore_lite/mindspore_lite.DataType.html>`_ .

                - DataType.FLOAT32: 32-bit floating-point number.
                - DataType.INT8:    8-bit integer.
                - DataType.UINT8:   unsigned 8-bit integer.
                - DataType.UNKNOWN: Set the Same DataType as the model output Tensor.

        Raises:
            TypeError: `output_data_type` is not a DataType.
            ValueError: `output_data_type` is not in [DataType.FLOAT32, DataType.INT8, DataType.UINT8, DataType.UNKNOWN]
                when `output_data_type` is a DataType.
        """
        check_isinstance("output_data_type", output_data_type, DataType)
        if output_data_type not in [DataType.FLOAT32, DataType.INT8, DataType.UINT8, DataType.UNKNOWN]:
            raise ValueError(f"output_data_type must be in [DataType.FLOAT32, DataType.INT8, DataType.UINT8, "
                             f"DataType.UNKNOWN].")
        self._converter.set_output_data_type(data_type_py_cxx_map.get(output_data_type))

    @property
    def save_type(self):
        """
        Get the model type needs to be export.

        Returns:
            ModelType, the model type needs to be export. Options are ``ModelType.MINDIR`` , ``ModelType.MINDIR_LITE``.
            Convert to MindSpore model is recommended. Currently, Convert to MindSpore Lite model is supported,
            but it will be deprecated in the future. For details, see
            `ModelType <https://mindspore.cn/lite/api/en/master/mindspore_lite/mindspore_lite.ModelType.html>`_ .
        """
        return model_type_cxx_py_map.get(self._converter.get_save_type())

    @save_type.setter
    def save_type(self, save_type):
        """
        Set the model type needs to be export.

        Args:
            save_type (ModelType): Set the model type needs to be export. Options are ModelType.MINDIR |
                ModelType.MINDIR_LITE. Convert to MindSpore model is recommended. Currently, Convert to MindSpore Lite
                model is supported, but it will be deprecated in the future. For details, see
                `ModelType <https://mindspore.cn/lite/api/en/master/mindspore_lite/mindspore_lite.ModelType.html>`_ .

        Raises:
            TypeError: `save_type` is not a ModelType.
        """
        check_isinstance("save_type", save_type, ModelType)
        self._converter.set_save_type(model_type_py_cxx_map.get(save_type))

    @property
    def weight_fp16(self):
        """
        Get the status whether the model will be saved as the float16 data type.

        Returns:
            bool, whether the model will be saved as the float16 data type. If it is ``True``, the const Tensor of the
            float32 in the model will be saved as the float16 data type during Converter, and the generated model
            size will be compressed. Then, according to `Context.CPU` 's `precision_mode` parameter determines the
            inputs' data type to perform inference. The priority of `weight_fp16` is very low. For example, if
            quantization is enabled, for the weight of the quantized, `weight_fp16` will not take effect again.
            `weight_fp16` only effective for the const Tensor in float32 data type.
        """
        return self._converter.get_weight_fp16()

    @weight_fp16.setter
    def weight_fp16(self, weight_fp16):
        """
        Set whether the model will be saved as the float16 data type.

        Args:
            weight_fp16 (bool): If it is True, the const Tensor of the float32 in the model will be saved as the float16
                data type during Converter, and the generated model size will be compressed. Then, according to
                `Context.CPU` 's `precision_mode` parameter determines the inputs' data type to perform inference. The
                priority of `weight_fp16` is very low. For example, if quantization is enabled, for the weight of the
                quantized, `weight_fp16` will not take effect again. `weight_fp16` only effective for the const Tensor
                in float32 data type.

        Raises:
            TypeError: `weight_fp16` is not a bool.
        """
        check_isinstance("weight_fp16", weight_fp16, bool)
        self._converter.set_weight_fp16(weight_fp16)

    @property
    def device_id(self):
        """
        Get device id of device target.

        Returns:
            int, device id of device target.
        """
        return self._converter.get_device_id()

    @device_id.setter
    def device_id(self, device_id):
        """
        Set device id of device target.

        Args:
            device_id (int): device id of device target.

        Raises:
            TypeError: `device_id` is not an int.
        """
        check_isinstance("device_id", device_id, int)
        self._converter.set_device_id(device_id)

    @property
    def rank_id(self):
        """
        Get rank id of device target.

        Returns:
            int, rank id of device target.
        """
        return self._converter.get_rank_id()

    @rank_id.setter
    def rank_id(self, rank_id):
        """
        Set rank id of device target.

        Args:
            rank_id (int): rank id of device target.

        Raises:
            TypeError: `rank_id` is not an int.
        """
        check_isinstance("rank_id", rank_id, int)
        self._converter.set_rank_id(rank_id)

    @set_env
    def convert(self, fmk_type, model_file, output_file, weight_file="", config_file=""):
        """
        Perform conversion, and convert the third-party model to the MindSpore model or MindSpore Lite model.

        Args:
            fmk_type (FmkType): Input model framework type. Options are ``FmkType.TF`` , ``FmkType.CAFFE`` ,
                ``FmkType.ONNX`` , ``FmkType.MINDIR`` , ``FmkType.TFLITE`` , ``FmkType.PYTORCH``. For details, see
                `FmkType <https://mindspore.cn/lite/api/en/master/mindspore_lite/mindspore_lite.FmkType.html>`_ .
            model_file (str): Set the path of the input model when convert. For example,
                ``"/home/user/model.prototxt"``. Options are TF: ``"model.pb"`` , CAFFE: ``"model.prototxt"`` ,
                ONNX: ``"model.onnx"`` , MINDIR: ``"model.mindir"`` , TFLITE: ``"model.tflite"`` ,
                PYTORCH: ``"model.pt or model.pth"``.
            output_file (str): Set the path of the output model. The suffix .ms or .mindir can be automatically
                generated. If set `save_type` to ``ModelType.MINDIR``, then MindSpore's model will be generated,
                which uses .mindir as suffix. If set `save_type` to ``ModelType.MINDIR_LITE``, then MindSpore Lite's
                model will be generated, which uses .ms as suffix. For example, the input model is
                "/home/user/model.prototxt", set `save_type` to ModelType.MINDIR, it will generate the model
                named model.prototxt.mindir in /home/user/.
            weight_file (str, optional): Set the path of input model weight file. Required only when `fmk_type` is
                ``FmkType.CAFFE``. The Caffe model is generally divided into two files: `model.prototxt`
                is model structure, corresponding to `model_file` parameter; `model.Caffemodel` is model weight
                value file, corresponding to `weight_file` parameter. For example,
                ``"/home/user/model.caffemodel"``. Default: ``""``, indicating no weight file.
            config_file (str, optional): Set the path of the configuration file of Converter can be used to
                post-training, offline split op to parallel, disable op fusion ability and set plugin so path.
                `config_file` uses the `key = value` method to define the related parameters.
                For the configuration parameters related to post training quantization, please refer to
                `quantization <https://www.mindspore.cn/lite/docs/en/master/use/post_training_quantization.html>`_ .
                For the configuration parameters related to extension, please refer to
                `extension  <https://www.mindspore.cn/lite/docs/en/master/use/nnie.html#extension-configuration>`_ .
                For example, "/home/user/model.cfg". Default: ``""``, indicating that no configuration file.

        Raises:
            TypeError: `fmk_type` is not a FmkType.
            TypeError: `model_file` is not a str.
            TypeError: `output_file` is not a str.
            TypeError: `weight_file` is not a str.
            TypeError: `config_file` is not a str.
            RuntimeError: `model_file` does not exist.
            RuntimeError: `weight_file` is not ``""``, but `weight_file` does not exist.
            RuntimeError: `config_file` is not ``""``, but `config_file` does not exist.
            RuntimeError: convert model failed.

        Examples:
            >>> import mindspore_lite as mslite
            >>> converter = mslite.Converter()
            >>> converter.save_type = mslite.ModelType.MINDIR
            >>> converter.convert(mslite.FmkType.TFLITE, "./mobilenetv2/mobilenet_v2_1.0_224.tflite",
            ...                     "mobilenet_v2_1.0_224.tflite")
            CONVERT RESULT SUCCESS:0
            >>> # mobilenet_v2_1.0_224.tflite.mindir model will be generated.
        """
        check_isinstance("fmk_type", fmk_type, FmkType)
        check_isinstance("model_file", model_file, str)
        check_isinstance("output_file", output_file, str)
        check_isinstance("weight_file", weight_file, str)
        check_isinstance("config_file", config_file, str)
        if not os.path.exists(model_file):
            raise RuntimeError(f"Perform convert method failed, model_file does not exist!")
        if weight_file != "":
            if not os.path.exists(weight_file):
                raise RuntimeError(f"Perform convert method failed, weight_file does not exist!")
        if config_file != "":
            if not os.path.exists(config_file):
                raise RuntimeError(f"Perform convert method failed, config_file does not exist!")
            self._converter.set_config_file(config_file)

        fmk_type_py_cxx_map = {
            FmkType.TF: _c_lite_wrapper.FmkType.kFmkTypeTf,
            FmkType.CAFFE: _c_lite_wrapper.FmkType.kFmkTypeCaffe,
            FmkType.ONNX: _c_lite_wrapper.FmkType.kFmkTypeOnnx,
            FmkType.MINDIR: _c_lite_wrapper.FmkType.kFmkTypeMs,
            FmkType.TFLITE: _c_lite_wrapper.FmkType.kFmkTypeTflite,
            FmkType.PYTORCH: _c_lite_wrapper.FmkType.kFmkTypePytorch,
        }
        ret = self._converter.convert(fmk_type_py_cxx_map.get(fmk_type), model_file, output_file, weight_file)
        if not ret.IsOk():
            raise RuntimeError(f"Converter model failed! model_file is {model_file}, error is {ret.ToString()}")

    def get_config_info(self):
        r"""
        Get config info of converter.It is used together with `set_config_info` method for online converter.
        Please use `set_config_info` method before `get_config_info` .

        Returns:
            :obj:`dict{str: dict{str: str}}`, the config info which has been set in converter.

        Examples:
            >>> import mindspore_lite as mslite
            >>> converter = mslite.Converter()
            >>> section = "common_quant_param"
            >>> config_info_in = {"quant_type": "WEIGHT_QUANT"}
            >>> converter.set_config_info(section, config_info_in)
            >>> config_info_out = converter.get_config_info()
            >>> print(config_info_out)
            {'common_quant_param': {'quant_type': 'WEIGHT_QUANT'}}
        """
        return self._converter.get_config_info()

    def set_config_info(self, section="", config_info=None):
        r"""
        Set config info for Converter.It is used together with `get_config_info` method for online converter.

        Args:
            section (str, optional): The category of the configuration parameter.
                Set the individual parameters of the configfile together with `config_info` .
                For example, for `section` = ``"common_quant_param"``, `config_info` = {"quant_type": "WEIGHT_QUANT"}.
                Default: ``""`` .

                For the configuration parameters related to post training quantization, please refer to
                `quantization <https://www.mindspore.cn/lite/docs/en/master/use/post_training_quantization.html>`_ .

                For the configuration parameters related to extension, please refer to
                `extension  <https://www.mindspore.cn/lite/docs/en/master/use/nnie.html#extension-configuration>`_ .

                - ``"common_quant_param"``: Common quantization parameter.
                - ``"mixed_bit_weight_quant_param"``: Mixed bit weight quantization parameter.
                - ``"full_quant_param"``: Full quantization parameter.
                - ``"data_preprocess_param"``: Data preprocess quantization parameter.
                - ``"registry"``: Extension configuration parameter.

            config_info (:obj:`dict{str: str}`, optional): List of configuration parameters.
                Set the individual parameters of the configfile together with `section` .
                For example, for `section` = ``"common_quant_param"``, `config_info` = {"quant_type": "WEIGHT_QUANT"}.
                Default: ``None``, ``None`` is equivalent to {}.

                For the configuration parameters related to post training quantization, please refer to
                `quantization <https://www.mindspore.cn/lite/docs/en/master/use/post_training_quantization.html>`_ .

                For the configuration parameters related to extension, please refer to
                `extension  <https://www.mindspore.cn/lite/docs/en/master/use/nnie.html#extension-configuration>`_ .

        Raises:
            TypeError: `section` is not a str.
            TypeError: `config_info` is not a dict .
            TypeError: `config_info` is a dict, but the keys are not str.
            TypeError: `config_info` is a dict, the keys are str, but the values are not str.

        Examples:
            >>> import mindspore_lite as mslite
            >>> converter = mslite.Converter()
            >>> section = "common_quant_param"
            >>> config_info = {"quant_type": "WEIGHT_QUANT"}
            >>> converter.set_config_info(section, config_info)
        """
        check_isinstance("section", section, str)
        check_config_info("config_info", config_info, enable_none=True)
        if section != "" and config_info is not None:
            self._converter.set_config_info(section, config_info)
