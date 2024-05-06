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
Test ImageFolderDataset
"""
import os
import shutil
import numpy as np
import pytest
import mindspore.dataset as ds
import mindspore.dataset.transforms as data_trans
import mindspore.dataset.vision as vision
from mindspore import log as logger
from util import config_get_set_seed

DATA_DIR = "../data/dataset/testPK/data"
DATA_DIR_2 = "../data/dataset/testImageNetData2/train"
DATA_DIR_3 = "../data/dataset/testImageNetData2/encrypt_train"


def test_imagefolder_basic():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset basic read
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case basic")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    data1 = ds.ImageFolderDataset(DATA_DIR)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 44


def test_imagefolder_numsamples():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with num_samples parameter
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case numSamples")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    data1 = ds.ImageFolderDataset(DATA_DIR, num_samples=10, num_parallel_workers=2)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 10

    random_sampler = ds.RandomSampler(num_samples=3, replacement=True)
    data1 = ds.ImageFolderDataset(DATA_DIR, num_parallel_workers=2, sampler=random_sampler)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):
        num_iter += 1

    assert num_iter == 3

    random_sampler = ds.RandomSampler(num_samples=3, replacement=False)
    data1 = ds.ImageFolderDataset(DATA_DIR, num_parallel_workers=2, sampler=random_sampler)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):
        num_iter += 1

    assert num_iter == 3


def test_imagefolder_numshards():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with num_shards parameter
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case numShards")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    data1 = ds.ImageFolderDataset(DATA_DIR, num_shards=4, shard_id=3)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 11


def test_imagefolder_shardid():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with shard_id parameter
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case withShardID")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    data1 = ds.ImageFolderDataset(DATA_DIR, num_shards=4, shard_id=1)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 11


def test_imagefolder_noshuffle():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with no shuffle
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case noShuffle")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    data1 = ds.ImageFolderDataset(DATA_DIR, shuffle=False)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 44


def test_imagefolder_extrashuffle():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with extra shuffle
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case extraShuffle")
    # define parameters
    repeat_count = 2

    # apply dataset operations
    data1 = ds.ImageFolderDataset(DATA_DIR, shuffle=True)
    data1 = data1.shuffle(buffer_size=5)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 88


def test_imagefolder_classindex():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with class_indexing parameter
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case classIndex")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    class_index = {"class3": 333, "class1": 111}
    data1 = ds.ImageFolderDataset(DATA_DIR, class_indexing=class_index, shuffle=False)
    data1 = data1.repeat(repeat_count)

    golden = [111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
              333, 333, 333, 333, 333, 333, 333, 333, 333, 333, 333]

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1, output_numpy=True):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        assert item["label"] == golden[num_iter]
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 22


def test_imagefolder_negative_classindex():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with negative class_indexing parameter
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case negative classIndex")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    class_index = {"class3": -333, "class1": 111}
    data1 = ds.ImageFolderDataset(DATA_DIR, class_indexing=class_index, shuffle=False)
    data1 = data1.repeat(repeat_count)

    golden = [111, 111, 111, 111, 111, 111, 111, 111, 111, 111, 111,
              -333, -333, -333, -333, -333, -333, -333, -333, -333, -333, -333]

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1, output_numpy=True):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        assert item["label"] == golden[num_iter]
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 22


def test_imagefolder_extensions():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with extensions parameter
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case extensions")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    ext = [".jpg", ".JPEG"]
    data1 = ds.ImageFolderDataset(DATA_DIR, extensions=ext)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 44


def test_imagefolder_decode():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with decode parameter
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case decode")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    ext = [".jpg", ".JPEG"]
    data1 = ds.ImageFolderDataset(DATA_DIR, extensions=ext, decode=True)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 44


def test_sequential_sampler():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with SequentialSampler
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case SequentialSampler")

    golden = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
              1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
              2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
              3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3]

    # define parameters
    repeat_count = 1

    # apply dataset operations
    sampler = ds.SequentialSampler()
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)
    data1 = data1.repeat(repeat_count)

    result = []
    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1, output_numpy=True):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        result.append(item["label"])
        num_iter += 1

    assert num_iter == 44
    logger.info("Result: {}".format(result))
    assert result == golden


def test_random_sampler():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with RandomSampler
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case RandomSampler")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    sampler = ds.RandomSampler()
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 44


def test_distributed_sampler():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with DistributedSampler
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case DistributedSampler")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    sampler = ds.DistributedSampler(10, 1)
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 5


def test_pk_sampler():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with PKSampler
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case PKSampler")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    sampler = ds.PKSampler(3)
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 12


def test_subset_random_sampler():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with SubsetRandomSampler
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case SubsetRandomSampler")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    indices = [0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 11]
    sampler = ds.SubsetRandomSampler(indices)
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 12


def test_weighted_random_sampler():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with WeightedRandomSampler
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case WeightedRandomSampler")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    weights = [1.0, 0.1, 0.02, 0.3, 0.4, 0.05, 1.2, 0.13, 0.14, 0.015, 0.16, 1.1]
    sampler = ds.WeightedRandomSampler(weights, 11)
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 11


def test_weighted_random_sampler_exception():
    """
    Feature: ImageFolderDataset
    Description: Test error cases for ImageFolderDataset with WeightedRandomSampler
    Expectation: Correct error is thrown as expected
    """
    logger.info("Test error cases for WeightedRandomSampler")
    error_msg_1 = "type of weights element must be number"
    with pytest.raises(TypeError, match=error_msg_1):
        weights = ""
        ds.WeightedRandomSampler(weights)

    error_msg_2 = "type of weights element must be number"
    with pytest.raises(TypeError, match=error_msg_2):
        weights = (0.9, 0.8, 1.1)
        ds.WeightedRandomSampler(weights)

    error_msg_3 = "WeightedRandomSampler: weights vector must not be empty"
    with pytest.raises(RuntimeError, match=error_msg_3):
        weights = []
        sampler = ds.WeightedRandomSampler(weights)
        sampler.parse()

    error_msg_4 = "WeightedRandomSampler: weights vector must not contain negative numbers, got: "
    with pytest.raises(RuntimeError, match=error_msg_4):
        weights = [1.0, 0.1, 0.02, 0.3, -0.4]
        sampler = ds.WeightedRandomSampler(weights)
        sampler.parse()

    error_msg_5 = "WeightedRandomSampler: elements of weights vector must not be all zero"
    with pytest.raises(RuntimeError, match=error_msg_5):
        weights = [0, 0, 0, 0, 0]
        sampler = ds.WeightedRandomSampler(weights)
        sampler.parse()


def test_chained_sampler_01():
    """
    Feature: Chained Sampler
    Description: Chained Samplers: Random and Sequential, with repeat
    Expectation: Get correct number of data
    """
    logger.info("Test Case Chained Sampler - Random and Sequential, with repeat")

    # Create chained sampler, random and sequential
    sampler = ds.RandomSampler()
    child_sampler = ds.SequentialSampler()
    sampler.add_child(child_sampler)
    # Create ImageFolderDataset with sampler
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)

    data1 = data1.repeat(count=3)

    # Verify dataset size
    data1_size = data1.get_dataset_size()
    logger.info("dataset size is: {}".format(data1_size))
    assert data1_size == 132

    # Verify number of iterations
    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 132


def test_chained_sampler_02():
    """
    Feature: Chained Sampler
    Description: Chained Samplers: Random and Sequential, with batch then repeat
    Expectation: Get correct number of data
    """
    logger.info("Test Case Chained Sampler - Random and Sequential, with batch then repeat")

    # Create chained sampler, random and sequential
    sampler = ds.RandomSampler()
    child_sampler = ds.SequentialSampler()
    sampler.add_child(child_sampler)
    # Create ImageFolderDataset with sampler
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)

    data1 = data1.batch(batch_size=5, drop_remainder=True)
    data1 = data1.repeat(count=2)

    # Verify dataset size
    data1_size = data1.get_dataset_size()
    logger.info("dataset size is: {}".format(data1_size))
    assert data1_size == 16

    # Verify number of iterations
    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 16


def test_chained_sampler_03():
    """
    Feature: Chained Sampler
    Description: Chained Samplers: Random and Sequential, with repeat then batch
    Expectation: Get correct number of data
    """
    logger.info("Test Case Chained Sampler - Random and Sequential, with repeat then batch")

    # Create chained sampler, random and sequential
    sampler = ds.RandomSampler()
    child_sampler = ds.SequentialSampler()
    sampler.add_child(child_sampler)
    # Create ImageFolderDataset with sampler
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)

    data1 = data1.repeat(count=2)
    data1 = data1.batch(batch_size=5, drop_remainder=False)

    # Verify dataset size
    data1_size = data1.get_dataset_size()
    logger.info("dataset size is: {}".format(data1_size))
    assert data1_size == 18

    # Verify number of iterations
    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 18


def test_chained_sampler_04():
    """
    Feature: Chained Sampler
    Description: Chained Samplers: Distributed and Random, with batch then repeat
    Expectation: Get correct number of data
    """
    logger.info("Test Case Chained Sampler - Distributed and Random, with batch then repeat")

    # Create chained sampler, distributed and random
    sampler = ds.DistributedSampler(num_shards=4, shard_id=3)
    child_sampler = ds.RandomSampler()
    sampler.add_child(child_sampler)
    # Create ImageFolderDataset with sampler
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)

    data1 = data1.batch(batch_size=5, drop_remainder=True)
    data1 = data1.repeat(count=3)

    # Verify dataset size
    data1_size = data1.get_dataset_size()
    logger.info("dataset size is: {}".format(data1_size))
    assert data1_size == 6

    # Verify number of iterations
    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    # Note: Each of the 4 shards has 44/4=11 samples
    # Note: Number of iterations is (11/5 = 2) * 3 = 6
    assert num_iter == 6


def test_chained_sampler_05():
    """
    Feature: Chained Sampler
    Description: Chained Samplers: Distributed and WeightedRandom
    Expectation: Get correct number of data
    """
    logger.info("Test Case Chained Sampler - Distributed and WeightedRandom")

    # Create chained sampler, Distributed and WeightedRandom
    sampler = ds.DistributedSampler(num_shards=2, shard_id=1)
    weights = [1.0, 0.1, 0.02, 0.3, 0.4, 0.05, 1.2, 0.13, 0.14, 0.015, 0.16, 0.5]
    child_sampler = ds.WeightedRandomSampler(weights, num_samples=24)
    sampler.add_child(child_sampler)
    # Create ImageFolderDataset with sampler
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)

    # Verify dataset size
    data1_size = data1.get_dataset_size()
    logger.info("dataset size is: {}".format(data1_size))
    assert data1_size == 12

    # Verify number of iterations
    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    # Note: Child WeightedRandomSampler produces 24 samples
    # Note: DistributedSampler produces 24/2=12 samples
    assert num_iter == 12


def test_chained_sampler_06():
    """
    Feature: Chained Sampler
    Description: Chained Samplers: WeightedRandom and PKSampler
    Expectation: Get correct number of data
    """
    logger.info("Test Case Chained Sampler - WeightedRandom and PKSampler")

    # Create chained sampler, WeightedRandom and PKSampler
    weights = [1.0, 0.1, 0.02, 0.3, 0.4, 0.05, 1.2, 0.13, 0.14, 0.015, 0.16, 0.5]
    sampler = ds.WeightedRandomSampler(weights=weights, num_samples=12)
    child_sampler = ds.PKSampler(num_val=3)  # Number of elements per class is 3 (and there are 4 classes)
    sampler.add_child(child_sampler)
    # Create ImageFolderDataset with sampler
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)

    # Verify dataset size
    data1_size = data1.get_dataset_size()
    logger.info("dataset size is: {}".format(data1_size))
    assert data1_size == 12

    # Verify number of iterations
    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    # Note: WeightedRandomSampler produces 12 samples
    # Note: Child PKSampler produces 12 samples
    assert num_iter == 12


def test_chained_sampler_07():
    """
    Feature: Chained Sampler
    Description: Chained Samplers: SubsetRandom and Distributed, 2 shards
    Expectation: Get correct number of data
    """
    logger.info("Test Case Chained Sampler - SubsetRandom and Distributed, 2 shards")

    # Create chained sampler, subset random and distributed
    indices = [0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 11]
    sampler = ds.SubsetRandomSampler(indices, num_samples=12)
    child_sampler = ds.DistributedSampler(num_shards=2, shard_id=1)
    sampler.add_child(child_sampler)
    # Create ImageFolderDataset with sampler
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)

    # Verify dataset size
    data1_size = data1.get_dataset_size()
    logger.info("dataset size is: {}".format(data1_size))
    assert data1_size == 12

    # Verify number of iterations

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    # Note: SubsetRandomSampler produces 12 samples
    assert num_iter == 12


def test_chained_sampler_08():
    """
    Feature: Chained Sampler
    Description: Chained Samplers: SubsetRandom and Distributed, 4 shards
    Expectation: Get correct number of data
    """
    logger.info("Test Case Chained Sampler - SubsetRandom and Distributed, 4 shards")

    # Create chained sampler, subset random and distributed
    indices = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    sampler = ds.SubsetRandomSampler(indices, num_samples=11)
    child_sampler = ds.DistributedSampler(num_shards=4, shard_id=1)
    sampler.add_child(child_sampler)
    # Create ImageFolderDataset with sampler
    data1 = ds.ImageFolderDataset(DATA_DIR, sampler=sampler)

    # Verify dataset size
    data1_size = data1.get_dataset_size()
    logger.info("dataset size is: {}".format(data1_size))
    assert data1_size == 11

    # Verify number of iterations
    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    # Note: SubsetRandomSampler returns 11 samples
    assert num_iter == 11


def test_imagefolder_rename():
    """
    Feature: ImageFolderDataset
    Description: Test rename on ImageFolderDataset
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case rename")
    # define parameters
    repeat_count = 1

    # apply dataset operations
    data1 = ds.ImageFolderDataset(DATA_DIR, num_samples=10)
    data1 = data1.repeat(repeat_count)

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 10

    data1 = data1.rename(input_columns=["image"], output_columns="image2")

    num_iter = 0
    for item in data1.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image2"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 10


