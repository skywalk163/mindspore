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
"""Cell of auto parallel"""
from __future__ import absolute_import
from __future__ import division

from mindspore.nn.cell import Cell
from mindspore.ops import operations as P
from mindspore.ops.operations.comm_ops import AllGather
from mindspore.communication import GlobalComm
from mindspore.common import jit

_ALLGATHER_CELL = None


class AllGatherCell(Cell):
    """
    Allgather cell, used in model parallel scenario.
    To allgather the selected parameter slice from each device.
    """
    def __init__(self, group, do_reshape, after_reshape_slice_shape):
        super(AllGatherCell, self).__init__(auto_prefix=False)
        self.allgather = AllGather(group)
        self.do_reshape = do_reshape
        self.after_reshape_slice_shape = tuple(after_reshape_slice_shape)
        self.add_flags(skip_auto_parallel_compile=True)

    @jit()
    def construct(self, x):
        if self.do_reshape:
            x = P.Reshape()(x, self.after_reshape_slice_shape)
        x = self.allgather(x)
        return x


class SaveOptShardCkptCell(Cell):
    """
    Allgather cell, used in optimizer parallel scenario.
    Firstly gather the tensor to original layout in the specified device group.
    Then gather the whole parameter slices from all devices.

    Note:
        This could be optimized later with less communication consumption.
    """
    def __init__(self, group, do_reshape, after_reshape_slice_shape):
        super(SaveOptShardCkptCell, self).__init__(auto_prefix=False)
        self.allgather1 = AllGather(group)
        self.allgather2 = AllGather()
        self.do_reshape = do_reshape
        self.after_reshape_slice_shape = tuple(after_reshape_slice_shape)
        self.add_flags(skip_auto_parallel_compile=True)

    def construct(self, x):
        x = self.allgather1(x)
        if self.do_reshape:
            x = P.Reshape()(x, self.after_reshape_slice_shape)
        x = self.allgather2(x)

        return x


def get_allgather_cell(group, need_merge_twice=False, do_reshape=False, after_reshape_slice_shape=()):
    """Get AllGatherCell object."""
    global _ALLGATHER_CELL
    if need_merge_twice:
        _ALLGATHER_CELL = SaveOptShardCkptCell(group, do_reshape, after_reshape_slice_shape)
    else:
        if group:
            _ALLGATHER_CELL = AllGatherCell(group, do_reshape, after_reshape_slice_shape)
        else:
            _ALLGATHER_CELL = AllGatherCell(GlobalComm.WORLD_COMM_GROUP, do_reshape, after_reshape_slice_shape)
    return _ALLGATHER_CELL


def destroy_allgather_cell():
    """Destroy AllGatherCell object."""
    global _ALLGATHER_CELL
    if _ALLGATHER_CELL:
        _ALLGATHER_CELL = None
