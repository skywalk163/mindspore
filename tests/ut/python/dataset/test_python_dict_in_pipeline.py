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
# ==============================================================================
"""
Test generic support of Python dictionaries in dataset pipeline
"""
import math
from time import sleep
import numpy as np
import pytest

import mindspore.dataset as ds
from mindspore import log as logger
from mindspore.common import Tensor


def index_generator(ds_size):
    for i in range(ds_size):
        yield i


def dict_generator(ds_size):
    for i in range(ds_size):
        if i == 118:  # used in a test to verify proper error is raised
            yield {'integer': i, 'boolean': True, 'string': "MY_EMPTY_STR", "tuple": (1, 2, 3),
                   1: np.array([i, i + 100], dtype=np.int32)}
        else:
            yield {'integer': i, 'boolean': True, 'string': "MY_EMPTY_STR", "tuple": (1, 2, 3),
                   1: np.array([i, i + 100, i + 1000], dtype=np.int32)}


def simple_pyfunc(x):
    return x


def build_dict(x):
    return {"integer": x, "a": x ** 2, "b": 1}


def remove_dict(x):
    return x["integer"]


def remove_dict_wrong_key(x):
    return x["non-existing"]


def build_exp_dict(x):
    return {"value": np.power(x, 1), "square": np.power(x, 2), "cube": np.power(x, 3)}


def create_dict_batch(col1, batch_info):
    ret = [build_exp_dict(x) for x in col1]
    return (ret,)


def modify_dict_batch(col1, batch_info):
    def convert(x):
        new_dict = x
        new_dict["integer"] = np.power(new_dict["integer"], 2)
        new_dict["boolean"] = 1
        return new_dict

    new_dicts = [convert(x) for x in col1]
    return (new_dicts,)


@pytest.mark.parametrize("my_iterator", ("tuple", "dict"))
@pytest.mark.parametrize("output_numpy", (False, True))
def test_dict_generator(my_iterator, output_numpy):
    """
    Feature: Dataset pipeline creates a Python dict object using a generator operation.
    Description: Values maintained in the dict object are converted to Tensor appropriately.
    Expectation: Python dict object is successfully maintained and converted in the dataset pipeline.
    """
    logger.info("test_dict_generator -- Generator(dicts) --> rename()")
    dataset_size = 5
    data1 = ds.GeneratorDataset(dict_generator(dataset_size), ["col1"])
    if my_iterator == "tuple":
        itr = data1.create_tuple_iterator(
            num_epochs=1, output_numpy=output_numpy)
    else:
        itr = data1.create_dict_iterator(
            num_epochs=1, output_numpy=output_numpy)
    for d in itr:
        if my_iterator == "tuple":
            data = d[0]
        else:
            data = d["col1"]
        assert isinstance(data, dict)
        if output_numpy:
            assert isinstance(data["integer"], np.ndarray)
            assert data["integer"].dtype == int
            assert isinstance(data["boolean"], np.ndarray)
            assert data["boolean"].dtype == np.bool_
            assert isinstance(data["string"], np.ndarray)
            assert data["string"].dtype.type == np.str_
            assert isinstance(data["tuple"], tuple)
            assert isinstance(data["tuple"][0], np.ndarray)
            assert data["tuple"][0].dtype == int
        else:  # tensor
            assert isinstance(data["integer"], Tensor)
            assert isinstance(data["boolean"], Tensor)
            assert isinstance(data["string"], Tensor)
            assert isinstance(data["tuple"], tuple)
            assert isinstance(data["tuple"][0], Tensor)


