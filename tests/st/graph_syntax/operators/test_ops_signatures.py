# Copyright 2024 Huawei Technologies Co., Ltd
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
""" test graph syntax """
import pytest
import torch
import mindspore as ms


ms_data = {
    "bool": ms.Tensor(1, dtype=ms.bool_),
    "int8": ms.Tensor(1, dtype=ms.int8),
    "int16": ms.Tensor(1, dtype=ms.int16),
    "int32": ms.Tensor(1, dtype=ms.int32),
    "int64": ms.Tensor(1, dtype=ms.int64),
    "uint8": ms.Tensor(1, dtype=ms.uint8),
    "float16": ms.Tensor(1, dtype=ms.float16),
    "float32": ms.Tensor(1, dtype=ms.float32),
    "float64": ms.Tensor(1, dtype=ms.float64),
    "bfloat16": ms.Tensor(1, dtype=ms.bfloat16),
}


# torch has no uint16, uint32, uint64.
torch_data = {
    "bool": torch.tensor(1, dtype=torch.bool),
    "int8": torch.tensor(1, dtype=torch.int8),
    "int16": torch.tensor(1, dtype=torch.int16),
    "int32": torch.tensor(1, dtype=torch.int32),
    "int64": torch.tensor(1, dtype=torch.int64),
    "uint8": torch.tensor(1, dtype=torch.uint8),
    "float16": torch.tensor(1, dtype=torch.float16),
    "float32": torch.tensor(1, dtype=torch.float32),
    "float64": torch.tensor(1, dtype=torch.float64),
    "bfloat16": torch.tensor(1, dtype=torch.bfloat16),
}


def get_dtype(x):
    if x.dtype in (ms.bfloat16, torch.bfloat16):
        return "bfloat16"
    if isinstance(x, (ms.Tensor, torch.Tensor)):
        return x.numpy().dtype
    return x.dtype


@pytest.mark.level0
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_tensor_add_tensor_compare(mode):
    """
    Feature: Implicit type conversion.
    Description: Prioritize matching torch.
    Expectation: No exception.
    """
    class Net(ms.nn.Cell):
        def construct(self, x, y):
            return x + y

    ms.context.set_context(mode=mode)
    for key1 in ms_data.keys():
        for key2 in ms_data.keys():
            if key1 == "bool" and key2 == "bool":
                continue
            out_ms = Net()(ms_data[key1], ms_data[key2])
            out_torch = torch_data[key1] + torch_data[key2]
            dtype_ms = get_dtype(out_ms)
            dtype_torch = get_dtype(out_torch)
            assert dtype_ms == dtype_torch, f"{key1} + {key2} = {dtype_ms}, expect {dtype_torch}"


@pytest.mark.level0
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [ms.GRAPH_MODE, ms.PYNATIVE_MODE])
def test_tensor_add_scalar_compare(mode):
    """
    Feature: Implicit type conversion.
    Description: Prioritize matching torch.
    Expectation: No exception.
    """
    class Net(ms.nn.Cell):
        def construct(self, x, y):
            return x + y

    ms.context.set_context(mode=mode)
    num_list = [True, 2, -2, 2.0, -2.0]
    num_dtype_list = ["bool", "positive_int", "negative_int", "positive_float", "negative_float"]
    for key in ms_data.keys():
        for num, num_dtype in zip(num_list, num_dtype_list):
            if key == "bool" and num_dtype == "bool":
                continue
            out_ms = Net()(ms_data[key], num)
            out_torch = torch_data[key] + num
            dtype_ms = get_dtype(out_ms)
            dtype_torch = get_dtype(out_torch)
            assert dtype_ms == dtype_torch, f"{key} + {num_dtype} = {dtype_ms}, expect {dtype_torch}"
