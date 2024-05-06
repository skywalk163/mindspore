# Copyright 2020-2023 Huawei Technologies Co., Ltd
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
"""Profiling api file."""
import os
import re
import shutil
import stat
import time
import json
from json import JSONDecodeError
import glob
import subprocess
import csv
import socket
from enum import Enum
from multiprocessing import Process
from typing import List
import numpy as np

from mindspore import log as logger, context
from mindspore.context import get_auto_parallel_context
from mindspore.communication.management import GlobalComm, get_rank, get_group_size, get_local_rank
import mindspore._c_expression as c_expression
import mindspore._c_dataengine as cde
from mindspore.profiler.common.exceptions.exceptions import ProfilerFileNotFoundException, \
    ProfilerIOException, ProfilerException, ProfilerRawFileException, ProfilerParamTypeErrorException
from mindspore.profiler.common.exceptions.exceptions import ProfilerPathErrorException
from mindspore.profiler.common.exceptions.exceptions import ProfilerDirNotFoundException
from mindspore.profiler.common.util import get_file_path
from mindspore.profiler.common.validator.validate_path import validate_and_normalize_path
from mindspore.profiler.parser.framework_parser import GpuFrameWorkParser, DynamicFrameWorkParser
from mindspore.profiler.parser.integrator import Integrator, DeviceTarget
from mindspore.profiler.parser.ascend_analysis.function_event import CANNEvent
from mindspore.profiler.parser.cpu_gpu_timeline_generator import GpuTimelineGenerator, CpuTimelineGenerator
from mindspore.profiler.parser.ascend_timeline_generator import AscendTimelineGenerator
from mindspore.profiler.parser.memory_usage_parser import MemoryUsageParser
from mindspore.profiler.parser.minddata_parser import MinddataParser
from mindspore.profiler.parser.minddata_analyzer import MinddataProfilingAnalyzer
from mindspore.profiler.parser.minddata_pipeline_parser import \
    MinddataPipelineParser
from mindspore.profiler.parser.step_trace_parser import GpuStepTraceParser, AscendStepTraceParser
from mindspore.profiler.parser.msadvisor_analyzer import Msadvisor
from mindspore.profiler.parser.profiler_info import ProfilerInfo
from mindspore.common.api import _pynative_executor
from mindspore.profiler.parser.ascend_msprof_exporter import AscendMsprofExporter
from mindspore.profiler.parser.ascend_msprof_generator import AscendMsprofDataGenerator
from mindspore.profiler.parser.ascend_fpbp_generator import AscendFPBPGenerator
from mindspore.profiler.parser.ascend_op_generator import AscendOPGenerator
from mindspore.profiler.parser.ascend_steptrace_generator import AscendStepTraceGenerator
from mindspore.profiler.parser.ascend_flops_generator import AscendFlopsGenerator
from mindspore.profiler.parser.ascend_cluster_generator import AscendClusterGenerator
from mindspore.profiler.parser.ascend_hccl_generator import AscendHCCLGenerator
from mindspore.profiler.parser.ascend_communicate_generator import AscendCommunicationGenerator

INIT_OP_NAME = 'Default/InitDataSetQueue'

AICORE_METRICS_DICT = {
    0: "ArithmeticUtilization",
    1: "PipeUtilization",
    2: "Memory",
    3: "MemoryL0",
    4: "ResourceConflictRatio",
    5: "MemoryUB",
    6: "L2Cache",
    -1: "None"
}


class ModelTraingMode(Enum):
    PYNATIVE = 0
    GRAPH = 1
    KERNEL_BY_KERNEL = 2
    UNKNOWN = 3


class DeviceSupportParam(Enum):
    """The device target enum."""
    CPU = ['start', 'start_profile', 'output_path', 'timeline_limit', 'profile_framework', 'op_time']
    GPU = [
        'start', 'start_profile', 'output_path', 'data_process', 'timeline_limit', 'sync_enable', 'op_time',
        'profile_framework'
    ]
    ASCEND = [
        'start', 'start_profile', 'output_path', 'data_process', 'timeline_limit', 'profile_memory',
        'parallel_strategy', 'profile_communication', 'aicore_metrics', 'l2_cache', 'hbm_ddr', 'pcie', 'op_time',
        'ascend_job_id', 'profile_framework'
    ]


ALWAYS_VALID_PARAM = [
    'start', 'start_profile', 'output_path', 'data_process', 'parallel_strategy', 'l2_cache',
    'hbm_ddr', 'pcie', 'ascend_job_id', 'op_time', 'profile_framework'
]


def _environment_check():
    if c_expression.security.enable_security():
        raise RuntimeError("Profiler is not supported when MindSpore is compiled with \'-s on\'.")


class ExecutionCalculator:
    """Calculate the average execution time and counts for each stage."""

    def __init__(self, event, stage, custom_info):
        self.event = event
        self.stage = stage
        self.custom_info = custom_info
        self.count = 0
        self.average_execution = 0


def _calculate_dataset_item(row, execution_time_map, ts_map):
    """Calculate dataset execution time for one row."""
    start_end = row['start_end']
    event = row['event']
    stage = row['stage']
    custom_info = row['custom_info']
    event_stage_tid_pid = event + '_' + stage + '_' + row['tid'] + '_' + row['pid']
    if start_end == '1' and event_stage_tid_pid in ts_map:
        title = event + '::' + stage + '::' + custom_info
        ts_end = int(row['time_stamp(us)'])
        ts = ts_map[event_stage_tid_pid]
        dur = ts_end - ts
        if title not in execution_time_map:
            execution_time_map[title] = ExecutionCalculator(event=event, stage=stage, custom_info=custom_info)
        execution_time_map[title].count += 1
        if execution_time_map[title].count != 0:
            execution_time_map[title].average_execution += \
                (dur - execution_time_map[title].average_execution) / execution_time_map[title].count
        del ts_map[event_stage_tid_pid]
    elif start_end == '0':
        ts = int(row['time_stamp(us)'])
        ts_map[event_stage_tid_pid] = ts
    elif start_end == '2':
        logger.info("It is a instant event, skip to calculate execution time. item: %s.", row)
    else:
        logger.warning("Can not map the start time for item: %s.", row)


def _calculate_dataset_execution_time(input_file, output_file):
    r"""
    Parse the host info into timeline file, so as to show on UI.

    Args:
        input_file: the original host_info file, in csv format.
        output_file: the output file, in csv format.
    """
    input_file = validate_and_normalize_path(input_file)
    # execution_time_map is used to store the ExecutionCalculator for each stage.
    execution_time_map = {}
    # ts_map is used to store the start time of each event_stage_tid_pid.
    ts_map = {}
    with open(input_file, 'r') as f:
        for row in csv.DictReader(f):
            try:
                module_name = row['module_name']
                if module_name != 'Dataset':
                    continue
                _calculate_dataset_item(row, execution_time_map, ts_map)
            except KeyError as e:
                logger.error("Error occur when analyse line: %s, Details is: %s", row, e)
                continue
    if ts_map:
        logger.warning("Only start time is record for these items:")
        for k, v in ts_map.items():
            logger.warning("event_stage_tid_pid: %s, time: %d us.", k, v)
    output_file = validate_and_normalize_path(output_file)
    flags = os.O_WRONLY | os.O_CREAT | os.O_TRUNC
    modes = stat.S_IWUSR | stat.S_IRUSR
    with os.fdopen(os.open(output_file, flags, modes), 'w') as f:
        csv_writer = csv.writer(f)
        csv_writer.writerow(['Operation', 'Stage', 'Occurrences', 'Avg. time (us)', 'Custom Info'])
        for _, v in execution_time_map.items():
            csv_writer.writerow([v.event, v.stage, v.count, v.average_execution, v.custom_info])
    os.chmod(output_file, modes)
    logger.info('Successfully calculate the execution time and write it to file: %s.', output_file)


def _extract_timeline_item(row, time_line, ts_map):
    """Process one row, try to extract a timeline item."""
    start_end = row['start_end']
    event_stage_tid_pid = row['event'] + '_' + row['stage'] + '_' + row['tid'] + '_' + row['pid']
    # map start and end, put the mapped event into timeline.
    if start_end == '1' and event_stage_tid_pid in ts_map:
        title = row['event'] + '::' + row['stage']
        event = {'name': title, 'cat': row['module_name']}
        ts_end = int(row['time_stamp(us)'])
        ts = ts_map[event_stage_tid_pid]
        event['ts'] = ts
        event['dur'] = ts_end - ts
        event['ph'] = 'X'
        event['pid'] = row['pid']
        event['tid'] = row['tid']
        event['args'] = {'parent_pid': row['parent_pid']}
        time_line.append(event)
        del ts_map[event_stage_tid_pid]
    elif start_end == '0':
        ts = int(row['time_stamp(us)'])
        ts_map[event_stage_tid_pid] = ts
    # Put the instance event into timeline.
    elif start_end == '2':
        title = row['event'] + '::' + row['stage']
        event = {
            'name': title, 'cat': row['module_name'], 'ts': int(row['time_stamp(us)']), 'ph': 'i',
            'pid': row['pid'], 'tid': row['tid'], 'args': {'parent_pid': row['parent_pid']}
        }
        time_line.append(event)
    else:
        logger.warning("Can not map the start time for item: %s.", row)