def test_imagefolder_zip():
    """
    Feature: ImageFolderDataset
    Description: Test zip on ImageFolderDataset
    Expectation: The dataset is processed as expected
    """
    logger.info("Test Case zip")
    # define parameters
    repeat_count = 2

    # apply dataset operations
    data1 = ds.ImageFolderDataset(DATA_DIR, num_samples=10)
    data2 = ds.ImageFolderDataset(DATA_DIR, num_samples=10)

    data1 = data1.repeat(repeat_count)
    # rename dataset2 for no conflict
    data2 = data2.rename(input_columns=["image", "label"], output_columns=["image1", "label1"])
    data3 = ds.zip((data1, data2))

    num_iter = 0
    for item in data3.create_dict_iterator(num_epochs=1):  # each data is a dictionary
        # in this example, each dictionary has keys "image" and "label"
        logger.info("image is {}".format(item["image"]))
        logger.info("label is {}".format(item["label"]))
        num_iter += 1

    logger.info("Number of data in data1: {}".format(num_iter))
    assert num_iter == 10


def test_imagefolder_exception():
    """
    Feature: ImageFolderDataset
    Description: Test error cases on ImageFolderDataset
    Expectation: Correct error is thrown as expected
    """
    logger.info("Test imagefolder exception")

    def exception_func(item):
        raise Exception("Error occur!")

    def exception_func2(image, label):
        raise Exception("Error occur!")

    try:
        data = ds.ImageFolderDataset(DATA_DIR)
        data = data.map(operations=exception_func, input_columns=["image"], num_parallel_workers=1)
        for _ in data.__iter__():
            pass
        assert False
    except RuntimeError as e:
        assert "map operation: [PyFunc] failed. The corresponding data file is" in str(e)

    try:
        data = ds.ImageFolderDataset(DATA_DIR)
        data = data.map(operations=exception_func2, input_columns=["image", "label"],
                        output_columns=["image", "label", "label1"],
                        num_parallel_workers=1)
        for _ in data.__iter__():
            pass
        assert False
    except RuntimeError as e:
        assert "map operation: [PyFunc] failed. The corresponding data file is" in str(e)

    try:
        data = ds.ImageFolderDataset(DATA_DIR)
        data = data.map(operations=vision.Decode(), input_columns=["image"], num_parallel_workers=1)
        data = data.map(operations=exception_func, input_columns=["image"], num_parallel_workers=1)
        for _ in data.__iter__():
            pass
        assert False
    except RuntimeError as e:
        assert "map operation: [PyFunc] failed. The corresponding data file is" in str(e)

    data_dir_invalid = "../data/dataset/testPK"
    try:
        data = ds.ImageFolderDataset(data_dir_invalid)
        for _ in data.__iter__():
            pass
        assert False
    except RuntimeError as e:
        assert "should be file, but got directory" in str(e)