def test_dict_generator_map_1():
    """
    Feature: Dataset pipeline contains a Python dict object.
    Description: Generator operation creates dictionaries while the next operation (map) removes them.
    Expectation: Python dict objects are successfully created, maintained, and deleted in the dataset pipeline.
    """
    logger.info("test_dict_generator_map_1 -- Generator(dicts) --> map(remove_dicts) --> rename()")
    dataset_size = 5
    data1 = ds.GeneratorDataset(lambda: dict_generator(dataset_size), ["col1"])
    data1 = data1.map(remove_dict)
    data1 = data1.rename(["col1"], ["renamed_col1"])

    count = 0
    itr = data1.create_dict_iterator(num_epochs=2, output_numpy=True)
    for _ in range(2):
        for i, d in enumerate(itr):
            count += 1
            assert isinstance(d["renamed_col1"], np.ndarray)
            assert d["renamed_col1"] == np.array([i])
    assert count == 10


def test_dict_generator_map_2():
    """
    Feature: Dataset pipeline contains a Python dict object.
    Description: Generator operation creates dictionaries while the following map operation's pyfunc accesses them.
    Expectation: Python dict objects are successfully created, maintained, and sent to user.
    """
    logger.info(
        "test_dict_generator_map_2 -- Generator(dicts) --> map(simple_pyfunc) --> rename()")
    dataset_size = 5
    data1 = ds.GeneratorDataset(lambda: dict_generator(dataset_size), ["col1"])
    data1 = data1.map(simple_pyfunc)
    data1 = data1.rename(["col1"], ["renamed_col1"])

    count = 0
    itr = data1.create_dict_iterator(num_epochs=2, output_numpy=True)
    for _ in range(2):
        for d in itr:
            count += 1
            assert isinstance(d["renamed_col1"], dict)
            assert isinstance(d["renamed_col1"]["integer"], np.ndarray)
            assert d["renamed_col1"]["integer"].dtype == int
            assert isinstance(d["renamed_col1"]["boolean"], np.ndarray)
            assert d["renamed_col1"]["boolean"].dtype == np.bool_
            assert isinstance(d["renamed_col1"]["string"], np.ndarray)
            assert d["renamed_col1"]["string"].dtype.type == np.str_
            assert isinstance(d["renamed_col1"]["tuple"], tuple)
            assert isinstance(d["renamed_col1"]["tuple"][0], np.ndarray)
            assert d["renamed_col1"]["tuple"][0].dtype == int
    assert count == 10


def test_dict_generator_map_3():
    """
    Feature: Dataset pipeline contains a Python dict object.
    Description: Generator operation creates dictionaries while the following map operation's pyfunc
        tries to access a non-existing key.
    Expectation: Appropriate error is raised in the dataset pipeline.
    """
    logger.info(
        "test_dict_generator_map_3 -- Generator(dicts) --> map(remove_dict_wrong_key) --> rename()")
    dataset_size = 5
    data1 = ds.GeneratorDataset(dict_generator(dataset_size), ["col1"])
    data1 = data1.map(remove_dict_wrong_key)
    data1 = data1.rename(["col1"], ["renamed_col1"])

    with pytest.raises(RuntimeError) as error_info:
        for _ in data1.create_dict_iterator(num_epochs=1, output_numpy=True):
            pass
    assert "KeyError" in str(error_info.value)


def test_dict_generator_batch_1():
    """
    Feature: Dataset pipeline contains a Python dict object.
    Description: Batch operation automatically constructs appropriate np arrays for each element in dictionaries.
    Expectation: Python dict objects are successfully created, maintained, and sent to user.
    """
    logger.info(
        "test_dict_generator_batch_1 -- Generator(dicts) --> rename() --> batch()")
    dataset_size = 15
    data1 = ds.GeneratorDataset(lambda: dict_generator(dataset_size), ["col1"])
    data1 = data1.rename(["col1"], ["renamed_col1"])
    data1 = data1.batch(2, drop_remainder=True)
    itr = data1.create_dict_iterator(num_epochs=2, output_numpy=True)
    count = 0
    for _ in range(2):
        for i, d in enumerate(itr):
            count += 1
            assert isinstance(d["renamed_col1"], dict)
            assert isinstance(d["renamed_col1"]["integer"], np.ndarray)
            assert d["renamed_col1"]["integer"].dtype == int
            assert isinstance(d["renamed_col1"]["boolean"], np.ndarray)
            assert d["renamed_col1"]["boolean"].dtype == np.bool_
            assert isinstance(d["renamed_col1"]["string"], np.ndarray)
            assert d["renamed_col1"]["string"].dtype.type == np.str_
            assert isinstance(d["renamed_col1"]["tuple"], np.ndarray)
            assert d["renamed_col1"]["tuple"].dtype == int
            assert isinstance(d["renamed_col1"][1], np.ndarray)
            assert d["renamed_col1"][1].dtype == np.int32
            assert d["renamed_col1"][1].shape == (2, 3)
            np.testing.assert_array_equal(d["renamed_col1"][1],
                                          np.array([[2 * i, 100 + 2 * i, 1000 + 2 * i],
                                                    [2 * i + 1, 100 + 2 * i + 1, 1000 + 2 * i + 1]],
                                                   dtype=np.int32))
    assert count == 14


