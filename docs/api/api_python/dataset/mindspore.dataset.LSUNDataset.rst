mindspore.dataset.LSUNDataset
=============================

.. py:class:: mindspore.dataset.LSUNDataset(dataset_dir, usage=None, classes=None, num_samples=None, num_parallel_workers=None, shuffle=None, decode=False, sampler=None, num_shards=None, shard_id=None, cache=None)

    LSUN（Large-scale Scene UNderstarding）数据集。

    生成的数据集有两列: `[image, label]` 。
    `image` 列的数据类型为uint8。
    `label` 列的数据类型为int32。

    参数：
        - **dataset_dir** (str) - 包含数据集文件的根目录路径。
        - **usage** (str, 可选) - 指定数据集的子集，可为 ``'train'`` 、 ``'test'`` 、 ``'valid'`` 或 ``'all'`` 。默认值： ``None`` ，将设置为 ``'all'`` 。
        - **classes** (Union[str, list[str]], 可选) - 读取数据集指定的类别。默认值： ``None`` ，表示读取所有类别。
        - **num_samples** (int, 可选) - 指定从数据集中读取的样本数。默认值： ``None`` ，读取全部图像。
        - **num_parallel_workers** (int, 可选) - 指定读取数据的工作线程数。默认值： ``None`` ，使用全局默认线程数(8)，也可以通过 :func:`mindspore.dataset.config.set_num_parallel_workers` 配置全局线程数。
        - **shuffle** (bool, 可选) - 是否混洗数据集。默认值： ``None`` 。下表中会展示不同参数配置的预期行为。
        - **decode** (bool, 可选) - 是否对读取的图片进行解码操作。默认值： ``False`` ，不解码。
        - **sampler** (Sampler, 可选) - 指定从数据集中选取样本的采样器。默认值： ``None`` 。下表中会展示不同配置的预期行为。
        - **num_shards** (int, 可选) - 指定分布式训练时将数据集进行划分的分片数。默认值： ``None`` 。指定此参数后， `num_samples` 表示每个分片的最大样本数。
        - **shard_id** (int, 可选) - 指定分布式训练时使用的分片ID号。默认值： ``None`` 。只有当指定了 `num_shards` 时才能指定此参数。
        - **cache** (:class:`~.dataset.DatasetCache`, 可选) - 单节点数据缓存服务，用于加快数据集处理，详情请阅读 `单节点数据缓存 <https://www.mindspore.cn/tutorials/experts/zh-CN/master/dataset/cache.html>`_ 。默认值： ``None`` ，不使用缓存。

    异常：
        - **RuntimeError** - `dataset_dir` 路径下不包含数据文件。
        - **RuntimeError** - 同时指定了 `sampler` 和 `shuffle` 参数。
        - **RuntimeError** - 同时指定了 `sampler` 和 `num_shards` 参数或同时指定了 `sampler` 和 `shard_id` 参数。
        - **RuntimeError** - 指定了 `num_shards` 参数，但是未指定 `shard_id` 参数。
        - **RuntimeError** - 指定了 `shard_id` 参数，但是未指定 `num_shards` 参数。
        - **ValueError** - 如果 `shard_id` 取值不在[0, `num_shards` )范围。
        - **ValueError** - `usage` 或 `classes` 参数错误（不为可选的类别）。

    教程样例：
        - `使用数据Pipeline加载 & 处理数据集
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/dataset/dataset_gallery.html>`_

    .. note:: 入参 `num_samples` 、 `shuffle` 、 `num_shards` 、 `shard_id` 可用于控制数据集所使用的采样器，其与入参 `sampler` 搭配使用的效果如下。

    .. include:: mindspore.dataset.sampler.rst

    **关于LSUN数据集：**
    
    LSUN（Large-Scale Scene Understanding）是一个大规模数据集，用于室内场景理解。
    LSUN最初是在2015年由斯坦福大学推出的，旨在为计算机视觉和机器学习领域的研究提供一个具有挑战性和多样性的数据集。
    该数据集的主要应用是室内场景分析。

    该数据集包含了十种不同的场景类别，包括卧室、客厅、餐厅、起居室、书房、厨房、浴室、走廊、儿童房和室外。
    每种类别都包含了来自不同视角的数万张图像，并且这些图像都是高质量、高分辨率的真实世界图像。

    您可以解压原始的数据集文件构建成如下目录结构，并通过MindSpore的API进行读取。

    .. code-block::

        .
        └── lsun_dataset_directory
            ├── test
            │    ├── ...
            ├── bedroom_train
            │    ├── 1_1.jpg
            │    ├── 1_2.jpg
            ├── bedroom_val
            │    ├── ...
            ├── classroom_train
            │    ├── ...
            ├── classroom_val
            │    ├── ...

    **引用：**

    .. code-block::

        article{yu15lsun,
            title={LSUN: Construction of a Large-scale Image Dataset using Deep Learning with Humans in the Loop},
            author={Yu, Fisher and Zhang, Yinda and Song, Shuran and Seff, Ari and Xiao, Jianxiong},
            journal={arXiv preprint arXiv:1506.03365},
            year={2015}
        }


.. include:: mindspore.dataset.api_list_vision.rst