def encrypt_func():
    """
    Feature: Encrypt function
    Description: Encrypt and save the image
    Expectation: Success
    """
    plain_dir = os.path.realpath(DATA_DIR_2)
    cipher_dir = os.path.realpath(DATA_DIR_3)

    for root, _, files in os.walk(plain_dir):
        for f in files:
            fn = os.path.join(root, f)

            enc_file = os.path.join(cipher_dir, os.path.relpath(fn, plain_dir))
            os.makedirs(os.path.dirname(enc_file), exist_ok=True)

            with open(fn, 'rb')as f:
                content = f.read()

            new_content = b'helloworld' + content

            with open(enc_file, 'wb')as f:
                f.write(new_content)


def decrypt_func(cipher_file):
    """
    Feature: Decrypt function
    Description: Decrypt encrypted image data
    Expectation: Decryption is successful, return bytes type data
    """
    with open(cipher_file, 'rb')as f:
        content = f.read()
        new_content = content[10:]
    return new_content


def test_imagefolder_decrypt():
    """
    Feature: Test imagefolder decrypt
    Description: Support decrypting encrypted image data
    Expectation: Success
    """
    logger.info("Test imagefolder decrypt")

    encrypt_func()

    resize_height = 224
    resize_width = 224

    # Create dataset and define map operations
    ds1 = ds.ImageFolderDataset(DATA_DIR_3, decrypt=decrypt_func)

    num_classes = 3
    decode_op = vision.Decode()
    resize_op = vision.Resize((resize_height, resize_width), vision.Inter.LINEAR)
    one_hot_encode = data_trans.OneHot(num_classes)  # num_classes is input argument

    ds1 = ds1.map(operations=decode_op, input_columns=["image"])
    ds1 = ds1.map(operations=resize_op, input_columns=["image"])
    ds1 = ds1.map(operations=one_hot_encode, input_columns=["label"])

    # apply batch operations
    batch_size = 3
    ds1 = ds1.batch(batch_size, drop_remainder=True)

    ds2 = ds1
    alpha = 0.2
    transforms = [vision.MixUp(batch_size=batch_size, alpha=alpha, is_single=False)
                  ]
    ds1 = ds1.map(operations=transforms, input_columns=["image", "label"])
    num_iter = 0
    batch1_image1 = 0
    for data1, data2 in zip(ds1.create_dict_iterator(num_epochs=1, output_numpy=True),
                            ds2.create_dict_iterator(num_epochs=1, output_numpy=True)):
        image1 = data1["image"]
        label1 = data1["label"]
        logger.info("label: {}".format(label1))

        image2 = data2["image"]
        label2 = data2["label"]
        logger.info("label2: {}".format(label2))

        if num_iter == 0:
            batch1_image1 = image1

        if num_iter == 1:
            lam = np.abs(label2 - label1)
            logger.info("lam value in multi: {}".format(lam))
            for index in range(batch_size):
                if np.square(lam[index]).mean() != 0:
                    lam_value = 1 - np.sum(lam[index]) / 2
                    img_golden = lam_value * image2[index] + (1 - lam_value) * batch1_image1[index]
                    assert image1[index].all() == img_golden.all()
                    logger.info("====test several batch mixup ok====")
            break
        num_iter += 1

    if os.path.exists(DATA_DIR_3):
        shutil.rmtree(DATA_DIR_3)


