# Copyright 2019-2022 Huawei Technologies Co., Ltd
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
# ==============================================================================
"""
This is the test module for mindrecord
"""
import os
import pytest
import numpy as np

import mindspore.dataset as ds
from mindspore import log as logger
from mindspore.mindrecord import FileWriter
from util import config_get_set_seed

FILES_NUM = 4
CV_DIR_NAME = "../data/mindrecord/testImageNetData"

@pytest.fixture
def add_and_remove_cv_file():
    """add/remove cv file"""
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    paths = ["{}{}".format(file_name, str(x).rjust(1, '0'))
             for x in range(FILES_NUM)]
    try:
        for x in paths:
            if os.path.exists("{}".format(x)):
                os.remove("{}".format(x))
            if os.path.exists("{}.db".format(x)):
                os.remove("{}.db".format(x))
        writer = FileWriter(file_name, FILES_NUM)
        data = get_data(CV_DIR_NAME, True)
        cv_schema_json = {"id": {"type": "int32"},
                          "file_name": {"type": "string"},
                          "label": {"type": "int32"},
                          "data": {"type": "bytes"}}
        writer.add_schema(cv_schema_json, "img_schema")
        writer.add_index(["file_name", "label"])
        writer.write_raw_data(data)
        writer.commit()
        yield "yield_cv_data"
    except Exception as error:
        for x in paths:
            os.remove("{}".format(x))
            os.remove("{}.db".format(x))
        raise error
    else:
        for x in paths:
            os.remove("{}".format(x))
            os.remove("{}.db".format(x))


