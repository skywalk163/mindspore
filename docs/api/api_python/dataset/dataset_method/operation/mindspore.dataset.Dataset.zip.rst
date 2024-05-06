mindspore.dataset.Dataset.zip
=============================

.. py:method:: mindspore.dataset.Dataset.zip(datasets)

    将多个dataset对象按列进行合并压缩，多个dataset对象不能有相同的列名。

    参数：
        - **datasets** (Union[Dataset, tuple[Dataset]]) - 要合并的（多个）dataset对象。

    返回：
        Dataset，应用了上述操作的新数据集对象。

    异常：
        - **TypeError** - `datasets` 参数不是dataset对象/tuple[dataset]。
