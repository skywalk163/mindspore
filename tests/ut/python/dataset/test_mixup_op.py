# Copyright 2020-2022 Huawei Technologies Co., Ltd
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
Testing the MixUpBatch op in DE
"""
import numpy as np
import pytest
import mindspore.dataset as ds
import mindspore.dataset.vision as vision
import mindspore.dataset.transforms as transforms
from mindspore import log as logger
from util import save_and_check_md5, diff_mse, visualize_list, config_get_set_seed, \
    config_get_set_num_parallel_workers

DATA_DIR = "../data/dataset/testCifar10Data"
DATA_DIR2 = "../data/dataset/testImageNetData2/train/"
DATA_DIR3 = "../data/dataset/testCelebAData/"

GENERATE_GOLDEN = False


def test_mixup_batch_success1(plot=False):
    """
    Feature: MixUpBatch op
    Description: Test MixUpBatch op with specified alpha parameter
    Expectation: Output is the same as expected output
    """
    logger.info("test_mixup_batch_success1")

    # Original Images
    ds_original = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)
    ds_original = ds_original.batch(5, drop_remainder=True)

    images_original = None
    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = image.asnumpy()
        else:
            images_original = np.append(images_original, image.asnumpy(), axis=0)

    # MixUp Images
    data1 = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)

    one_hot_op = transforms.OneHot(num_classes=10)
    data1 = data1.map(operations=one_hot_op, input_columns=["label"])
    mixup_batch_op = vision.MixUpBatch(2)
    data1 = data1.batch(5, drop_remainder=True)
    data1 = data1.map(operations=mixup_batch_op, input_columns=["image", "label"])

    images_mixup = None
    for idx, (image, _) in enumerate(data1):
        if idx == 0:
            images_mixup = image.asnumpy()
        else:
            images_mixup = np.append(images_mixup, image.asnumpy(), axis=0)
    if plot:
        visualize_list(images_original, images_mixup)

    num_samples = images_original.shape[0]
    mse = np.zeros(num_samples)
    for i in range(num_samples):
        mse[i] = diff_mse(images_mixup[i], images_original[i])
    logger.info("MSE= {}".format(str(np.mean(mse))))


def test_mixup_batch_success2(plot=False):
    """
    Feature: MixUpBatch op
    Description: Test MixUpBatch op with specified alpha parameter on ImageFolderDataset
    Expectation: Output is the same as expected output
    """
    logger.info("test_mixup_batch_success2")

    # Original Images
    ds_original = ds.ImageFolderDataset(dataset_dir=DATA_DIR2, shuffle=False)
    decode_op = vision.Decode()
    ds_original = ds_original.map(operations=[decode_op], input_columns=["image"])
    ds_original = ds_original.batch(4, drop_remainder=True)

    images_original = None
    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = image.asnumpy()
        else:
            images_original = np.append(images_original, image.asnumpy(), axis=0)

    # MixUp Images
    data1 = ds.ImageFolderDataset(dataset_dir=DATA_DIR2, shuffle=False)

    decode_op = vision.Decode()
    data1 = data1.map(operations=[decode_op], input_columns=["image"])

    one_hot_op = transforms.OneHot(num_classes=10)
    data1 = data1.map(operations=one_hot_op, input_columns=["label"])

    mixup_batch_op = vision.MixUpBatch(2.0)
    data1 = data1.batch(4, drop_remainder=True)
    data1 = data1.map(operations=mixup_batch_op, input_columns=["image", "label"])

    images_mixup = None
    for idx, (image, _) in enumerate(data1):
        if idx == 0:
            images_mixup = image.asnumpy()
        else:
            images_mixup = np.append(images_mixup, image.asnumpy(), axis=0)
    if plot:
        visualize_list(images_original, images_mixup)

    num_samples = images_original.shape[0]
    mse = np.zeros(num_samples)
    for i in range(num_samples):
        mse[i] = diff_mse(images_mixup[i], images_original[i])
    logger.info("MSE= {}".format(str(np.mean(mse))))


def test_mixup_batch_success3(plot=False):
    """
    Feature: MixUpBatch op
    Description: Test MixUpBatch op without specified alpha parameter (alpha parameter will be selected by default)
    Expectation: Output is the same as expected output
    """
    logger.info("test_mixup_batch_success3")

    # Original Images
    ds_original = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)
    ds_original = ds_original.batch(5, drop_remainder=True)

    images_original = None
    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = image.asnumpy()
        else:
            images_original = np.append(images_original, image.asnumpy(), axis=0)

    # MixUp Images
    data1 = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)

    one_hot_op = transforms.OneHot(num_classes=10)
    data1 = data1.map(operations=one_hot_op, input_columns=["label"])
    mixup_batch_op = vision.MixUpBatch()
    data1 = data1.batch(5, drop_remainder=True)
    data1 = data1.map(operations=mixup_batch_op, input_columns=["image", "label"])

    images_mixup = np.array([])
    for idx, (image, _) in enumerate(data1):
        if idx == 0:
            images_mixup = image.asnumpy()
        else:
            images_mixup = np.append(images_mixup, image.asnumpy(), axis=0)
    if plot:
        visualize_list(images_original, images_mixup)

    num_samples = images_original.shape[0]
    mse = np.zeros(num_samples)
    for i in range(num_samples):
        mse[i] = diff_mse(images_mixup[i], images_original[i])
    logger.info("MSE= {}".format(str(np.mean(mse))))


def test_mixup_batch_success4(plot=False):
    """
    Feature: MixUpBatch op
    Description: Test MixUpBatch op on dataset where OneHot returns a 2D vector (alpha parameter selected by default)
    Expectation: Output is the same as expected output
    """
    logger.info("test_mixup_batch_success4")

    # Original Images
    ds_original = ds.CelebADataset(DATA_DIR3, shuffle=False)
    decode_op = vision.Decode()
    ds_original = ds_original.map(operations=[decode_op], input_columns=["image"])
    ds_original = ds_original.batch(2, drop_remainder=True)

    images_original = None
    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = image.asnumpy()
        else:
            images_original = np.append(images_original, image.asnumpy(), axis=0)

    # MixUp Images
    data1 = ds.CelebADataset(DATA_DIR3, shuffle=False)

    decode_op = vision.Decode()
    data1 = data1.map(operations=[decode_op], input_columns=["image"])

    one_hot_op = transforms.OneHot(num_classes=100)
    data1 = data1.map(operations=one_hot_op, input_columns=["attr"])

    mixup_batch_op = vision.MixUpBatch()
    data1 = data1.batch(2, drop_remainder=True)
    data1 = data1.map(operations=mixup_batch_op, input_columns=["image", "attr"])

    images_mixup = np.array([])
    for idx, (image, _) in enumerate(data1):
        if idx == 0:
            images_mixup = image.asnumpy()
        else:
            images_mixup = np.append(images_mixup, image.asnumpy(), axis=0)
    if plot:
        visualize_list(images_original, images_mixup)

    num_samples = images_original.shape[0]
    mse = np.zeros(num_samples)
    for i in range(num_samples):
        mse[i] = diff_mse(images_mixup[i], images_original[i])
    logger.info("MSE= {}".format(str(np.mean(mse))))


def test_mixup_batch_md5():
    """
    Feature: MixUpBatch op
    Description: Test MixUpBatch op with md5
    Expectation: Passes the md5 check test
    """
    logger.info("test_mixup_batch_md5")
    original_seed = config_get_set_seed(0)
    original_num_parallel_workers = config_get_set_num_parallel_workers(1)

    # MixUp Images
    data = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)

    one_hot_op = transforms.OneHot(num_classes=10)
    data = data.map(operations=one_hot_op, input_columns=["label"])
    mixup_batch_op = vision.MixUpBatch()
    data = data.batch(5, drop_remainder=True)
    data = data.map(operations=mixup_batch_op, input_columns=["image", "label"])

    filename = "mixup_batch_c_result.npz"
    save_and_check_md5(data, filename, generate_golden=GENERATE_GOLDEN)

    # Restore config setting
    ds.config.set_seed(original_seed)
    ds.config.set_num_parallel_workers(original_num_parallel_workers)


def test_mixup_batch_float_label():
    """
    Feature: MixUpBatch
    Description: Test MixUpBatch with label in type of float
    Expectation: Output is as expected
    """
    original_seed = config_get_set_seed(0)

    image = np.random.randint(0, 255, (3, 28, 28, 1), dtype=np.uint8)
    label = np.random.randint(0, 5, (3, 1))
    decode_label = transforms.OneHot(5)(label)
    float_label = transforms.TypeCast(float)(decode_label)
    _, mix_label = vision.MixUpBatch()(image, float_label)
    expected_label = np.array([[0., 0.8252811, 0., 0., 0.1747189],
                               [0., 0., 0., 0., 1.],
                               [0., 0.1747189, 0., 0., 0.8252811]])
    np.testing.assert_almost_equal(mix_label, expected_label)

    ds.config.set_seed(original_seed)


def test_mixup_batch_then_cut_mix_batch():
    """
    Feature: MixUpBatch
    Description: Test MixUpBatch called twice
    Expectation: Output is as expected
    """
    original_seed = config_get_set_seed(1)

    dataset = ds.Cifar10Dataset(DATA_DIR, num_samples=3, shuffle=False)
    one_hot = transforms.OneHot(num_classes=10)
    dataset = dataset.map(operations=one_hot, input_columns=["label"])
    mix_up_batch = vision.MixUpBatch()
    cut_mix_batch = vision.CutMixBatch(vision.ImageBatchFormat.NHWC, 2.0, 0.5)
    dataset = dataset.batch(3, drop_remainder=False)
    dataset = dataset.map(operations=mix_up_batch, input_columns=["image", "label"])
    dataset = dataset.map(operations=cut_mix_batch, input_columns=["image", "label"])

    expected_label = np.array([[0.5112224, 0.3333217, 0.1554559, 0., 0., 0., 0., 0., 0., 0.],
                               [0.2947904, 0.7052096, 0., 0., 0., 0., 0., 0., 0., 0.],
                               [0., 0.2947904, 0.7052096, 0., 0., 0., 0., 0., 0., 0.]])
    for item in dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        np.testing.assert_almost_equal(item["label"], expected_label)

    ds.config.set_seed(original_seed)


def test_mixup_batch_fail1():
    """
    Feature: MixUpBatch op
    Description: Test MixUpBatch op with images and labels that are not batched
    Expectation: Error is raised as expected
    """
    logger.info("test_mixup_batch_fail1")

    # Original Images
    ds_original = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)
    ds_original = ds_original.batch(5)

    images_original = np.array([])
    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = image.asnumpy()
        else:
            images_original = np.append(images_original, image.asnumpy(), axis=0)

    # MixUp Images
    data1 = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)

    one_hot_op = transforms.OneHot(num_classes=10)
    data1 = data1.map(operations=one_hot_op, input_columns=["label"])
    mixup_batch_op = vision.MixUpBatch(0.1)
    with pytest.raises(RuntimeError) as error:
        data1 = data1.map(operations=mixup_batch_op, input_columns=["image", "label"])
        for idx, (image, _) in enumerate(data1):
            if idx == 0:
                images_mixup = image.asnumpy()
            else:
                images_mixup = np.append(images_mixup, image.asnumpy(), axis=0)
        error_message = "You must make sure images are HWC or CHW and batched"
        assert error_message in str(error.value)


def test_mixup_batch_fail2():
    """
    Feature: MixUpBatch op
    Description: Test MixUpBatch op with negative alpha parameter
    Expectation: Error is raised as expected
    """
    logger.info("test_mixup_batch_fail2")

    # Original Images
    ds_original = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)
    ds_original = ds_original.batch(5)

    images_original = np.array([])
    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = image.asnumpy()
        else:
            images_original = np.append(images_original, image.asnumpy(), axis=0)

    # MixUp Images
    data1 = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)

    one_hot_op = transforms.OneHot(num_classes=10)
    data1 = data1.map(operations=one_hot_op, input_columns=["label"])
    with pytest.raises(ValueError) as error:
        vision.MixUpBatch(-1)
        error_message = "Input is not within the required interval"
        assert error_message in str(error.value)


def test_mixup_batch_fail3():
    """
    Feature: MixUpBatch op
    Description: Test MixUpBatch op where label column is not passed
    Expectation: Error is raised as expected
    """
    logger.info("test_mixup_batch_fail3")
    # Original Images
    ds_original = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)
    ds_original = ds_original.batch(5, drop_remainder=True)

    images_original = None
    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = image.asnumpy()
        else:
            images_original = np.append(images_original, image.asnumpy(), axis=0)

    # MixUp Images
    data1 = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)

    one_hot_op = transforms.OneHot(num_classes=10)
    data1 = data1.map(operations=one_hot_op, input_columns=["label"])
    mixup_batch_op = vision.MixUpBatch()
    data1 = data1.batch(5, drop_remainder=True)
    data1 = data1.map(operations=mixup_batch_op, input_columns=["image"])

    with pytest.raises(RuntimeError) as error:
        images_mixup = np.array([])
        for idx, (image, _) in enumerate(data1):
            if idx == 0:
                images_mixup = image.asnumpy()
            else:
                images_mixup = np.append(images_mixup, image.asnumpy(), axis=0)
    error_message = "size of input data should be 2 (including images or labels)"
    assert error_message in str(error.value)


def test_mixup_batch_fail4():
    """
    Feature: MixUpBatch op
    Description: Test MixUpBatch op where alpha parameter is 0
    Expectation: Error is raised as expected
    """
    logger.info("test_mixup_batch_fail4")

    # Original Images
    ds_original = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)
    ds_original = ds_original.batch(5)

    images_original = np.array([])
    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = image.asnumpy()
        else:
            images_original = np.append(images_original, image.asnumpy(), axis=0)

    # MixUp Images
    data1 = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)

    one_hot_op = transforms.OneHot(num_classes=10)
    data1 = data1.map(operations=one_hot_op, input_columns=["label"])
    with pytest.raises(ValueError) as error:
        vision.MixUpBatch(0.0)
        error_message = "Input is not within the required interval"
        assert error_message in str(error.value)


def test_mixup_batch_fail5():
    """
    Feature: MixUpBatch op
    Description: Test MixUpBatch op where labels are not OneHot encoded
    Expectation: Error is raised as expected
    """
    logger.info("test_mixup_batch_fail5")

    # Original Images
    ds_original = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)
    ds_original = ds_original.batch(5)

    images_original = np.array([])
    for idx, (image, _) in enumerate(ds_original):
        if idx == 0:
            images_original = image.asnumpy()
        else:
            images_original = np.append(images_original, image.asnumpy(), axis=0)

    # MixUp Images
    data1 = ds.Cifar10Dataset(DATA_DIR, num_samples=10, shuffle=False)

    mixup_batch_op = vision.MixUpBatch()
    data1 = data1.batch(5, drop_remainder=True)
    data1 = data1.map(operations=mixup_batch_op, input_columns=["image", "label"])

    with pytest.raises(RuntimeError) as error:
        images_mixup = np.array([])
        for idx, (image, _) in enumerate(data1):
            if idx == 0:
                images_mixup = image.asnumpy()
            else:
                images_mixup = np.append(images_mixup, image.asnumpy(), axis=0)
    error_message = "wrong labels shape. The second column (labels) must have a shape of NC or NLC"
    assert error_message in str(error.value)


def test_mix_up_batch_invalid_label_type():
    """
    Feature: MixUpBatch
    Description: Test MixUpBatch with label in str type
    Expectation: Error is raised as expected
    """
    image = np.random.randint(0, 255, (3, 28, 28, 1), dtype=np.uint8)
    label = np.array([["one"], ["two"], ["three"]])
    with pytest.raises(RuntimeError) as error:
        _ = vision.MixUpBatch()(image, label)
    error_message = "MixUpBatch: invalid label type, label must be in a numeric type"
    assert error_message in str(error.value)


if __name__ == "__main__":
    test_mixup_batch_success1(plot=True)
    test_mixup_batch_success2(plot=True)
    test_mixup_batch_success3(plot=True)
    test_mixup_batch_success4(plot=True)
    test_mixup_batch_md5()
    test_mixup_batch_float_label()
    test_mixup_batch_then_cut_mix_batch()
    test_mixup_batch_fail1()
    test_mixup_batch_fail2()
    test_mixup_batch_fail3()
    test_mixup_batch_fail4()
    test_mixup_batch_fail5()
    test_mix_up_batch_invalid_label_type()