def test_cv_minddataset_pk_sample_no_column(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with PKSampler without any columns_list in the dataset
    Expectation: Output is equal to the expected output
    """
    num_readers = 4
    sampler = ds.PKSampler(2)
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    data_set = ds.MindDataset(file_name + "0", None, num_readers,
                              sampler=sampler)

    assert data_set.get_dataset_size() == 6
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info("-------------- item[file_name]: \
                {}------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1


def test_cv_minddataset_pk_sample_basic(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test basic read MindDataset with PKSampler
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    sampler = ds.PKSampler(2)
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)

    assert data_set.get_dataset_size() == 6
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info("-------------- item[data]: \
                {}------------------------".format(item["data"][:10]))
        logger.info("-------------- item[file_name]: \
                {}------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1


def test_cv_minddataset_pk_sample_shuffle(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with PKSampler with shuffle=True
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    sampler = ds.PKSampler(3, None, True)
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)

    assert data_set.get_dataset_size() == 9
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info("-------------- item[file_name]: \
                {}------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
    assert num_iter == 9


def test_cv_minddataset_pk_sample_shuffle_1(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with PKSampler with shuffle=True and
        with num_samples larger than get_dataset_size
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    sampler = ds.PKSampler(3, None, True, 'label', 5)
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)

    assert data_set.get_dataset_size() == 5
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info("-------------- item[file_name]: \
                {}------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
    assert num_iter == 5


def test_cv_minddataset_pk_sample_shuffle_2(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with PKSampler with shuffle=True and
        with num_samples larger than get_dataset_size
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    sampler = ds.PKSampler(3, None, True, 'label', 10)
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)

    assert data_set.get_dataset_size() == 9
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info("-------------- item[file_name]: \
                {}------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
    assert num_iter == 9


def test_cv_minddataset_pk_sample_out_of_range_0(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with PKSampler with shuffle=True and num_val that is out of range
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    sampler = ds.PKSampler(5, None, True)
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)
    assert data_set.get_dataset_size() == 15
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info("-------------- item[file_name]: \
                {}------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
    assert num_iter == 15


def test_cv_minddataset_pk_sample_out_of_range_1(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with PKSampler with shuffle=True, num_val that is out of range, and
        num_samples larger than get_dataset_size
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    sampler = ds.PKSampler(5, None, True, 'label', 20)
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)
    assert data_set.get_dataset_size() == 15
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info("-------------- item[file_name]: \
                {}------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
    assert num_iter == 15


def test_cv_minddataset_pk_sample_out_of_range_2(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with PKSampler with shuffle=True, num_val that is out of range, and
        num_samples that is equal to get_dataset_size
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    sampler = ds.PKSampler(5, None, True, 'label', 10)
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)
    assert data_set.get_dataset_size() == 10
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info("-------------- item[file_name]: \
                {}------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
    assert num_iter == 10


def test_cv_minddataset_subset_random_sample_basic(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test basic read MindDataset with SubsetRandomSampler
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    indices = [1, 2, 3, 5, 7]
    samplers = (ds.SubsetRandomSampler(indices), ds.SubsetSampler(indices))
    for sampler in samplers:
        data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                                  sampler=sampler)
        assert data_set.get_dataset_size() == 5
        num_iter = 0
        for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
            logger.info(
                "-------------- cv reader basic: {} ------------------------".format(num_iter))
            logger.info(
                "-------------- item[data]: {}  -----------------------------".format(item["data"]))
            logger.info(
                "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
            logger.info(
                "-------------- item[label]: {} ----------------------------".format(item["label"]))
            num_iter += 1
        assert num_iter == 5


def test_cv_minddataset_subset_random_sample_replica(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with SubsetRandomSampler with duplicate index in the indices
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    indices = [1, 2, 2, 5, 7, 9]
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    samplers = ds.SubsetRandomSampler(indices), ds.SubsetSampler(indices)
    for sampler in samplers:
        data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                                  sampler=sampler)
        assert data_set.get_dataset_size() == 6
        num_iter = 0
        for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
            logger.info(
                "-------------- cv reader basic: {} ------------------------".format(num_iter))
            logger.info(
                "-------------- item[data]: {}  -----------------------------".format(item["data"]))
            logger.info(
                "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
            logger.info(
                "-------------- item[label]: {} ----------------------------".format(item["label"]))
            num_iter += 1
        assert num_iter == 6


def test_cv_minddataset_subset_random_sample_empty(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with SubsetRandomSampler with empty indices
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    indices = []
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    samplers = ds.SubsetRandomSampler(indices), ds.SubsetSampler(indices)
    for sampler in samplers:
        data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                                  sampler=sampler)
        assert data_set.get_dataset_size() == 0
        num_iter = 0
        for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
            logger.info(
                "-------------- cv reader basic: {} ------------------------".format(num_iter))
            logger.info(
                "-------------- item[data]: {}  -----------------------------".format(item["data"]))
            logger.info(
                "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
            logger.info(
                "-------------- item[label]: {} ----------------------------".format(item["label"]))
            num_iter += 1
        assert num_iter == 0


def test_cv_minddataset_subset_random_sample_out_of_range(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with SubsetRandomSampler with indices that are out of range
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    indices = [1, 2, 4, 11, 13]
    samplers = ds.SubsetRandomSampler(indices), ds.SubsetSampler(indices)
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    for sampler in samplers:
        data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                                  sampler=sampler)
        assert data_set.get_dataset_size() == 5
        num_iter = 0
        for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
            logger.info(
                "-------------- cv reader basic: {} ------------------------".format(num_iter))
            logger.info(
                "-------------- item[data]: {}  -----------------------------".format(item["data"]))
            logger.info(
                "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
            logger.info(
                "-------------- item[label]: {} ----------------------------".format(item["label"]))
            num_iter += 1
        assert num_iter == 5


def test_cv_minddataset_subset_random_sample_negative(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with SubsetRandomSampler with negative indices
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    indices = [1, 2, 4, -1, -2]
    samplers = ds.SubsetRandomSampler(indices), ds.SubsetSampler(indices)
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    for sampler in samplers:
        data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                                  sampler=sampler)
        assert data_set.get_dataset_size() == 5
        num_iter = 0
        for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
            logger.info(
                "-------------- cv reader basic: {} ------------------------".format(num_iter))
            logger.info(
                "-------------- item[data]: {}  -----------------------------".format(item["data"]))
            logger.info(
                "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
            logger.info(
                "-------------- item[label]: {} ----------------------------".format(item["label"]))
            num_iter += 1
        assert num_iter == 5


def test_cv_minddataset_random_sampler_basic(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test basic read MindDataset with RandomSampler
    Expectation: Output is equal to the expected output
    """
    data = get_data(CV_DIR_NAME, True)
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    sampler = ds.RandomSampler()
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)
    assert data_set.get_dataset_size() == 10
    num_iter = 0
    new_dataset = []
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
        new_dataset.append(item['file_name'])
    assert num_iter == 10
    assert new_dataset != [x['file_name'] for x in data]


