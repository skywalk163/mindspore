﻿mindspore.dataset.ManifestDataset
==================================

.. py:class:: mindspore.dataset.ManifestDataset(dataset_file, usage='train', num_samples=None, num_parallel_workers=None, shuffle=None, sampler=None, class_indexing=None, decode=False, num_shards=None, shard_id=None, cache=None)

    读取和解析Manifest数据文件构建数据集。

    生成的数据集有两列： `[image, label]` 。 `image` 列的数据类型为uint8类型。 `label` 列的数据类型为uint64类型。

    参数：
        - **dataset_file** (str) - 数据集文件的目录路径。
        - **usage** (str, 可选) - 指定数据集的子集，可取值为 ``'train'`` 、 ``'eval'`` 或 ``'inference'`` 。默认值： ``'train'`` 。
        - **num_samples** (int, 可选) - 指定从数据集中读取的样本数，可以小于数据集总数。默认值： ``None`` ，读取全部样本图片。
        - **num_parallel_workers** (int, 可选) - 指定读取数据的工作线程数。默认值： ``None`` ，使用全局默认线程数(8)，也可以通过 :func:`mindspore.dataset.config.set_num_parallel_workers` 配置全局线程数。
        - **shuffle** (bool, 可选) - 是否混洗数据集。默认值： ``None`` 。下表中会展示不同参数配置的预期行为。
        - **sampler** (Sampler, 可选) - 指定从数据集中选取样本的采样器。默认值： ``None`` 。下表中会展示不同配置的预期行为。
        - **class_indexing** (dict, 可选) - 指定一个从label名称到label索引的映射，要求映射规则为string到int。索引值从0开始，并且要求每个label名称对应的索引值唯一。默认值： ``None`` ，不指定。
        - **decode** (bool, 可选) - 是否对读取的图片进行解码操作。默认值： ``False`` ，不解码。
        - **num_shards** (int, 可选) - 指定分布式训练时将数据集进行划分的分片数。默认值： ``None`` 。指定此参数后， `num_samples` 表示每个分片的最大样本数。
        - **shard_id** (int, 可选) - 指定分布式训练时使用的分片ID号。默认值： ``None`` 。只有当指定了 `num_shards` 时才能指定此参数。
        - **cache** (:class:`~.dataset.DatasetCache`, 可选) - 单节点数据缓存服务，用于加快数据集处理，详情请阅读 `单节点数据缓存 <https://www.mindspore.cn/tutorials/experts/zh-CN/master/dataset/cache.html>`_ 。默认值： ``None`` ，不使用缓存。

    异常：
        - **RuntimeError** - `dataset_files` 路径下不包含任何数据文件。
        - **ValueError** - `num_parallel_workers` 参数超过系统最大线程数。
        - **RuntimeError** - 同时指定了 `sampler` 和 `shuffle` 参数。
        - **RuntimeError** - 同时指定了 `sampler` 和 `num_shards` 参数或同时指定了 `sampler` 和 `shard_id` 参数。
        - **RuntimeError** - 指定了 `num_shards` 参数，但是未指定 `shard_id` 参数。
        - **RuntimeError** - 指定了 `shard_id` 参数，但是未指定 `num_shards` 参数。
        - **RuntimeError** - `class_indexing` 参数的类型不是dict。
        - **ValueError** - `shard_id` 参数值错误（小于0或者大于等于 `num_shards`）。

    教程样例：
        - `使用数据Pipeline加载 & 处理数据集
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/dataset/dataset_gallery.html>`_

    .. note::
        - 如果 `decode` 为 ``False`` ，`image` 列返回图像的一维原始字节。否则，将返回 shape 为 :math:`[H,W,C]` 的解码图像。
        - 入参 `num_samples` 、 `shuffle` 、 `num_shards` 、 `shard_id` 可用于控制数据集所使用的采样器，其与入参 `sampler` 搭配使用的效果如下。

    .. include:: mindspore.dataset.sampler.rst

    **关于Manifest数据集：**

    Manifest文件包含数据集中包含的文件列表，包括文件名和文件ID等基本文件信息，以及扩展文件元数据。
    Manifest是华为ModelArts支持的数据格式文件，详细说明请参见 `Manifest文档 <https://support.huaweicloud.com/engineers-modelarts/modelarts_23_0009.html>`_ 。

    以下是原始Manifest数据集结构。可以将数据集文件解压缩到此目录结构中，并由MindSpore的API读取。

    .. code-block::

        .
        └── manifest_dataset_directory
            ├── train
            │    ├── 1.JPEG
            │    ├── 2.JPEG
            │    ├── ...
            ├── eval
            │    ├── 1.JPEG
            │    ├── 2.JPEG
            │    ├── ...


.. include:: mindspore.dataset.api_list_vision.rst