def test_imagefolder_error_sample_sourceop():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with sample errors. Include just source op in pipeline.
    Expectation: The dataset is processed as expected.
    """

    def test_config(my_seed, my_error_sample_data_file, my_total_samples):
        # Set configuration since default Random Sampler is used
        original_seed = config_get_set_seed(my_seed)

        # For ImageFolderDataset, use decode=False
        data1 = ds.ImageFolderDataset(my_error_sample_data_file, num_samples=None, decode=False)
        count = 0
        for _ in data1.create_dict_iterator(num_epochs=1, output_numpy=True):
            count += 1
        assert count == my_total_samples

        # For ImageFolderDataset, use decode=True
        data2 = ds.ImageFolderDataset(my_error_sample_data_file, num_samples=None, decode=True)
        with pytest.raises(RuntimeError) as error_info:
            for _ in data2.create_dict_iterator(num_epochs=1, output_numpy=True):
                pass
        assert "Invalid image" in str(error_info.value)

        # Restore configuration
        ds.config.set_seed(original_seed)

    # Test empty sample
    test_config(2, "../data/dataset/testImageNetError/Sample1_empty/train", 3)
    # Test corrupt sample
    test_config(3, "../data/dataset/testImageNetError/Sample2_corrupt1mid/train", 6)
    # Test text sample, instead of image sample
    test_config(1, "../data/dataset/testImageNetError/Sample3_text/train", 3)


def test_imagefolder_error_sample_mapop():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset with sample errors. Include map op in pipeline.
    Expectation: The dataset is processed as expected.
        Note: The invalid JPEG file is detected.
    """

    def test_config(my_seed, my_error_sample_data_file):
        # Set configuration since default Random Sampler is used
        original_seed = config_get_set_seed(my_seed)

        # For ImageFolderDataset, use decode default (of False)
        data3 = ds.ImageFolderDataset(my_error_sample_data_file, num_samples=None)
        # Add map op to the pipeline. Use C++ implemented ops
        data3 = data3.map(operations=[vision.Decode(),
                                      vision.HorizontalFlip()],
                          input_columns=["image"], num_parallel_workers=1)
        with pytest.raises(RuntimeError) as error_info:
            for _ in data3.create_dict_iterator(num_epochs=1, output_numpy=True):
                pass
        assert "map operation: [Decode] failed" in str(error_info.value)

        # For ImageFolderDataset, use decode default (of False)
        data4 = ds.ImageFolderDataset(my_error_sample_data_file, num_samples=None)
        # Add map op to the pipeline. Use Python implemented ops
        data4 = data4.map(operations=[vision.Decode(to_pil=True),
                                      vision.RandomHorizontalFlip(0.7)],
                          input_columns=["image"], num_parallel_workers=1)
        with pytest.raises(RuntimeError) as error_info:
            for _ in data4.create_dict_iterator(num_epochs=1, output_numpy=True):
                pass
        assert "map operation: [PyFunc] failed" in str(error_info.value)

        # Restore configuration
        ds.config.set_seed(original_seed)

    # Test empty sample
    test_config(2, "../data/dataset/testImageNetError/Sample1_empty/train")
    # Test corrupt sample
    test_config(3, "../data/dataset/testImageNetError/Sample2_corrupt1mid/train")
    # Test text sample, instead of image sample
    test_config(1, "../data/dataset/testImageNetError/Sample3_text/train")


