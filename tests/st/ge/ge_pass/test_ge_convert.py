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
import tests.st.ge.ge_test_utils as utils


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_convert_return():
    """
    Feature: convert ge graph
    Description: test Return node
    Expectation: success
    """
    utils.run_testcase('ge_convert', 'test_convert_return')


@pytest.mark.level0
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_convert_update_state():
    """
    Feature: convert ge graph
    Description: test UpdateState node
    Expectation: success
    """
    utils.run_testcase('ge_convert', 'test_convert_update_state')


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_convert_load():
    """
    Feature: convert ge graph
    Description: test Load node
    Expectation: success
    """
    utils.run_testcase('ge_convert', 'test_convert_load')


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_convert_make_tuple():
    """
    Feature: convert ge graph
    Description: test MakeTuple node
    Expectation: success
    """
    utils.run_testcase('ge_convert', 'test_convert_make_tuple')


@pytest.mark.level1
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_convert_tuple_get_item():
    """
    Feature: convert ge graph
    Description: test TupleGetItem node
    Expectation: success
    """
    utils.run_testcase('ge_convert', 'test_convert_tuple_get_item')


@pytest.mark.level0
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
def test_convert_make_tuple_make_tuple():
    """
    Feature: convert ge graph
    Description: test MakeTuple's input is MakeTuple
    Expectation: success
    """
    utils.run_testcase('ge_convert', 'test_convert_make_tuple_make_tuple')


def test_convert_tuple_get_item_dynamic_output():
    """
    Feature: convert ge graph
    Description: test TupleGetItem's input is dynamic output
    Expectation: success
    """
    utils.run_testcase('ge_convert', 'test_convert_tuple_get_item_dynamic_output')
