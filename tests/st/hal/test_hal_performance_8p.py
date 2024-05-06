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
import os
import pytest


@pytest.mark.level2
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_single
def test_pynative_hccl_allreduce_8p():
    """
    Feature: 'msrun' launch utility.
    Description: Launch distributed training job with dynamic cluster using msrun.
    Expectation: All workers are successfully spawned and running training.
    """
    return_code = os.system(
        "msrun --worker_num=8 --local_worker_num=8 --master_port=10969 test_hal_pynative_allreduce_net.py"
    )
    assert return_code == 0