def _parse_host_info(input_file, output_timeline_file, output_memory_file, is_develop_user=True):
    r"""
    Parse the host info into timeline file, so as to show on UI.

    Args:
        input_file: the original host_info file, in csv format.
        output_timeline_file: the output timeline file, in json format.
        output_memory_file: the output memory_usage file, in csv format.
        is_develop_user: some data only shown to develop users, other users no need to analyse it.
    """
    input_file = validate_and_normalize_path(input_file)
    time_line = []
    # ts_map is used to store the start time of each event_stage_tid_pid
    ts_map = {}
    memory_header = [
        'tid', 'pid', 'parent_pid', 'module_name', 'event', 'stage', 'level', 'start_end', 'custom_info',
        'memory_usage(kB)', 'time_stamp(us)'
    ]
    memory_info = []
    with open(input_file, 'r') as f:
        for row in csv.DictReader(f):
            try:
                level = row['level']
                if level == '0' and not is_develop_user:
                    continue
                if int(row['time_stamp(us)']) > 0:
                    _extract_timeline_item(row, time_line, ts_map)
                if int(row['memory_usage(kB)']) > 0:
                    memory_info.append(row)
            except KeyError as e:
                logger.error("Error occur when analyse line: %s, Details is: %s", row, e)
                continue
    if memory_info:
        with os.fdopen(os.open(output_memory_file, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), 'w') as csv_file:
            csv_writer = csv.DictWriter(csv_file, fieldnames=memory_header)
            csv_writer.writeheader()
            for item in memory_info:
                csv_writer.writerow(item)
        os.chmod(output_memory_file, stat.S_IREAD | stat.S_IWRITE)
    else:
        logger.warning("No memory_usage is record in file: %s", input_file)

    if ts_map:
        logger.warning("Only start time is record for these items:")
        for k, v in ts_map.items():
            logger.warning("event_stage_tid_pid: %s, time: %d us.", k, v)
            last_dash = k.rfind('_')
            if last_dash == -1:
                logger.error("Can't find pid in the event_stage_tid_pid string: %s", k)
                continue
            second_last_dash = k.rfind('_', 0, last_dash - 1)
            if second_last_dash == -1:
                logger.error("Can't find tid in the event_stage_tid_pid string: %s", k)
                continue
            pid = k[last_dash + 1:]
            tid = k[second_last_dash + 1: last_dash]
            title = k[:second_last_dash]
            unfinished_timeline = {'name': title, 'pid': pid, 'tid': tid, 'ph': 'B', 'ts': int(v)}
            time_line.append(unfinished_timeline)

    if time_line:
        timeline_file = validate_and_normalize_path(output_timeline_file)
        with os.fdopen(os.open(timeline_file, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), 'w') as json_file:
            json.dump(time_line, json_file)
        os.chmod(timeline_file, stat.S_IREAD | stat.S_IWRITE)
    else:
        logger.warning("No valid time_stamp is record in file: %s", input_file)


def _ascend_graph_msprof_generator(mindstudio_profiler_output, model_iteration_dict):
    """Executing the msprof export mode."""
    try:
        ProfilerInfo.set_export_start_time(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()))
        msprof_exporter = AscendMsprofExporter(mindstudio_profiler_output)
        flag = msprof_exporter.export(model_iteration_dict)
        ProfilerInfo.set_export_end_time(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()))
        return flag

    except ProfilerException as err:
        logger.warning(err.message)
        return False


def _ascend_graph_msprof_analyse(mindstudio_profiler_output):
    """
    Ascend graph model msprof data analyse.

    Returns:
        list[obj]: The list is : df_op_summary, df_op_statistic, df_step_trace, df_step_trace_model
    """
    res = ([], [], [], [])
    try:
        msprof_analyser = AscendMsprofDataGenerator(mindstudio_profiler_output)
        res = msprof_analyser.parse()
        return res

    except ProfilerException as err:
        logger.warning(err.message)
    finally:
        pass
    return res


