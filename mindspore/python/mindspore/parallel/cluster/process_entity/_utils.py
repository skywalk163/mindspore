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
# ============================================================================
"""Utils for ms_run"""
import os
import json
import requests
import mindspore.log as logger

def _generate_cmd(cmd, cmd_args, output_name):
    """
    Generates a command string to execute a Python script in the background, r
    edirecting the output to a log file.

    """
    if cmd not in ['python', 'pytest']:
        # If user don't set binary file name, defaulty use 'python' to launch the job.
        command = f"python {cmd} {' '.join(cmd_args)} > {output_name} 2>&1 &"
    else:
        command = f"{cmd} {' '.join(cmd_args)} > {output_name} 2>&1 &"
    return command


def _generate_cmd_args_list(cmd, cmd_args):
    """
    Generates arguments list for 'Popen'. It consists of a binary file name and subsequential arguments.
    """
    if cmd not in ['python', 'pytest']:
        # If user don't set binary file name, defaulty use 'python' to launch the job.
        return ['python'] + [cmd] + cmd_args
    return [cmd] + cmd_args


def _generate_cmd_args_list_with_core(cmd, cmd_args, cpu_start, cpu_end):
    """
    Generates arguments list for 'Popen'. It consists of a binary file name and subsequential arguments.
    """
    # Bind cpu cores to this process.
    taskset_args = ['taskset'] + ['-c'] + [str(cpu_start) + '-' + str(cpu_end)]
    final_cmd = []
    if cmd not in ['python', 'pytest']:
        # If user don't set binary file name, defaulty use 'python' to launch the job.
        final_cmd = taskset_args + ['python'] + [cmd] + cmd_args
    else:
        final_cmd = taskset_args + [cmd] + cmd_args
    logger.info(f"Launch process with command: {' '.join(final_cmd)}")
    return final_cmd


def _generate_url(addr, port):
    """
    Generates a url string by addr and port

    """
    url = f"http://{addr}:{port}/"
    return url


def _is_local_ip(ip_address):
    """
    Check if the current input IP address is a local IP address.

    """
    p = os.popen("ip -j addr")
    addr_info_str = p.read()
    p.close()
    addr_infos = json.loads(addr_info_str)
    for info in addr_infos:
        for addr in info["addr_info"]:
            if addr["local"] == ip_address:
                logger.info(f"IP address found on this node. Address info:{addr}.")
                return True
    return False


def _send_scale_num(url, scale_num):
    """
    Send an HTTP request to a specified URL, informing scale_num.

    """
    try:
        response = requests.post(url, data={"scale_num": scale_num}, timeout=100)
        response.raise_for_status()
        response_data = response.json()
        response_bool = bool(response_data)
        return response_bool
    except requests.exceptions.RequestException:
        return None


def _get_status_and_params(url):
    """
    Send an HTTP request to a specified URL to query status and retrieve partial parameters.

    """
    try:
        response = requests.get(url, timeout=100)
        response.raise_for_status()
        response_data = response.json()
        network_changed = response_data.get("network_changed")
        worker_num = response_data.get("worker_num")
        local_worker_num = response_data.get("local_worker_num")
        return network_changed, worker_num, local_worker_num
    except requests.exceptions.RequestException:
        return None