def test_dict_generator_batch_2():
    """
    Feature: Dataset pipeline contains a Python dict object.
    Description: Batch operation's per_batch_map adds dictionaries to the dataset pipeline.
    Expectation: Python dict objects are successfully created, maintained, and sent to user.
    """
    # input: int, with per_batch_map creating dict
    logger.info(
        "test_dict_generator_batch_2 -- Generator() --> batch(create_dict_batch)")
    dataset_size = 5
    data1 = ds.GeneratorDataset(lambda: index_generator(dataset_size), ["col1"])
    data1 = data1.batch(2, per_batch_map=create_dict_batch,
                        drop_remainder=True)
    itr = data1.create_dict_iterator(num_epochs=2, output_numpy=True)
    count = 0
    for _ in range(2):
        for d in itr:
            count += 1
            assert isinstance(d["col1"], dict)
            assert isinstance(d["col1"]["value"], list)
            assert d["col1"]["value"][0].dtype == int
            assert isinstance(d["col1"]["square"], list)
            assert d["col1"]["square"][0].dtype == int
            assert isinstance(d["col1"]["cube"], list)
            assert d["col1"]["cube"][0].dtype == int
    assert count == 4


def test_dict_generator_batch_3():
    """
    Feature: Dataset pipeline contains python dict objects.
    Description: Batch operation's per_batch_map modifies existing dict objects in the pipeline.
    Expectation: Python dict objects are successfully created, maintained, and sent to user.
    """
    logger.info(
        "test_dict_generator_batch_3 -- Generator(dict_generator) --> rename() --> batch(modify_dict_batch)")
    dataset_size = 5
    data1 = ds.GeneratorDataset(lambda: dict_generator(dataset_size), ["col1"])
    data1 = data1.rename(["col1"], ["renamed_col1"])
    data1 = data1.batch(2, per_batch_map=modify_dict_batch,
                        drop_remainder=True)
    itr = data1.create_dict_iterator(num_epochs=2, output_numpy=True)
    counter = 0
    for _ in range(2):
        for d in itr:
            counter += 1
            assert isinstance(d["renamed_col1"], dict)
            assert isinstance(d["renamed_col1"]["integer"], list)
            assert d["renamed_col1"]["integer"][0].dtype == int
            assert isinstance(d["renamed_col1"]["boolean"], list)
            assert d["renamed_col1"]["boolean"][0].dtype == int  # changed to int in per_batch_map
            assert isinstance(d["renamed_col1"]["string"], list)
            assert d["renamed_col1"]["string"][0].dtype.type == np.str_
            assert isinstance(d["renamed_col1"]["tuple"], list)
            assert isinstance(d["renamed_col1"]["tuple"][0], tuple)
            assert d["renamed_col1"]["tuple"][0][0].dtype == int
    assert counter == 4


def wrong_batch1(col1, col2, batch_info):
    return {"a": 1}, col2  # 1 dict vs list of dicts


def wrong_batch2(col1, col2, batch_info):
    return {"a": 1}, {"a"}  # 1 dict vs 1 set


def wrong_batch3(col1, col2, batch_info):
    return {"a": 1}, [1]  # 1 dict vs a list (not a numpy array)


