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

import pytest
import numpy as np
import mindspore as ms
from mindspore.nn import Cell
from mindspore import context, Tensor, Parameter
from mindspore.ops.auto_generate import DecoderKVCache
from parallel.utils.utils import ParallelValidator, compile_net


def setup_function():
    context.set_auto_parallel_context(dataset_strategy="full_batch")

class DecoderKVCacheNet(Cell):
    def __init__(self, strategy):
        super(DecoderKVCacheNet, self).__init__()
        self.decoder_k_v_cache = DecoderKVCache().shard(strategy)

    def construct(self, cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len):
        return self.decoder_k_v_cache(cache, update, valid_seq_len, batch_index, seq_len_axis,
                                      new_max_seq_len, cur_max_seq_len)


def test_decoder_k_v_cache_net_4dims():
    """
    Feature: test decoder_k_v_cache auto parallel 4dims
    Description: auto parallel
    Expectation: shape is as expected.
    """
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel", device_num=8, global_rank=0)
    strategy = ((4, 1, 1, 1,), (4, 1, 1, 1,), (4,), (1,), (1,), (1,), (1,))
    net = DecoderKVCacheNet(strategy)
    cache = Parameter(Tensor(np.ones([4, 40, 1024, 128]), dtype=ms.float16), "cache")
    update = Parameter(Tensor(np.ones([4, 40, 1, 128]), dtype=ms.float16), "update")
    valid_seq_len = Parameter(Tensor(np.ones([4]), dtype=ms.int64), "valid_seq_len")
    batch_index = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "batch_index")
    seq_len_axis = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "seq_len_axis")
    new_max_seq_len = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "new_max_seq_len")
    cur_max_seq_len = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "cur_max_seq_len")
    net.set_inputs(cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len)

    phase = compile_net(net, cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len)
    validator = ParallelValidator(net, phase)
    assert validator.check_parameter_shape('cache', [1, 40, 1024, 128])
    assert validator.check_parameter_shape('update', [1, 40, 1, 128])
    assert validator.check_parameter_shape('valid_seq_len', [1])


def test_decoder_k_v_cache_net_3dims():
    """
    Feature: test decoder_k_v_cache auto parallel 3dims
    Description: auto parallel
    Expectation: shape is as expected.
    """
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel", device_num=8, global_rank=0)
    strategy = ((4, 1, 1,), (4, 1, 1,), (4,), (1,), (1,), (1,), (1,))
    net = DecoderKVCacheNet(strategy)
    cache = Parameter(Tensor(np.ones([4, 1024, 5120]), dtype=ms.float16), "cache")
    update = Parameter(Tensor(np.ones([4, 1, 5120]), dtype=ms.float16), "update")
    valid_seq_len = Parameter(Tensor(np.ones([4]), dtype=ms.int64), "valid_seq_len")
    batch_index = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "batch_index")
    seq_len_axis = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "seq_len_axis")
    new_max_seq_len = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "new_max_seq_len")
    cur_max_seq_len = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "cur_max_seq_len")
    net.set_inputs(cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len)

    phase = compile_net(net, cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len)
    validator = ParallelValidator(net, phase)
    assert validator.check_parameter_shape('cache', [1, 1024, 5120])
    assert validator.check_parameter_shape('update', [1, 1, 5120])
    assert validator.check_parameter_shape('valid_seq_len', [1])


def test_decoder_k_v_cache_net_3dims_seq_len():
    """
    Feature: test decoder_k_v_cache auto parallel 3dims
    Description: auto parallel
    Expectation: shape is as expected.
    """
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel", device_num=8, global_rank=0)
    strategy = ((1, 1, 4,), (1, 1, 4,), (1,), (1,), (1,), (1,), (1,))
    net = DecoderKVCacheNet(strategy)
    cache = Parameter(Tensor(np.ones([4, 1024, 5120]), dtype=ms.float16), "cache")
    update = Parameter(Tensor(np.ones([4, 1, 5120]), dtype=ms.float16), "update")
    valid_seq_len = Parameter(Tensor(np.ones([4]), dtype=ms.int64), "valid_seq_len")
    batch_index = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "batch_index")
    seq_len_axis = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "seq_len_axis")
    new_max_seq_len = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "new_max_seq_len")
    cur_max_seq_len = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "cur_max_seq_len")
    net.set_inputs(cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len)

    phase = compile_net(net, cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len)
    validator = ParallelValidator(net, phase)
    assert validator.check_parameter_shape('cache', [4, 1024, 1280])
    assert validator.check_parameter_shape('update', [4, 1, 1280])


def test_decoder_k_v_cache_strategy_error():
    """
    Feature: test invalid strategy for DecoderKVCache
    Description: illegal strategy
    Expectation: raise RuntimeError
    """
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel", device_num=8, global_rank=0)
    strategy = ((4, 1, 1, 1,), (4, 2, 1, 1,), (4,), (1,), (1,), (1,), (1,))
    net = DecoderKVCacheNet(strategy)
    cache = Parameter(Tensor(np.ones([4, 1024, 5120]), dtype=ms.float16), "cache")
    update = Parameter(Tensor(np.ones([4, 1, 5120]), dtype=ms.float16), "update")
    valid_seq_len = Parameter(Tensor(np.ones([4]), dtype=ms.int64), "valid_seq_len")
    batch_index = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "batch_index")
    seq_len_axis = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "seq_len_axis")
    new_max_seq_len = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "new_max_seq_len")
    cur_max_seq_len = Parameter(Tensor(np.ones([1]), dtype=ms.int64), "cur_max_seq_len")
    with pytest.raises(RuntimeError):
        compile_net(net, cache, update, valid_seq_len, batch_index, seq_len_axis, new_max_seq_len, cur_max_seq_len)
