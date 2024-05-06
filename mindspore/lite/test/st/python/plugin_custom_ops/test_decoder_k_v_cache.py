# Copyright 2023 Huawei Technologies Co., Ltd
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
Test DecoderKVCache plugin custom ops.
"""
import os
import numpy as np
import mindspore_lite as mslite
import mindspore.nn as nn
import mindspore.ops as ops
from mindspore import Tensor, context
from mindspore.train.serialization import export
from mindspore.ops.auto_generate import DecoderKVCache


b = 26
h = 40
s = 32
d = 128
us = 1
ps = s - us

is_4d = True


class DecoderKVCacheNet(nn.Cell):
    """
    DecoderKVCacheNet.
    """

    def __init__(self):
        super().__init__()
        self.add = ops.Add()
        self.sub = ops.Sub()
        self.decoder_k_v_cache = DecoderKVCache()

    def construct(self, cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len):
        out = self.decoder_k_v_cache(cache, update, valid_seq_len, batch_index, seq_len_axis,
                                     new_max_seq_len, cur_max_seq_len)
        add_out = self.add(cache, 1)
        sub_out = self.sub(add_out, 1)
        return sub_out


def np_inference(cache, update, valid_seq_len):
    """
    np_inference
    """
    cache_rank = len(cache.shape)
    ans = cache.copy()
    if cache_rank == 4:
        s_ = cache.shape[2]
        us_ = update.shape[2]
    elif cache_rank == 3:
        s_ = cache.shape[1]
        us_ = update.shape[1]
    for b_idx in range(cache.shape[0]):
        s_idx = valid_seq_len[b_idx]
        if s_idx < 0:
            continue
        if s_idx + us_ > s_:
            s_idx %= s_
        if is_4d:
            ans[b_idx, :, s_idx, :] = update[b_idx, :, 0, :]
        else:
            ans[b_idx, s_idx, :] = update[b_idx, 0, :]
    return ans


def generate_random_array(max_num: int, block_size: int, size: int) -> np.ndarray:
    arr = np.zeros(size, dtype=int)
    for i in range(size):
        while True:
            arr[i] = np.random.randint(low=-1, high=max_num)
            if np.count_nonzero(arr[:i] % block_size == arr[i] % block_size) == 0:
                break
    return arr


def create_numpy_inputs():
    """
    create inputs
    """
    global is_4d
    if is_4d:
        cache_shape = (b, h, s, d)
        update_shape = (b, h, us, d)
    else:
        cache_shape = (b, s, h * d)
        update_shape = (b, us, h * d)
    cache = np.random.rand(*cache_shape).astype(np.float16)
    update = np.random.rand(*update_shape).astype(np.float16)
    valid_seq_len = generate_random_array(4*s, s, b).astype(np.int64)
    batch_index = np.array([1]).astype(np.int64)
    seq_len_axis = np.array([2]).astype(np.int64)
    new_max_seq_len = np.array([s]).astype(np.int64)
    cur_max_seq_len = np.array([s]).astype(np.int64)
    return (cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len)


def create_ms_inputs():
    """
    create inputs
    """
    cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len = create_numpy_inputs()
    ms_cache = Tensor(cache)
    ms_update = Tensor(update)
    ms_valid_seq_len = Tensor(valid_seq_len)
    ms_batch_index = Tensor(batch_index)
    ms_seq_len_axis = Tensor(seq_len_axis)
    ms_new_max_seq_len = Tensor(new_max_seq_len)
    ms_cur_max_seq_len = Tensor(cur_max_seq_len)
    return (ms_cache, ms_update, ms_valid_seq_len, ms_batch_index, ms_seq_len_axis,
            ms_new_max_seq_len, ms_cur_max_seq_len)


def create_lite_inputs(cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len):
    """
    create_lite_inputs
    """
    cache = mslite.Tensor(cache)
    update = mslite.Tensor(update)
    valid_seq_len = mslite.Tensor(valid_seq_len)
    batch_index = mslite.Tensor(batch_index)
    seq_len_axis = mslite.Tensor(seq_len_axis)
    new_max_seq_len = mslite.Tensor(new_max_seq_len)
    cur_max_seq_len = mslite.Tensor(cur_max_seq_len)
    return (cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len)


def inference_decoder_k_v_cache(mindir_model):
    """
    inference model
    """

    lite_ctx1 = mslite.Context()
    lite_ctx1.target = ["ascend"]
    lite_ctx1.ascend.device_id = 0
    lite_ctx1.ascend.provider = "ge"

    model = mslite.Model()
    model.build_from_file(
        mindir_model, mslite.ModelType.MINDIR, lite_ctx1, "", {})

    for i in range(50):
        cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, \
            cur_max_seq_len = create_numpy_inputs()
        input_lists = list(create_lite_inputs(cache, update, valid_seq_len, batch_index, seq_len_axis,
                                              new_max_seq_len, cur_max_seq_len))
        mslite_output = model.predict(input_lists)
        expect_output = np_inference(cache, update, valid_seq_len)
        assert np.allclose(
            mslite_output[0].get_data_to_numpy(), expect_output, 0.001, 0.001)
        print(f"decoder_k_v_cache st {i} times: inference success.")


def export_decoder_k_v_cache_model():
    """
    export model
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="CPU")
    cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len = create_ms_inputs()

    net = DecoderKVCacheNet()
    if is_4d:
        file_name = "decoder_k_v_cache_primitive_4d"
    else:
        file_name = "decoder_k_v_cache_primitive_3d"

    export(net, cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len,
           file_name=file_name, file_format='MINDIR')
    model_name = file_name + ".mindir"
    assert os.path.exists(model_name)
    return model_name


def test_4d():
    global is_4d
    is_4d = True
    model_path = export_decoder_k_v_cache_model()
    print("decoder_k_v_cache 4d st : export success path: ", model_path)

    inference_decoder_k_v_cache(model_path)
    print(f"decoder_k_v_cache 4d st : inference end.")

def test_3d():
    global is_4d
    is_4d = False
    model_path = export_decoder_k_v_cache_model()
    print("decoder_k_v_cache 3d st : export success path: ", model_path)

    inference_decoder_k_v_cache(model_path)
    print(f"decoder_k_v_cache 3d st : inference end.")


if __name__ == '__main__':
    test_3d()
    test_4d()