def wrong_batch4(col1, col2, batch_info):
    return {"a": 1}, [np.array([1]), np.array([1])]  # 1 dict vs list (not a numpy array)


def wrong_batch5(col1, col2, batch_info):
    return col1, np.array([1])  # 1 list of dicts vs 1 np (insufficient data in np to split)


@pytest.mark.parametrize("wrong_dict_batch", [wrong_batch1, wrong_batch2, wrong_batch3, wrong_batch4, wrong_batch5])
def test_dict_generator_batch_4(wrong_dict_batch):
    """
    Feature: Dataset pipeline contains python dict objects.
    Description: Batch operation's per_batch_map modifies existing dict objects in the pipeline.
    Expectation: Appropriate error is raised in the dataset pipeline.
    """
    logger.info(
        "test_dict_generator_batch_4 -- Generator(dict_generator) x 2 --> zip() --> batch()")
    dataset_size = 5
    data1 = ds.GeneratorDataset(dict_generator(dataset_size), ["col1"])
    data2 = ds.GeneratorDataset(dict_generator(dataset_size), ["col2"])
    data3 = ds.zip((data1, data2))
    data3 = data3.batch(2, per_batch_map=wrong_dict_batch,
                        drop_remainder=True)

    with pytest.raises(RuntimeError) as error_info:
        for _ in data3.create_dict_iterator(num_epochs=1, output_numpy=True):
            pass
    # pylint: disable=comparison-with-callable
    if wrong_dict_batch == wrong_batch5:
        assert "Invalid data, column: col2 expects: 2 rows returned from 'per_batch_map'" in str(error_info.value)
    else:
        assert "mismatched types returned from per_batch_map" in str(error_info.value)


def correct_batch1(col1, col2, batch_info):
    return {"a": 1}, {"a": 2}  # 1 dict vs 1 dict


def correct_batch2(col1, col2, batch_info):
    return col2, col1  # 1 list of dicts vs 1 list of dicts


def correct_batch3(col1, col2, batch_info):
    return {"a": 1}, np.array([1, 2, 3])  # 1 dict vs 1 np


def correct_batch4(col1, col2, batch_info):
    return col1, [1, 2]  # 1 list of dicts vs 1 list of ints


def correct_batch5(col1, col2, batch_info):
    return col1, np.array([1] * len(col2))  # 1 list of dicts vs 1 np (sufficient data)


@pytest.mark.parametrize("my_batch", [correct_batch1, correct_batch2, correct_batch3, correct_batch4, correct_batch5])
def test_dict_generator_batch_5(my_batch):
    """
    Feature: Dataset pipeline contains python dict objects.
    Description: Batch operation's per_batch_map modifies existing dict objects in the pipeline.
    Expectation: Python dict objects are successfully created, maintained, and sent to user.
    """
    logger.info(
        "test_dict_generator_batch_5 -- Generator(dict_generator) x 2 --> zip() --> batch()")
    dataset_size = 5
    data1 = ds.GeneratorDataset(dict_generator(dataset_size), ["col1"])
    data2 = ds.GeneratorDataset(dict_generator(dataset_size), ["col2"])
    data3 = ds.zip((data1, data2))
    data3 = data3.batch(2, per_batch_map=my_batch,
                        drop_remainder=True)
    counter = 0
    for _ in data3.create_dict_iterator(num_epochs=1, output_numpy=True):
        counter += 1
    assert counter == 2


def test_dict_generator_batch_6():
    """
    Feature: Dataset pipeline contains a Python dict object.
    Description: Batch operation detects mismatches between rows for a batch.
    Expectation: Appropriate error is raised in the dataset pipeline.
    """
    logger.info(
        "test_dict_generator_batch_6 -- Generator(dicts) --> batch()")
    dataset_size = 200
    # Note: For dataset_size >= 118, a mismatch for rows is created for this error test
    data1 = ds.GeneratorDataset(lambda: dict_generator(dataset_size), ["col1"])
    data1 = data1.batch(2, drop_remainder=True)

    with pytest.raises(RuntimeError) as error_info:
        for _ in data1.create_dict_iterator(num_epochs=2, output_numpy=True):
            pass
    assert "Batch: failed to create a NumPy array with primitive types" in str(error_info.value)