def test_imagefolder_classindexing():
    """
    Feature: ImageFolderDataset
    Description: Test ImageFolderDataset get class indexing
    Expectation: The dataset is processed as expected
    """
    # define ImageFolderDataset with different class_indexing
    data1 = ds.ImageFolderDataset(DATA_DIR, class_indexing=None)
    index1 = data1.get_class_indexing()
    assert index1 == {'class1': 0, 'class2': 1, 'class3': 2, 'class4': 3}

    data2 = ds.ImageFolderDataset(DATA_DIR, class_indexing={"class1": 4, "class2": 0})
    data2 = data2.repeat(2)
    index2 = data2.get_class_indexing()
    assert index2 == {'class1': 4, 'class2': 0}


if __name__ == '__main__':
    test_imagefolder_basic()
    test_imagefolder_numsamples()
    test_sequential_sampler()
    test_random_sampler()
    test_distributed_sampler()
    test_pk_sampler()
    test_subset_random_sampler()
    test_weighted_random_sampler()
    test_weighted_random_sampler_exception()
    test_chained_sampler_01()
    test_chained_sampler_02()
    test_chained_sampler_03()
    test_chained_sampler_04()
    test_chained_sampler_05()
    test_chained_sampler_06()
    test_chained_sampler_07()
    test_chained_sampler_08()
    test_imagefolder_numshards()
    test_imagefolder_shardid()
    test_imagefolder_noshuffle()
    test_imagefolder_extrashuffle()
    test_imagefolder_classindex()
    test_imagefolder_negative_classindex()
    test_imagefolder_extensions()
    test_imagefolder_decode()
    test_imagefolder_rename()
    test_imagefolder_zip()
    test_imagefolder_exception()
    test_imagefolder_decrypt()
    test_imagefolder_error_sample_sourceop()
    test_imagefolder_error_sample_mapop()
    test_imagefolder_classindexing()