def test_cv_minddataset_random_sampler_repeat(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with RandomSampler followed by Repeat op
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    sampler = ds.RandomSampler()
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)
    assert data_set.get_dataset_size() == 10
    ds1 = data_set.repeat(3)
    num_iter = 0
    epoch1_dataset = []
    epoch2_dataset = []
    epoch3_dataset = []
    for item in ds1.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
        if num_iter <= 10:
            epoch1_dataset.append(item['file_name'])
        elif num_iter <= 20:
            epoch2_dataset.append(item['file_name'])
        else:
            epoch3_dataset.append(item['file_name'])
    assert num_iter == 30
    assert epoch1_dataset not in (epoch2_dataset, epoch3_dataset)
    assert epoch2_dataset not in (epoch1_dataset, epoch3_dataset)
    assert epoch3_dataset not in (epoch1_dataset, epoch2_dataset)


def test_cv_minddataset_random_sampler_replacement(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with RandomSampler with replacement=True
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    sampler = ds.RandomSampler(replacement=True, num_samples=5)
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)
    assert data_set.get_dataset_size() == 5
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
    assert num_iter == 5


def test_cv_minddataset_random_sampler_replacement_false_1(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with RandomSampler with replacement=False and num_samples <= dataset size
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    sampler = ds.RandomSampler(replacement=False, num_samples=2)
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)
    assert data_set.get_dataset_size() == 2
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
    assert num_iter == 2


def test_cv_minddataset_random_sampler_replacement_false_2(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with RandomSampler with replacement=False and num_samples > dataset size
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    sampler = ds.RandomSampler(replacement=False, num_samples=20)
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)
    assert data_set.get_dataset_size() == 10
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
    assert num_iter == 10


def test_cv_minddataset_sequential_sampler_basic(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test basic read MindDataset with SequentialSampler
    Expectation: Output is equal to the expected output
    """
    data = get_data(CV_DIR_NAME, True)
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    sampler = ds.SequentialSampler(1, 4)
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)
    assert data_set.get_dataset_size() == 4
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        assert item['file_name'] == np.array(data[num_iter + 1]['file_name'])
        num_iter += 1
    assert num_iter == 4


def test_cv_minddataset_sequential_sampler_offeset(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with SequentialSampler with offset on starting index
    Expectation: Output is equal to the expected output
    """
    data = get_data(CV_DIR_NAME, True)
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    sampler = ds.SequentialSampler(2, 10)
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)
    dataset_size = data_set.get_dataset_size()
    assert dataset_size == 10
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        assert item['file_name'] == np.array(data[(num_iter + 2) % dataset_size]['file_name'])
        num_iter += 1
    assert num_iter == 10