@pytest.mark.parametrize("batch_size", [5, 12])
@pytest.mark.parametrize("output_numpy", [True, False])
def test_dict_generator_batch_7(batch_size, output_numpy):
    """
    Feature: Dataset pipeline contains a Python dict object.
    Description: Batch operation works with different sizes, and creates proper np arrays or tensors.
    Expectation: Python dict objects are successfully created, maintained, and sent to user.
    """
    logger.info(
        "test_dict_generator_batch_7 -- Generator(dicts) --> batch()")
    dataset_size = 100
    data1 = ds.GeneratorDataset(lambda: dict_generator(dataset_size), ["col1"])
    data1 = data1.batch(batch_size, drop_remainder=True)
    count = 0
    for _ in data1.create_dict_iterator(num_epochs=1, output_numpy=output_numpy):
        count += 1
    assert count == dataset_size // batch_size


@pytest.mark.parametrize("batch_size", (2, 5, 13))
def test_dict_generator_batch_8(batch_size):
    """
    Feature: Dataset pipeline contains a Python dict object.
    Description: Batch operation automatically constructs appropriate np arrays when drop_remainder=False.
    Expectation: Python dict objects are successfully created, maintained, and sent to user.
    """
    logger.info(
        "test_dict_generator_batch_8 -- Generator(dicts) --> rename() --> batch()")
    dataset_size = 101
    data1 = ds.GeneratorDataset(lambda: dict_generator(dataset_size), ["col1"])
    data1 = data1.rename(["col1"], ["renamed_col1"])
    data1 = data1.batch(batch_size, drop_remainder=False)
    itr = data1.create_dict_iterator(num_epochs=2, output_numpy=True)
    count = 0
    for _ in range(2):
        for i, d in enumerate(itr):
            count += 1
            assert isinstance(d["renamed_col1"], dict)
            assert isinstance(d["renamed_col1"]["integer"], np.ndarray)
            assert d["renamed_col1"]["integer"].dtype == int
            assert isinstance(d["renamed_col1"]["boolean"], np.ndarray)
            assert d["renamed_col1"]["boolean"].dtype == np.bool_
            assert isinstance(d["renamed_col1"]["string"], np.ndarray)
            assert d["renamed_col1"]["string"].dtype.type == np.str_
            if min(batch_size, dataset_size - i * batch_size) == 1:  # last batch has 1 row
                assert isinstance(d["renamed_col1"]["tuple"], tuple)
                assert isinstance(d["renamed_col1"]["tuple"][0], np.ndarray)
                assert d["renamed_col1"]["tuple"][0].dtype == int
            else:
                assert isinstance(d["renamed_col1"]["tuple"], np.ndarray)
                assert d["renamed_col1"]["tuple"].dtype == int
            assert isinstance(d["renamed_col1"][1], np.ndarray)
            assert d["renamed_col1"][1].dtype == np.int32
            assert d["renamed_col1"][1].shape[-1] == 3  # other dimensions are different for last batch
            np.testing.assert_array_equal(
                d["renamed_col1"][1],
                np.array([[batch_size * i + j, 100 + batch_size * i + j, 1000 + batch_size * i + j]
                          for j in range(min((i + 1) * batch_size, dataset_size) - i * batch_size)],
                         dtype=np.int32).squeeze())

    assert count == 2 * math.ceil(dataset_size / batch_size)


