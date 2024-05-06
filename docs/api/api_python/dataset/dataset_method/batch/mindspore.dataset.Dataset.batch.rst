mindspore.dataset.Dataset.batch
===============================

.. py:method:: mindspore.dataset.Dataset.batch(batch_size, drop_remainder=False, num_parallel_workers=None, **kwargs)

    将数据集中连续 `batch_size` 条数据组合为一个批数据，并可通过可选参数 `per_batch_map` 指定组合前要进行的预处理操作。

    `batch` 操作要求每列中的数据具有相同的shape。

    执行流程参考下图：

    .. image:: batch_cn.png

    .. note::
        执行 `repeat` 和 `batch` 操作的先后顺序，会影响批处理数据的数量及 `per_batch_map` 的结果。建议在 `batch` 操作完成后执行 `repeat` 操作。

    参数：
        - **batch_size** (Union[int, Callable]) - 指定每个批处理数据包含的数据条目。
          如果 `batch_size` 为整型，则直接表示每个批处理数据大小；
          如果为可调用对象，则可以通过自定义行为动态指定每个批处理数据大小，要求该可调用对象接收一个参数BatchInfo，返回一个整形代表批处理大小，用法请参考样例（3）。
        - **drop_remainder** (bool, 可选) - 当最后一个批处理数据包含的数据条目小于 `batch_size` 时，是否将该批处理丢弃，不传递给下一个操作。默认值： ``False`` ，不丢弃。
        - **num_parallel_workers** (int, 可选) - 指定 `batch` 操作的并发进程数/线程数（由参数 `python_multiprocessing` 决定当前为多进程模式或多线程模式）。
          默认值： ``None`` ，使用全局默认线程数(8)，也可以通过 :func:`mindspore.dataset.config.set_num_parallel_workers` 配置全局线程数。
        - **\*\*kwargs** - 其他参数。

          - per_batch_map (Callable[[List[numpy.ndarray], ..., List[numpy.ndarray], BatchInfo], (List[numpy.ndarray],..., List[numpy.ndarray])], 可选) - 可调用对象，
            以(list[numpy.ndarray], ..., list[numpy.ndarray], BatchInfo)作为输入参数，
            处理后返回(list[numpy.ndarray], list[numpy.ndarray],...)作为新的数据列。输入参数中每个list[numpy.ndarray]代表给定数据列中的一批numpy.ndarray，
            list[numpy.ndarray]的个数应与 `input_columns` 中传入列名的数量相匹配，在返回的(list[numpy.ndarray], list[numpy.ndarray], ...)中，
            list[numpy.ndarray]的个数应与输入相同，如果输出列数与输入列数不一致，则需要指定 `output_columns` 。该可调用对象的最后一个输入参数始终是BatchInfo，
            用于获取数据集的信息，用法参考样例（2）。
          - input_columns (Union[str, list[str]], 可选) - 指定 `batch` 操作的输入数据列。
            如果 `per_batch_map` 不为 ``None`` ，列表中列名的个数应与 `per_batch_map` 中包含的列数匹配。默认值： ``None`` ，不指定。
          - output_columns (Union[str, list[str]], 可选) - 指定 `batch` 操作的输出数据列。如果输入数据列与输入数据列的长度不相等，则必须指定此参数。
            此列表中列名的数量必须与 `per_batch_map` 方法的返回值数量相匹配。默认值： ``None`` ，输出列将与输入列具有相同的名称。
          - python_multiprocessing (bool, 可选) - 是否启动Python多进程模式并行执行 `per_batch_map` ， ``True`` 意为Python多进程模式， ``False`` 意为Python多线程模式。如果 `per_batch_map` 是I/O密集型任务可以用多线程，CPU密集型任务建议使用多进程以避免GIL锁影响。默认值： ``False`` ，启用多线程模式。
          - max_rowsize (Union[int, list[int]], 可选) - 指定在多进程之间复制数据时，共享内存分配的基本单位，总占用的共享内存会随着 ``num_parallel_workers`` 和 :func:`mindspore.dataset.config.set_prefetch_size` 增加而变大，仅当 `python_multiprocessing` 为 ``True`` 时，该选项有效。如果是int值，代表 ``input_columns`` 和 ``output_columns`` 均使用该值为单位创建共享内存；如果是列表，第一个元素代表 ``input_columns`` 使用该值为单位创建共享内存，第二个元素代表 ``output_columns`` 使用该值为单位创建共享内存。默认值： ``16`` ，单位为MB。

    返回：
        Dataset，应用了上述操作的新数据集对象。
