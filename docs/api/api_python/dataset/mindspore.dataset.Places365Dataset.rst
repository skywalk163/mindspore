mindspore.dataset.Places365Dataset
==================================

.. py:class:: mindspore.dataset.Places365Dataset(dataset_dir, usage=None, small=True, decode=False, num_samples=None, num_parallel_workers=None, shuffle=None, sampler=None, num_shards=None, shard_id=None, cache=None)

    Places365数据集。

    生成的数据集有两列: `[image, label]`。 
    `image` 列的数据类型为uint8。 `label` 列的数据类型为uint32。

    参数：
        - **dataset_dir** (str) - 包含数据集文件的根目录路径。
        - **usage** (str, 可选) - 指定数据集的子集，可取值为 ``'train-standard'`` 、 ``'train-challenge'`` 或 ``'val'`` 。默认值： ``None``，将使用 ``'train-standard'`` 。
        - **small** (bool, 可选) - 是否使用256*256的低分辨率图像（True）或高分辨率图像（False）。默认值： ``True`` ，使用低分辨率图像。
        - **decode** (bool, 可选) - 是否对读取的图片进行解码操作。默认值： ``False`` ，不解码。
        - **num_samples** (int, 可选) - 指定从数据集中读取的样本数。默认值： ``None`` ，读取所有样本。
        - **num_parallel_workers** (int, 可选) - 指定读取数据的工作线程数。默认值： ``None`` ，使用全局默认线程数(8)，也可以通过 :func:`mindspore.dataset.config.set_num_parallel_workers` 配置全局线程数。
        - **shuffle** (bool, 可选) - 是否混洗数据集。默认值： ``None`` 。下表中会展示不同参数配置的预期行为。
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
        - **ValueError** - `num_parallel_workers` 参数超过系统最大线程数。
        - **ValueError** - `shard_id` 参数错误，参数小于0或者大于等于 `num_shards` 。
        - **ValueError** - `usage` 不是 ``'train-standard'`` 、 ``'train-challenge'`` 或 ``'val'`` 。

    教程样例：
        - `使用数据Pipeline加载 & 处理数据集
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/dataset/dataset_gallery.html>`_

    .. note:: 入参 `num_samples` 、 `shuffle` 、 `num_shards` 、 `shard_id` 可用于控制数据集所使用的采样器，其与入参 `sampler` 搭配使用的效果如下。

    .. include:: mindspore.dataset.sampler.rst

    **关于Places365数据集：**

    在Places2数据库上训练的卷积神经网络（CNN）可用于场景识别，也可用于视觉识别的通用深度场景特征。

    Places作者向公众发布了Places365-Standard数据集和Places365-Challenge数据集。
    Places365-Standard数据集是Places2数据库的核心集，该数据库已用于训练Places365-CNN。
    Places作者将在未来的Places365-Standard数据集上添加其他类型的标注。
    Places365-Challenge数据集是Places2数据库的竞赛数据集，与Places365-Standard数据集相比，该数据库有620万张额外的图像。此数据集用于2016年的Places挑战赛。

    可以将原始的Places365数据集文件解压缩到此目录结构中，并由MindSpore的API读取。

    .. code-block::

        .
        └── categories_places365
            ├── places365_train-standard.txt
            ├── places365_train-challenge.txt
            ├── val_large/
            │    ├── Places365_val_00000001.jpg
            │    ├── Places365_val_00000002.jpg
            │    ├── Places365_val_00000003.jpg
            │    ├── ...
            ├── val_256/
            │    ├── ...
            ├── data_large_standard/
            │    ├── ...
            ├── data_256_standard/
            │    ├── ...
            ├── data_large_challenge/
            │    ├── ...
            ├── data_256_challenge /
            │    ├── ...

    **引用：**

    .. code-block::

        article{zhou2017places,
            title={Places: A 10 million Image Database for Scene Recognition},
            author={Zhou, Bolei and Lapedriza, Agata and Khosla, Aditya and Oliva, Aude and Torralba, Antonio},
            journal={IEEE Transactions on Pattern Analysis and Machine Intelligence},
            year={2017},
            publisher={IEEE}
        }


.. include:: mindspore.dataset.api_list_vision.rst