def test_dict_advanced_pyfunc_dict():
    """
    Feature: Dataset pipeline contains Python dict objects.
    Description: Various generator, map, and batch operations are used to create and remove dict objects.
    Expectation: Python dict objects are successfully created, maintained, and sent to user.
    """
    logger.info("test_dict_advanced_pyfunc_dict")
    dataset_size = 125

    def my_batch_map(x1, x2, x3, y):
        return (x1, x2, x3)

    def my_delay_f(x1, x2, x3):
        sleep(0.01)  # sleep for 0.01s
        return (x1, x2, x3)

    data1 = ds.GeneratorDataset(index_generator(dataset_size), ["data1"])
    data2 = ds.GeneratorDataset(dict_generator(dataset_size), ["data2"])
    data3 = ds.GeneratorDataset(index_generator(dataset_size), ["data3"])
    data4 = ds.zip((data1, data2, data3))
    data4 = data4.map(build_dict, ["data1"])
    data4 = data4.map(remove_dict, ["data2"])
    data4 = data4.map(build_dict, ["data2"])
    data4 = data4.skip(3)
    data4 = data4.repeat(2)
    data4 = data4.map(build_dict, ["data3"])
    data4 = data4.map(remove_dict, ["data2"])
    data4 = data4.map(build_dict, ["data2"])
    data4 = data4.map(remove_dict, ["data2"])
    data4 = data4.take(40)
    data4 = data4.map(my_delay_f, ["data1", "data2", "data3"])
    data4 = data4.rename(["data1"], ["data1new"])
    data4 = data4.batch(2, per_batch_map=my_batch_map)
    data4 = data4.batch(2, drop_remainder=False)

    count = 0
    for d in data4.create_dict_iterator(num_epochs=1, output_numpy=True):
        count += 1
        assert len(d) == 3  # 3 columns
        assert isinstance(d["data1new"], dict)
        np.testing.assert_array_equal(d["data1new"]["b"], np.array([[1, 1], [1, 1]]))
    assert count == 10


@pytest.mark.parametrize("my_iterator", ("tuple", "dict"))
@pytest.mark.parametrize("output_numpy", (False, True))
def test_dict_generator_mixed(my_iterator, output_numpy):
    """
    Feature: Dataset pipeline creates a Python dict object using a generator operation.
    Description: Values maintained in the dict object are converted to Tensor appropriately.
    Expectation: Python dict object is successfully maintained and converted in the dataset pipeline.
    """
    logger.info("test_dict_generator_mixed -- Generator(dicts) --> rename()")

    def mixed_dict_generator(ds_size):
        for i in range(ds_size):
            yield ({'integer': i, 'boolean': True, 'string': "MY_EMPTY_STR", "tuple": (1, 2, 3)}, True, 4, "String")

    dataset_size = 15
    data1 = ds.GeneratorDataset(mixed_dict_generator(dataset_size), ["col1", "col2", "col3", "col4"])
    if my_iterator == "tuple":
        itr = data1.create_tuple_iterator(
            num_epochs=1, output_numpy=output_numpy)
    else:
        itr = data1.create_dict_iterator(
            num_epochs=1, output_numpy=output_numpy)
    count = 0
    for data in itr:
        count += 1
        if my_iterator == "tuple":
            if output_numpy:
                assert isinstance(data[0], dict)
                assert isinstance(data[0]["integer"], np.ndarray)
                assert data[0]["integer"].dtype == int
                assert isinstance(data[0]["boolean"], np.ndarray)
                assert data[0]["boolean"].dtype == np.bool_
                assert isinstance(data[0]["string"], np.ndarray)
                assert data[0]["string"].dtype.type == np.str_
                assert isinstance(data[0]["tuple"], tuple)
                assert isinstance(data[0]["tuple"][0], np.ndarray)
                assert data[0]["tuple"][0].dtype == int
                assert isinstance(data[1], np.ndarray)
                assert data[1].dtype == np.bool_
                assert isinstance(data[2], np.ndarray)
                assert data[2].dtype == int
                assert isinstance(data[3], np.ndarray)
                assert data[3].dtype.type == np.str_
            else:  # tensor
                assert isinstance(data[0], dict)
                assert isinstance(data[0]["integer"], Tensor)
                assert isinstance(data[1], Tensor)
                assert isinstance(data[2], Tensor)
                assert isinstance(data[3], Tensor)
        else:  # dict iterator
            if output_numpy:
                assert isinstance(data["col1"], dict)
                assert isinstance(data["col1"]["integer"], np.ndarray)
                assert data["col1"]["integer"].dtype == int
                assert isinstance(data["col1"]["boolean"], np.ndarray)
                assert data["col1"]["boolean"].dtype == np.bool_
                assert isinstance(data["col1"]["string"], np.ndarray)
                assert data["col1"]["string"].dtype.type == np.str_
                assert isinstance(data["col1"]["tuple"], tuple)
                assert isinstance(data["col1"]["tuple"][0], np.ndarray)
                assert data["col1"]["tuple"][0].dtype == int
                assert isinstance(data["col2"], np.ndarray)
                assert data["col2"].dtype == np.bool_
                assert isinstance(data["col3"], np.ndarray)
                assert data["col3"].dtype == int
                assert isinstance(data["col4"], np.ndarray)
                assert data["col4"].dtype.type == np.str_
            else:  # tensor
                assert isinstance(data["col1"], dict)
                assert isinstance(data["col1"]["integer"], Tensor)
                assert isinstance(data["col1"]["tuple"], tuple)
                assert isinstance(data["col1"]["tuple"][0], Tensor)
                assert isinstance(data["col2"], Tensor)
                assert isinstance(data["col3"], Tensor)
                assert isinstance(data["col4"], Tensor)
    assert count == 15