class Profiler:
    r"""
    This class to enable the profiling of MindSpore neural networks.
    MindSpore users can import the mindspore.Profiler, initialize the Profiler object to start profiling,
    and use Profiler.analyse() to stop profiling and analyse the results.
    Users can visualize the results using the `MindSpore Insight
    <https://www.mindspore.cn/mindinsight/docs/en/master/index.html>`_ tool.
    Now, Profiler supports AICORE operator, AICPU operator, HostCPU operator, memory,
    correspondence, cluster, etc data analysis.

    Args:
        output_path (str, optional): Output data path. Default: ``"./data"`` .
        op_time (bool, optional): (Ascend/GPU) Whether to collect operators performance data. Default value: ``True``.
        profile_communication (bool, optional): (Ascend only) Whether to collect communication performance data in
            a multi devices training,collect when True. Setting this parameter has no effect during single card
            training. When using this parameter, `op_time` must be set to ``True`` . Default: ``False`` .
        profile_memory (bool, optional): (Ascend only) Whether to collect tensor memory data, collect when ``True`` .
            When using this parameter, `op_time` must be set to True. Default: ``False`` .
        parallel_strategy (bool, optional): (Ascend only) Whether to collect parallel policy performance data.
            Default value: ``True`` .
        start_profile (bool, optional): The start_profile parameter controls whether to enable or disable performance
            data collection based on conditions. Default: ``True`` .
        aicore_metrics (int, optional): (Ascend only) Types of AICORE performance data collected, when using this
            parameter, `op_time` must be set to ``True`` , and the value must be in [-1, 0, 1, 2, 3, 4, 5],
            Default: ``0`` , the data items contained in each metric are as follows:

            - -1: Does not collect AICORE data.
            - 0: ArithmeticUtilization contains mac_fp16/int8_ratio, vec_fp32/fp16/int32_ratio, vec_misc_ratio etc.
            - 1: PipeUtilization contains vec_ratio, mac_ratio, scalar_ratio, mte1/mte2/mte3_ratio, icache_miss_rate
              etc.
            - 2: Memory contains ub_read/write_bw, l1_read/write_bw, l2_read/write_bw, main_mem_read/write_bw etc.
            - 3: MemoryL0 contains l0a_read/write_bw, l0b_read/write_bw, l0c_read/write_bw etc.
            - 4: ResourceConflictRatio contains vec_bankgroup/bank/resc_cflt_ratio etc.
            - 5: MemoryUB contains ub_read/write_bw_mte, ub_read/write_bw_vector, ub\_/write_bw_scalar etc.
            - 6: L2Cache contains write_cache_hit, write_cache_miss_allocate, r0_read_cache_hit, r1_read_cache_hit etc.

        l2_cache (bool, optional): (Ascend only) Whether to collect l2 cache data, collect when True.
            Default: ``False`` .
        hbm_ddr (bool, optional): (Ascend only) Whether to collect HBM/DDR read and write rate data, collect when True.
            Default: ``False`` .
        pcie (bool, optional): (Ascend only) Whether to collect PCIe bandwidth data, collect when True.
            Default: ``False`` .
        sync_enable (bool, optional): (GPU only) Whether the profiler collects operators in a synchronous way.
            Default: ``True`` .

            - True: The synchronous way. Before sending the operator to the GPU, the CPU records the start timestamp.
              Then the operator is returned to the CPU after execution, and the end timestamp is recorded,
              The duration of the operator is the difference between the two timestamps.
            - False: The asynchronous way. The duration of the operator is that of sending from the CPU to the GPU.
              This method can reduce the impact of adding profiler on overall training time.
        data_process (bool, optional): (Ascend/GPU) Whether to collect data to prepare performance data.
            Default value: ``True`` .
        timeline_limit (int, optional): (Ascend/GPU) Set the maximum storage size of the timeline file (unit M).
            When using this parameter, `op_time` must be set to True. Default value: ``500`` .
        profile_framework (str, optional): (Ascend/GPU) The host information to collect, it must be one of
            ["all", "time", "memory", None], When is not set to None, a subdirectory host_info will be generated in the
            specified profiler directory, which stores the collected memory and time files on the Host side.
            Default: "all".

            - "all": Record both host timestamp and host memory usage.
            - "time": Only record host timestamp.
            - "memory": Only record host memory usage.
            - None: Not record host information.

    Raises:
        RuntimeError: When the version of CANN does not match the version of MindSpore,
            MindSpore cannot parse the generated ascend_job_id directory structure.

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore as ms
        >>> from mindspore import nn
        >>> import mindspore.dataset as ds
        >>> from mindspore import Profiler
        >>>
        >>> class Net(nn.Cell):
        ...     def __init__(self):
        ...         super(Net, self).__init__()
        ...         self.fc = nn.Dense(2,2)
        ...     def construct(self, x):
        ...         return self.fc(x)
        >>>
        >>> def generator():
        ...     for i in range(2):
        ...         yield (np.ones([2, 2]).astype(np.float32), np.ones([2]).astype(np.int32))
        >>>
        >>> def train(net):
        ...     optimizer = nn.Momentum(net.trainable_params(), 1, 0.9)
        ...     loss = nn.SoftmaxCrossEntropyWithLogits(sparse=True)
        ...     data = ds.GeneratorDataset(generator, ["data", "label"])
        ...     model = ms.train.Model(net, loss, optimizer)
        ...     model.train(1, data)
        >>>
        >>> if __name__ == '__main__':
        ...     # If the device_target is GPU, set the device_target to "GPU"
        ...     ms.set_context(mode=ms.GRAPH_MODE, device_target="Ascend")
        ...
        ...     # Init Profiler
        ...     # Note that the Profiler should be initialized before model.train
        ...     profiler = Profiler()
        ...
        ...     # Train Model
        ...     net = Net()
        ...     train(net)
        ...
        ...     # Profiler end
        ...     profiler.analyse()
    """

    _hwts_output_filename_target = "output_format_data_hwts_"
    _opcompute_output_filename_target = "output_op_compute_time_"
    _aicpu_op_output_filename_target = "output_data_preprocess_aicpu_"
    _has_analysed = False
    _has_initialized = False
    _ascend_profiling_options = ""
    _ascend_job_id = ""
    ENABLE_STATUS = "on"
    DISABLE_STATUS = "off"

    def __init__(self, **kwargs):
        if os.getenv("PROFILING_MODE"):
            raise RuntimeError("Profiling is already enabled by env.")

        self._dev_id = None
        self._cpu_profiler = None
        self._gpu_profiler = None
        self._md_profiler = None
        self._is_heterogeneous = False
        self._profiler_manager = None
        self._timeline_meta = []
        self._init_time = None
        self._ascend_job_id = ''
        self._job_id_env = None
        self._filt_optype_names = ''
        self._output_path = ''
        self._rank_size = 1
        self._rank_id = 0
        self._ascend_profiler = None
        self._timeline_size_limit_byte = 500 * 1024 * 1024  # 500MB
        self._parallel_strategy = True
        self._model_iteration_dict = None
        _environment_check()
        # default aicore_metrics type is ArithmeticUtilization
        self._aicore_metrics_id = 0
        self._l2_cache = self.DISABLE_STATUS
        self._hbm_ddr = self.DISABLE_STATUS
        self._pcie = self.DISABLE_STATUS
        self._data_process = True
        self._op_time = True
        self._profile_communication = False
        self._has_started = False
        self._has_started_twice = False
        self.start_profile = True
        self._profile_memory = False
        self._sync_enable = True
        self._stop_time = 0
        self._dynamic_status = False
        self._profile_framework = "all"
        self._msprof_enable = os.getenv("PROFILER_SAMPLECONFIG")
        self._pretty_json = False
        self._analyse_only = kwargs.get("analyse_only", False)
        if self._msprof_enable:
            return
        self._start_time = int(time.time() * 1e6)  # us
        self._monotonic_time = int(time.monotonic() * 1e6)  # us
        logger.info("Profiling: start time: %d", self._start_time)
        if kwargs.get("env_enable"):
            self._profiler_init(kwargs)
            return
        Profiler._has_initialized = True
        # get device_id and device_target
        if self._analyse_only:
            self._device_target = DeviceTarget.ASCEND.value
            self._rank_id = kwargs.get("rank_id", 0)
        else:
            self._get_devid_rankid_and_devtarget()
            self._parser_kwargs(kwargs)
            self._get_output_path(kwargs)
            self._decide_device_target(kwargs)
            if self.start_profile:
                self.start()

    @staticmethod
    def _get_prof_rank(prof_path: str):
        """get rank id."""
        sub_dirs = os.listdir(os.path.realpath(prof_path))
        info_json_path = ""
        for sub_dir in sub_dirs:
            if sub_dir.startswith("device_"):
                device_id = sub_dir.split("_")[-1]
                info_json_path = os.path.join(prof_path, sub_dir, f"info.json.{device_id}")
        if not os.path.exists(info_json_path):
            return -1
        rank_id, _ = Profiler._parse_info_json(info_json_path)
        return rank_id

    @staticmethod
    def _check_output_path(output_path):
        """Checking path validity."""
        try:
            output_path = validate_and_normalize_path(output_path)
        except RuntimeError as err:
            raise ProfilerPathErrorException(f'profiling data output path {output_path} is invalid.') from err
        finally:
            pass
        if not os.path.isdir(output_path):
            raise ProfilerDirNotFoundException(output_path)
        return output_path

    @staticmethod
    def _parse_job_start_time(prof_dir):
        """
        Get the start time of the job.

        Args:
             input_file (str): The file path of the host start log file.

        Returns:
            str, job start time.
        """
        AscendMsprofExporter.check_msprof_env()
        script_path = AscendMsprofExporter.get_msprof_info_path()
        if not script_path:
            logger.warning("Can`t find get_msprof_info.py path, use single-export mode instead.")
            return None

        logger.info("get_msprof_info.py path is : %s", script_path)
        host_dir = os.path.join(prof_dir, 'host')
        cmd = ['python',
               script_path,
               '-dir', host_dir]
        try:
            outs, _ = AscendMsprofExporter.run_cmd(cmd)
            if not outs:
                logger.warning('Can`t find the msprof info result')
                return None
            result = json.loads(outs)
            if result.get('status', 1) == 1:
                return None
            jor_start_time = result.get('data', {}).get('collection_info', {}).get('Collection start time', None)
            if jor_start_time is not None:
                return float(jor_start_time.strip())
            return None
        except (RuntimeError, JSONDecodeError, AttributeError) as err:
            logger.warning('Get the drvVersion error, use single-export mode instead. detail : %s', err)
            return None

    @staticmethod
    def _parse_info_json(info_file):
        """
        Parse info log file, get the rank id and device id of the job.
        Args:
             input_file (str): The file path of the parse info log file.

        Returns:
            rank id, device id
        """
        with open(info_file, "r") as f:
            info_dict = json.load(f)

            rank_id = info_dict.get("rank_id", 0)
            dev_info = info_dict.get("DeviceInfo", [])
            dev_id = dev_info[0].get("id", -1)

            if int(rank_id) < 0:
                rank_id = 0

            return str(rank_id), str(dev_id)

    @classmethod
    def offline_analyse(cls, path: str, pretty=False, step_list=None):
        """
        Analyze training performance data offline, which is invoked after performance data collection is completed.

        Args:
            path (str): The profiling data path which need to be analyzed offline.
                There needs to be a profiler directory in this path.
            pretty (bool, optional): Whether to pretty json files. Default: ``False``.
            step_list (list, optional): A list of steps that need to be analyzed. Default: ``None``.
                By default, all steps will be analyzed.

        Examples:
            >>> from mindspore import Profiler
            >>> Profiler.offline_analyse("./profiling_path")
        """
        profiler_path = os.path.join(path, "profiler")
        if not os.path.exists(profiler_path):
            raise ProfilerPathErrorException(f'There must be a profiler folder in the data path: {path}.')

        rank_set = set()
        sub_dirs = os.listdir(os.path.realpath(profiler_path))
        for sub_dir in sub_dirs:
            sub_path = os.path.join(profiler_path, sub_dir)
            if os.path.isdir(sub_path) and re.match(r"^PROF_\d+_\d+_[a-zA-Z0-9]+", sub_dir):
                rank = cls._get_prof_rank(sub_path)
                rank_set.add(rank)
        if not rank_set:
            return

        process_list = []
        for rank_id in rank_set:
            profiler = cls(analyse_only=True, rank_id=rank_id)
            process = Process(target=profiler.analyse,
                              args=(path, pretty, step_list))
            process.start()
            process_list.append(process)

        for process in process_list:
            process.join()

    def op_analyse(self, op_name, device_id=None):
        """
        Profiler users can use this interface to obtain operator performance data.

        Args:
            op_name (str or list): The primitive operator name to query.
            device_id (int, optional): ID of the target device. This parameter is optional during network training or
                inference, and users can use device_id parameter to specify which card operator performance data to
                parse. If this interface is used for offline data parsing, Default: ``0`` .

        Raises:
            TypeError: If the `op_name` parameter type is incorrect.
            TypeError: If the `device_id` parameter type is incorrect.
            RuntimeError: If MindSpore runs on Ascend, this interface cannot be used.

        Supported Platforms:
            ``GPU`` ``CPU``

        Examples:
            >>> from mindspore import Profiler
            >>> from mindspore import nn
            >>> from mindspore import Model
            >>> # Profiler init.
            >>> profiler = Profiler()
            >>> # Train Model or eval Model, taking LeNet5 as an example.
            >>> # Refer to https://gitee.com/mindspore/docs/blob/master/docs/mindspore/code/lenet.py
            >>> net = LeNet5()
            >>> optimizer = nn.Momentum(net.trainable_params(), learning_rate=0.1, momentum=0.9)
            >>> loss = nn.SoftmaxCrossEntropyWithLogits(sparse=True)
            >>> # Create the dataset taking MNIST as an example.
            >>> # Refer to https://gitee.com/mindspore/docs/blob/master/docs/mindspore/code/mnist.py
            >>> dataloader = create_dataset()
            >>> model = Model(net, loss, optimizer)
            >>> model.train(5, dataloader, dataset_sink_mode=False)
            >>>
            >>> # Profiler end
            >>> profiler.analyse()
            >>>
            >>> profiler.op_analyse(op_name=["BiasAdd", "Conv2D"])
        """
        if self._device_target == 'ascend':
            raise RuntimeError("The Interface 'Profiler.op_analyse()' is not supported on Ascend currently.")
        if device_id and not isinstance(device_id, int):
            raise TypeError(f"For 'Profiler.op_analyse()', the parameter device_id must be int, "
                            f"but got type {type(device_id)}")
        online_device_id = int(self._dev_id)
        self._dev_id = self._dev_id if device_id is None else device_id
        if self._dev_id is None:
            self._dev_id = 0
        if not isinstance(op_name, str) and not isinstance(op_name, list):
            raise TypeError(f"For 'Profiler.op_analyse()', the parameter op_name must be str or list, "
                            f"but got type {type(op_name)}")
        if not op_name:
            raise TypeError(f"For 'Profiler.op_analyse()', the parameter op_name cannot be "", '' or [].")
        parser = GpuFrameWorkParser(self._output_path, self._dev_id, op_name)
        op_info = parser.parse()
        if self._rank_size > 1:
            if online_device_id == int(self._dev_id):
                return op_info
            if online_device_id != int(self._dev_id):
                message = f"For 'Profiler.op_analyse()', the parameter device_id is equal to {self._dev_id}, but the " \
                          f"current device id is {online_device_id}, so no operator performance information is queried."
                return message
        return op_info

    def analyse(self, offline_path=None, pretty=False, step_list=None):
        """
        Collect and analyze training performance data, support calls during and after training. The example shows above.

        Args:
            offline_path (Union[str, None], optional): The data path which need to be analyzed with offline mode.
                Offline mode isused in abnormal exit scenario. This parameter should be set to ``None``
                for online mode. Default: ``None``.
            pretty (bool, optional): Whether to pretty json files. Default: ``False``.
            step_list (list, optional): A list of steps that need to be analyzed. Default: ``None``.
                By default, all steps will be analyzed.
        """
        if isinstance(pretty, bool):
            self._pretty_json = pretty
        model_iteration_dict = {}
        if step_list is not None and not isinstance(step_list, list):
            raise ProfilerParamTypeErrorException("Parameter step_list must be a list.")
        if step_list:
            if not isinstance(step_list[0], int):
                raise ProfilerParamTypeErrorException("The elements of the parameter step_list must be integers.")
            model_iteration_dict.setdefault(4294967295, []).append(step_list[0])
        self._analyse(offline_path=offline_path, model_iteration_dict=model_iteration_dict)

    def _analyse(self, offline_path=None, model_iteration_dict=None):
        """
        Collect and analyze training performance data, support calls during and after training. The example shows above.

        Args:
            offline_path (Union[str, None], optional): The data path which need to be analysed with offline mode.
                Offline mode isused in abnormal exit scenario. This parameter should be set to ``None``
                for online mode. Default: ``None``.
            model_iteration_dict: Dictionary with model id as the key and iteration id as the value, Default: ``None``.
        """
        self._model_iteration_dict = model_iteration_dict
        self._init_profiler_info()
        self._is_support_step_info_collect()
        parallel_mode = get_auto_parallel_context("parallel_mode")
        stage_num = get_auto_parallel_context("pipeline_stages")

        ProfilerInfo.set_parallel_info(parallel_mode, stage_num)
        ProfilerInfo.set_rank_size(self._rank_size)
        ProfilerInfo.set_heterogeneous(self._is_heterogeneous)
        if offline_path:
            ProfilerInfo.set_analyse_start_time(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()))
            self._ascend_graph_analyse(offline_path=offline_path)
            ProfilerInfo.set_analyse_end_time(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()))
            ProfilerInfo.save(self._output_path)
            _offline_parse(offline_path)
            return
        if self._msprof_enable:
            return

        # Stop data collection after all operators are executed.
        _pynative_executor.sync()

        Profiler._has_initialized = False
        self._dynamic_status = self._profiler_manager.dynamic_status()
        _environment_check()

        cpu_op_file = glob.glob(os.path.join(self._output_path, 'cpu_op_type_info_*'))
        if self._device_target and self._device_target != DeviceTarget.CPU.value and cpu_op_file:
            self._is_heterogeneous = True
        ProfilerInfo.set_analyse_start_time(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()))
        if self._device_target and self._device_target == DeviceTarget.CPU.value:
            self._cpu_analyse()

        if self._device_target and self._device_target == DeviceTarget.GPU.value:
            self._gpu_analyse()

        elif self._device_target and self._device_target == DeviceTarget.ASCEND.value:
            self._ascend_analyse()
        if self._profile_framework:
            if self._device_target != DeviceTarget.CPU.value:
                self._host_info_analyse()
            else:
                logger.warning("The parameter 'profile_framework' is not support for CPU, so there no host_info"
                               " directory in the output path.")
        logger.info("Profiling: all the data have been analyzed.")
        ProfilerInfo.set_analyse_end_time(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()))
        ProfilerInfo.save(self._output_path)

    def start(self):
        """
        Used for Ascend, GPU, start profiling. Profiling can be turned on based on step and epoch.

        Raises:
            RuntimeError: If the profiler has already started.
            RuntimeError: If the `start_profile` parameter is not set or is set to ``True``.

        Examples:
            >>> from mindspore.train import Callback
            >>> from mindspore import Profiler
            >>> class StopAtStep(Callback):
            ...     def __init__(self, start_step, stop_step):
            ...         super(StopAtStep, self).__init__()
            ...         self.start_step = start_step
            ...         self.stop_step = stop_step
            ...         self.profiler = Profiler(start_profile=False)
            ...
            ...     def step_begin(self, run_context):
            ...         cb_params = run_context.original_args()
            ...         step_num = cb_params.cur_step_num
            ...         if step_num == self.start_step:
            ...             self.profiler.start()
            ...
            ...     def step_end(self, run_context):
            ...         cb_params = run_context.original_args()
            ...         step_num = cb_params.cur_step_num
            ...         if step_num == self.stop_step:
            ...             self.profiler.stop()
            ...
            ...     def end(self, run_context):
            ...         self.profiler.analyse()
        """
        if self._msprof_enable:
            return

        if not self._has_started:
            if not self._has_started_twice:
                self._has_started = True
        else:
            raise RuntimeError("The profiler has already started. Do not turn on again in the open state.")

        self._cpu_profiler.step_profiling_enable(True)
        if self._op_time:
            self._cpu_profiler.enable_op_time()

        if self._device_target and self._device_target == DeviceTarget.GPU.value:
            if self._data_process:
                self._md_profiler.start()
                self._gpu_profiler.data_process_enable(True)
            if self._profile_framework or self._op_time:
                self._gpu_profiler.step_profiling_enable(True)
                if self._op_time:
                    self._gpu_profiler.enable_op_time()
        elif self._device_target and self._device_target == DeviceTarget.ASCEND.value:
            if self._data_process:
                self._md_profiler.start()
            self._ascend_graph_start()
        ProfilerInfo.set_profiling_start_time(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()))
        ProfilerInfo.set_system_time(int(c_expression.get_clock_time() * 1e3)) # cast us to ns
        ProfilerInfo.set_system_cnt(c_expression.get_clock_syscnt())

    def stop(self):
        """
        Used for Ascend, GPU, stop profiling. Profiling can be turned off based on step and epoch.

        Raises:
            RuntimeError: If the profiler has not started, this function is disabled.

        Examples:
            >>> from mindspore.train import Callback
            >>> from mindspore import Profiler
            >>> class StopAtEpoch(Callback):
            ...     def __init__(self, start_epoch, stop_epoch):
            ...         super(StopAtEpoch, self).__init__()
            ...         self.start_epoch = start_epoch
            ...         self.stop_epoch = stop_epoch
            ...         self.profiler = Profiler(start_profile=False)
            ...
            ...     def epoch_begin(self, run_context):
            ...         cb_params = run_context.original_args()
            ...         epoch_num = cb_params.cur_epoch_num
            ...         if epoch_num == self.start_epoch:
            ...             self.profiler.start()
            ...
            ...     def epoch_end(self, run_context):
            ...         cb_params = run_context.original_args()
            ...         epoch_num = cb_params.cur_epoch_num
            ...         if epoch_num == self.stop_epoch:
            ...             self.profiler.stop()
            ...
            ...     def end(self, run_context):
            ...         self.profiler.analyse()
        """
        if self._msprof_enable:
            return

        if self._has_started:
            self._has_started = False
        else:
            raise RuntimeError("The profiler has not started, so can not stop. Please call the start() method "
                               "before calling the stop() method.")

        # Stop data collection after all operators are executed.
        _pynative_executor.sync()

        self._cpu_profiler.stop()
        if self._data_process and self._md_profiler is not None:
            self._md_profiler.stop()
            self._md_profiler.save(self._output_path)

        if self._device_target and self._device_target == DeviceTarget.GPU.value:
            self._gpu_profiler.stop()
        elif self._device_target and self._device_target == DeviceTarget.ASCEND.value:
            self._ascend_profiler.stop()

            self._stop_time = int(time.time() * 10000000)
        ProfilerInfo.set_profiling_stop_time(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()))
        self._init_profiler_info()
        ProfilerInfo.set_diff_time(self._start_time - self._monotonic_time)
        ProfilerInfo.save(self._output_path)
        logger.info("Profiling: stop time: %d", self._stop_time)

    def _set_ascend_job_id(self, ascend_job_id):
        """Set output_path for offline parsing performance data."""
        if not ascend_job_id:
            return
        self._ascend_job_id = validate_and_normalize_path(ascend_job_id)
        if not os.path.exists(self._ascend_job_id):
            msg = f"Invalid ascend_job_id: {self._ascend_job_id}, Please pass the absolute path of the JOB dir"
            logger.critical(msg)
            raise ValueError(msg)
        self._output_path, _ = os.path.split(self._ascend_job_id)

    def _profiler_init(self, kwargs):
        """Initialize variables when profiler is enabled by environment variables."""
        options = kwargs.get("env_enable")
        self._has_started = True
        self._start_time = options.get("start_time")
        self._output_path = options.get('file_output_path')
        self._profile_memory = options.get('profile_memory')
        self._parallel_strategy = options.get('parallel_strategy')
        self._timeline_size_limit_byte = options.get('timeline_limit') * 1024 * 1024
        self._data_process = options.get('data_process')
        self._profile_communication = options.get('profile_communication')
        self._op_time = options.get('op_time')
        self._device_target = context.get_context("device_target").lower()
        self._profile_framework = options.get('profile_framework', 'all')
        self._profiler_manager = c_expression.ProfilerManager.get_instance()
        self._cpu_profiler = c_expression.Profiler.get_instance("CPU")
        if self._data_process:
            self._md_profiler = cde.GlobalContext.profiling_manager()
        if self._device_target == DeviceTarget.GPU.value:
            self._gpu_profiler = c_expression.Profiler.get_instance("GPU")

        if self._device_target == DeviceTarget.ASCEND.value:
            self._ascend_profiler = c_expression.Profiler.get_instance("Ascend")
        self._get_devid_rankid_and_devtarget()

    def _init_profiler_info(self):
        """Init profiler info filer."""
        mode = "graph"
        if context.get_context("mode") == context.PYNATIVE_MODE:
            mode = "pynative"
        store_id = self._dev_id if self._device_target == DeviceTarget.GPU.value else self._rank_id
        ProfilerInfo.init_info(mode, store_id)

    def _decide_device_target(self, kwargs):
        """Complete Profiler initialization according to device_target"""
        profiler_manager = c_expression.ProfilerManager
        self._profiler_manager = profiler_manager.get_instance()
        if self._profile_framework is None:
            self._profiler_manager.set_profile_framework("NULL")
        else:
            self._profiler_manager.set_profile_framework(self._profile_framework)
        if self._device_target:
            cpu_profiler = c_expression.Profiler
            self._cpu_profiler = cpu_profiler.get_instance("CPU")
            self._cpu_profiler.init(self._output_path)

        if self._device_target and self._device_target == DeviceTarget.CPU.value:
            self._cpu_profiler_init(kwargs)

        if self._device_target and self._device_target == DeviceTarget.GPU.value:
            self._gpu_profiler_init(kwargs)

        elif self._device_target and self._device_target == DeviceTarget.ASCEND.value:
            self._ascend_profiler_init(kwargs)

    def _cpu_profiler_init(self, kwargs):
        """Cpu profiler init."""
        self.start_profile = kwargs.pop("start_profile", True)
        if not isinstance(self.start_profile, bool):
            raise TypeError(f"For '{self.__class__.__name__}', the parameter start_profile must be bool, "
                            f"but got type {type(self.start_profile)}")

    def _gpu_profiler_init(self, kwargs):
        """Gpu profiler init."""
        # Setup and start MindData Profiling
        if self._data_process:
            self._md_profiler = cde.GlobalContext.profiling_manager()
            self._md_profiler.init()
        self._parse_parameter_for_gpu(kwargs)

        gpu_profiler = c_expression.Profiler
        self._gpu_profiler = gpu_profiler.get_instance("GPU")
        self._gpu_profiler.init(self._output_path)
        self._gpu_profiler.sync_enable(self._sync_enable)
        if GlobalComm.WORLD_COMM_GROUP == "nccl_world_group":
            self._dev_id = str(get_rank())
        os.environ['DEVICE_ID'] = self._dev_id
        self._rank_id = self._dev_id

    def _ascend_profiler_init(self, kwargs):
        """Ascend profiler init."""
        # Setup and start MindData Profiling
        if self._data_process:
            self._md_profiler = cde.GlobalContext.profiling_manager()
            self._md_profiler.init()
        self._init_time = int(time.time() * 10000000)
        logger.info("Profiling: profiling init time: %d", self._init_time)
        self._parse_parameter_for_ascend(kwargs)
        os.environ['DEVICE_ID'] = self._dev_id

        self._ascend_profiling_options = json.dumps(self._construct_profiling_options())
        # Characters longer than 2048 are ignored, resulting in profiling option resolution errors
        if len(self._ascend_profiling_options) > 2048:
            msg = f"For '{self.__class__.__name__}', the environment parameter length exceeds " \
                  f"the limit (2048), please input valid parameters."
            logger.critical(msg)
            raise ValueError(msg)
        # use context interface to open profiling, for the new mindspore version(after 2020.5.21)
        self._ascend_profiler = c_expression.Profiler.get_instance("Ascend")
        self._ascend_profiler.init(self._output_path, int(self._dev_id), self._ascend_profiling_options)
        base_profiling_container_path = os.path.join(self._output_path, "container")
        container_path = os.path.join(base_profiling_container_path, self._dev_id)
        data_path = os.path.join(container_path, "data")
        data_path = validate_and_normalize_path(data_path)
        if not os.path.exists(data_path):
            os.makedirs(data_path, exist_ok=True)

    def _construct_profiling_options(self):
        """
        Construct profiling options to determine which profiling data should be collected.
        """
        fp_point = os.environ.get("PROFILING_FP_START", "")
        bp_point = os.environ.get("PROFILING_BP_END", "")

        profiling_options = {
            "output": self._output_path,
            "fp_point": fp_point,
            "bp_point": bp_point,
            "training_trace": self.ENABLE_STATUS if self._op_time else self.DISABLE_STATUS,
            "task_trace": self.ENABLE_STATUS if self._op_time else self.DISABLE_STATUS,
            "aic_metrics": AICORE_METRICS_DICT.get(self._aicore_metrics_id, "ArithmeticUtilization"),
            "aicpu": self.ENABLE_STATUS if self._data_process or self._op_time else self.DISABLE_STATUS,
            "profile_memory": self.ENABLE_STATUS if self._op_time and self._profile_memory else self.DISABLE_STATUS,
            "hccl": self.ENABLE_STATUS if self._op_time and self._profile_communication else self.DISABLE_STATUS,
            "l2_cache": self._l2_cache,
            "hbm_ddr": self._hbm_ddr,
            "pcie": self._pcie,
            "parallel_strategy": self.ENABLE_STATUS if self._parallel_strategy else self.DISABLE_STATUS,
            "op_time": self.ENABLE_STATUS if self._op_time else self.DISABLE_STATUS,
            "profile_framework": self._profile_framework
        }

        return profiling_options

    def _parse_parameter_for_gpu(self, kwargs):
        """Parse parameter in Profiler when the device target is GPU."""
        self.start_profile = kwargs.pop("start_profile", True)
        if not isinstance(self.start_profile, bool):
            raise TypeError(f"For '{self.__class__.__name__}', the parameter start_profile must be bool, "
                            f"but got type {type(self.start_profile)}")

        self._sync_enable = kwargs.pop("sync_enable", True)
        if not isinstance(self._sync_enable, bool):
            logger.warning("The parameter sync_enable is an invalid value, it will be set to True.")
            self._sync_enable = True

    def _parse_parameter_for_ascend(self, kwargs):
        """Parse parameter in Profiler when the device target is Ascend."""
        ascend_job_id = kwargs.pop("ascend_job_id", "")
        self._set_ascend_job_id(ascend_job_id)
        self.start_profile = kwargs.pop("start_profile", True)
        if not isinstance(self.start_profile, bool):
            raise TypeError(f"For '{self.__class__.__name__}', the parameter start_profile must be bool, "
                            f"but got type {type(self.start_profile)}")

        self._profile_communication = kwargs.pop("profile_communication", False)
        if not isinstance(self._profile_communication, bool):
            logger.warning(f"For '{self.__class__.__name__}', the parameter profile_communication must be bool, "
                           f"but got type {type(self._profile_communication)}, it will be set to False.")
            self._profile_communication = False

        if self._profile_communication:
            hccl_option = {"output": self._output_path, "task_trace": self.ENABLE_STATUS}
            os.environ['PROFILING_OPTIONS'] = json.dumps(hccl_option)

        self._profile_memory = kwargs.pop("profile_memory", False)
        if not isinstance(self._profile_memory, bool):
            logger.warning(f"For '{self.__class__.__name__}', the parameter profile_memory must be bool, "
                           f"but got type {type(self._profile_memory)}, it will be set to False.")
            self._profile_memory = False

        self._aicore_metrics_id = kwargs.pop("aicore_metrics", 0)
        if not isinstance(self._aicore_metrics_id, int):
            logger.warning(f"For '{self.__class__.__name__}', the parameter aicore_metrics must be int, "
                           f"but got type {type(self._aicore_metrics_id)}, it will be set to 0.")
            self._aicore_metrics_id = 0

        if self._aicore_metrics_id not in AICORE_METRICS_DICT:
            logger.warning(f"For '{self.__class__.__name__}', the parameter aicore_metrics must be in "
                           f"[-1, 0, 1, 2, 3, 4, 5], but got {self._aicore_metrics_id}, it will be set to 0.")
            self._aicore_metrics_id = 0

        l2_cache_enable = kwargs.pop("l2_cache", False)
        if not isinstance(l2_cache_enable, bool):
            logger.warning(f"For '{self.__class__.__name__}', the parameter l2_cache must be bool, "
                           f"but got type {type(l2_cache_enable)}, it will be set to False.")
            l2_cache_enable = False
        self._l2_cache = self.ENABLE_STATUS if l2_cache_enable else self.DISABLE_STATUS

        hbm_ddr_enable = kwargs.pop("hbm_ddr", False)
        if not isinstance(hbm_ddr_enable, bool):
            logger.warning(f"For '{self.__class__.__name__}', the parameter hbm_ddr must be bool, "
                           f"but got type {type(hbm_ddr_enable)}, it will be set to False.")
            hbm_ddr_enable = False
        self._hbm_ddr = self.ENABLE_STATUS if hbm_ddr_enable else self.DISABLE_STATUS

        pcie_enable = kwargs.pop("pcie", False)
        if not isinstance(pcie_enable, bool):
            logger.warning(f"For '{self.__class__.__name__}', the parameter pcie must be bool, "
                           f"but got type {type(pcie_enable)}, it will be set to False.")
            pcie_enable = False
        self._pcie = self.ENABLE_STATUS if pcie_enable else self.DISABLE_STATUS

        self._parallel_strategy = kwargs.pop("parallel_strategy", True)
        if not isinstance(self._parallel_strategy, bool):
            logger.warning(f"For '{self.__class__.__name__}', the parameter parallel_strategy must be bool, "
                           f"but got type {type(self._parallel_strategy)}, it will be set to True.")
            self._parallel_strategy = True

    def _ascend_analyse(self):
        """Collect and analyse ascend performance data."""
        self._rank_size = 1
        if self._profile_communication and not GlobalComm.INITED:
            self._profile_communication = False

        if GlobalComm.INITED:
            self._rank_size = get_group_size()
        else:
            self._rank_size = int(os.getenv('RANK_SIZE', '1'))

        if self._has_started:
            self.stop()
        else:
            logger.info("No need to stop profiler because profiler has been stopped.")
        # export op data before analyse
        self._ascend_graph_analyse()

    def _minddata_analyse(self, source_path):
        """Analyse mindadata for ascend graph model."""
        if not self._data_process:
            return
        store_id = self._rank_id if self._device_target == DeviceTarget.ASCEND.value else self._dev_id
        # Parsing minddata AICPU profiling
        if self._device_target == DeviceTarget.ASCEND.value:
            logger.info("Profiling: analyzing the minddata AICPU data.")
            MinddataParser.execute(source_path, self._output_path, store_id)

        # parse minddata pipeline operator and queue
        try:
            MinddataPipelineParser(self._output_path, store_id, self._output_path).parse()
        except ProfilerException as err:
            logger.warning(err.message)
        finally:
            pass

        # Analyze minddata information
        logger.info("Profiling: analyzing the minddata information.")
        try:
            MinddataProfilingAnalyzer(self._output_path, store_id,
                                      self._output_path, pretty=self._pretty_json).analyze()
        except ProfilerException as err:
            logger.warning(err.message)
        finally:
            pass

    def _ascend_fpbp_analyse(self, op_summary, steptrace):
        """
        Ascned graph model op analyse.

        Returns:
            dict[obj]: points: the fp bp information
        """
        points = None
        try:
            dev_id = self._rank_id if self._device_target == DeviceTarget.ASCEND.value else self._dev_id
            step_trace_point_info_path = os.path.join(self._output_path, f'step_trace_point_info_{dev_id}.json')

            step_trace_point_info_path = validate_and_normalize_path(step_trace_point_info_path)

            fpbp_analyse = AscendFPBPGenerator(op_summary, steptrace, pretty=self._pretty_json)
            points, _ = fpbp_analyse.parse()
            fpbp_analyse.write(step_trace_point_info_path)
        except ProfilerException as err:
            logger.warning(err.message)
        finally:
            pass
        return points

    def _ascend_op_analyse(self, op_summary, op_statistic, dynamic_status, launch_ops: List):
        """
        Ascend graph model hwts analyse.

        Returns:
            list[obj]: The list is: framework_parser, aicpu_data_parser, optime_parser, op_task_dict
        """
        try:
            dev_id = self._rank_id if self._device_target == DeviceTarget.ASCEND.value else self._dev_id

            op_intermediate_detail_path = os.path.join(self._output_path,
                                                       f'aicore_intermediate_{dev_id}_detail.csv')
            op_intermediate_type_path = os.path.join(self._output_path, f'aicore_intermediate_{dev_id}_type.csv')
            aicpu_intermediate_detail_path = os.path.join(self._output_path, f'aicpu_intermediate_{dev_id}.csv')
            framework_raw_path = os.path.join(self._output_path, f'framework_raw_{dev_id}.csv')

            op_intermediate_detail_path = validate_and_normalize_path(op_intermediate_detail_path)
            op_intermediate_type_path = validate_and_normalize_path(op_intermediate_type_path)
            aicpu_intermediate_detail_path = validate_and_normalize_path(aicpu_intermediate_detail_path)
            framework_raw_path = validate_and_normalize_path(framework_raw_path)

            if context.get_context("mode") == context.GRAPH_MODE:
                output_timeline_data_path = os.path.join(self._output_path, f'output_timeline_data_{dev_id}.txt')
                output_timeline_data_path = validate_and_normalize_path(output_timeline_data_path)
            else:
                output_timeline_data_path = None

            op_analyser = AscendOPGenerator(op_summary, op_statistic, dynamic_status, launch_ops)
            op_analyser.parse()
            op_analyser.write(op_intermediate_detail_path, op_intermediate_type_path,
                              aicpu_intermediate_detail_path, framework_raw_path, output_timeline_data_path)
        except ProfilerException as err:
            logger.warning(err.message)
        finally:
            pass

    def _ascend_step_trace_analyse(self, steptrace):
        """Analyse step trace info."""
        try:
            if not self._dynamic_status:
                dev_id = self._rank_id if self._device_target == DeviceTarget.ASCEND.value else self._dev_id
                step_trace_intermediate_path = os.path.join(self._output_path,
                                                            f'step_trace_raw_{dev_id}_detail_time.csv')

                step_trace_intermediate_path = validate_and_normalize_path(step_trace_intermediate_path)

                steptrace_analyser = AscendStepTraceGenerator(steptrace)
                steptrace_analyser.parse()
                steptrace_analyser.write(step_trace_intermediate_path)
        except ProfilerException as err:
            logger.warning(err.message)
        finally:
            pass

    def _ascend_timeline_analyse(self, op_summary, steptrace, source_path, mindstudio_profiler_output) -> List:
        """Analyse timeline info."""
        try:
            logger.info("Profiling: analyzing the timeline data")
            timeline_analyser = AscendTimelineGenerator(self._output_path, source_path, mindstudio_profiler_output,
                                                        self._rank_id, context.get_context('mode'))
            timeline_analyser.parse_cluster_data(op_summary, steptrace)
            timeline_analyser.parse_timeline_data(pretty=self._pretty_json)
            timeline_analyser.write_timeline_display()
            timeline_analyser.write_timeline_summary()

        except (ProfilerIOException, ProfilerFileNotFoundException, RuntimeError) as err:
            logger.warning('Fail to write timeline data: %s', err)
        finally:
            pass
        return timeline_analyser.get_kernel_event_list()

    def _ascend_dynamic_net_analyse(self, op_summary):
        """Analyse dynamic shape network info."""
        if self._profile_communication:
            logger.warning(
                "The profile_communication parameter cannot be set on the dynamic shape network.")
        if self._profile_memory:
            logger.warning("The profile_memory parameter cannot be set on the dynamic shape network.")
        logger.warning(
            "[Profiler]Dynamic Shape network does not support collecting step trace performance data currently.")
        dynamic_parser = DynamicFrameWorkParser(self._output_path, self._rank_id, pretty=self._pretty_json)
        dynamic_parser.write_dynamic_shape_data(op_summary)

    def _ascend_flops_analyse(self, op_summary, launch_ops):
        """Get op FLOPs from op_summary, write output_op_flops_x.csv."""
        if 'vector_fops' not in op_summary.dtype.names and 'cube_fops' not in op_summary.dtype.names:
            logger.warning("[Profiler] Can not found cube fops and vector fops data in the op summary.")
            return

        try:
            dev_id = self._rank_id if self._device_target == DeviceTarget.ASCEND.value else self._dev_id

            flops_path = os.path.join(self._output_path, f'flops_{dev_id}.txt')
            flops_summary_path = os.path.join(self._output_path, f'flops_summary_{dev_id}.json')

            flops_path = validate_and_normalize_path(flops_path)
            flops_summary_path = validate_and_normalize_path(flops_summary_path)

            flops_analyser = AscendFlopsGenerator(op_summary, launch_ops, pretty=self._pretty_json)
            flops_analyser.parse()
            flops_analyser.write(flops_path, flops_summary_path)

        except ProfilerException as err:
            logger.warning(err.message)
        finally:
            pass

    def _ascend_graph_memory_analyse(self, points):
        """Analyse memory usage info."""
        if not self._profile_memory:
            return
        if self._profile_memory and context.get_context("mode") == context.PYNATIVE_MODE:
            logger.warning("[Profiler]The parameter profile_memory is not supported on Ascend "
                           "PyNative mode currently.")
        try:
            logger.info("Profiling: analyzing the memory usage info.")
            self._analyse_memory_usage(points)
        except (ProfilerIOException, ProfilerFileNotFoundException, ProfilerRawFileException) as err:
            logger.warning(err.message)
        finally:
            pass

    def _ascend_ms_analyze(self, source_path):
        """Ascend ms generate"""

        timestamp = time.strftime("%Y%m%d%H%M%S", time.localtime(time.time()))
        if self._rank_id:
            ascend_ms_path = f"rank-{self._rank_id}_{timestamp}_ascend_ms"
        else:
            ascend_ms_path = f"{socket.gethostname()}--{os.getpid()}_{timestamp}_ascend_ms"
        ascend_ms_path = os.path.join(self._output_path, ascend_ms_path)

        dev_id = self._rank_id if self._device_target == DeviceTarget.ASCEND.value else self._dev_id
        ascend_profiler_output_path = os.path.join(ascend_ms_path, 'ASCEND_PROFILER_OUTPUT')
        os.makedirs(ascend_profiler_output_path, exist_ok=True)

        source_profiler_info_path = os.path.join(self._output_path, f"profiler_info_{dev_id}.json")
        target_profiler_info_path = os.path.join(ascend_ms_path, f"profiler_info_{dev_id}.json")
        shutil.copy(source_profiler_info_path, target_profiler_info_path)

        source_timeline_path = os.path.join(self._output_path, f"ascend_timeline_display_{dev_id}.json")
        target_timeline_path = os.path.join(ascend_profiler_output_path, f"trace_view.json")
        shutil.copy(source_timeline_path, target_timeline_path)

        self._ascend_graph_cluster_analyse(source_path, ascend_profiler_output_path)
        self._ascend_graph_communicate_analyse(source_path, ascend_profiler_output_path)

    def _ascend_graph_cluster_analyse(self, source_path, ascend_profiler_output_path):
        """Analyse step trace time info"""

        try:
            logger.info("Profiling: analyzing the step trace time profiler info.")

            step_trace_time_path = os.path.join(ascend_profiler_output_path, f'step_trace_time.csv')
            step_trace_time_path = validate_and_normalize_path(step_trace_time_path)

            cluster_analyse = AscendClusterGenerator(source_path)
            cluster_analyse.parse()
            cluster_analyse.write(step_trace_time_path)
        except (ProfilerIOException, ProfilerFileNotFoundException, ProfilerRawFileException) as err:
            logger.warning(err.message)
        finally:
            pass

    def _ascend_graph_communicate_analyse(self, source_path, ascend_profiler_output_path):
        """Analyse communicate info"""
        if not self._profile_communication:
            return

        try:
            logger.info("Profiling: analyzing the communicate and communicate_matrix profiler info.")

            communication_file_path = os.path.join(ascend_profiler_output_path, f'communication.json')
            communication_file_path = validate_and_normalize_path(communication_file_path)

            communication_matrix_file_path = os.path.join(ascend_profiler_output_path,
                                                          f"communication_matrix.json")
            communication_matrix_file_path = validate_and_normalize_path(communication_matrix_file_path)

            analyze_path = os.path.abspath(os.path.join(source_path, os.path.pardir, 'analyze'))
            communicate_analyser = AscendCommunicationGenerator(analyze_path)
            communicate_analyser.parse()
            communicate_analyser.write(communication_file_path, communication_matrix_file_path)
        except (ProfilerIOException, ProfilerFileNotFoundException, ProfilerRawFileException) as err:
            logger.warning(err.message)
        finally:
            pass

    def _ascend_graph_hccl_analyse(self, mindstudio_profiler_output, steptrace):
        """Analyse hccl profiler info."""
        if not self._profile_communication:
            return
        if self._profile_communication and context.get_context("mode") == context.PYNATIVE_MODE:
            logger.warning("[Profiler]The parameter profile_communication is not supported on Ascend "
                           "PyNative mode currently.")
            return
        try:
            logger.info("Profiling: analyzing the hccl profiler info.")
            dev_id = self._rank_id if self._device_target == DeviceTarget.ASCEND.value else self._dev_id

            hccl_raw_path = os.path.join(self._output_path, f'hccl_raw_{dev_id}.csv')
            hccl_raw_path = validate_and_normalize_path(hccl_raw_path)
            hccl_analyse = AscendHCCLGenerator(mindstudio_profiler_output, steptrace)
            hccl_analyse.parse()
            hccl_analyse.write(hccl_raw_path)

        except (ProfilerIOException, ProfilerFileNotFoundException, ProfilerRawFileException) as err:
            logger.warning(err.message)
        finally:
            pass

    def _ascend_graph_msadvisor_analyse(self, job_id):
        """Call MSAdvisor function."""
        logger.info("MSAdvisor starts running.")
        msadvisor = Msadvisor(job_id, self._rank_id, self._output_path, pretty=self._pretty_json)
        try:
            msadvisor.analyse()
        except FileNotFoundError as err:
            logger.warning("MSAdvisor: command not found,"
                           "please check if installed ascend-toolkit and set environment path correctly. %s", err)
        except OSError as err:
            logger.warning("Cannot execute binary file: Exec format error. %s", err)
        except subprocess.CalledProcessError:
            logger.warning("MSAdvisor running failed, please check MSAdvisor running log.")
        except (ValueError, ProfilerFileNotFoundException) as err:
            logger.warning("MSAdvisor running failed. %s", err)
        finally:
            pass
        if context.get_context("mode") == context.PYNATIVE_MODE:
            logger.warning("Pynative mode does not support MSAdvisor analyzer currently.")

    def _get_kernel_op_map(self, op_summary, kernels: List[CANNEvent]) -> List:
        """Get the mapping between framework operator and device kernel."""
        if not kernels:
            return []
        kernel_map = {}
        for kernel in kernels:
            key = kernel.name if kernel.is_comm_op else (kernel.name, str(kernel.ts))
            kernel_map[key] = kernel.parent
        launch_ops = [None] * len(op_summary)
        for index, summary in enumerate(op_summary):
            ts = str(summary['Task Start Time(us)']).strip("\t")
            name = summary['Op Name']
            key = name if name.startswith("hcom_") else (name, ts)
            launch_op = kernel_map.get(key)
            if not launch_op:
                logger.warning(f"Failed to get launch operator for {name}!")
                continue
            launch_ops[index] = launch_op.name
        return launch_ops

    def _ascend_graph_analyse(self, offline_path=None):
        """Ascend graph mode analyse."""
        if not offline_path:
            self._ascend_profiler.finalize()

        job_id = self._get_profiling_job_id(offline_path)
        if not job_id:
            return
        logger.info("Profiling: job id is %s ", job_id)

        self._check_output_path(output_path=self._output_path)
        source_path = os.path.join(self._output_path, job_id)
        self._minddata_analyse(source_path)
        if self._op_time:
            mindstudio_profiler_output = os.path.abspath(os.path.join(source_path, os.path.pardir,
                                                                      'mindstudio_profiler_output'))
            flag = _ascend_graph_msprof_generator(mindstudio_profiler_output, self._model_iteration_dict)
            if not flag:
                logger.warning('Current driver package not support all export mode, use single export mode, '
                               'this may lead to performance degradation. Suggest upgrading the driver package.')
            ProfilerInfo.set_export_flag(flag)
            op_summary, op_statistic, steptrace, steptrace_model \
                = _ascend_graph_msprof_analyse(mindstudio_profiler_output)
            kernels = self._ascend_timeline_analyse(op_summary, steptrace, source_path, mindstudio_profiler_output)
            launch_ops = self._get_kernel_op_map(op_summary, kernels)
            self._ascend_op_analyse(op_summary, op_statistic, self._dynamic_status, launch_ops)
            graph_ids = np.unique(op_summary['Model ID']).tolist()
            points = self._ascend_fpbp_analyse(op_summary, steptrace)
            if len(graph_ids) == 1:
                self._ascend_step_trace_analyse(steptrace)
            else:
                self._ascend_step_trace_analyse(steptrace_model)
            if self._dynamic_status:
                self._ascend_dynamic_net_analyse(op_summary)
            self._ascend_flops_analyse(op_summary, launch_ops)
            self._ascend_graph_memory_analyse(points)
            self._ascend_ms_analyze(mindstudio_profiler_output)
            self._ascend_graph_hccl_analyse(mindstudio_profiler_output, steptrace)
            self._ascend_graph_msadvisor_analyse(job_id)
            ProfilerInfo.set_graph_ids(graph_ids)

    def _ascend_graph_start(self):
        """Ascend graph mode start profiling."""
        op_range_file = os.path.join(self._framework_path, "op_range_" + str(self._rank_id))
        if os.path.exists(op_range_file):
            os.remove(op_range_file)
            logger.info("Clear old op range filer.")
        self._ascend_profiler.start()

    def _gpu_analyse(self):
        """Collect and analyse gpu performance data."""
        self._dev_id = context.get_context("device_id")
        self._rank_size = 1
        if GlobalComm.WORLD_COMM_GROUP == "nccl_world_group":
            self._dev_id = str(get_rank())

        if GlobalComm.INITED:
            self._rank_size = get_group_size()
        else:
            self._rank_size = int(os.getenv('RANK_SIZE', '1'))

        if self._has_started:
            self.stop()
        else:
            logger.info("No need to stop profiler because profiler has been stopped.")

        self._minddata_analyse(self._output_path)

        try:
            self._analyse_step_relation_info()
        except ProfilerException as err:
            logger.warning(err.message)
        finally:
            pass

    def _is_support_step_info_collect(self, analyse_step_trace=True):
        """Whether iteration related information needs to be parsed."""
        profiler_info = ProfilerInfo.get_profiler_info()
        graph_ids = profiler_info.get("graph_ids")
        if graph_ids and len(graph_ids) > 1:
            analyse_step_trace = False
            logger.warning(
                "[Profiler]Current model has multiple sub graphs, the segmentation of steps may be inaccurate.")
        if context.get_context("mode") == context.PYNATIVE_MODE:
            analyse_step_trace = False
            logger.warning(
                "[Profiler]Pynative mode does not support collecting step trace performance data currently.")
        if self._is_heterogeneous:
            analyse_step_trace = False
            logger.warning(
                "[Profiler]Profiler does not support collecting step trace performance data for heterogeneous "
                "scenarios currently.")
        return analyse_step_trace

    def _analyse_step_relation_info(self):
        """Parse iteration related information."""
        if not self._op_time:
            return
        reduce_op_type = self._get_step_reduce_op_type()
        timeline_generator = self._generate_timeline(reduce_op_type)
        parser = GpuFrameWorkParser(self._output_path, self._dev_id)
        graph_ids = parser.get_graph_ids()
        ProfilerInfo.set_graph_ids(graph_ids)
        self._analyse_step_trace(
            is_training_mode_flag=timeline_generator.check_op_name('Gradients'),
            is_gpu_kernel_async_launch_flag=timeline_generator.is_gpu_kernel_async_launch()
        )
        if self._dynamic_status:
            parser.analyse_dynamic_shape_data(self._timeline_meta)

    def _get_step_reduce_op_type(self):
        """Gets all communication operator names."""

        step_trace_original_filename = f'step_trace_profiling_{self._dev_id}.txt'
        step_trace_file_path = os.path.join(self._output_path, step_trace_original_filename)
        step_trace_file_path = validate_and_normalize_path(step_trace_file_path)
        reduce_op_type = []
        with open(step_trace_file_path, 'r') as f_obj:
            one_step_info = f_obj.readline().strip().split()
            # The communication operator starts at index 4.
            for reduce_item in one_step_info[4:]:
                reduce_op_type.append(reduce_item.split(',')[0].split('/')[-1])
        return reduce_op_type

    def _cpu_analyse(self):
        """Collect and analyse cpu performance data."""
        if self._has_started:
            self.stop()
        else:
            logger.info("No need to stop profiler because profiler has been stopped.")

        if not self._op_time:
            return
        try:
            timeline_generator = CpuTimelineGenerator(self._output_path, self._rank_id, context.get_context("mode"))
            timeline_generator.init_timeline(pretty=self._pretty_json)
            timeline_generator.write_timeline(self._timeline_size_limit_byte)
            timeline_generator.write_timeline_summary()
        except (ProfilerIOException, ProfilerFileNotFoundException, RuntimeError) as err:
            logger.warning('Fail to write timeline data: %s', err)
            raise RuntimeError('Fail to write timeline data.') from err
        if context.get_context("mode") == context.PYNATIVE_MODE:
            raise RuntimeError("Currently, the CPU platform does not support Pynative mode to collect performance "
                               "data.")

    def _analyse_step_trace(self, source_path=None, framework_parser=None, is_training_mode_flag=True,
                            is_gpu_kernel_async_launch_flag=False):
        """
        Analyse step trace data and save the result.

        Args:
            source_path (str): The directory that contains the step trace original data.
            framework_parser (FrameworkParser): The framework parse instance.
            is_training_mode_flag (bool): Whether in training mode or not.
        """
        logger.info("Begin to parse step trace.")
        # construct output path
        dev_id = self._rank_id if self._device_target == DeviceTarget.ASCEND.value else self._dev_id
        step_trace_intermediate_file_path = os.path.join(
            self._output_path,
            f'step_trace_raw_{dev_id}_detail_time.csv'
        )
        point_info_file_path = os.path.join(
            self._output_path,
            f'step_trace_point_info_{dev_id}.json'
        )
        step_trace_intermediate_file_path = validate_and_normalize_path(step_trace_intermediate_file_path)
        point_info_file_path = validate_and_normalize_path(point_info_file_path)

        if self._device_target and self._device_target == DeviceTarget.GPU.value:
            if context.get_context("mode") != context.PYNATIVE_MODE:
                input_file_path = os.path.join(self._output_path, f'step_trace_profiling_{self._dev_id}.txt')
                input_file_path = validate_and_normalize_path(input_file_path)
                parser = GpuStepTraceParser(input_dir=input_file_path,
                                            output_file_path=step_trace_intermediate_file_path,
                                            is_training_mode=is_training_mode_flag,
                                            is_gpu_kernel_async_launch=is_gpu_kernel_async_launch_flag)
                parser.parse_and_save()
                point_info = parser.record_point_info(point_info_file_path)
                # print parser result
                parser.show()
                logger.info("Finish saving the intermediate result: %s", step_trace_intermediate_file_path)
                logger.info("The point info is: %s", point_info)

                return point_info, is_training_mode_flag
            return {}, is_training_mode_flag

        # whether keep the first step
        skip_first_step_flag = framework_parser.check_op_name(INIT_OP_NAME)
        # recognize inference or training mode
        is_training_mode_flag = framework_parser.check_op_name("Gradients")
        # parser the step trace files and save the result to disk
        source_path = validate_and_normalize_path(source_path)
        parser = AscendStepTraceParser(input_dir=source_path,
                                       output_file_path=step_trace_intermediate_file_path,
                                       skip_first_step=skip_first_step_flag,
                                       is_training_mode=is_training_mode_flag)
        parser.set_task_id_op_name_dict(framework_parser.to_task_id_full_op_name_dict())
        parser.parse_and_save()
        point_info = parser.record_point_info(point_info_file_path)

        # print parser result
        parser.show()
        logger.info("Finish saving the intermediate result: %s", step_trace_intermediate_file_path)
        logger.info("The point info is: %s", point_info)

        return point_info, is_training_mode_flag

    def _generate_timeline(self, reduce_op_type):
        """Used for gpu, generate timeline info, write to json format file."""
        try:
            timeline_generator = GpuTimelineGenerator(self._output_path, self._dev_id, self._rank_size,
                                                      context.get_context("mode"))
            timeline_generator.init_timeline(reduce_op_type)
            self._timeline_meta = timeline_generator.write_timeline(self._timeline_size_limit_byte)
            timeline_generator.write_timeline_summary()
            return timeline_generator
        except (ProfilerIOException, ProfilerFileNotFoundException, RuntimeError) as err:
            logger.warning('Fail to write timeline data: %s', err)
            raise RuntimeError('Fail to write timeline data.') from err

    def _analyse_memory_usage(self, points):
        """Analyse memory usage data."""
        integrator = Integrator(self._output_path, self._rank_id)
        aicore_detail_data = integrator.get_aicore_detail_data()
        memory_parser = MemoryUsageParser(self._output_path, self._rank_id, pretty=self._pretty_json)
        memory_parser.init_memory_usage_info(aicore_detail_data, points)
        memory_parser.write_memory_files()

    def _get_profiling_job_id(self, offline_path):
        """Get profiling job id, which was generated by ada service.

        Returns:
            str, profiling job id.
        """

        if offline_path:
            self._output_path = os.path.join(offline_path, 'profiler')

        job_id = ""
        job_dirs = filter(lambda item: item.startswith('JOB') or item.startswith('PROF') and os.path.isdir(
            os.path.join(self._output_path, item)), os.listdir(self._output_path))
        sorted_job_dirs = sorted(
            job_dirs, key=lambda x: os.path.getmtime(os.path.join(self._output_path, x)), reverse=True)

        for dir_name in sorted_job_dirs:
            prof_dir = os.path.join(self._output_path, dir_name)
            device_dir = [dir for dir in os.listdir(prof_dir) \
                          if dir.startswith('device') and os.path.isdir(os.path.join(prof_dir, dir))]
            job_dir = os.path.join(self._output_path, dir_name, device_dir[0])

            if get_file_path(job_dir, "start_info") is None:
                logger.warning("Find profiling job path %s, but host_start.log not exist, "
                               "profiler will ignore this job dir.", job_dir)
                continue

            info_file_path = get_file_path(job_dir, "info.json")
            if info_file_path is None:
                logger.warning("Find profiling job path %s, but info.json not exist, "
                               "profiler will ignore this job dir.", job_dir)
                continue

            prof_rank_id, prof_device_id = self._parse_info_json(info_file_path)
            job_start_time = self._parse_job_start_time(prof_dir)

            if offline_path:
                if self._rank_id != prof_rank_id:
                    continue
                self._start_time = int(job_start_time)
            else:
                if self._dev_id != prof_device_id and self._rank_id != prof_rank_id:
                    logger.debug("Find profiling find job path %s, but not current training device id. "
                                 "Current training rank id %s, but job path rank id: %s, "
                                 "profiler will ignore this job dir.", job_dir, self._rank_id, prof_rank_id)
                    continue

                if job_start_time < self._start_time:
                    logger.warning("Find profiling job path %s, but start_time(%d) is earlier than this training "
                                   "start_time(%d), profiler will ignore this job dir.",
                                   job_dir, job_start_time, self._start_time)
                    continue

            job_id = os.path.join(dir_name, device_dir[0])
            break

        if not job_id:
            msg = "Fail to get profiling job, output path is {}, " \
                  "please check whether job dir or prof dir(name startswith JOB or PROF) in output path " \
                  "was generated, or may be the device id from job dir dismatch the " \
                  "device_id in current process.".format(self._output_path)
            logger.warning(msg)

        return job_id

    def _query_op_type_info(self):
        """
        Query AICORE operator type information.

        Returns:
            list[list], the AICORE operator type and execution time information.
        """
        integrator = Integrator(self._output_path, self._rank_id)
        return integrator.get_aicore_data()

    def _query_op_detail_info(self, op_type_order):
        """
        Query AICORE operator detail information.

        Args:
            op_type_order(list): The name of the op type in order.

        Returns:
            dict, the AICORE operator detail information.
        """

        op_type_condition = {}
        if self._filt_optype_names:
            op_type_condition['not_in'] = self._filt_optype_names

        filter_condition = {
            'op_type': op_type_condition,
            'is_display_detail': False,
        }
        integrator = Integrator(self._output_path, self._rank_id)
        return integrator.query_and_sort_by_op_type(filter_condition, op_type_order)

    def _get_devid_rankid_and_devtarget(self):
        """Get device id and rank id and target of this training."""

        device_target = ""
        dev_id = ""
        rank_id = ""
        try:
            dev_id = str(context.get_context("device_id"))
            device_target = context.get_context("device_target").lower()
        except ValueError as err:
            logger.error("Profiling: fail to get context, %s", err)

        if not dev_id or not dev_id.isdigit():
            dev_id = str(get_local_rank()) if GlobalComm.INITED and device_target == DeviceTarget.ASCEND.value \
                else os.getenv('DEVICE_ID')
        if not dev_id or not dev_id.isdigit():
            dev_id = "0"
            logger.warning("Fail to get DEVICE_ID, use 0 instead.")

        if device_target and device_target not in [DeviceTarget.ASCEND.value, DeviceTarget.GPU.value,
                                                   DeviceTarget.CPU.value]:
            msg = "Profiling: unsupported backend: %s" % device_target
            raise RuntimeError(msg)

        rank_id = str(get_rank()) if GlobalComm.INITED and device_target == DeviceTarget.ASCEND.value \
            else os.getenv("RANK_ID")
        if not rank_id or not rank_id.isdigit():
            rank_id = "0"
            logger.warning(f"For '{self.__class__.__name__}', fail to get RANK_ID from environment, "
                           f"use 0 instead.")

        self._dev_id = dev_id
        self._device_target = device_target.lower()
        if device_target == DeviceTarget.GPU.value:
            self._rank_id = dev_id
        else:
            self._rank_id = rank_id

    def _get_output_path(self, kwargs):
        """Get output path of profiling data."""
        if os.getenv("MS_DIAGNOSTIC_DATA_PATH") and kwargs.get("output_path") is not None:
            logger.warning("Both parameter output_path and environment variable MS_DIAGNOSTIC_DATA_PATH"
                           " have values set, and the profiling data saving path is the value set "
                           "in parameter output_path")
        if kwargs.get("output_path") is None:
            if "output_path" in kwargs:
                kwargs.pop("output_path")
            # Environment variables are mainly set for the convenience of cloud profiler.
            output_path = os.getenv("MS_DIAGNOSTIC_DATA_PATH")
            if output_path:
                self._output_path = validate_and_normalize_path(output_path)
            else:
                output_path = "data"
                self._output_path = validate_and_normalize_path(output_path)
        else:
            output_path = kwargs.pop("output_path")
            self._output_path = validate_and_normalize_path(output_path)

        self._output_path = os.path.join(self._output_path, "profiler")
        if not os.path.exists(self._output_path):
            os.makedirs(self._output_path, exist_ok=True)
            os.chmod(self._output_path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
        else:
            logger.warning("The target dir already exists. "
                           "There may be some old profiling data, and they will be rewritten in the end.")
        self._framework_path = os.path.join(self._output_path, "FRAMEWORK")
        if not os.path.exists(self._framework_path):
            os.makedirs(self._framework_path, exist_ok=True)
            os.chmod(self._framework_path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)

    def _parser_kwargs(self, kwargs):
        """Parse kwargs vale."""
        self._op_time = kwargs.get("op_time", True)

        env_run_config = json.loads(os.getenv("MS_PROFILER_RUN_CONFIG", "{}"))
        params = list(kwargs.keys())
        if not env_run_config.get("start"):
            for param in params:
                if param not in DeviceSupportParam.__getattr__(f'{self._device_target}'.upper()).value:
                    logger.warning("%s is an invalid param which doesn't work.", param)
                    kwargs.pop(param)
                elif not self._op_time and param not in ALWAYS_VALID_PARAM:
                    logger.warning(f"When op_time is set to False, the parameter '{param}' setting is invalid.")

        if not isinstance(self._op_time, bool):
            logger.warning(f"For '{self.__class__.__name__}', the parameter op_time must be bool, "
                           f"but got type {type(self._op_time)}, it will be set to True.")
            self._op_time = True

        self._data_process = kwargs.pop("data_process", True)
        if not isinstance(self._data_process, bool):
            logger.warning(f"For '{self.__class__.__name__}', the parameter data_process must be bool, "
                           f"but got type {type(self._data_process)}, it will be set to True.")
            self._data_process = True

        timeline_limit = kwargs.pop("timeline_limit", 500)
        if isinstance(timeline_limit, bool) or not isinstance(timeline_limit, int):
            logger.warning(f"For '{self.__class__.__name__}', the parameter timeline_limit must be int, "
                           f"but got type {type(timeline_limit)}, it will be set to 500.")
            timeline_limit = 500
        if timeline_limit <= 0:
            logger.warning(
                "[Profiler]The 'timeline_limit' parameter must be greater than 0, it will be set to 500.")
            timeline_limit = 500
        self._timeline_size_limit_byte = timeline_limit * 1024 * 1024
        self._profile_framework = kwargs.pop("profile_framework", "all")
        if self._profile_framework not in ["memory", "time", "all", None]:
            logger.warning(f"For '{self.__class__.__name__}', the parameter profile_framework must be one of ['memory',"
                           f" 'time', 'all', None], but got {self._profile_framework}, it will be set to 'all'.")
            self._profile_framework = "all"

    def _host_info_analyse(self):
        """
        Read data from the csv file, and write it into timeline file, so the timeline can be show on tracing tool.
        """
        logger.info("Profiling HostInfo start.")
        host_dir = os.path.join(self._output_path, 'host_info')
        host_dir = validate_and_normalize_path(host_dir)
        if not os.path.exists(host_dir):
            logger.error("Host info directory: %s not exist.", host_dir)
            return
        csv_file_name = 'host_info_' + str(self._rank_id) + '.csv'
        json_file_name = 'timeline_' + str(self._rank_id) + '.json'
        memory_file_name = 'host_memory_' + str(self._rank_id) + '.csv'
        dataset_file_name = 'dataset_' + str(self._rank_id) + '.csv'
        host_info_file = os.path.join(self._output_path, 'host_info', csv_file_name)
        timeline_file = os.path.join(self._output_path, 'host_info', json_file_name)
        memory_file = os.path.join(self._output_path, 'host_info', memory_file_name)
        dataset_execution_file = os.path.join(self._output_path, 'host_info', dataset_file_name)
        _parse_host_info(host_info_file, timeline_file, memory_file)
        _calculate_dataset_execution_time(host_info_file, dataset_execution_file)
        logger.info("Profile HostInfo finished.")


def _offline_parse(offline_path):
    """Parse data in abnormal scenario, only support for host_info at present."""
    logger.info("Profiling HostInfo offline start.")
    host_dir = os.path.join(offline_path, 'profiler', 'host_info')
    host_dir = validate_and_normalize_path(host_dir)
    if not os.path.exists(host_dir):
        logger.error("Host info directory: %s not exist.", host_dir)
        return
    files = os.listdir(host_dir)
    for file in files:
        if not file.startswith("host_info_") or not file.endswith(".csv"):
            continue
        rank_id = file.split('_')[-1].split('.')[0]
        if not rank_id.isdigit():
            logger.info("Cannot get rank_id from file: %s, skip it", file)
            return
        host_info_file = os.path.join(host_dir, file)
        timeline_file = os.path.join(host_dir, f'timeline_{rank_id}.json')
        memory_file = os.path.join(host_dir, f'host_memory_{rank_id}.csv')
        dataset_execution_file = os.path.join(host_dir, f'dataset_{rank_id}.csv')
        _parse_host_info(host_info_file, timeline_file, memory_file)
        _calculate_dataset_execution_time(host_info_file, dataset_execution_file)
    logger.info("Profile HostInfo offline finished.")
