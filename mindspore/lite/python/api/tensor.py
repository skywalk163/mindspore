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
Tensor API.
"""
from __future__ import absolute_import
from enum import Enum

import numpy

from mindspore_lite.lib import _c_lite_wrapper
from mindspore_lite._checkparam import check_tensor_input_param, check_isinstance

__all__ = ['TensorMeta', 'DataType', 'Format', 'Tensor']


class TensorMeta:
    """
    The `TensorMeta` class defines a TensorInfo in MindSpore Lite.

    Args:
        tensor(): The info to be stored in a new TensorMeta.
    """

    def __init__(self):
        self.name = ""
        self.dtype = DataType.UNKNOWN
        self.shape = []
        self.format = Format.DEFAULT
        self.element_num = 0
        self.data_size = 0

    def __str__(self):
        res = f"name: {self.name},\n" \
              f"dtype: {self.dtype},\n" \
              f"shape: {self.shape},\n" \
              f"format: {self.format},\n" \
              f"element_num: {self.element_num},\n" \
              f"data_size: {self.data_size}."
        return res


class DataType(Enum):
    """
    The `DataType` class defines the data type of the Tensor in MindSpore Lite.

    Currently, the following 'DataType' are supported:

    ===========================  ==================================================================
    Definition                    Description
    ===========================  ==================================================================
    `DataType.UNKNOWN`           No matching any of the following known types.
    `DataType.BOOL`              Boolean `True` or `False` .
    `DataType.INT8`              8-bit integer.
    `DataType.INT16`             16-bit integer.
    `DataType.INT32`             32-bit integer.
    `DataType.INT64`             64-bit integer.
    `DataType.UINT8`             unsigned 8-bit integer.
    `DataType.UINT16`            unsigned 16-bit integer.
    `DataType.UINT32`            unsigned 32-bit integer.
    `DataType.UINT64`            unsigned 64-bit integer.
    `DataType.FLOAT16`           16-bit floating-point number.
    `DataType.FLOAT32`           32-bit floating-point number.
    `DataType.FLOAT64`           64-bit floating-point number.
    `DataType.INVALID`           The maximum threshold value of DataType to prevent invalid types.
    ===========================  ==================================================================

    Examples:
        >>> # Method 1: Import mindspore_lite package
        >>> import mindspore_lite as mslite
        >>> print(mslite.DataType.FLOAT32)
        DataType.FLOAT32
        >>> # Method 2: from mindspore_lite package import DataType
        >>> from mindspore_lite import DataType
        >>> print(DataType.FLOAT32)
        DataType.FLOAT32
    """

    UNKNOWN = 0
    BOOL = 30
    INT8 = 32
    INT16 = 33
    INT32 = 34
    INT64 = 35
    UINT8 = 37
    UINT16 = 38
    UINT32 = 39
    UINT64 = 40
    FLOAT16 = 42
    FLOAT32 = 43
    FLOAT64 = 44
    INVALID = 2147483647  # INT32_MAX


class Format(Enum):
    """
    The `Format` class defines the format of the Tensor in MindSpore Lite.

    Currently, the following 'Format' are supported:

    ===========================  ===================================================================================
    Definition                    Description
    ===========================  ===================================================================================
    `Format.DEFAULT`             default format.
    `Format.NCHW`                Store Tensor data in the order of batch N, channel C, height H and width W.
    `Format.NHWC`                Store Tensor data in the order of batch N, height H, width W and channel C.
    `Format.NHWC4`               C-axis 4-byte aligned `Format.NHWC` .
    `Format.HWKC`                Store Tensor data in the order of height H, width W, kernel num K and channel C.
    `Format.HWCK`                Store Tensor data in the order of height H, width W, channel C and kernel num K.
    `Format.KCHW`                Store Tensor data in the order of kernel num K, channel C, height H and width W.
    `Format.CKHW`                Store Tensor data in the order of channel C, kernel num K, height H and width W.
    `Format.KHWC`                Store Tensor data in the order of kernel num K, height H, width W and channel C.
    `Format.CHWK`                Store Tensor data in the order of channel C, height H, width W and kernel num K.
    `Format.HW`                  Store Tensor data in the order of height H and width W.
    `Format.HW4`                 w-axis 4-byte aligned `Format.HW` .
    `Format.NC`                  Store Tensor data in the order of batch N and channel C.
    `Format.NC4`                 C-axis 4-byte aligned `Format.NC` .
    `Format.NC4HW4`              C-axis 4-byte aligned and W-axis 4-byte aligned `Format.NCHW` .
    `Format.NCDHW`               Store Tensor data in the order of batch N, channel C, depth D, height H and width W.
    `Format.NWC`                 Store Tensor data in the order of batch N, width W and channel C.
    `Format.NCW`                 Store Tensor data in the order of batch N, channel C and width W.
    `Format.NDHWC`               Store Tensor data in the order of batch N, depth D, height H, width W and channel C.
    `Format.NC8HW8`              C-axis 8-byte aligned and W-axis 8-byte aligned `Format.NCHW` .
    ===========================  ===================================================================================

    Examples:
        >>> # Method 1: Import mindspore_lite package
        >>> import mindspore_lite as mslite
        >>> print(mslite.Format.NHWC)
        Format.NHWC
        >>> # Method 2: from mindspore_lite package import Format
        >>> from mindspore_lite import Format
        >>> print(Format.NHWC)
        Format.NHWC
    """

    DEFAULT = -1
    NCHW = 0
    NHWC = 1
    NHWC4 = 2
    HWKC = 3
    HWCK = 4
    KCHW = 5
    CKHW = 6
    KHWC = 7
    CHWK = 8
    HW = 9
    HW4 = 10
    NC = 11
    NC4 = 12
    NC4HW4 = 13
    NCDHW = 15
    NWC = 16
    NCW = 17
    NDHWC = 18
    NC8HW8 = 19


data_type_py_cxx_map = {
    DataType.UNKNOWN: _c_lite_wrapper.DataType.kTypeUnknown,
    DataType.BOOL: _c_lite_wrapper.DataType.kNumberTypeBool,
    DataType.INT8: _c_lite_wrapper.DataType.kNumberTypeInt8,
    DataType.INT16: _c_lite_wrapper.DataType.kNumberTypeInt16,
    DataType.INT32: _c_lite_wrapper.DataType.kNumberTypeInt32,
    DataType.INT64: _c_lite_wrapper.DataType.kNumberTypeInt64,
    DataType.UINT8: _c_lite_wrapper.DataType.kNumberTypeUInt8,
    DataType.UINT16: _c_lite_wrapper.DataType.kNumberTypeUInt16,
    DataType.UINT32: _c_lite_wrapper.DataType.kNumberTypeUInt32,
    DataType.UINT64: _c_lite_wrapper.DataType.kNumberTypeUInt64,
    DataType.FLOAT16: _c_lite_wrapper.DataType.kNumberTypeFloat16,
    DataType.FLOAT32: _c_lite_wrapper.DataType.kNumberTypeFloat32,
    DataType.FLOAT64: _c_lite_wrapper.DataType.kNumberTypeFloat64,
    DataType.INVALID: _c_lite_wrapper.DataType.kInvalidType,
}

data_type_cxx_py_map = {
    _c_lite_wrapper.DataType.kTypeUnknown: DataType.UNKNOWN,
    _c_lite_wrapper.DataType.kNumberTypeBool: DataType.BOOL,
    _c_lite_wrapper.DataType.kNumberTypeInt8: DataType.INT8,
    _c_lite_wrapper.DataType.kNumberTypeInt16: DataType.INT16,
    _c_lite_wrapper.DataType.kNumberTypeInt32: DataType.INT32,
    _c_lite_wrapper.DataType.kNumberTypeInt64: DataType.INT64,
    _c_lite_wrapper.DataType.kNumberTypeUInt8: DataType.UINT8,
    _c_lite_wrapper.DataType.kNumberTypeUInt16: DataType.UINT16,
    _c_lite_wrapper.DataType.kNumberTypeUInt32: DataType.UINT32,
    _c_lite_wrapper.DataType.kNumberTypeUInt64: DataType.UINT64,
    _c_lite_wrapper.DataType.kNumberTypeFloat16: DataType.FLOAT16,
    _c_lite_wrapper.DataType.kNumberTypeFloat32: DataType.FLOAT32,
    _c_lite_wrapper.DataType.kNumberTypeFloat64: DataType.FLOAT64,
    _c_lite_wrapper.DataType.kInvalidType: DataType.INVALID,
}

numpy_data_type_map = {
    numpy.bool_: DataType.BOOL,
    numpy.int8: DataType.INT8,
    numpy.int16: DataType.INT16,
    numpy.int32: DataType.INT32,
    numpy.int64: DataType.INT64,
    numpy.uint8: DataType.UINT8,
    numpy.uint16: DataType.UINT16,
    numpy.uint32: DataType.UINT32,
    numpy.uint64: DataType.UINT64,
    numpy.float16: DataType.FLOAT16,
    numpy.float32: DataType.FLOAT32,
    numpy.float64: DataType.FLOAT64,
}

ms_to_numpy_data_type_map = {
    DataType.BOOL: numpy.bool_,
    DataType.INT8: numpy.int8,
    DataType.INT16: numpy.int16,
    DataType.INT32: numpy.int32,
    DataType.INT64: numpy.int64,
    DataType.UINT8: numpy.uint8,
    DataType.UINT16: numpy.uint16,
    DataType.UINT32: numpy.uint32,
    DataType.UINT64: numpy.uint64,
    DataType.FLOAT16: numpy.float16,
    DataType.FLOAT32: numpy.float32,
    DataType.FLOAT64: numpy.float64,
}

format_py_cxx_map = {
    Format.DEFAULT: _c_lite_wrapper.Format.DEFAULT_FORMAT,
    Format.NCHW: _c_lite_wrapper.Format.NCHW,
    Format.NHWC: _c_lite_wrapper.Format.NHWC,
    Format.NHWC4: _c_lite_wrapper.Format.NHWC4,
    Format.HWKC: _c_lite_wrapper.Format.HWKC,
    Format.HWCK: _c_lite_wrapper.Format.HWCK,
    Format.KCHW: _c_lite_wrapper.Format.KCHW,
    Format.CKHW: _c_lite_wrapper.Format.CKHW,
    Format.KHWC: _c_lite_wrapper.Format.KHWC,
    Format.CHWK: _c_lite_wrapper.Format.CHWK,
    Format.HW: _c_lite_wrapper.Format.HW,
    Format.HW4: _c_lite_wrapper.Format.HW4,
    Format.NC: _c_lite_wrapper.Format.NC,
    Format.NC4: _c_lite_wrapper.Format.NC4,
    Format.NC4HW4: _c_lite_wrapper.Format.NC4HW4,
    Format.NCDHW: _c_lite_wrapper.Format.NCDHW,
    Format.NWC: _c_lite_wrapper.Format.NWC,
    Format.NCW: _c_lite_wrapper.Format.NCW,
    Format.NDHWC: _c_lite_wrapper.Format.NDHWC,
    Format.NC8HW8: _c_lite_wrapper.Format.NC8HW8,
}

format_cxx_py_map = {
    _c_lite_wrapper.Format.DEFAULT_FORMAT: Format.DEFAULT,
    _c_lite_wrapper.Format.NCHW: Format.NCHW,
    _c_lite_wrapper.Format.NHWC: Format.NHWC,
    _c_lite_wrapper.Format.NHWC4: Format.NHWC4,
    _c_lite_wrapper.Format.HWKC: Format.HWKC,
    _c_lite_wrapper.Format.HWCK: Format.HWCK,
    _c_lite_wrapper.Format.KCHW: Format.KCHW,
    _c_lite_wrapper.Format.CKHW: Format.CKHW,
    _c_lite_wrapper.Format.KHWC: Format.KHWC,
    _c_lite_wrapper.Format.CHWK: Format.CHWK,
    _c_lite_wrapper.Format.HW: Format.HW,
    _c_lite_wrapper.Format.HW4: Format.HW4,
    _c_lite_wrapper.Format.NC: Format.NC,
    _c_lite_wrapper.Format.NC4: Format.NC4,
    _c_lite_wrapper.Format.NC4HW4: Format.NC4HW4,
    _c_lite_wrapper.Format.NCDHW: Format.NCDHW,
    _c_lite_wrapper.Format.NWC: Format.NWC,
    _c_lite_wrapper.Format.NCW: Format.NCW,
    _c_lite_wrapper.Format.NDHWC: Format.NDHWC,
    _c_lite_wrapper.Format.NC8HW8: Format.NC8HW8,
}


class Tensor:
    """
    The `Tensor` class defines a Tensor in MindSpore Lite.

    Args:
        tensor(Tensor, optional): The data to be stored in a new Tensor. It can be from another Tensor.
            Default: ``None``.
        shape(list, optional): The shape of the Tensor.
            Default: ``None``.
        dtype(DataType, optional): The dtype of the Tensor.
            Default: ``None``.
        device(str, optional): The device type of the Tensor.
            Default: ``None``.
    Raises:
        TypeError: `tensor` is neither a Tensor nor ``None``.

    Examples:
        >>> import mindspore_lite as mslite
        >>> tensor = mslite.Tensor()
        >>> tensor.name = "tensor1"
        >>> print(tensor.name)
        tensor1
        >>> tensor.dtype = mslite.DataType.FLOAT32
        >>> print(tensor.dtype)
        DataType.FLOAT32
        >>> tensor.shape = [1, 3, 2, 2]
        >>> print(tensor.shape)
        [1, 3, 2, 2]
        >>> tensor.format = mslite.Format.NCHW
        >>> print(tensor.format)
        Format.NCHW
        >>> print(tensor.element_num)
        12
        >>> print(tensor.data_size)
        48
        >>> print(tensor)
        name: tensor1,
        dtype: DataType.FLOAT32,
        shape: [1, 3, 2, 2],
        format: Format.NCHW,
        element_num: 12,
        data_size: 48.
    """

    def __init__(self, tensor=None, shape=None, dtype=None, device=None):
        # check shape, dtype and device
        check_tensor_input_param(shape, device)
        device_type = ""
        device_id = -1
        if device is not None:
            device_type = device.split(":")[0]
            if len(device.split(":")) == 2:
                device_id = int(device.split(":")[1])
        check_isinstance("dtype", dtype, DataType, True)
        if tensor is not None:
            # use tensor to init tensor
            if isinstance(tensor, _c_lite_wrapper.TensorBind):
                self._tensor = tensor
            elif isinstance(tensor, Tensor):
                tensor_shape = tensor.shape
                if shape is not None and list(shape) != list(tensor_shape):
                    raise TypeError(
                        f"user set shape is not equal numpy shape, user's shape: {shape}, "
                        f"tensor shape is: {tensor_shape}.")
                tensor_dtype = tensor.dtype
                if dtype is not None and tensor_dtype != dtype:
                    raise TypeError(
                        f"user set dtype is not equal tensor dtype, user's dtype: {dtype}, "
                        f"tensor dtype is: {tensor_dtype}.")
                numpy_data = tensor.get_data_to_numpy()
                self._tensor = _c_lite_wrapper.create_tensor_by_numpy(numpy_data, device_type, device_id)
            # use numpy to init tensor
            elif isinstance(tensor, numpy.ndarray):
                if not tensor.flags['FORC']:
                    tensor = numpy.ascontiguousarray(tensor)
                numpy_shape = tensor.shape
                numpy_dtype = tensor.dtype
                if numpy_dtype.type not in numpy_data_type_map:
                    raise TypeError(f"Unsupported numpy dtype value {numpy_dtype}")
                ms_dtype = numpy_data_type_map.get(numpy_dtype.type)
                if shape is not None and list(shape) != list(numpy_shape):
                    raise TypeError(
                        f"user set shape is not equal numpy shape, user shape: {shape}, "
                        f"numpy shape is: {numpy_shape}.")
                if dtype is not None and ms_dtype != dtype:
                    raise TypeError(
                        f"user set dtype is not equal numpy dtype, user dtype: {dtype}, "
                        f"numpy dtype is: {numpy_dtype}.")
                self._tensor = _c_lite_wrapper.create_tensor_by_numpy(tensor, device_type, device_id)
            else:
                raise TypeError(
                    f"tensor must be MindSpore Lite's Tensor._tensor or numpy ndarray, but got {type(tensor)}.")
        else:
            if dtype is not None and shape is not None:
                self._tensor = _c_lite_wrapper.create_tensor(data_type_py_cxx_map.get(dtype), shape, device_type,
                                                             device_id)
            else:
                self._tensor = _c_lite_wrapper.create_tensor(data_type_py_cxx_map.get(DataType.FLOAT32), (), "", -1)

    def __str__(self):
        res = f"name: {self.name},\n" \
              f"dtype: {self.dtype},\n" \
              f"shape: {self.shape},\n" \
              f"format: {self.format},\n" \
              f"element_num: {self.element_num},\n" \
              f"data_size: {self.data_size}.\n" \
              f"device: {self.device}."
        return res

    @property
    def data_size(self):
        """
        Get the data size of the Tensor.

        Data size of the Tensor = the element num of the Tensor * size of unit data type of the Tensor.

        Returns:
            int, the data size of the Tensor data.
        """
        return self._tensor.get_data_size()

    @property
    def dtype(self):
        """
        Get the data type of the Tensor.

        Returns:
            DataType, the data type of the Tensor.
        """
        return data_type_cxx_py_map.get(self._tensor.get_data_type())

    @dtype.setter
    def dtype(self, dtype):
        """
        Set data type for the Tensor.

        Args:
            dtype (DataType): The data type of the Tensor. For details, see
                `DataType <https://mindspore.cn/lite/api/en/master/mindspore_lite/mindspore_lite.DataType.html>`_ .

        Raises:
            TypeError: `dtype` is not a DataType.
        """
        if not isinstance(dtype, DataType):
            raise TypeError(f"dtype must be DataType, but got {type(dtype)}.")
        self._tensor.set_data_type(data_type_py_cxx_map.get(dtype))

    @property
    def element_num(self):
        """
        Get the element num of the Tensor.

        Returns:
            int, the element num of the Tensor data.
        """
        return self._tensor.get_element_num()

    @property
    def format(self):
        """
        Get the format of the Tensor.

        Returns:
            Format, the format of the Tensor.
        """
        return format_cxx_py_map.get(self._tensor.get_format())

    @format.setter
    def format(self, tensor_format):
        """
        Set format of the Tensor.

        Args:
            tensor_format (Format): The format of the Tensor. For details, see
                `Format <https://mindspore.cn/lite/api/en/master/mindspore_lite/mindspore_lite.Format.html>`_ .

        Raises:
            TypeError: `tensor_format` is not a Format.
        """
        if not isinstance(tensor_format, Format):
            raise TypeError(f"format must be Format, but got {type(tensor_format)}.")
        self._tensor.set_format(format_py_cxx_map.get(tensor_format))

    @property
    def name(self):
        """
        Get the name of the Tensor.

        Returns:
            str, the name of the Tensor.
        """
        return self._tensor.get_tensor_name()

    @name.setter
    def name(self, name):
        """
        Set the name of the Tensor.

        Args:
            name (str): The name of the Tensor.

        Raises:
            TypeError: `name` is not a str.
        """
        if not isinstance(name, str):
            raise TypeError(f"name must be str, but got {type(name)}.")
        self._tensor.set_tensor_name(name)

    @property
    def shape(self):
        """
        Get the shape of the Tensor.

        Returns:
            list[int], the shape of the Tensor.
        """
        return self._tensor.get_shape()

    @shape.setter
    def shape(self, shape):
        """
        Set shape for the Tensor.

        Args:
            shape (list[int]): The shape of the Tensor.

        Raises:
            TypeError: `shape` is not a list.
            TypeError: `shape` is a list, but the elements is not int.
        """
        if not isinstance(shape, (list, tuple)):
            raise TypeError(f"shape must be list or tuple, but got {type(shape)}.")
        for i, element in enumerate(shape):
            if not isinstance(element, int):
                raise TypeError(f"shape element must be int, but got {type(element)} at index {i}.")
        self._tensor.set_shape(shape)

    def get_data_to_numpy(self):
        """
        Get the data from the Tensor to the numpy object.

        Returns:
            numpy.ndarray, the numpy object from Tensor data.

        Examples:
            >>> import mindspore_lite as mslite
            >>> import numpy as np
            >>> tensor = mslite.Tensor()
            >>> tensor.shape = [1, 3, 2, 2]
            >>> tensor.dtype = mslite.DataType.FLOAT32
            >>> in_data = np.arange(1 * 3 * 2 * 2, dtype=np.float32)
            >>> tensor.set_data_from_numpy(in_data)
            >>> data = tensor.get_data_to_numpy()
            >>> print(data)
            [[[[ 0.  1.]
               [ 2.  3.]]
              [[ 4.  5.]
               [ 6.  7.]]
              [[ 8.  9.]
               [ 10. 11.]]]]
        """
        return self._tensor.get_data_to_numpy()

    def set_data_from_numpy(self, numpy_obj):
        """
        Set the data for the Tensor from the numpy object.

        Args:
            numpy_obj(numpy.ndarray): the numpy object.

        Raises:
            TypeError: `numpy_obj` is not a numpy.ndarray.
            RuntimeError: The data type of `numpy_obj` is not equivalent to the data type of the Tensor.
            RuntimeError: The data size of `numpy_obj` is not equal to the data size of the Tensor.

        Examples:
            >>> # 1. set Tensor data which is from file
            >>> import mindspore_lite as mslite
            >>> import numpy as np
            >>> tensor = mslite.Tensor()
            >>> tensor.shape = [1, 3, 224, 224]
            >>> tensor.dtype = mslite.DataType.FLOAT32
            >>> in_data = np.fromfile("input.bin", dtype=np.float32)
            >>> tensor.set_data_from_numpy(in_data)
            >>> print(tensor)
            name: ,
            dtype: DataType.FLOAT32,
            shape: [1, 3, 224, 224],
            format: Format.NCHW,
            element_num: 150528,
            data_size: 602112.
            >>> # 2. set Tensor data which is numpy arange
            >>> import mindspore_lite as mslite
            >>> import numpy as np
            >>> tensor = mslite.Tensor()
            >>> tensor.shape = [1, 3, 2, 2]
            >>> tensor.dtype = mslite.DataType.FLOAT32
            >>> in_data = np.arange(1 * 3 * 2 * 2, dtype=np.float32)
            >>> tensor.set_data_from_numpy(in_data)
            >>> print(tensor)
            name: ,
            dtype: DataType.FLOAT32,
            shape: [1, 3, 2, 2],
            format: Format.NCHW,
            element_num: 12,
            data_size: 48.
        """
        if not isinstance(numpy_obj, numpy.ndarray):
            raise TypeError(f"numpy_obj must be numpy.ndarray, but got {type(numpy_obj)}.")
        if not numpy_obj.flags['FORC']:
            numpy_obj = numpy.ascontiguousarray(numpy_obj)
        if numpy_obj.dtype.type not in numpy_data_type_map:
            raise TypeError(f"Unsupported numpy dtype value {numpy_obj.dtype}")
        if numpy_data_type_map.get(numpy_obj.dtype.type) != self.dtype:
            raise RuntimeError(
                f"data type not equal! Numpy type: {numpy_obj.dtype.type}, Tensor type: {self.dtype}")
        if numpy_obj.nbytes != self.data_size:
            raise RuntimeError(
                f"data size not equal! Numpy size: {numpy_obj.nbytes}, Tensor size: {self.data_size}")
        self._tensor.set_data_from_numpy(numpy_obj)

    @property
    def device(self):
        """
        Get the device type of the Tensor.

        Returns:
            str, the device type of the Tensor.
        """
        return self._tensor.get_tensor_device_type()