@pytest.mark.parametrize("output_numpy", (False, True))
def test_dict_generator_nested_dicts(output_numpy):
    """
    Feature: Dataset pipeline contains a Python dict object.
    Description: Generator operation creates nested dictionaries.
    Expectation: Python dict objects are successfully created, maintained, and deleted in the dataset pipeline.
    """
    logger.info("test_dict_generator_nested_dicts -- Generator(nested_dicts)")
    dataset_size = 5

    def nested_dict_generator(ds_size):
        for i in range(ds_size):
            yield {"integer": i, "dict": {"a": 0, "b": 1, "tuple": (1, 2, 3)}}

    data1 = ds.GeneratorDataset(lambda: nested_dict_generator(dataset_size), ["col1"])

    count = 0
    itr = data1.create_dict_iterator(num_epochs=2, output_numpy=output_numpy)
    for _ in range(2):
        for d in itr:
            count += 1
            if output_numpy:
                assert isinstance(d["col1"], dict)
                assert isinstance(d["col1"]["integer"], np.ndarray)
                assert d["col1"]["integer"].dtype == int
                assert isinstance(d["col1"]["dict"], dict)
                assert isinstance(d["col1"]["dict"]["a"], np.ndarray)
                assert d["col1"]["dict"]["a"].dtype == int
                assert isinstance(d["col1"]["dict"]["tuple"], tuple)
                assert isinstance(d["col1"]["dict"]["tuple"][0], np.ndarray)
                assert d["col1"]["dict"]["tuple"][0].dtype == int
            else:
                assert isinstance(d["col1"], dict)
                assert isinstance(d["col1"]["integer"], Tensor)
                assert isinstance(d["col1"]["dict"], dict)
                assert isinstance(d["col1"]["dict"]["a"], Tensor)
                assert isinstance(d["col1"]["dict"]["tuple"], tuple)
                assert isinstance(d["col1"]["dict"]["tuple"][0], Tensor)
    assert count == 10


if __name__ == '__main__':
    test_dict_generator("tuple", False)
    test_dict_generator_map_1()
    test_dict_generator_map_2()
    test_dict_generator_map_3()
    test_dict_generator_batch_1()
    test_dict_generator_batch_2()
    test_dict_generator_batch_3()
    test_dict_generator_batch_4(wrong_batch1)
    test_dict_generator_batch_5(correct_batch1)
    test_dict_generator_batch_6()
    test_dict_generator_batch_7(5, True)
    test_dict_generator_batch_8(2)
    test_dict_advanced_pyfunc_dict()
    test_dict_generator_mixed("tuple", False)
    test_dict_generator_nested_dicts(True)
