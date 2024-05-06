# Copyright 2022 Huawei Technologies Co., Ltd
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

import numpy as np
import pytest

import mindspore as ms
import mindspore.nn as nn
import mindspore.ops as ops
from mindspore import Tensor


class MarginRankingLoss(nn.Cell):
    def __init__(self, reduction):
        super(MarginRankingLoss, self).__init__()
        self.reduction = reduction

    def construct(self, x, y, label, margin):
        return ops.margin_ranking_loss(x, y, label, margin, reduction=self.reduction)


@pytest.mark.parametrize('reduction', ["none", "mean", "sum"])
def test_margin_ranking_loss(reduction):
    """
    Feature: test MarginRankingLoss op with reduction none.
    Description: Verify the result of MarginRankingLoss.
    Expectation: expect correct forward result.
    """
    loss = MarginRankingLoss(reduction)
    input1 = Tensor(np.array([0.3864, -2.4093, -1.4076]), ms.float32)
    input2 = Tensor(np.array([-0.6012, -1.6681, 1.2928]), ms.float32)
    target = Tensor(np.array([-1, -1, 1]), ms.float32)
    loss(input1, input2, target, 0.0)
