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
import pytest
import os
from tests.st.control.cases_register import case_register
from mindspore import context


@pytest.mark.level0
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_level0_ascend_cases():
    """
    Feature: control flow.
    Description: Execute all test cases with level0 and with device_target Ascend in one process.
    Expectation: All cases passed.
    """
    os.environ['ASCEND_GLOBAL_LOG_LEVEL'] = '3'
    context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")
    case_register.check_and_run("Ascend", 0)


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_level0_gpu_cases():
    """
    Feature: control flow.
    Description: Execute all test cases with level0 and with device_target GPU in one process.
    Expectation: All cases passed.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")
    case_register.check_and_run(f"GPU", 0)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu_training
@pytest.mark.env_onecard
def test_level0_cpu_cases():
    """
    Feature: control flow.
    Description: Execute all test cases with level0 and with device_target CPU in one process.
    Expectation: All cases passed.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="CPU")
    case_register.check_and_run("CPU", 0)


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_level1_ascend_cases():
    """
    Feature: control flow.
    Description: Execute all test cases with level1 and with device_target Ascend in one process.
    Expectation: All cases passed.
    """
    os.environ['ASCEND_GLOBAL_LOG_LEVEL'] = '3'
    context.set_context(mode=context.GRAPH_MODE, device_target="Ascend")
    case_register.check_and_run("Ascend", 1)


@pytest.mark.level1
@pytest.mark.platform_x86_gpu_training
@pytest.mark.env_onecard
def test_level1_gpu_cases():
    """
    Feature: control flow.
    Description: Execute all test cases with level1 and with device_target GPU in one process.
    Expectation: All cases passed.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="GPU")
    case_register.check_and_run("GPU", 1)


@pytest.mark.level1
@pytest.mark.platform_x86_cpu_training
@pytest.mark.env_onecard
def test_level1_cpu_cases():
    """
    Feature: control flow.
    Description: Execute all test cases with level1 and with device_target CPU in one process.
    Expectation: All cases passed.
    """
    context.set_context(mode=context.GRAPH_MODE, device_target="CPU")
    case_register.check_and_run("CPU", 1)