def test_cv_minddataset_sequential_sampler_exceed_size(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with SequentialSampler with offset on starting index and
        num_samples > dataset size
    Expectation: Output is equal to the expected output
    """
    data = get_data(CV_DIR_NAME, True)
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    sampler = ds.SequentialSampler(2, 20)
    data_set = ds.MindDataset(file_name + "0", columns_list, num_readers,
                              sampler=sampler)
    dataset_size = data_set.get_dataset_size()
    assert dataset_size == 10
    num_iter = 0
    for item in data_set.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- cv reader basic: {} ------------------------".format(num_iter))
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        assert item['file_name'] == np.array(data[(num_iter + 2) % dataset_size]['file_name'])
        num_iter += 1
    assert num_iter == 10


def test_cv_minddataset_split_basic(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test basic read MindDataset after Split op is applied
    Expectation: Output is equal to the expected output
    """
    data = get_data(CV_DIR_NAME, True)
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    d = ds.MindDataset(file_name + "0", columns_list,
                       num_readers, shuffle=False)
    d1, d2 = d.split([8, 2], randomize=False)
    assert d.get_dataset_size() == 10
    assert d1.get_dataset_size() == 8
    assert d2.get_dataset_size() == 2
    num_iter = 0
    for item in d1.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        assert item['file_name'] == np.array(data[num_iter]['file_name'])
        num_iter += 1
    assert num_iter == 8
    num_iter = 0
    for item in d2.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        assert item['file_name'] == np.array(data[num_iter + 8]['file_name'])
        num_iter += 1
    assert num_iter == 2


def test_cv_minddataset_split_exact_percent(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset after Split op is applied using exact percentages
    Expectation: Output is equal to the expected output
    """
    data = get_data(CV_DIR_NAME, True)
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    d = ds.MindDataset(file_name + "0", columns_list,
                       num_readers, shuffle=False)
    d1, d2 = d.split([0.8, 0.2], randomize=False)
    assert d.get_dataset_size() == 10
    assert d1.get_dataset_size() == 8
    assert d2.get_dataset_size() == 2
    num_iter = 0
    for item in d1.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        assert item['file_name'] == np.array(data[num_iter]['file_name'])
        num_iter += 1
    assert num_iter == 8
    num_iter = 0
    for item in d2.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        assert item['file_name'] == np.array(data[num_iter + 8]['file_name'])
        num_iter += 1
    assert num_iter == 2


def test_cv_minddataset_split_fuzzy_percent(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset after Split op is applied using fuzzy percentages
    Expectation: Output is equal to the expected output
    """
    data = get_data(CV_DIR_NAME, True)
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    d = ds.MindDataset(file_name + "0", columns_list,
                       num_readers, shuffle=False)
    d1, d2 = d.split([0.41, 0.59], randomize=False)
    assert d.get_dataset_size() == 10
    assert d1.get_dataset_size() == 4
    assert d2.get_dataset_size() == 6
    num_iter = 0
    for item in d1.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        assert item['file_name'] == np.array(data[num_iter]['file_name'])
        num_iter += 1
    assert num_iter == 4
    num_iter = 0
    for item in d2.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        assert item['file_name'] == np.array(data[num_iter + 4]['file_name'])
        num_iter += 1
    assert num_iter == 6


def test_cv_minddataset_split_deterministic(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset after deterministic Split op is applied
    Expectation: Output is equal to the expected output
    """
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    d = ds.MindDataset(file_name + "0", columns_list,
                       num_readers, shuffle=False)
    # should set seed to avoid data overlap
    original_seed = config_get_set_seed(111)
    d1, d2 = d.split([0.8, 0.2])
    assert d.get_dataset_size() == 10
    assert d1.get_dataset_size() == 8
    assert d2.get_dataset_size() == 2

    d1_dataset = []
    d2_dataset = []
    num_iter = 0
    for item in d1.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        d1_dataset.append(item['file_name'])
        num_iter += 1
    assert num_iter == 8
    num_iter = 0
    for item in d2.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        d2_dataset.append(item['file_name'])
        num_iter += 1
    assert num_iter == 2
    inter_dataset = [x for x in d1_dataset if x in d2_dataset]
    assert inter_dataset == []  # intersection of  d1 and d2
    ds.config.set_seed(original_seed)


def test_cv_minddataset_split_sharding(add_and_remove_cv_file):
    """
    Feature: MindDataset
    Description: Test read MindDataset with DistributedSampler after deterministic Split op is applied
    Expectation: Output is equal to the expected output
    """
    data = get_data(CV_DIR_NAME, True)
    columns_list = ["data", "file_name", "label"]
    num_readers = 4
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]
    d = ds.MindDataset(file_name + "0", columns_list,
                       num_readers, shuffle=False)
    # should set seed to avoid data overlap
    original_seed = config_get_set_seed(111)
    d1, d2 = d.split([0.8, 0.2])
    assert d.get_dataset_size() == 10
    assert d1.get_dataset_size() == 8
    assert d2.get_dataset_size() == 2
    distributed_sampler = ds.DistributedSampler(2, 0)
    d1.use_sampler(distributed_sampler)
    assert d1.get_dataset_size() == 4

    num_iter = 0
    d1_shard1 = []
    for item in d1.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
        d1_shard1.append(item['file_name'])
    assert num_iter == 4
    assert d1_shard1 != [x['file_name'] for x in data[0:4]]

    distributed_sampler = ds.DistributedSampler(2, 1)
    d1.use_sampler(distributed_sampler)
    assert d1.get_dataset_size() == 4

    d1s = d1.repeat(3)
    epoch1_dataset = []
    epoch2_dataset = []
    epoch3_dataset = []
    num_iter = 0
    for item in d1s.create_dict_iterator(num_epochs=1, output_numpy=True):
        logger.info(
            "-------------- item[data]: {}  -----------------------------".format(item["data"]))
        logger.info(
            "-------------- item[file_name]: {} ------------------------".format(item["file_name"]))
        logger.info(
            "-------------- item[label]: {} ----------------------------".format(item["label"]))
        num_iter += 1
        if num_iter <= 4:
            epoch1_dataset.append(item['file_name'])
        elif num_iter <= 8:
            epoch2_dataset.append(item['file_name'])
        else:
            epoch3_dataset.append(item['file_name'])
    assert len(epoch1_dataset) == 4
    assert len(epoch2_dataset) == 4
    assert len(epoch3_dataset) == 4
    inter_dataset = [x for x in d1_shard1 if x in epoch1_dataset]
    assert inter_dataset == []  # intersection of d1's shard1 and d1's shard2
    assert epoch1_dataset not in (epoch2_dataset, epoch3_dataset)
    assert epoch2_dataset not in (epoch1_dataset, epoch3_dataset)
    assert epoch3_dataset not in (epoch1_dataset, epoch2_dataset)

    epoch1_dataset.sort()
    epoch2_dataset.sort()
    epoch3_dataset.sort()
    assert epoch1_dataset != epoch2_dataset
    assert epoch2_dataset != epoch3_dataset
    assert epoch3_dataset != epoch1_dataset

    ds.config.set_seed(original_seed)


def get_data(dir_name, sampler=False):
    """
    usage: get data from imagenet dataset
    params:
    dir_name: directory containing folder images and annotation information

    """
    if not os.path.isdir(dir_name):
        raise IOError("Directory {} not exists".format(dir_name))
    img_dir = os.path.join(dir_name, "images")
    if sampler:
        ann_file = os.path.join(dir_name, "annotation_sampler.txt")
    else:
        ann_file = os.path.join(dir_name, "annotation.txt")
    with open(ann_file, "r") as file_reader:
        lines = file_reader.readlines()

    data_list = []
    for i, line in enumerate(lines):
        try:
            filename, label = line.split(",")
            label = label.strip("\n")
            with open(os.path.join(img_dir, filename), "rb") as file_reader:
                img = file_reader.read()
            data_json = {"id": i,
                         "file_name": filename,
                         "data": img,
                         "label": int(label)}
            data_list.append(data_json)
        except FileNotFoundError:
            continue
    return data_list


def check_pksampler(file_name, col_type):
    """check the PKSampler with type string, int, float"""
    if os.path.exists("{}".format(file_name)):
        os.remove("{}".format(file_name))
    if os.path.exists("{}.db".format(file_name)):
        os.remove("{}.db".format(file_name))

    if col_type == "string":
        schema_json = {"file_name": {"type": "string"}, "label": {"type": "string"}, "data": {"type": "bytes"}}
    elif col_type == "int32":
        schema_json = {"file_name": {"type": "string"}, "label": {"type": "int32"}, "data": {"type": "bytes"}}
    elif col_type == "int64":
        schema_json = {"file_name": {"type": "string"}, "label": {"type": "int64"}, "data": {"type": "bytes"}}
    elif col_type == "float32":
        schema_json = {"file_name": {"type": "string"}, "label": {"type": "float32"}, "data": {"type": "bytes"}}
    elif col_type == "float64":
        schema_json = {"file_name": {"type": "string"}, "label": {"type": "float64"}, "data": {"type": "bytes"}}
    else:
        raise RuntimeError("Parameter {} error".format(col_type))


    writer = FileWriter(file_name=file_name, shard_num=1, overwrite=True)
    _ = writer.add_schema(schema_json, "test_schema")
    indexes = ["file_name", "label"]
    _ = writer.add_index(indexes)
    for i in range(1000):
        if col_type == "string":
            data = [{"file_name": str(i) + ".jpg", "label": str(int(i / 100)),
                     "data": b"\x10c\xb3w\xa8\xee$o&<q\x8c\x8e(\xa2\x90\x90\x96\xbc\xb1\x1e\xd4QER\x13?\xff"}]
        elif col_type == "int32" or col_type == "int64":
            data = [{"file_name": str(i) + ".jpg", "label": int(i / 100),
                     "data": b"\x10c\xb3w\xa8\xee$o&<q\x8c\x8e(\xa2\x90\x90\x96\xbc\xb1\x1e\xd4QER\x13?\xff"}]
        elif col_type == "float32" or col_type == "float64":
            data = [{"file_name": str(i) + ".jpg", "label": float(int(i / 100) + 0.5),
                     "data": b"\x10c\xb3w\xa8\xee$o&<q\x8c\x8e(\xa2\x90\x90\x96\xbc\xb1\x1e\xd4QER\x13?\xff"}]
        else:
            raise RuntimeError("Parameter {} error".format(col_type))
        _ = writer.write_raw_data(data)
    _ = writer.commit()

    sampler = ds.PKSampler(5, class_column='label')
    data_set = ds.MindDataset(dataset_files=file_name, sampler=sampler)
    assert data_set.get_dataset_size() == 50

    count = 0
    for item in data_set.create_dict_iterator(output_numpy=True):
        print("item name:", item["label"].dtype, item["label"])
        if col_type == "string":
            assert item["label"].dtype == np.array("9").dtype
        elif col_type == "int32":
            assert item["label"].dtype == np.int32
        elif col_type == "int64":
            assert item["label"].dtype == np.int64
        elif col_type == "float32":
            assert item["label"].dtype == np.float32
        elif col_type == "float64":
            assert item["label"].dtype == np.float64
        else:
            raise RuntimeError("Parameter {} error".format(col_type))
        count += 1
    assert count == 50

    if os.path.exists("{}".format(file_name)):
        os.remove("{}".format(file_name))
    if os.path.exists("{}.db".format(file_name)):
        os.remove("{}.db".format(file_name))


def test_cv_minddataset_pksampler_with_diff_type():
    """
    Feature: MindDataset
    Description: Test read MindDataset with PKSampler and use string, int, float type
    Expectation: Output is equal to the expected output
    """
    file_name = os.environ.get('PYTEST_CURRENT_TEST').split(':')[-1].split(' ')[0]

    check_pksampler(file_name, "string")
    check_pksampler(file_name, "int32")
    check_pksampler(file_name, "int64")
    check_pksampler(file_name, "float32")
    check_pksampler(file_name, "float64")


if __name__ == '__main__':
    test_cv_minddataset_pk_sample_no_column(add_and_remove_cv_file)
    test_cv_minddataset_pk_sample_basic(add_and_remove_cv_file)
    test_cv_minddataset_pk_sample_shuffle(add_and_remove_cv_file)
    test_cv_minddataset_pk_sample_out_of_range(add_and_remove_cv_file)
    test_cv_minddataset_subset_random_sample_basic(add_and_remove_cv_file)
    test_cv_minddataset_subset_random_sample_replica(add_and_remove_cv_file)
    test_cv_minddataset_subset_random_sample_empty(add_and_remove_cv_file)
    test_cv_minddataset_subset_random_sample_out_of_range(add_and_remove_cv_file)
    test_cv_minddataset_subset_random_sample_negative(add_and_remove_cv_file)
    test_cv_minddataset_random_sampler_basic(add_and_remove_cv_file)
    test_cv_minddataset_random_sampler_repeat(add_and_remove_cv_file)
    test_cv_minddataset_random_sampler_replacement(add_and_remove_cv_file)
    test_cv_minddataset_sequential_sampler_basic(add_and_remove_cv_file)
    test_cv_minddataset_sequential_sampler_exceed_size(add_and_remove_cv_file)
    test_cv_minddataset_split_basic(add_and_remove_cv_file)
    test_cv_minddataset_split_exact_percent(add_and_remove_cv_file)
    test_cv_minddataset_split_fuzzy_percent(add_and_remove_cv_file)
    test_cv_minddataset_split_deterministic(add_and_remove_cv_file)
    test_cv_minddataset_split_sharding(add_and_remove_cv_file)
    test_cv_minddataset_pksampler_with_diff_type()
